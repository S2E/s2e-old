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
#include "sockets.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "vl.h"

int  proxy_log = 0;

void
proxy_LOG(const char*  fmt, ...)
{
    va_list  args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void
proxy_set_verbose(int  mode)
{
    proxy_log = mode;
}

/** Global connection list
 **/

static ProxyConnection  s_connections[1];

static void
hex_dump( void*   base, int  size, const char*  prefix )
{
    uint8_t*   p         = (uint8_t*)base;
    const int  max_count = 16;

    while (size > 0) {
        int          count = size > max_count ? max_count : size;
        int          n;
        const char*  space = prefix;

        for (n = 0; n < count; n++) {
            proxy_LOG( "%s%02x", space, p[n] );
            space = " ";
        }

        proxy_LOG( "%-*s", 4 + 3*(max_count-n), "" );

        for (n = 0; n < count; n++) {
            int  c = p[n];

            if (c < 32 || c > 127)
                c = '.';
            proxy_LOG( "%c", c );
        }
        proxy_LOG( "\n" );
        size -= count;
        p    += count;
    }
}


void
proxy_connection_init( ProxyConnection*     conn,
                       int                  socket,
                       struct sockaddr_in*  address,
                       ProxyService*        service )
{
    conn->socket    = socket;
    conn->address   = address[0];
    conn->service   = service;
    conn->next      = NULL;

    {
        uint32_t  ip   = ntohl(address->sin_addr.s_addr);
        uint16_t  port = ntohs(address->sin_port);
        int       type = socket_get_type(socket);

        snprintf( conn->name, sizeof(conn->name),
                  "%s:%d.%d.%d.%d:%d(%d)",
                  (type == SOCK_STREAM) ? "tcp" : "udp",
                  (ip >> 24) & 255, (ip >> 16) & 255,
                  (ip >> 8)  & 255, ip & 255, port,
                  socket );

        /* just in case */
        conn->name[sizeof(conn->name)-1] = 0;
    }

    conn->buffer_pos = 0;
    conn->buffer_len = 0;
    conn->buffer     = conn->buffer0;
}

void
proxy_connection_done( ProxyConnection*  conn )
{
    if (conn->buffer != conn->buffer0) {
        qemu_free(conn->buffer);
    }
}


int
proxy_connection_send( ProxyConnection*  conn )
{
    int  result = -1;
    int  fd     = conn->socket;
    int  avail  = conn->buffer_len - conn->buffer_pos;

    if (proxy_log) {
        PROXY_LOG("%s: sending %d bytes:\n", conn->name, avail );
        hex_dump( conn->buffer + conn->buffer_pos, avail, ">> " );
    }

    while (avail > 0) {
        int  n = send(fd, conn->buffer + conn->buffer_pos, avail, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return 0;
            PROXY_LOG("%s: error: %s\n", conn->name, strerror(errno));
            return -1;
        }
        conn->buffer_pos += n;
        avail            -= n;
    }
    return 1;
}

int
proxy_connection_receive( ProxyConnection*  conn )
{
    int  result = -1;
    int  fd     = conn->socket;
    int  avail  = conn->buffer_len - conn->buffer_pos;

    while (avail > 0) {
        int  n = recv(fd, conn->buffer + conn->buffer_pos, avail, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return 0;
            PROXY_LOG("%s: error: %s\n", conn->name, strerror(errno));
            return -1;
        }

        if (proxy_log) {
            PROXY_LOG("%s: received %d bytes:\n", conn->name, n );
            hex_dump( conn->buffer + conn->buffer_pos, n, ">> " );
        }

        conn->buffer_pos += n;
        avail            -= n;
    }
    return 1;
}

int
proxy_connection_receive_line( ProxyConnection*  conn )
{
    int  result = -1;
    int  fd     = conn->socket;

    for (;;) {
        char  c;
        int   n = recv(fd, &c, 1, 0);
        if (n == 0) {
            PROXY_LOG("%s: disconnected from server\n", conn->name );
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                PROXY_LOG("%s: blocked\n", conn->name);
                return 0;
            }
            PROXY_LOG("%s: error: %s\n", conn->name, strerror(errno));
            return -1;
        }

        if (c == '\n') {
            if (conn->buffer_pos > 0 && conn->buffer[conn->buffer_pos-1] == '\r')
                conn->buffer_pos -= 1;

            conn->buffer[conn->buffer_pos] = 0;

            PROXY_LOG("%s: received '%.*s'\n", conn->name,
                      conn->buffer_pos, conn->buffer);
            return 1;
        }

        conn->buffer[ conn->buffer_pos++ ] = c;
        if (conn->buffer_pos == conn->buffer_len) {
            PROXY_LOG("%s: line received from proxy is too long\n", conn->name);
            return -1;
        }
    }
}



