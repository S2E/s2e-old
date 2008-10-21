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
#include "sockets.h"
#include "vl.h"
#include <fcntl.h>
#include "android_debug.h"

/* QSOCKET_CALL is used to deal with the fact that EINTR happens pretty
 * easily in QEMU since we use SIGALRM to implement periodic timers
 */
#ifdef _WIN32
#  define  QSOCKET_CALL(_ret,_cmd)   \
    do { _ret = (_cmd); } while ( _ret < 0 && WSAGetLastError() == WSAEINTR )
#else
#  define  QSOCKET_CALL(_ret,_cmd)   \
    do { _ret = (_cmd); } while ( _ret < 0 && errno == EINTR )
#endif

#ifdef _WIN32
const char*  socket_strerr(void)
{
	int   err = WSAGetLastError();
	switch (err) {
		case WSA_INVALID_HANDLE:
			return "invalid handle";
		case WSA_NOT_ENOUGH_MEMORY:
			return "not enough memory";
		case WSA_INVALID_PARAMETER:
			return "invalid parameter";
		case WSA_OPERATION_ABORTED:
			return "operation aborted";
		case WSA_IO_INCOMPLETE:
			return "incomplete i/o";
		case WSA_IO_PENDING:
			return "pending i/o";
		case WSAEINTR:
			return "interrupted";
		case WSAEBADF:
			return "bad file descriptor";
		case WSAEACCES:
			return "permission denied";
		case WSAEFAULT:
			return "bad address";
		case WSAEINVAL:
			return "invalid argument";
		case WSAEMFILE:
			return "too many opened files";
		case WSAEWOULDBLOCK:
			return "resource temporarily unavailable";
		case WSAEINPROGRESS:
			return "operation in progress";
		case WSAEALREADY:
			return "operation already in progress";
		case WSAENOTSOCK:
			return "socket operation not on socket";
		case WSAEDESTADDRREQ:
			return "destination address required";
		case WSAEMSGSIZE:
			return "message too long";
		case WSAEPROTOTYPE:
			return "wrong protocol for socket type";
		case WSAENOPROTOOPT:
			return "bad option for protocol";
		case WSAEPROTONOSUPPORT:
			return "protocol not supported";
	    case WSAEADDRINUSE:
			return "address already in use";
	    case WSAEADDRNOTAVAIL:
			return "address not available";
	    case WSAENETDOWN:
			return "network is down";
	    case WSAENETUNREACH:
			return "network unreachable";
	    case WSAENETRESET:
			return "network dropped connection on reset";
	    case WSAECONNABORTED:
			return "connection aborted";
	    case WSAECONNRESET:
			return "connection reset by peer";
	    case WSAENOBUFS:
			return "no buffer space available";
	    case WSAETIMEDOUT:
			return "connection timed out";
	    case WSAECONNREFUSED:
			return "connection refused";
	    case WSAEHOSTDOWN:
			return "host is down";
	    case WSAEHOSTUNREACH:
			return "no route to host";
		default:
			return "unknown/TODO";
	}
}
#endif

int socket_get_type(int fd)
{
    int  opt    = -1;
    int  optlen = sizeof(opt);
    getsockopt(fd, SOL_SOCKET, SO_TYPE, (void*)&opt, (void*)&optlen );
    return opt;
}

int socket_set_nonblock(int fd)
{
#ifdef _WIN32
    unsigned long opt = 1;
    return ioctlsocket(fd, FIONBIO, &opt);
#else
    return fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
}

int socket_set_blocking(int fd)
{
#ifdef _WIN32
    unsigned long opt = 0;
    return ioctlsocket(fd, FIONBIO, &opt);
#else
    return fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
}


int socket_set_xreuseaddr(int  fd)
{
#ifdef _WIN32
   /* on Windows, SO_REUSEADDR is used to indicate that several programs can
    * bind to the same port. this is completely different from the Unix
    * semantics. instead of SO_EXCLUSIVEADDR to ensure that explicitely prevent
    * this.
    */
    BOOL  flag = 1;
    return setsockopt( fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&flag, sizeof(flag) );
#else
    int  flag = 1;
    return setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag) );
#endif
}


int socket_set_oobinline(int  fd)
{
#ifdef _WIN32
    BOOL   flag = 1;
#else
    int    flag = 1;
#endif
    /* enable low-latency */
    return setsockopt( fd, SOL_SOCKET, SO_OOBINLINE, (const char*)&flag, sizeof(flag) );
}


int  socket_set_lowlatency(int  fd)
{
#ifdef _WIN32
    BOOL   flag = 1;
#else
    int    flag = 1;
#endif
    /* enable low-latency */
    return setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag) );
}


#ifdef _WIN32
#include <stdlib.h>

static void socket_cleanup(void)
{
    WSACleanup();
}

int socket_init(void)
{
    WSADATA Data;
    int ret, err;

    ret = WSAStartup(MAKEWORD(2,2), &Data);
    if (ret != 0) {
        err = WSAGetLastError();
        return -1;
    }
    atexit(socket_cleanup);
    return 0;
}

#else /* !_WIN32 */

int socket_init(void)
{
   return 0;   /* nothing to do on Unix */
}

#endif /* !_WIN32 */

#ifdef _WIN32

