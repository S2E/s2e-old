/* Copyright (C) 2010 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * Contains UI-side of UI control protocol.
 */

#include "console.h"
#include "android/android.h"
#include "android/globals.h"
#include "android/looper.h"
#include "android/core-connection.h"
#include "android/async-utils.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/sync-utils.h"
#include "android/ui-ctl-common.h"
#include "android/ui-ctl-ui.h"

#define  PANIC(...) do { fprintf(stderr, __VA_ARGS__);  \
                         exit(1);                       \
                    } while (0)


/*
 * Enumerates states for the request reader in CoreUICtlClient instance.
 */
typedef enum CoreUICtlClientState {
    /* The reader is waiting on request header. */
    WAIT_HEADER,

    /* The reader is waiting on request data. */
    WAIT_DATA,
} CoreUICtlClientState;

/* Common descriptor for UI control clients. */
typedef struct UICtlCommon {
    /* Core connection instance for the UI control client. */
    CoreConnection*     core_connection;

    /* Socket wrapper for sync writes. */
    SyncSocket*         sync_writer;

    /* Socket descriptor for the UI control client. */
    int                 sock;
} UICtlCommon;

/* Descriptor for the Core->UI control client. */
typedef struct CoreUICtlClient {
    /* Common UI control client descriptor. */
    UICtlCommon             common;

    /* Current reader state. */
    CoreUICtlClientState    reader_state;

    /* Incoming request header. */
    UICtlHeader             req_header;

    /* Reader's buffer. */
    uint8_t*                reader_buffer;

    /* Offset in the reader's buffer where to read next chunk of data. */
    size_t                  reader_offset;

    /* Total number of bytes the reader expects to read. */
    size_t                  reader_bytes;
} CoreUICtlClient;

/* Descriptor for the UI->Core control client. */
typedef struct UICoreCtlClient {
    /* Common UI control client descriptor. */
    UICtlCommon         common;

    /* Socket wrapper for sync reads. */
    SyncSocket*         sync_reader;
} UICoreCtlClient;

/* One and only one Core->UI control client instance. */
static CoreUICtlClient  _core_ui_client;

/* One and only one UI->Core control client instance. */
static UICoreCtlClient  _ui_core_client;

/* Calculates timeout for transferring the given number of bytes via UI control
 * socket.
 * Return:
 *  Number of milliseconds during which the entire number of bytes is expected
 *  to be transferred.
 */
static int
_get_transfer_timeout(size_t data_size)
{
    // Min 200 millisec + one millisec for each transferring byte.
    // TODO: Come up with a better arithmetics here.
    return 200 + data_size;
}

/* Initializes UICtlCommon instance.
 * Param:
 *  console_socket - Addresses core's console service.
 *  name - Name of the core's service to attach to ("ui-core client",
 *  or "core-ui client").
 *  uictl_common - UICtlCommon instance to initialize.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_clientuictl_create_client(SockAddress* console_socket,
                           char* name,
                           UICtlCommon* uictl_common)
{
    char* connect_message = NULL;
    char switch_cmd[256];

    // Connect to the console service.
    uictl_common->core_connection = core_connection_create(console_socket);
    if (uictl_common->core_connection == NULL) {
        derror("UI control client %s is unable to connect to the console: %s\n",
               name, errno_str);
        return -1;
    }
    if (core_connection_open(uictl_common->core_connection)) {
        core_connection_free(uictl_common->core_connection);
        uictl_common->core_connection = NULL;
        derror("UI control client %s is unable to open the console: %s\n",
               name, errno_str);
        return -1;
    }
    snprintf(switch_cmd, sizeof(switch_cmd), "%s", name);
    if (core_connection_switch_stream(uictl_common->core_connection, switch_cmd,
                                      &connect_message)) {
        derror("Unable to connect to the UI control service %s: %s\n",
               name, connect_message ? connect_message : "");
        if (connect_message != NULL) {
            free(connect_message);
        }
        core_connection_close(uictl_common->core_connection);
        core_connection_free(uictl_common->core_connection);
        uictl_common->core_connection = NULL;
        return -1;
    }
    if (connect_message != NULL) {
        free(connect_message);
    }

    // Initialize UICtlCommon instance.
    uictl_common->sock = core_connection_get_socket(uictl_common->core_connection);
    uictl_common->sync_writer = syncsocket_init(uictl_common->sock);
    if (uictl_common->sync_writer == NULL) {
        derror("Unable to initialize sync writer for %s UI control client: %s\n",
               name, errno_str);
        return -1;
    }
    return 0;
}

/* Destroys UICtlCommon instance. */
static void
_uictlcommon_destroy(UICtlCommon* desc)
{
    if (desc->core_connection != NULL) {
        // Disable I/O callbacks.
        qemu_set_fd_handler(desc->sock, NULL, NULL, NULL);
        syncsocket_close(desc->sync_writer);
        syncsocket_free(desc->sync_writer);
        core_connection_close(desc->core_connection);
        core_connection_free(desc->core_connection);
        desc->core_connection = NULL;
    }
}