static void
proxy_connection_insert( ProxyConnection*  conn, ProxyConnection*  after )
{
    conn->next        = after->next;
    after->next->prev = conn;
    after->next       = conn;
    conn->prev        = after;
}

static void
proxy_connection_remove( ProxyConnection*  conn )
{
    conn->prev->next = conn->next;
    conn->next->prev = conn->prev;

    conn->next = conn->prev = conn;
}

/** Global service list
 **/

#define  MAX_SERVICES  4

static  ProxyService*  s_services[ MAX_SERVICES ];
static  int            s_num_services;
static  int            s_init;

static void  proxy_manager_atexit( void );

static void
proxy_manager_init(void)
{
    s_init = 1;
    s_connections->next = s_connections;
    s_connections->prev = s_connections;
    atexit( proxy_manager_atexit );
}


extern int
proxy_manager_add_service( ProxyService*  service )
{
    if (!service || s_num_services >= MAX_SERVICES)
        return -1;

    if (!s_init)
        proxy_manager_init();

    s_services[s_num_services++] = service;
    return 0;
}


extern void
proxy_manager_atexit( void )
{
    ProxyConnection*  conn = s_connections->next;
    int               n;

    /* free all proxy connections */
    while (conn != s_connections) {
        ProxyConnection*  next = conn->next;
        conn->service->conn_free( conn );
        conn = next;
    }
    conn->next = conn;
    conn->prev = conn;

    /* free all proxy services */
    for (n = s_num_services; n-- > 0;) {
        ProxyService*  service = s_services[n];
        service->serv_free( service->opaque );
    }
    s_num_services = 0;
}


void
proxy_connection_free( ProxyConnection*  conn,
                       ProxyEvent        event )
{
    if (conn) {
        int  fd = conn->socket;

        proxy_connection_remove(conn);

        if (event != PROXY_EVENT_NONE)
            conn->ev_func( conn->ev_opaque, event );

        conn->service->conn_free(conn);
    }
}


int
proxy_manager_add( int                  socket,
                   struct sockaddr_in*  address,
                   void*                ev_opaque,
                   ProxyEventFunc       ev_func )
{
    int  n;

    if (!s_init) {
        proxy_manager_init();
    }

    socket_set_nonblock(socket);

    for (n = 0; n < s_num_services; n++) {
        ProxyService*     service = s_services[n];
        ProxyConnection*  conn    = service->serv_connect( service->opaque,
                                                           socket,
                                                           address );
        if (conn != NULL) {
            conn->ev_opaque = ev_opaque;
            conn->ev_func   = ev_func;
            proxy_connection_insert(conn, s_connections->prev);
            return 0;
        }
    }
    return -1;
}


/* remove an on-going proxified socket connection from the manager's list.
 * this is only necessary when the socket connection must be canceled before
 * the connection accept/refusal occured
 */
void
proxy_manager_del( int  socket )
{
    ProxyConnection*  conn = s_connections->next;
    for ( ; conn != s_connections; conn = conn->next ) {
        if (conn->socket == socket) {
            int  fd = conn->socket;
            proxy_connection_remove(conn);
            conn->service->conn_free(conn);
            socket_close(fd);
            return;
        }
    }
}

/* this function is called to update the select file descriptor sets
 * with those of the proxified connection sockets that are currently managed */
