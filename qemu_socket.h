#ifndef _qemu_socket_h
#define _qemu_socket_h

#include "sockets.h"
#define  socket_error()  socket_errno
#define  closesocket     socket_close

#endif /* _qemu_socket_h */