/*
 * Core->UI control client implementation.
 */

/* Implemented in android/qemulator.c */
extern void android_emulator_set_window_scale( double  scale, int  is_dpi );

/* Destroys CoreUICtlClient instance. */
static void
_core_ui_client_destroy()
{
    _uictlcommon_destroy(&_core_ui_client.common);
    if (_core_ui_client.reader_buffer != NULL &&
        _core_ui_client.reader_buffer != (uint8_t*)&_core_ui_client.req_header) {
        free(_core_ui_client.reader_buffer);
    }
}

/*
 * Handles UI control request received from the core.
 * Param:
 *  uictl - CoreUICtlClient instance that received the request.
 *  header - UI control request header.
 *  data - Request data formatted accordingly to the request type.
 */
static void
_core_ui_ctl_handle_request(CoreUICtlClient* uictl,
                            UICtlHeader* header,
                            uint8_t* data)
{
    switch (header->msg_type) {
        case ACORE_UICTL_SET_WINDOWS_SCALE:
        {
            UICtlSetWindowsScale* req = (UICtlSetWindowsScale*)data;
            android_emulator_set_window_scale(req->scale, req->is_dpi);
            break;
        }
        default:
            derror("Unknown Core UI control %d\n", header->msg_type);
            break;
    }
}

/*
 * Asynchronous I/O callback launched when UI control requests received from the
 * core are ready to be read.
 * Param:
 *  opaque - CoreUICtlClient instance.
 */
static void
_core_ui_client_read_cb(void* opaque)
{
    CoreUICtlClient* uictl = opaque;
    int  ret;

    // Read requests while they are immediately available.
    for (;;) {
        // Read next chunk of data.
        ret = read(uictl->common.sock,
                   uictl->reader_buffer + uictl->reader_offset,
                   uictl->reader_bytes - uictl->reader_offset);
        if (ret == 0) {
            /* disconnection ! */
            _core_ui_client_destroy();
            return;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                /* loop on EINTR */
                continue;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Chunk is not avalable at this point. Come back later.
                return;
            }
        }

        uictl->reader_offset += ret;
        if (uictl->reader_offset != uictl->reader_bytes) {
            // There are still some data left in the pipe.
            continue;
        }

        // All expected data has been read. Time to change the state.
        if (uictl->reader_state == WAIT_HEADER) {
            // Header has been read. Prepare for the data.
            uictl->reader_state = WAIT_DATA;
            uictl->reader_offset = 0;
            uictl->reader_bytes = uictl->req_header.msg_data_size;
            uictl->reader_buffer = malloc(uictl->reader_bytes);
            if (uictl->reader_buffer == NULL) {
                PANIC("Unable to allocate memory for UI control request.\n");
            }
        } else {
            _core_ui_ctl_handle_request(uictl, &uictl->req_header,
                                        uictl->reader_buffer);
            free(uictl->reader_buffer);
            uictl->reader_state = WAIT_HEADER;
            uictl->reader_offset = 0;
            uictl->reader_bytes = sizeof(uictl->req_header);
            uictl->reader_buffer = (uint8_t*)&uictl->req_header;
        }
    }
}

/*
 * UI->Core control client implementation.
 */

/* Sends UI request to the core.
 * Param:
 *  msg_type, msg_data, msg_data_size - Define the request.
 * Return:
 *  0 On success, or < 0 on failure.
 */
