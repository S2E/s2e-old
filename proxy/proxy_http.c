/* Copyright (C) 2007-2008 The Android Open Source Project
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
#include "proxy_int.h"
#include "proxy_http.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "vl.h"

typedef enum {
    HTTP_NONE = 0,
    HTTP_CONNECTING,           /* connecting to the server */
    HTTP_SEND_HEADER,          /* connected, sending header to the server */
    HTTP_RECEIVE_ANSWER_LINE1,
    HTTP_RECEIVE_ANSWER_LINE2  /* connected, reading server's answer */
} HttpConnectionState;


typedef struct {
    ProxyConnection      root[1];
    HttpConnectionState  state;
} HttpConnection;


typedef struct {
    ProxyService        root[1];
    struct sockaddr_in  server_addr;  /* server address and port */
    char*               footer;      /* the footer contains the static parts of the */
    int                 footer_len;  /* connection header, we generate it only once */
    char                footer0[512];
} HttpService;


static void
http_connection_free( HttpConnection*  conn )
{
    proxy_connection_done(conn->root);
    qemu_free(conn);
}


#define  HTTP_VERSION  "1.1"

static int
http_connection_init( HttpConnection*  conn )
{
    HttpService*      service = (HttpService*) conn->root->service;
    ProxyConnection*  root    = conn->root;
    char*             p       = root->buffer0;
    char*             end     = p + sizeof(root->buffer0);
    int               wlen, ret;
    uint32_t          address  = ntohl(conn->root->address.sin_addr.s_addr);
    int               port     = ntohs(conn->root->address.sin_port);

    root->buffer_pos = 0;
    root->buffer     = p;

    p += snprintf(p, end-p, "CONNECT %d.%d.%d.%d:%d HTTP/" HTTP_VERSION "\r\n",
                 (address >> 24) & 0xff, (address >> 16) & 0xff,
                 (address >> 8)  & 0xff, address & 0xff, port);
    if (p >= end) goto Overflow;

    p += snprintf(p, end-p, "%.*s", service->footer_len, service->footer);

    if (p >= end) {
    Overflow:
        PROXY_LOG("%s: buffer overflow in proxy connection header\n", root->name);
        return -1;
    }

    root->buffer_len = (p - root->buffer);

    ret = connect( root->socket,
                   (struct sockaddr*) &service->server_addr,
                   sizeof(service->server_addr) );
    if (ret == 0) {
        /* immediate connection ?? */
        conn->state = HTTP_SEND_HEADER;
        PROXY_LOG("%s: immediate connection\n", root->name);
    }
    else {
        if (socket_errno == EINPROGRESS || socket_errno == EWOULDBLOCK) {
            conn->state = HTTP_CONNECTING;
            PROXY_LOG("%s: connecting\n", conn->root->name);
        }
        else {
            PROXY_LOG("%s: cannot connect to proxy: %s\n", root->name, strerror(errno));
            return -1;
        }
    }
    return 0;
}


static unsigned
http_connection_select( HttpConnection*  conn )
{
    unsigned  flags;

    switch (conn->state) {
        case HTTP_RECEIVE_ANSWER_LINE1:
        case HTTP_RECEIVE_ANSWER_LINE2:
            flags = PROXY_SELECT_READ;
            break;

        case HTTP_CONNECTING:
        case HTTP_SEND_HEADER:
            flags = PROXY_SELECT_WRITE;
            break;

        default:
            flags = 0;
    };
    return flags;
}

