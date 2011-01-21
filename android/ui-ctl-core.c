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

#include "qemu-common.h"
#include "android/globals.h"
#include "android/android.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/ui-ctl-common.h"
#include "android/ui-ctl-core.h"
#include "android/hw-sensors.h"
#include "telephony/modem_driver.h"
#include "trace.h"
#include "audio/audio.h"

/* Enumerates state values for UICoreCtl descriptor. */
typedef enum UICoreCtlState {
    /* UI message header is expected in the pipe. */
    UI_STATE_EXPECT_HEADER,
    /* UI message data are expected in the pipe. */
    UI_STATE_EXPECT_DATA
} UICoreCtlState;

/* Core UI control service descriptor used for UI->Core communication. */
typedef struct UICoreCtl {
    /* Reader to detect UI disconnection. */
    AsyncReader     async_reader;

    /* I/O associated with this descriptor. */
    LoopIo          io;

    /* Looper used to communicate user events. */
    Looper*         looper;

    /* Writer to send responses to UI requests. */
    SyncSocket*     sync_writer;

    /* Socket descriptor for this service. */
    int             sock;

    /* State of incoming requests. */
    UICoreCtlState  in_req_state;

    /* Incoming request header. */
    UICtlHeader     req_header;

    /* A buffer for small incoming requests. */
    uint8_t         req_data[256];

    /* Buffer to use for reading incoming request data. Depending on expected
     * incoming request size this buffer can point to req_data field of this
     * structure (for small requests), or can be allocated for large requests. */
    void*           req_data_buffer;
} UICoreCtl;

/* Core UI control service descriptor used for Core->UI communication. */
typedef struct CoreUICtl {
    /* I/O associated with this descriptor. */
    LoopIo          io;

    /* Looper associated with this descriptor. */
    Looper*         looper;

    /* Writer to send UI commands. */
    SyncSocket*     sync_writer;

    /* Socket descriptor for this service. */
    int             sock;
} CoreUICtl;

/* One and only one CoreUICtl instance. */
static CoreUICtl    _core_ui_ctl;

/* One and only one UICoreCtl instance. */
static UICoreCtl    _ui_core_ctl;

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

/*
 * Core -> UI control implementation
 */

/* Implemented in android/console.c */
extern void destroy_core_ui_ctl_client(void);

/* Sends request to the UI client.
 * Param:
 *  msg_type, msg_data, msg_data_size - Define core request to send.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_coreuictl_send_request(uint8_t msg_type,
                        void* msg_data,
                        uint32_t msg_data_size)
{
    UICtlHeader header;
    int status = syncsocket_start_write(_core_ui_ctl.sync_writer);
    if (!status) {

        // Initialize and send the header.
        header.msg_type = msg_type;
        header.msg_data_size = msg_data_size;
        status = syncsocket_write(_core_ui_ctl.sync_writer, &header, sizeof(header),
                                  _get_transfer_timeout(sizeof(header)));
        // If there is request data, send it too.
        if (status > 0 && msg_data != NULL && msg_data_size > 0) {
            status = syncsocket_write(_core_ui_ctl.sync_writer, msg_data,
                                      msg_data_size,
                                      _get_transfer_timeout(msg_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(_core_ui_ctl.sync_writer);
    }
    if (status < 0) {
        derror("Unable to send core UI control request: %s\n", errno_str);
    }
    return status;
}

/*
 * Asynchronous I/O callback for CoreUICtl instance.
 * We expect this callback to be called only on UI detachment condition. In this
 * case the event should be LOOP_IO_READ, and read should fail with errno set
 * to ECONNRESET.
 * Param:
 *  opaque - CoreUICtl instance.
 */
static void
_coreuictl_io_func(void* opaque, int fd, unsigned events)
{
    CoreUICtl* uictl = (CoreUICtl*)opaque;
    AsyncReader reader;
    AsyncStatus status;
    uint8_t read_buf[1];

    if (events & LOOP_IO_WRITE) {
        derror("Unexpected LOOP_IO_WRITE in coreuictl_io_func\n");
        return;
    }

    // Try to read
    asyncReader_init(&reader, read_buf, sizeof(read_buf), &uictl->io);
    status = asyncReader_read(&reader, &uictl->io);
    // We expect only error status here.
    if (status != ASYNC_ERROR) {
        derror("Unexpected read status %d in coreuictl_io_func\n", status);
        return;
    }
    // We expect only socket disconnection here.
    if (errno != ECONNRESET) {
        derror("Unexpected read error %d (%s) in coreuictl_io_func\n",
               errno, errno_str);
        return;
    }

    // Client got disconnectted.
    destroy_core_ui_ctl_client();
}

