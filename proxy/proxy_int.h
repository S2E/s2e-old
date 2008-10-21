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
#ifndef _PROXY_INT_H
#define _PROXY_INT_H

#include "proxy_common.h"
#include "sockets.h"

extern int  proxy_log;

extern void
proxy_LOG(const char*  fmt, ...);

#define  PROXY_LOG(...)   \
    do { if (proxy_log) proxy_LOG(__VA_ARGS__); } while (0)


/* sockets proxy manager internals */

typedef struct ProxyConnection   ProxyConnection;
typedef struct ProxyService      ProxyService;


/* root ProxyConnection object */
struct ProxyConnection {
    int                 socket;
    struct sockaddr_in  address;  /* for debugging */
    ProxyConnection*    next;
    ProxyConnection*    prev;
    ProxyEventFunc      ev_func;
    void*               ev_opaque;
    ProxyService*       service;

    /* the following is useful for all types of services */
    char                name[64];    /* for debugging purposes */
    int                 buffer_pos;
    int                 buffer_len;
    char*               buffer;
    char                buffer0[ 1024 ];

    /* rest of data depend on ProxyService */
};



extern void
proxy_connection_init( ProxyConnection*     conn,
                       int                  socket,
                       struct sockaddr_in*  address,
                       ProxyService*        service );

extern void
proxy_connection_done( ProxyConnection*  conn );

extern void
proxy_connection_free( ProxyConnection*  conn,
                       ProxyEvent        event );

/* tries to send data from the connection's buffer to the proxy.
 * returns 1 when all data has been sent (i.e. buffer_pos == buffer_len),
 * 0 if there is still some data to send, or -1 in case of error
 */
extern int
proxy_connection_send( ProxyConnection*  conn );

/* tries to receive data from the connection's buffer from the proxy
 * returns 1 when all data has been received (buffer_pos == buffer_len)
 * returns 0 if there is still some data to receive
 * returns -1 in case of error
 */
extern int
proxy_connection_receive( ProxyConnection*  conn );

/* tries to receive a line of text from the proxy
 * returns 1 when a line has been received
 * returns 0 if there is still some data to receive
 * returns -1 in case of error
 */
extern int
proxy_connection_receive_line( ProxyConnection*  conn );

/* base64 encode a source string, returns size of encoded result,
 * or -1 if there was not enough room in the destination buffer
 */
extern int
proxy_base64_encode( const char*  src, int  srclen,
                     char*        dst, int  dstlen );

extern int
proxy_resolve_server( struct sockaddr_in*  addr,
                      const char*          servername,
                      int                  servernamelen,
                      int                  serverport );

/* a ProxyService is really a proxy server and associated options */

enum {
    PROXY_SELECT_READ  = (1 << 0),
    PROXY_SELECT_WRITE = (1 << 1),
    PROXY_SELECT_ERROR = (1 << 2)
};

/* destroy a given proxy service */
typedef void              (*ProxyServiceFreeFunc)      ( void*  opaque );

/* tries to create a new proxified connection, returns NULL if the service can't
 * handle this address */
typedef ProxyConnection*  (*ProxyServiceConnectFunc)( void*                opaque,
                                                      int                  socket,
                                                      struct sockaddr_in*  address );

/* free a given proxified connection */
typedef void              (*ProxyConnectionFreeFunc)   ( ProxyConnection*  conn );

/* return flags corresponding to the select() events to wait to a proxified connection */
typedef unsigned          (*ProxyConnectionSelectFunc) ( ProxyConnection*  conn );

/* action a proxy connection when select() returns certain events for its socket */
typedef void              (*ProxyConnectionPollFunc)   ( ProxyConnection*  conn,
                                                         unsigned          select_flags );

struct ProxyService {
    void*                      opaque;
    ProxyServiceFreeFunc       serv_free;
    ProxyServiceConnectFunc    serv_connect;
    ProxyConnectionFreeFunc    conn_free;
    ProxyConnectionSelectFunc  conn_select;
    ProxyConnectionPollFunc    conn_poll;
};

extern int
proxy_manager_add_service( ProxyService*  service );


#endif /* _PROXY_INT_H */