static void
http_connection_poll( HttpConnection*  conn,
                      unsigned         flags )
{
    int               ret;
    ProxyConnection*  root = conn->root;

    switch (conn->state)
    {
        case HTTP_CONNECTING:
            PROXY_LOG("%s: connected to http proxy, sending header\n", root->name);
            conn->state = HTTP_SEND_HEADER;
            break;

        case HTTP_SEND_HEADER:
            {
                int  ret = proxy_connection_send(root);

                if (ret < 0) {
                    proxy_connection_free( root, PROXY_EVENT_SERVER_ERROR );
                    return;
                }
                if (ret == 0)
                    return;

                root->buffer_len = sizeof(root->buffer0);
                root->buffer_pos = 0;
                conn->state      = HTTP_RECEIVE_ANSWER_LINE1;
                PROXY_LOG("%s: header sent, receiving first answer line\n", root->name);
            }
            break;

        case HTTP_RECEIVE_ANSWER_LINE1:
        case HTTP_RECEIVE_ANSWER_LINE2:
            {
                int  ret = proxy_connection_receive_line(root);

                if (ret < 0) {
                    proxy_connection_free( root, PROXY_EVENT_SERVER_ERROR );
                    return;
                }
                if (ret == 0)
                    return;

                if (conn->state == HTTP_RECEIVE_ANSWER_LINE1) {
                    int  http1, http2, codenum;

                    if ( sscanf(root->buffer, "HTTP/%d.%d %d", &http1, &http2, &codenum) != 3 ) {
                        PROXY_LOG( "%s: invalid answer from proxy: '%s'\n",
                                   root->name, root->buffer );
                        proxy_connection_free( root, PROXY_EVENT_SERVER_ERROR );
                        return;
                    }

                    /* success is 2xx */
                    if (codenum/2 != 100) {
                        PROXY_LOG( "%s: connection refused, error=%d\n",
                                   root->name, codenum );
                        proxy_connection_free( root, PROXY_EVENT_CONNECTION_REFUSED );
                        return;
                    }
                    PROXY_LOG("%s: receiving second answer line\n", root->name);
                    conn->state      = HTTP_RECEIVE_ANSWER_LINE2;
                    root->buffer_pos = 0;
                } else {
                    /* ok, we're connected */
                    PROXY_LOG("%s: connection succeeded\n", root->name);
                    proxy_connection_free( root, PROXY_EVENT_CONNECTED );
                }
            }
            break;

        default:
            PROXY_LOG("%s: invalid state for read event: %d\n", root->name, conn->state);
    }
}

static void
http_service_free( HttpService*  service )
{
    PROXY_LOG("%s\n", __FUNCTION__);
    if (service->footer != service->footer0)
        qemu_free(service->footer);
    qemu_free(service);
}


static ProxyConnection*
http_service_connect( HttpService*         service,
                      int                  socket,
                      struct sockaddr_in*  address )
{
    HttpConnection*  conn = qemu_mallocz(sizeof(*conn));
    int              sock_type = socket_get_type(socket);

    /* the HTTP proxy can only handle TCP connections */
    if (sock_type != SOCK_STREAM)
        return NULL;

    /* if the client tries to directly connect to the proxy, let it do so */
    if (address->sin_addr.s_addr == service->server_addr.sin_addr.s_addr &&
        address->sin_port        == service->server_addr.sin_port)
        return NULL;

    proxy_connection_init( conn->root, socket, address, service->root );

    if ( http_connection_init( conn ) < 0 ) {
        http_connection_free( conn );
        return NULL;
    }

    return conn->root;
}