int
coreuictl_create(int fd)
{
    // Initialize _core_ui_ctl instance.
    _core_ui_ctl.sock = fd;
    _core_ui_ctl.looper = looper_newCore();
    loopIo_init(&_core_ui_ctl.io, _core_ui_ctl.looper, _core_ui_ctl.sock,
                _coreuictl_io_func, &_core_ui_ctl);
    loopIo_wantRead(&_core_ui_ctl.io);
    _core_ui_ctl.sync_writer = syncsocket_init(fd);
    if (_core_ui_ctl.sync_writer == NULL) {
        derror("Unable to initialize CoreUICtl writer: %s\n", errno_str);
        return -1;
    }
    return 0;
}

void
coreuictl_destroy()
{
    if (_core_ui_ctl.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_core_ui_ctl.io);
        looper_free(_core_ui_ctl.looper);
        _core_ui_ctl.looper = NULL;
    }
    if (_core_ui_ctl.sync_writer != NULL) {
        syncsocket_close(_core_ui_ctl.sync_writer);
        syncsocket_free(_core_ui_ctl.sync_writer);
    }
    _core_ui_ctl.sock = -1;
}

int
coreuictl_set_window_scale(double scale, int is_dpi)
{
    UICtlSetWindowsScale msg;
    msg.scale = scale;
    msg.is_dpi = is_dpi;
    return _coreuictl_send_request(ACORE_UICTL_SET_WINDOWS_SCALE, &msg,
                                   sizeof(msg));
}

/*
 * UI -> Core control implementation
 */

/* Implemented in android/console.c */
extern void destroy_ui_core_ctl_client(void);
/* Implemented in vl-android.c */
extern char* qemu_find_file(int type, const char* filename);

/* Properly initializes req_data_buffer field in UICoreCtl instance to receive
 * the expected incoming request data buffer.
 */
static uint8_t*
_alloc_req_data_buffer(UICoreCtl* uictl, uint32_t size)
{
    if (size < sizeof(uictl->req_data)) {
        // req_data can contain all request data.
        uictl->req_data_buffer = &uictl->req_data[0];
    } else {
        // Expected request us too large to fit into preallocated buffer.
        uictl->req_data_buffer = qemu_malloc(size);
    }
    return uictl->req_data_buffer;
}

/* Properly frees req_data_buffer field in UICoreCtl instance.
 */
static void
_free_req_data_buffer(UICoreCtl* uictl)
{
    if (uictl->req_data_buffer != &uictl->req_data[0]) {
        qemu_free(uictl->req_data_buffer);
        uictl->req_data_buffer = &uictl->req_data[0];
    }
}

/* Sends response back to the UI
 * Param:
 *  uictl - UICoreCtl instance to use for the response sending.
 *  resp - Response header.
 *  resp_data - Response data. Data size is defined by the header.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_uicorectl_send_response(UICoreCtl* uictl, UICtlRespHeader* resp, void* resp_data)
{
    int status = syncsocket_start_write(uictl->sync_writer);
    if (!status) {
        // Write the header
        status = syncsocket_write(uictl->sync_writer, resp,
                                  sizeof(UICtlRespHeader),
                                  _get_transfer_timeout(sizeof(UICtlRespHeader)));
        // Write response data (if any).
        if (status > 0 && resp_data != NULL && resp->resp_data_size != 0) {
            status = syncsocket_write(uictl->sync_writer, resp_data,
                                      resp->resp_data_size,
                                      _get_transfer_timeout(resp->resp_data_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(uictl->sync_writer);
    }
    if (status < 0) {
        derror("Unable to send UI control response: %s\n", errno_str);
    }
    return status;
}

/* Handles UI control request from the UI.
 * Param:
 *  uictl - UICoreCtl instance that received the request.
 *  req_header - Request header.
 *  req_data - Request data.
 */