void
proxy_manager_select_fill( int  *pcount, fd_set*  read_fds, fd_set*  write_fds, fd_set*  err_fds)
{
    ProxyConnection*  conn;

    if (!s_init)
        proxy_manager_init();

    conn = s_connections->next;
    for ( ; conn != s_connections; conn = conn->next ) {
        unsigned  flags = conn->service->conn_select(conn);
        int       fd    = conn->socket;

        if (!flags)
            continue;

        if (*pcount < fd+1)
            *pcount = fd+1;

        if (flags & PROXY_SELECT_READ) {
            FD_SET( fd, read_fds );
        }
        if (flags & PROXY_SELECT_WRITE) {
            FD_SET( fd, write_fds );
        }
        if (flags & PROXY_SELECT_ERROR) {
            FD_SET( fd, err_fds );
        }
    }
}

/* this function is called to act on proxified connection sockets when network events arrive */
void
proxy_manager_poll( fd_set*  read_fds, fd_set*  write_fds, fd_set*  err_fds )
{
    ProxyConnection*  conn = s_connections->next;
    while (conn != s_connections) {
        ProxyConnection*  next  = conn->next;
        int               fd    = conn->socket;
        unsigned          flags = 0;

        if ( FD_ISSET(fd, read_fds) )
            flags |= PROXY_SELECT_READ;
        if ( FD_ISSET(fd, write_fds) )
            flags |= PROXY_SELECT_WRITE;
        if ( FD_ISSET(fd, err_fds) )
            flags |= PROXY_SELECT_ERROR;

        if (flags != 0) {
            conn->service->conn_poll( conn, flags );
        }
        conn = next;
    }
}


int
proxy_base64_encode( const char*  src, int  srclen,
                     char*        dst, int  dstlen )
{
    static const char cb64[64]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char*       srcend = src + srclen;
    int               result = 0;

    while (src+3 <= srcend && result+4 <= dstlen)
    {
        dst[result+0] = cb64[ src[0] >> 2 ];
        dst[result+1] = cb64[ ((src[0] & 3) << 4) | ((src[1] & 0xf0) >> 4) ];
        dst[result+2] = cb64[ ((src[1] & 0xf) << 2) | ((src[2] & 0xc0) >> 6) ];
        dst[result+3] = cb64[ src[2] & 0x3f ];
        src    += 3;
        result += 4;
    }

    if (src < srcend) {
        unsigned char  in[4];

        if (result+4 > dstlen)
            return -1;

        in[0] = src[0];
        in[1] = src+1 < srcend ? src[1] : 0;
        in[2] = src+2 < srcend ? src[2] : 0;

        dst[result+0] = cb64[ in[0] >> 2 ];
        dst[result+1] = cb64[ ((in[0] & 3) << 4) | ((in[1] & 0xf0) >> 4) ];
        dst[result+2] = (unsigned char) (src+1 < srcend ? cb64[ ((in[1] & 0xf) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
        dst[result+3] = (unsigned char) (src+2 < srcend ? cb64[ in[2] & 0x3f ] : '=');
        result += 4;
    }
    return result;
}

int
proxy_resolve_server( struct sockaddr_in*  addr,
                      const char*          servername,
                      int                  servernamelen,
                      int                  serverport )
{
    char              name0[64], *name = name0;
    int               result = -1;
    struct hostent*   host;

    if (servernamelen < 0)
        servernamelen = strlen(servername);

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port   = htons(serverport);

    if (servernamelen >= sizeof(name0)) {
        name = qemu_malloc(servernamelen+1);
        if (name == NULL)
            return -1;
    }

    memcpy(name, servername, servernamelen);
    name[servernamelen] = 0;

    host = gethostbyname(name);
    if (host == NULL) {
        PROXY_LOG("%s: can't resolve proxy server name '%s'\n",
                  __FUNCTION__, name);
        goto Exit;
    }

    addr->sin_addr = *(struct in_addr*)host->h_addr;
    {
        uint32_t  a = ntohl(addr->sin_addr.s_addr);
        PROXY_LOG("server name '%s' resolved to %d.%d.%d.%d\n", name, (a>>24)&255, (a>>16)&255,(a>>8)&255,a&255);
    }
    result = 0;

Exit:
    if (name != name0)
        qemu_free(name);

    return result;
}