int
proxy_http_setup( const char*         servername,
                  int                 servernamelen,
                  int                 serverport,
                  int                 num_options,
                  const ProxyOption*  options )
{
    HttpService*        service;
    struct sockaddr_in  server_addr;
    const ProxyOption*  opt_nocache   = NULL;
    const ProxyOption*  opt_keepalive = NULL;
    const ProxyOption*  opt_auth_user = NULL;
    const ProxyOption*  opt_auth_pass = NULL;
    const ProxyOption*  opt_user_agent = NULL;

    if (servernamelen < 0)
        servernamelen = strlen(servername);

    PROXY_LOG( "%s: creating http proxy service connecting to: %.*s:%d\n",
               __FUNCTION__, servernamelen, servername, serverport );

    /* resolve server address */
    if (proxy_resolve_server(&server_addr, servername,
                             servernamelen, serverport) < 0)
    {
        return -1;
    }

    /* create service object */
    service = qemu_mallocz(sizeof(*service));
    if (service == NULL) {
        PROXY_LOG("%s: not enough memory to allocate new proxy service\n", __FUNCTION__);
        return -1;
    }

    service->server_addr = server_addr;

    /* parse options */
    {
        const ProxyOption*  opt = options;
        const ProxyOption*  end = opt + num_options;

        for ( ; opt < end; opt++ ) {
            switch (opt->type) {
                case PROXY_OPTION_HTTP_NOCACHE:     opt_nocache    = opt; break;
                case PROXY_OPTION_HTTP_KEEPALIVE:   opt_keepalive  = opt; break;
                case PROXY_OPTION_AUTH_USERNAME:    opt_auth_user  = opt; break;
                case PROXY_OPTION_AUTH_PASSWORD:    opt_auth_pass  = opt; break;
                case PROXY_OPTION_HTTP_USER_AGENT:  opt_user_agent = opt; break;
                default: ;
            }
        }
    }

    /* prepare footer */
    {
        int    wlen;
        char*  p    = service->footer0;
        char*  end  = p + sizeof(service->footer0);

        /* no-cache */
        if (opt_nocache) {
            p += snprintf(p, end-p, "Pragma: no-cache\r\nCache-Control: no-cache\r\n");
            if (p >= end) goto FooterOverflow;
        }
        /* keep-alive */
        if (opt_keepalive) {
            p += snprintf(p, end-p, "Connection: Keep-Alive\r\nProxy-Connection: Keep-Alive\r\n");
            if (p >= end) goto FooterOverflow;
        }
        /* authentication */
        if (opt_auth_user && opt_auth_pass) {
            char  user_pass[256];
            char  encoded[512];
            int   uplen;

            uplen = snprintf( user_pass, sizeof(user_pass), "%.*s:%.*s",
                              opt_auth_user->string_len, opt_auth_user->string,
                              opt_auth_pass->string_len, opt_auth_pass->string );

            if (uplen >= sizeof(user_pass)) goto FooterOverflow;

            wlen = proxy_base64_encode(user_pass, uplen, encoded, (int)sizeof(encoded));
            if (wlen < 0) {
                PROXY_LOG( "could not base64 encode '%.*s'\n", uplen, user_pass);
                goto FooterOverflow;
            }

            p += snprintf(p, end-p, "Proxy-authorization: Basic %.*s\r\n", wlen, encoded);
            if (p >= end) goto FooterOverflow;
        }
        /* user agent */
        if (opt_user_agent) {
            p += snprintf(p, end-p, "User-Agent: %.*s\r\n",
                          opt_user_agent->string_len,
                          opt_user_agent->string);
            if (p >= end) goto FooterOverflow;
        }

        p += snprintf(p, end-p, "\r\n");

        if (p >= end) {
        FooterOverflow:
            PROXY_LOG( "%s: buffer overflow when creating connection footer\n",
                       __FUNCTION__);
            http_service_free(service);
            return -1;
        }

        service->footer     = service->footer0;
        service->footer_len = (p - service->footer);
    }

    PROXY_LOG( "%s: creating HTTP Proxy Service Footer is (len=%d):\n'%.*s'\n",
               __FUNCTION__, service->footer_len,
               service->footer_len, service->footer );

    service->root->opaque       = service;
    service->root->serv_free    = (ProxyServiceFreeFunc)       http_service_free;
    service->root->serv_connect = (ProxyServiceConnectFunc)    http_service_connect;
    service->root->conn_free    = (ProxyConnectionFreeFunc)    http_connection_free;
    service->root->conn_select  = (ProxyConnectionSelectFunc)  http_connection_select;
    service->root->conn_poll    = (ProxyConnectionPollFunc)    http_connection_poll;

    if (proxy_manager_add_service( service->root ) < 0) {
        http_service_free(service);
        return -1;
    }
    return 0;
}