static void
_handle_uictl_request(UICoreCtl* uictl,
                      const UICtlHeader* req_header,
                      const uint8_t* req_data)
{
    switch (req_header->msg_type) {
        case AUI_UICTL_SET_COARSE_ORIENTATION:
        {
            UICtlSetCoarseOrientation* req = (UICtlSetCoarseOrientation*)req_data;
            android_sensors_set_coarse_orientation(req->orient);
            break;
        }

        case AUI_UICTL_TOGGLE_NETWORK:
            qemu_net_disable = !qemu_net_disable;
            if (android_modem) {
                amodem_set_data_registration(
                        android_modem,
                qemu_net_disable ? A_REGISTRATION_UNREGISTERED
                    : A_REGISTRATION_HOME);
            }
            break;

        case AUI_UICTL_TRACE_CONTROL:
        {
            UICtlTraceControl* req = (UICtlTraceControl*)req_data;
            if (req->start) {
                start_tracing();
            } else {
                stop_tracing();
            }
            break;
        }

        case AUI_UICTL_CHK_NETWORK_DISABLED:
        {
            UICtlRespHeader resp;
            resp.resp_data_size = 0;
            resp.result = qemu_net_disable;
            _uicorectl_send_response(uictl, &resp, NULL);
            break;
        }

        case AUI_UICTL_GET_NETSPEED:
        {
            UICtlRespHeader resp;
            UICtlGetNetSpeedResp* resp_data = NULL;
            UICtlGetNetSpeed* req = (UICtlGetNetSpeed*)req_data;

            resp.resp_data_size = 0;
            resp.result = 0;

            if (req->index >= android_netspeeds_count ||
                android_netspeeds[req->index].name == NULL) {
                resp.result = -1;
            } else {
                const NetworkSpeed* netspeed = &android_netspeeds[req->index];
                // Calculate size of the response data:
                // fixed header + zero-terminated netspeed name.
                resp.resp_data_size = sizeof(UICtlGetNetSpeedResp) +
                                      strlen(netspeed->name) + 1;
                // Count in zero-terminated netspeed display.
                if (netspeed->display != NULL) {
                    resp.resp_data_size += strlen(netspeed->display) + 1;
                } else {
                    resp.resp_data_size++;
                }
                // Allocate and initialize response data buffer.
                resp_data =
                    (UICtlGetNetSpeedResp*)qemu_malloc(resp.resp_data_size);
                resp_data->upload = netspeed->upload;
                resp_data->download = netspeed->download;
                strcpy(resp_data->name, netspeed->name);
                if (netspeed->display != NULL) {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1,
                           netspeed->display);
                } else {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1, "");
                }
            }
            _uicorectl_send_response(uictl, &resp, resp_data);
            if (resp_data != NULL) {
                qemu_free(resp_data);
            }
            break;
        }

        case AUI_UICTL_GET_NETDELAY:
        {
            UICtlRespHeader resp;
            UICtlGetNetDelayResp* resp_data = NULL;
            UICtlGetNetDelay* req = (UICtlGetNetDelay*)req_data;

            resp.resp_data_size = 0;
            resp.result = 0;

            if (req->index >= android_netdelays_count ||
                android_netdelays[req->index].name == NULL) {
                resp.result = -1;
            } else {
                const NetworkLatency* netdelay = &android_netdelays[req->index];
                // Calculate size of the response data:
                // fixed header + zero-terminated netdelay name.
                resp.resp_data_size = sizeof(UICtlGetNetDelayResp) +
                                      strlen(netdelay->name) + 1;
                // Count in zero-terminated netdelay display.
                if (netdelay->display != NULL) {
                    resp.resp_data_size += strlen(netdelay->display) + 1;
                } else {
                    resp.resp_data_size++;
                }
                // Allocate and initialize response data buffer.
                resp_data =
                    (UICtlGetNetDelayResp*)qemu_malloc(resp.resp_data_size);
                resp_data->min_ms = netdelay->min_ms;
                resp_data->max_ms = netdelay->max_ms;
                strcpy(resp_data->name, netdelay->name);
                if (netdelay->display != NULL) {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1,
                           netdelay->display);
                } else {
                    strcpy(resp_data->name + strlen(resp_data->name) + 1, "");
                }
            }
            _uicorectl_send_response(uictl, &resp, resp_data);
            if (resp_data != NULL) {
                qemu_free(resp_data);
            }
            break;
        }

        case AUI_UICTL_GET_QEMU_PATH:
        {
            UICtlRespHeader resp;
            UICtlGetQemuPath* req = (UICtlGetQemuPath*)req_data;
            char* filepath = NULL;

            resp.resp_data_size = 0;
            resp.result = -1;
            filepath = qemu_find_file(req->type, req->filename);
            if (filepath != NULL) {
                resp.resp_data_size = strlen(filepath) + 1;
            }
            _uicorectl_send_response(uictl, &resp, filepath);
            if (filepath != NULL) {
                qemu_free(filepath);
            }
            break;
        }

        default:
            derror("Unknown UI control request %d\n", req_header->msg_type);
            break;
    }
}