static int
_ui_core_ctl_send_request(uint8_t msg_type,
                          void* msg_data,
                          uint32_t msg_data_size)
{
    int status;
    UICtlHeader header;

    // Prepare and send the header.
    header.msg_type = msg_type;
    header.msg_data_size = msg_data_size;
    status = syncsocket_start_write(_ui_core_client.common.sync_writer);
    if (!status) {
        // Send the header.
        status = syncsocket_write(_ui_core_client.common.sync_writer, &header,
                                  sizeof(header),
                                  _get_transfer_timeout(sizeof(header)));
        // If there is request data, send it too.
        if (status > 0 && msg_data != NULL && msg_data_size > 0) {
            status = syncsocket_write(_ui_core_client.common.sync_writer, msg_data,
                                      msg_data_size,
                                      _get_transfer_timeout(msg_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(_ui_core_client.common.sync_writer);
    }
    if (status < 0) {
        derror("Unable to send UI control request: %s\n", errno_str);
    }
    return status;
}

/* Reads response to a UI control request from the core.
 * Param:
 *  resp - Upon success contains response header.
 *  resp_data - Upon success contains allocated reponse data (if any). The caller
 *      is responsible for deallocating of the memory returned in this parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_ui_core_ctl_get_response(UICtlRespHeader* resp, void** resp_data)
{
    int status =  syncsocket_start_read(_ui_core_client.sync_reader);
    if (!status) {
        // Read the header.
        status = syncsocket_read(_ui_core_client.sync_reader, resp,
                                 sizeof(UICtlRespHeader),
                                 _get_transfer_timeout(sizeof(UICtlRespHeader)));
        // Read response data (if any).
        if (status > 0 && resp->resp_data_size) {
            *resp_data = malloc(resp->resp_data_size);
            if (*resp_data == NULL) {
                PANIC("Unable to allocate response data buffer\n");
            }
            status = syncsocket_read(_ui_core_client.sync_reader, *resp_data,
                                     resp->resp_data_size,
                                     _get_transfer_timeout(resp->resp_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_read(_ui_core_client.sync_reader);
    }
    if (status < 0) {
        derror("Unable to get UI control response: %s\n", errno_str);
    }
    return status;
}

int
clientuictl_set_coarse_orientation(AndroidCoarseOrientation orient)
{
    UICtlSetCoarseOrientation msg;
    msg.orient = orient;
    return _ui_core_ctl_send_request(AUI_UICTL_SET_COARSE_ORIENTATION,
                                     &msg, sizeof(msg));
}

int
clientuictl_toggle_network()
{
    return _ui_core_ctl_send_request(AUI_UICTL_TOGGLE_NETWORK, NULL, 0);
}

int
clientuictl_trace_control(int start)
{
    UICtlTraceControl msg;
    msg.start = start;
    return _ui_core_ctl_send_request(AUI_UICTL_TRACE_CONTROL,
                                     &msg, sizeof(msg));
}

int
clientuictl_check_network_disabled()
{
    UICtlRespHeader resp;
    void* tmp = NULL;
    int status;

    status = _ui_core_ctl_send_request(AUI_UICTL_CHK_NETWORK_DISABLED, NULL, 0);
    if (status < 0) {
        return status;
    }
    status = _ui_core_ctl_get_response(&resp, &tmp);
    if (status < 0) {
        return status;
    }
    return resp.result;
}

int
clientuictl_get_netspeed(int index, NetworkSpeed** netspeed)
{
    UICtlGetNetSpeed req;
    UICtlRespHeader resp;
    UICtlGetNetSpeedResp* resp_data = NULL;
    int status;

    // Initialize and send the query.
    req.index = index;
    status = _ui_core_ctl_send_request(AUI_UICTL_GET_NETSPEED, &req, sizeof(req));
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _ui_core_ctl_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result) {
        NetworkSpeed* ret;
        // Allocate memory for the returning NetworkSpeed instance.
        // It includes: NetworkSpeed structure +
        // size of zero-terminated "name" and "display" strings saved in
        // resp_data.
        *netspeed = malloc(sizeof(NetworkSpeed) + 1 +
                           resp.resp_data_size - sizeof(UICtlGetNetSpeedResp));
        ret = *netspeed;

        // Copy data obtained from the core to the returning NetworkSpeed
        // instance.
        ret->upload = resp_data->upload;
        ret->download = resp_data->download;
        ret->name = (char*)ret + sizeof(NetworkSpeed);
        strcpy((char*)ret->name, resp_data->name);
        ret->display = ret->name + strlen(ret->name) + 1;
        strcpy((char*)ret->display, resp_data->name + strlen(resp_data->name) + 1);
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
clientuictl_get_netdelay(int index, NetworkLatency** netdelay)
{
    UICtlGetNetDelay req;
    UICtlRespHeader resp;
    UICtlGetNetDelayResp* resp_data = NULL;
    int status;

    // Initialize and send the query.
    req.index = index;
    status = _ui_core_ctl_send_request(AUI_UICTL_GET_NETDELAY, &req, sizeof(req));
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _ui_core_ctl_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result) {
        NetworkLatency* ret;
        // Allocate memory for the returning NetworkLatency instance.
        // It includes: NetworkLatency structure +
        // size of zero-terminated "name" and "display" strings saved in
        // resp_data.
        *netdelay = malloc(sizeof(NetworkLatency) + 1 +
                           resp.resp_data_size - sizeof(UICtlGetNetDelayResp));
        ret = *netdelay;

        // Copy data obtained from the core to the returning NetworkLatency
        // instance.
        ret->min_ms = resp_data->min_ms;
        ret->max_ms = resp_data->max_ms;
        ret->name = (char*)ret + sizeof(NetworkLatency);
        strcpy((char*)ret->name, resp_data->name);
        ret->display = ret->name + strlen(ret->name) + 1;
        strcpy((char*)ret->display, resp_data->name + strlen(resp_data->name) + 1);
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
clientuictl_get_qemu_path(int type, const char* filename, char** path)
{
    UICtlRespHeader resp;
    char* resp_data = NULL;
    int status;

    // Initialize and send the query.
    uint32_t req_data_size = sizeof(UICtlGetQemuPath) + strlen(filename) + 1;
    UICtlGetQemuPath* req = (UICtlGetQemuPath*)malloc(req_data_size);
    if (req == NULL) {
        PANIC("Unable to allocate query qemu path request\n");
    }
    req->type = type;
    strcpy(req->filename, filename);
    status = _ui_core_ctl_send_request(AUI_UICTL_GET_QEMU_PATH, req,
                                       req_data_size);
    if (status < 0) {
        return status;
    }

    // Obtain the response from the core.
    status = _ui_core_ctl_get_response(&resp, (void**)&resp_data);
    if (status < 0) {
        return status;
    }
    if (!resp.result && resp_data != NULL) {
        *path = strdup(resp_data);
    }
    if (resp_data != NULL) {
        free(resp_data);
    }
    return resp.result;
}

int
clientuictl_create(SockAddress* console_socket)
{
    // Connect to Core->UI service
    if (_clientuictl_create_client(console_socket, "core-ui control",
                                   &_core_ui_client.common)) {
        return -1;
    }
    _core_ui_client.reader_state = WAIT_HEADER;
    if (qemu_set_fd_handler(_core_ui_client.common.sock, _core_ui_client_read_cb,
                            NULL, &_core_ui_client)) {
        derror("Unable to set up UI control read callback\n");
        core_connection_close(_core_ui_client.common.core_connection);
        core_connection_free(_core_ui_client.common.core_connection);
        _core_ui_client.common.core_connection = NULL;
        return -1;
    }
    fprintf(stdout, "Core->UI client is now attached to the core %s\n",
            sock_address_to_string(console_socket));

    // Connect to UI->Core service
    if (_clientuictl_create_client(console_socket, "ui-core control",
                                   &_ui_core_client.common)) {
        _core_ui_client_destroy();
        return -1;
    }
    _ui_core_client.sync_reader = syncsocket_init(_ui_core_client.common.sock);
    if (_ui_core_client.sync_reader == NULL) {
        derror("Unable to create reader for CoreUICtlClient instance: %s\n",
               errno_str);
        _core_ui_client_destroy();
        return -1;
    }

    fprintf(stdout, "UI->Core client is now attached to the core %s\n",
            sock_address_to_string(console_socket));

    return 0;
}
