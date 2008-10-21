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
/* headers to use the BSD sockets */
#ifndef QEMU_SOCKET_H
#define QEMU_SOCKET_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define socket_errno     WSAGetLastError()
#define socket_errstr()  socket_strerr()

#undef EINTR
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINTR       WSAEINTR
#define EINPROGRESS WSAEINPROGRESS

extern const char*  socket_strerr(void);

#else

/* if this code is included from slirp/ sources, don't include the
 * system header files because this might conflict with some of the
 * slirp definitions
 */
#include <errno.h>
#ifndef SLIRP_COMPILATION
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

#define socket_errno     errno
#define socket_errstr()  strerror(errno)

#endif /* !_WIN32 */

int  socket_init( void );

int  socket_get_type(int  fd);

/* set SO_REUSEADDR on Unix, SO_EXCLUSIVEADDR on Windows */
int  socket_set_xreuseaddr(int  fd);
int  socket_set_nonblock(int fd);
int  socket_set_blocking(int fd);
int  socket_set_lowlatency(int fd);
int  socket_set_oobinline(int  fd);
void socket_close(int  fd);

int  socket_loopback_server( int  port, int  type );
int  socket_loopback_client( int  port, int  type );
#ifndef _WIN32
int  socket_unix_server( const char*  name, int  type );
int  socket_unix_client( const char*  name, int  type );
#endif
int  socket_network_client( const char*  host, int  port, int  type );
int  socket_anyaddr_server( int  port, int  type );
int  socket_accept_any( int  server_fd );

#endif /* QEMU_SOCKET_H */
