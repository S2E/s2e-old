/*
 *  inet and unix socket functions for qemu
 *
 *  (c) 2008 Gerd Hoffmann <kraxel@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "qemu_socket.h"
#include "qemu-common.h" /* for qemu_isdigit */

#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif

static int sockets_debug = 0;
static const int on=1, off=0;

static const char *sock_address_strfamily(SockAddress *s)
{
    switch (sock_address_get_family(s)) {
    case SOCKET_IN6:   return "ipv6";
    case SOCKET_INET:  return "ipv4";
    case SOCKET_UNIX:  return "unix";
    default:           return "????";
    }
}

int inet_listen(const char *str, char *ostr, int olen,
                SocketType socktype, int port_offset)
{
    SockAddress**  list;
    SockAddress*   e;
    unsigned       flags = SOCKET_LIST_PASSIVE;
    char addr[64];
    char port[33];
    char uaddr[256+1];
    const char *opts, *h;
    int slisten,pos,to,try_next,nn;

    /* parse address */
    if (str[0] == ':') {
        /* no host given */
        addr[0] = '\0';
        if (1 != sscanf(str,":%32[^,]%n",port,&pos)) {
            fprintf(stderr, "%s: portonly parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
    } else if (str[0] == '[') {
        /* IPv6 addr */
        if (2 != sscanf(str,"[%64[^]]]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: ipv6 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        flags |= SOCKET_LIST_FORCE_IN6;
    } else if (qemu_isdigit(str[0])) {
        /* IPv4 addr */
        if (2 != sscanf(str,"%64[0-9.]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: ipv4 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        flags |= SOCKET_LIST_FORCE_INET;
    } else {
        /* hostname */
        if (2 != sscanf(str,"%64[^:]:%32[^,]%n",addr,port,&pos)) {
            fprintf(stderr, "%s: hostname parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
    }

    /* parse options */
    opts = str + pos;
    h = strstr(opts, ",to=");
    to = h ? atoi(h+4) : 0;
    if (strstr(opts, ",ipv4")) {
        flags &= ~SOCKET_LIST_FORCE_IN6;
        flags |= SOCKET_LIST_FORCE_INET;
    }
    if (strstr(opts, ",ipv6")) {
        flags &= SOCKET_LIST_FORCE_INET;
        flags |= SOCKET_LIST_FORCE_IN6;
    }

    /* lookup */
    if (port_offset)
        snprintf(port, sizeof(port), "%d", atoi(port) + port_offset);

    list = sock_address_list_create( strlen(addr) ? addr : NULL,
                                       port,
                                       flags );
    if (list == NULL) {
        fprintf(stderr,"%s: getaddrinfo(%s,%s): %s\n", __FUNCTION__,
                addr, port, errno_str);
        return -1;
    }

    /* create socket + bind */
    for (nn = 0; list[nn] != NULL; nn++) {
        SocketFamily  family;

        e      = list[nn];
        family = sock_address_get_family(e);
        slisten = socket_create(family, socktype);
        if (slisten < 0) {
            fprintf(stderr,"%s: socket(%s): %s\n", __FUNCTION__,
                    sock_address_strfamily(e), errno_str);
            continue;
        }

        socket_set_xreuseaddr(slisten);
#ifdef IPV6_V6ONLY
        /* listen on both ipv4 and ipv6 */
        if (family == PF_INET6) {
            socket_set_ipv6only(slisten);
        }
#endif

        for (;;) {
            if (socket_bind(slisten, e) == 0) {
                if (sockets_debug)
                    fprintf(stderr,"%s: bind(%s,%s,%d): OK\n", __FUNCTION__,
                        sock_address_strfamily(e), uaddr, sock_address_get_port(e));
                goto listen;
            }
            socket_close(slisten);
            try_next = to && (sock_address_get_port(e) <= to + port_offset);
            if (!try_next || sockets_debug)
                fprintf(stderr,"%s: bind(%s,%s,%d): %s\n", __FUNCTION__,
                        sock_address_strfamily(e), uaddr, sock_address_get_port(e),
                        strerror(errno));
            if (try_next) {
                sock_address_set_port(e, sock_address_get_port(e) + 1);
                continue;
            }
            break;
        }
    }
    sock_address_list_free(list);
    fprintf(stderr, "%s: FAILED\n", __FUNCTION__);
    return -1;

listen:
    if (socket_listen(slisten,1) != 0) {
        perror("listen");
        socket_close(slisten);
        return -1;
    }
    if (ostr) {
        if (flags & SOCKET_LIST_FORCE_IN6) {
            snprintf(ostr, olen, "[%s]:%d%s", uaddr,
                     sock_address_get_port(e) - port_offset, opts);
        } else {
            snprintf(ostr, olen, "%s:%d%s", uaddr,
                     sock_address_get_port(e) - port_offset, opts);
        }
    }
    sock_address_list_free(list);
    return slisten;
}

int inet_connect(const char *str, SocketType socktype)
{
    SockAddress**  list;
    SockAddress*   e;
    unsigned       flags = 0;
    char addr[64];
    char port[33];
    int sock, nn;

    /* parse address */
    if (str[0] == '[') {
        /* IPv6 addr */
        if (2 != sscanf(str,"[%64[^]]]:%32[^,]",addr,port)) {
            fprintf(stderr, "%s: ipv6 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        flags |= SOCKET_LIST_FORCE_IN6;
    } else if (qemu_isdigit(str[0])) {
        /* IPv4 addr */
        if (2 != sscanf(str,"%64[0-9.]:%32[^,]",addr,port)) {
            fprintf(stderr, "%s: ipv4 parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
        flags |= SOCKET_LIST_FORCE_INET;
    } else {
        /* hostname */
        if (2 != sscanf(str,"%64[^:]:%32[^,]",addr,port)) {
            fprintf(stderr, "%s: hostname parse error (%s)\n",
                    __FUNCTION__, str);
            return -1;
        }
    }

    /* parse options */
    if (strstr(str, ",ipv4")) {
        flags &= SOCKET_LIST_FORCE_IN6;
        flags |= SOCKET_LIST_FORCE_INET;
    }
    if (strstr(str, ",ipv6")) {
        flags &= SOCKET_LIST_FORCE_INET;
        flags |= SOCKET_LIST_FORCE_IN6;
    }

    /* lookup */
    list = sock_address_list_create(addr, port, flags);
    if (list == NULL) {
        fprintf(stderr,"getaddrinfo(%s,%s): %s\n",
                addr, port, errno_str);
        return -1;
    }

    for (nn = 0; list[nn] != NULL; nn++) {
        e     = list[nn];
        sock = socket_create(sock_address_get_family(e), socktype);
        if (sock < 0) {
            fprintf(stderr,"%s: socket(%s): %s\n", __FUNCTION__,
            sock_address_strfamily(e), errno_str);
            continue;
        }
        socket_set_xreuseaddr(sock);

        /* connect to peer */
        if (socket_connect(sock,e) < 0) {
            if (sockets_debug)
                fprintf(stderr, "%s: connect(%s,%s,%s,%s): %s\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port, strerror(errno));
            socket_close(sock);
            continue;
        }
        if (sockets_debug)
            fprintf(stderr, "%s: connect(%s,%s,%s,%s): OK\n", __FUNCTION__,
                        sock_address_strfamily(e),
                        sock_address_to_string(e), addr, port);

        goto EXIT;
    }
    sock = -1;
EXIT:
    sock_address_list_free(list);
    return sock;
}

#ifndef _WIN32

int unix_listen(const char *str, char *ostr, int olen)
{
    SockAddress  un;
    char         unpath[PATH_MAX];
    char *path, *upath, *opts;
    int sock, fd, len;

    opts = strchr(str, ',');
    if (opts) {
        len = opts - str;
        path = qemu_malloc(len+1);
        snprintf(path, len+1, "%.*s", len, str);
    } else
        path = qemu_strdup(str);

    if (path || strlen(path) > 0) {
        upath = path;
    } else {
        char *tmpdir = getenv("TMPDIR");
        snprintf(unpath, sizeof(unpath), "%s/qemu-socket-XXXXXX",
                 tmpdir ? tmpdir : "/tmp");
        /*
         * This dummy fd usage silences the mktemp() unsecure warning.
         * Using mkstemp() doesn't make things more secure here
         * though.  bind() complains about existing files, so we have
         * to unlink first and thus re-open the race window.  The
         * worst case possible is bind() failing, i.e. a DoS attack.
         */
        fd = mkstemp(unpath); close(fd);
        upath = unpath;
    }
    snprintf(ostr, olen, "%s%s", path, opts ? opts : "");

    sock = socket_unix_server(upath, SOCKET_STREAM);
    sock_address_done(&un);

    if (sock < 0) {
        fprintf(stderr, "bind(unix:%s): %s\n", upath, errno_str);
        goto err;
    }

    if (sockets_debug)
        fprintf(stderr, "bind(unix:%s): OK\n", upath);

    qemu_free(path);
    return sock;

err:
    qemu_free(path);
    socket_close(sock);
    return -1;
}

int unix_connect(const char *path)
{
    SockAddress  un;
    int ret, sock;

    sock = socket_create_unix(SOCKET_STREAM);
    if (sock < 0) {
        perror("socket(unix)");
        return -1;
    }

    sock_address_init_unix(&un, path);
    ret = socket_connect(sock, &un);
    sock_address_done(&un);
    if (ret < 0) {
        fprintf(stderr, "connect(unix:%s): %s\n", path, errno_str);
        return -1;
    }


    if (sockets_debug)
        fprintf(stderr, "connect(unix:%s): OK\n", path);
    return sock;
}

#else

int unix_listen(const char *path, char *ostr, int olen)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

int unix_connect(const char *path)
{
    fprintf(stderr, "unix sockets are not available on windows\n");
    return -1;
}

#endif