static void
socket_close_handler( void*  _fd )
{
    int   fd = (int)_fd;
    int   ret;
    char  buff[64];

    /* we want to drain the read side of the socket before closing it */
    do {
        ret = recv( fd, buff, sizeof(buff), 0 );
    } while (ret < 0 && socket_errno == EINTR);

    if (ret < 0 && socket_errno == EWOULDBLOCK)
        return;

    qemu_set_fd_handler( fd, NULL, NULL, NULL );
    closesocket( fd );
}

void
socket_close( int  fd )
{
    shutdown( fd, SD_BOTH );
    /* we want to drain the socket before closing it */
    qemu_set_fd_handler( fd, socket_close_handler, NULL, (void*)fd );
}

#else /* !_WIN32 */

#include <unistd.h>

void
socket_close( int  fd )
{
    shutdown( fd, SHUT_RDWR );
    close( fd );
}

#endif /* !_WIN32 */


static int
socket_bind_server( int  s, const struct sockaddr*  addr, socklen_t  addrlen, int  type )
{
    int   ret;

    socket_set_xreuseaddr(s);

    QSOCKET_CALL(ret, bind(s, addr, addrlen));
    if ( ret < 0 ) {
        dprint("could not bind server socket: %s", socket_errstr());
        socket_close(s);
        return -1;
    }

    if (type == SOCK_STREAM) {
        QSOCKET_CALL( ret, listen(s, 4) );
        if ( ret < 0 ) {
            dprint("could not listen server socket: %s", socket_errstr());
            socket_close(s);
            return -1;
        }
    }
    return  s;
}


static int
socket_connect_client( int  s, const struct sockaddr*  addr, socklen_t  addrlen )
{
    int  ret;

    QSOCKET_CALL(ret, connect(s, addr, addrlen));
    if ( ret < 0 ) {
        dprint( "could not connect client socket: %s\n", socket_errstr() );
        socket_close(s);
        return -1;
    }

    socket_set_nonblock( s );
    return s;
}


static int
socket_in_server( int  address, int  port, int  type )
{
    struct sockaddr_in  addr;
    int                 s;

    memset( &addr, 0, sizeof(addr) );
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(address);

    s = socket(PF_INET, type, 0);
    if (s < 0) return -1;

    return socket_bind_server( s, (struct sockaddr*) &addr, sizeof(addr), type );
}


static int
socket_in_client( struct sockaddr_in*  addr, int  type )
{
    int  s;

    s = socket(addr->sin_family, type, 0);
    if (s < 0) return -1;

    return socket_connect_client( s, (struct sockaddr*) addr, sizeof(*addr) );
}


int
socket_loopback_server( int  port, int  type )
{
    return socket_in_server( INADDR_LOOPBACK, port, type );
}

int
socket_loopback_client( int  port, int  type )
{
    struct sockaddr_in  addr;
    memset( &addr, 0, sizeof(addr) );
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    return socket_in_client( &addr, type );
}


int
socket_network_client( const char*  host, int  port, int  type )
{
    struct hostent*     hp;
    struct sockaddr_in  addr;

    hp = gethostbyname(host);
    if (hp == 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port   = htons(port);
    memcpy( &addr.sin_addr, hp->h_addr, hp->h_length );

    return socket_in_client( &addr, type );
}


int
socket_anyaddr_server( int  port, int  type )
{
    return socket_in_server( INADDR_ANY, port, type );
}

int
socket_accept_any( int  server_fd )
{
    int  fd;

    QSOCKET_CALL(fd, accept( server_fd, NULL, 0 ));
    if (fd < 0) {
        dprint( "could not accept client connection from fd %d: %s",
                server_fd, socket_errstr() );
        return -1;
    }

    /* set to non-blocking */
    socket_set_nonblock( fd );
    return fd;
}


#ifndef _WIN32

#include <sys/un.h>

static int
socket_unix_prepare_address( struct sockaddr_un*  addr, const char*  name )
{
    size_t  namelen = strlen(name);
    size_t  offset  = offsetof(struct sockaddr_un, sun_path);

    if (offset + namelen + 1 > sizeof(*addr)) {
        fprintf(stderr, "unix socket path too long\n");
        return -1;
    }
    memset( addr, 0, sizeof(*addr) );
    addr->sun_family = AF_LOCAL;
    memcpy( addr->sun_path, name, namelen+1 );
    return offset + namelen + 1;
}

int
socket_unix_server( const char*  name, int  type )
{
    struct sockaddr_un  addr;
    int                 addrlen;
    int                 s, ret;

    do {
        s = socket(AF_LOCAL, type, 0);
    } while (s < 0 && socket_errno == EINTR);
    if (s < 0) return -1;

    addrlen = socket_unix_prepare_address( &addr, name );
    if (addrlen < 0) {
        socket_close(s);
        return -1;
    }

    do {
        ret = unlink( addr.sun_path );
    } while (ret < 0 && errno == EINTR);

    return socket_bind_server( s, (struct sockaddr*) &addr, (socklen_t)addrlen, type );
}

int
socket_unix_client( const char*  name, int  type )
{
    struct sockaddr_un  addr;
    int                 addrlen;
    int                 s;

    do {
        s = socket(AF_LOCAL, type, 0);
    } while (s < 0 && socket_errno == EINTR);
    if (s < 0) return -1;

    addrlen = socket_unix_prepare_address( &addr, name );
    if (addrlen < 0) {
        socket_close(s);
        return -1;
    }

    return socket_connect_client( s, (struct sockaddr*) &addr, (socklen_t)addrlen );
}
#endif

