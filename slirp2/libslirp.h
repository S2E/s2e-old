#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#include <stdint.h>
#include "sockets.h"
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define socket_close  winsock2_socket_close3
#  include <winsock2.h>
#  undef socket_close
#else
#  include <sys/select.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int    inet_strtoip(const char*  str, uint32_t  *ip);
char*  inet_iptostr(uint32_t  ip);

void slirp_init(void);

void slirp_select_fill(int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(const uint8_t *pkt, int pkt_len);

/* you must provide the following functions: */
int slirp_can_output(void);
void slirp_output(const uint8_t *pkt, int pkt_len);

int slirp_redir(int is_udp, int host_port,
                uint32_t guest_addr, int guest_port);

int slirp_unredir(int is_udp, int host_port);

int slirp_add_dns_server(const SockAddress*  dns_addr);
int slirp_get_system_dns_servers(void);

extern const char *tftp_prefix;
extern char slirp_hostname[33];

#ifdef __cplusplus
}
#endif

#endif