/* Asynchronous read I/O callback launched when reading UI control requests.
 */
static void
_uicorectl_io_read(UICoreCtl* uictl)
{
    // Read whatever is expected from the socket.
    const AsyncStatus status =
        asyncReader_read(&uictl->async_reader, &uictl->io);

    switch (status) {
        case ASYNC_COMPLETE:
            switch (uictl->in_req_state) {
                case UI_STATE_EXPECT_HEADER:
                    // We just read the request header. Now we expect the data.
                    if (uictl->req_header.msg_data_size != 0) {
                        uictl->in_req_state = UI_STATE_EXPECT_DATA;
                        // Setup the reader to read expected amount of the data.
                        _alloc_req_data_buffer(uictl,
                                               uictl->req_header.msg_data_size);
                        asyncReader_init(&uictl->async_reader,
                                         uictl->req_data_buffer,
                                         uictl->req_header.msg_data_size,
                                         &uictl->io);
                    } else {
                        // Request doesn't contain data. Go ahead and handle it.
                        _handle_uictl_request(uictl, &uictl->req_header,
                                              uictl->req_data_buffer);
                        // Prepare for the next header.
                        asyncReader_init(&uictl->async_reader,
                                         &uictl->req_header,
                                         sizeof(uictl->req_header), &uictl->io);
                    }
                    break;

                case UI_STATE_EXPECT_DATA:
                    // Request header and data are received. Handle the request.
                    _handle_uictl_request(uictl, &uictl->req_header,
                                          uictl->req_data_buffer);
                    _free_req_data_buffer(uictl);
                    // Prepare for the next request.
                    uictl->in_req_state = UI_STATE_EXPECT_HEADER;
                    asyncReader_init(&uictl->async_reader, &uictl->req_header,
                                     sizeof(uictl->req_header), &uictl->io);
                    break;
            }
            break;
        case ASYNC_ERROR:
            loopIo_dontWantRead(&uictl->io);
            if (errno == ECONNRESET) {
                // UI has exited. We need to destroy the service.
                destroy_ui_core_ctl_client();
            }
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

/*
 * Asynchronous I/O callback launched when UI control is received from the UI.
 * Param:
 *  opaque - UICoreCtl instance.
 */
static void
_uicorectl_io_func(void* opaque, int fd, unsigned events)
{
    if (events & LOOP_IO_READ) {
        _uicorectl_io_read((UICoreCtl*)opaque);
    } else if (events & LOOP_IO_WRITE) {
        // We don't use async writer here, so we don't expect
        // any write callbacks.
        derror("Unexpected LOOP_IO_WRITE in _uicorectl_io_func\n");
    }
}

int
uicorectl_create(int fd)
{
    _ui_core_ctl.sock = fd;
    _ui_core_ctl.looper = looper_newCore();
    loopIo_init(&_ui_core_ctl.io, _ui_core_ctl.looper, _ui_core_ctl.sock,
                _uicorectl_io_func, &_ui_core_ctl);
    _ui_core_ctl.in_req_state = UI_STATE_EXPECT_HEADER;
    _ui_core_ctl.req_data_buffer = &_ui_core_ctl.req_data[0];
    asyncReader_init(&_ui_core_ctl.async_reader, &_ui_core_ctl.req_header,
                     sizeof(_ui_core_ctl.req_header), &_ui_core_ctl.io);
    _ui_core_ctl.sync_writer = syncsocket_init(fd);
    if (_ui_core_ctl.sync_writer == NULL) {
        derror("Unable to create writer for UICoreCtl instance: %s\n", errno_str);
        return -1;
    }
    return 0;
}

void
uicorectl_destroy()
{
    if (_ui_core_ctl.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_ui_core_ctl.io);
        looper_free(_ui_core_ctl.looper);
        _ui_core_ctl.looper = NULL;
    }
    if (_ui_core_ctl.sync_writer != NULL) {
        syncsocket_close(_ui_core_ctl.sync_writer);
        syncsocket_free(_ui_core_ctl.sync_writer);
    }
    _free_req_data_buffer(&_ui_core_ctl);
}
