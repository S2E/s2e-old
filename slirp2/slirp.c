#include "slirp.h"
#include "proxy_common.h"
#include "android/utils/debug.h"  /* for dprint */
#include "android/utils/bufprint.h"
#include "android/android.h"
#include "sockets.h"

#define  D(...)   VERBOSE_PRINT(slirp,__VA_ARGS__)
#define  DN(...)  do { if (VERBOSE_CHECK(slirp)) dprintn(__VA_ARGS__); } while (0)

/* host address */
uint32_t our_addr_ip;
/* host dns address */
uint32_t dns_addr[DNS_ADDR_MAX];
int      dns_addr_count;

/* host loopback address */
uint32_t loopback_addr_ip;

/* address for slirp virtual addresses */
uint32_t  special_addr_ip;

/* virtual address alias for host */
uint32_t alias_addr_ip;

const uint8_t special_ethaddr[6] = {
    0x52, 0x54, 0x00, 0x12, 0x35, 0x00
};

uint8_t client_ethaddr[6] = {
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56
};

int do_slowtimo;
int link_up;
struct timeval tt;
FILE *lfd;

/* XXX: suppress those select globals */
fd_set *global_readfds, *global_writefds, *global_xfds;

char slirp_hostname[33];

int slirp_add_dns_server(const SockAddress*  new_dns_addr)
{
    int   dns_ip;

    if (dns_addr_count >= DNS_ADDR_MAX)
        return -1;

    dns_ip = sock_address_get_ip(new_dns_addr);
    if (dns_ip < 0)
        return -1;

    dns_addr[dns_addr_count++] = dns_ip;
    return 0;
}


#ifdef _WIN32

int slirp_get_system_dns_servers()
{
    FIXED_INFO *FixedInfo=NULL;
    ULONG    BufLen;
    DWORD    ret;
    IP_ADDR_STRING *pIPAddr;

    if (dns_addr_count > 0)
        return dns_addr_count;

    FixedInfo = (FIXED_INFO *)GlobalAlloc(GPTR, sizeof(FIXED_INFO));
    BufLen = sizeof(FIXED_INFO);

    if (ERROR_BUFFER_OVERFLOW == GetNetworkParams(FixedInfo, &BufLen)) {
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        FixedInfo = GlobalAlloc(GPTR, BufLen);
    }

    if ((ret = GetNetworkParams(FixedInfo, &BufLen)) != ERROR_SUCCESS) {
        printf("GetNetworkParams failed. ret = %08x\n", (u_int)ret );
        if (FixedInfo) {
            GlobalFree(FixedInfo);
            FixedInfo = NULL;
        }
        return -1;
    }

    D( "DNS Servers:");
    pIPAddr = &(FixedInfo->DnsServerList);
    while (pIPAddr && dns_addr_count < DNS_ADDR_MAX) {
        uint32_t  ip;
        D( "  %s", pIPAddr->IpAddress.String );
        if (inet_strtoip(pIPAddr->IpAddress.String, &ip) == 0) {
            if (ip == loopback_addr_ip)
                ip = our_addr_ip;
            if (dns_addr_count < DNS_ADDR_MAX)
                dns_addr[dns_addr_count++] = ip;
        }
        pIPAddr = pIPAddr->Next;
    }

    if (FixedInfo) {
        GlobalFree(FixedInfo);
        FixedInfo = NULL;
    }
    if (dns_addr_count <= 0)
        return -1;

    return dns_addr_count;
}

#else

int slirp_get_system_dns_servers(void)
{
    char buff[512];
    char buff2[256];
    FILE *f;

    if (dns_addr_count > 0)
        return dns_addr_count;

#ifdef CONFIG_DARWIN
    /* on Darwin /etc/resolv.conf is a symlink to /private/var/run/resolv.conf
     * in some siutations, the symlink can be destroyed and the system will not
     * re-create it. Darwin-aware applications will continue to run, but "legacy"
     * Unix ones will not.
     */
     f = fopen("/private/var/run/resolv.conf", "r");
     if (!f)
        f = fopen("/etc/resolv.conf", "r");  /* desperate attempt to sanity */
#else
    f = fopen("/etc/resolv.conf", "r");
#endif
    if (!f)
        return -1;

    DN("emulator: IP address of your DNS(s): ");
    while (fgets(buff, 512, f) != NULL) {
        if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1) {
            uint32_t  tmp_ip;

            if (inet_strtoip(buff2, &tmp_ip) < 0)
                continue;
            if (tmp_ip == loopback_addr_ip)
                tmp_ip = our_addr_ip;
            if (dns_addr_count < DNS_ADDR_MAX) {
                dns_addr[dns_addr_count++] = tmp_ip;
                if (dns_addr_count > 1)
                    DN(", ");
                DN("%s", inet_iptostr(tmp_ip));
            } else {
                DN("(more)");
                break;
            }
        }
    }
    DN("\n");
    fclose(f);

    if (!dns_addr_count)
        return -1;

    return dns_addr_count;
}

#endif

extern void  slirp_init_shapers();

void slirp_init(void)
{
#if DEBUG
    int   slirp_logmask = 0;
    char  slirp_logfile[512];
    {
        const char*  env = getenv( "ANDROID_SLIRP_LOGMASK" );
        if (env != NULL)
            slirp_logmask = atoi(env);
         else if (VERBOSE_CHECK(slirp))
            slirp_logmask = DEBUG_DEFAULT;
    }

    {
        char*  p   = slirp_logfile;
        char*  end = p + sizeof(slirp_logfile);

        p = bufprint_temp_file( p, end, "slirp.log" );
        if (p >= end) {
            dprint( "cannot create slirp log file in temporary directory" );
            slirp_logmask = 0;
        }
    }
    if (slirp_logmask) {
        dprint( "sending slirp logs with mask %x to %s", slirp_logmask, slirp_logfile );
        debug_init( slirp_logfile, slirp_logmask );
    }
#endif

    link_up = 1;

    if_init();
    ip_init();

    /* Initialise mbufs *after* setting the MTU */
    mbuf_init();

    /* set default addresses */
    inet_strtoip("127.0.0.1", &loopback_addr_ip);

    if (dns_addr_count == 0) {
        if (slirp_get_system_dns_servers() < 0) {
            dns_addr[0]    = loopback_addr_ip;
            dns_addr_count = 1;
            fprintf (stderr, "Warning: No DNS servers found\n");
        }
    }

    inet_strtoip(CTL_SPECIAL, &special_addr_ip);

    alias_addr_ip = special_addr_ip | CTL_ALIAS;
    getouraddr();

    slirp_init_shapers();
}

#define CONN_CANFSEND(so) (((so)->so_state & (SS_FCANTSENDMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define CONN_CANFRCV(so) (((so)->so_state & (SS_FCANTRCVMORE|SS_ISFCONNECTED)) == SS_ISFCONNECTED)
#define UPD_NFDS(x) if (nfds < (x)) nfds = (x)

/*
 * curtime kept to an accuracy of 1ms
 */
#ifdef _WIN32
static void updtime(void)
{
    struct _timeb tb;

    _ftime(&tb);
    curtime = (u_int)tb.time * (u_int)1000;
    curtime += (u_int)tb.millitm;
}
#else
static void updtime(void)
{
	gettimeofday(&tt, 0);

	curtime = (u_int)tt.tv_sec * (u_int)1000;
	curtime += (u_int)tt.tv_usec / (u_int)1000;

	if ((tt.tv_usec % 1000) >= 500)
	   curtime++;
}
#endif

void slirp_select_fill(int *pnfds,
                       fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    struct timeval timeout;
    int nfds;
    int tmp_time;

    /* fail safe */
    global_readfds = NULL;
    global_writefds = NULL;
    global_xfds = NULL;

    nfds = *pnfds;
    /*
        * First, TCP sockets
        */
    do_slowtimo = 0;
    if (link_up) {
        /*
            * *_slowtimo needs calling if there are IP fragments
            * in the fragment queue, or there are TCP connections active
            */
        do_slowtimo = ((tcb.so_next != &tcb) ||
                      (&ipq.ip_link != ipq.ip_link.next));

        for (so = tcb.so_next; so != &tcb; so = so_next) {
            so_next = so->so_next;

               /*
                * See if we need a tcp_fasttimo
                */
            if (time_fasttimo == 0 && so->so_tcpcb->t_flags & TF_DELACK)
                time_fasttimo = curtime; /* Flag when we want a fasttimo */

               /*
                * NOFDREF can include still connecting to local-host,
                * newly socreated() sockets etc. Don't want to select these.
                */
            if (so->so_state & SS_NOFDREF || so->s == -1)
                continue;

                        /*
                         * don't register proxified socked connections here
                         */
                         if ((so->so_state & SS_PROXIFIED) != 0)
                            continue;

               /*
                * Set for reading sockets which are accepting
                */
            if (so->so_state & SS_FACCEPTCONN) {
                                FD_SET(so->s, readfds);
                UPD_NFDS(so->s);
                continue;
            }

               /*
                * Set for writing sockets which are connecting
                */
            if (so->so_state & SS_ISFCONNECTING) {
                FD_SET(so->s, writefds);
                UPD_NFDS(so->s);
                continue;
            }

               /*
                * Set for writing if we are connected, can send more, and
                * we have something to send
                */
            if (CONN_CANFSEND(so) && so->so_rcv.sb_cc) {
                FD_SET(so->s, writefds);
                UPD_NFDS(so->s);
            }

               /*
                * Set for reading (and urgent data) if we are connected, can
                * receive more, and we have room for it XXX /2 ?
                */
            if (CONN_CANFRCV(so) && (so->so_snd.sb_cc < (so->so_snd.sb_datalen/2))) {
                FD_SET(so->s, readfds);
                FD_SET(so->s, xfds);
                UPD_NFDS(so->s);
            }
        }

        /*
            * UDP sockets
            */
        for (so = udb.so_next; so != &udb; so = so_next) {
            so_next = so->so_next;

            if ((so->so_state & SS_PROXIFIED) != 0)
                continue;

               /*
                * See if it's timed out
                */
            if (so->so_expire) {
                if (so->so_expire <= curtime) {
                    udp_detach(so);
                    continue;
                } else
                    do_slowtimo = 1; /* Let socket expire */
            }

               /*
                * When UDP packets are received from over the
                * link, they're sendto()'d straight away, so
                * no need for setting for writing
                * Limit the number of packets queued by this session
                * to 4.  Note that even though we try and limit this
                * to 4 packets, the session could have more queued
                * if the packets needed to be fragmented
                * (XXX <= 4 ?)
                */
            if ((so->so_state & SS_ISFCONNECTED) && so->so_queued <= 4) {
                FD_SET(so->s, readfds);
                UPD_NFDS(so->s);
            }
        }
    }

       /*
        * Setup timeout to use minimum CPU usage, especially when idle
        */

       /*
        * First, see the timeout needed by *timo
        */
    timeout.tv_sec = 0;
    timeout.tv_usec = -1;
    /*
        * If a slowtimo is needed, set timeout to 500ms from the last
        * slow timeout. If a fast timeout is needed, set timeout within
        * 200ms of when it was requested.
        */
    if (do_slowtimo) {
        /* XXX + 10000 because some select()'s aren't that accurate */
        timeout.tv_usec = ((500 - (curtime - last_slowtimo)) * 1000) + 10000;
        if (timeout.tv_usec < 0)
            timeout.tv_usec = 0;
        else if (timeout.tv_usec > 510000)
            timeout.tv_usec = 510000;

        /* Can only fasttimo if we also slowtimo */
        if (time_fasttimo) {
            tmp_time = (200 - (curtime - time_fasttimo)) * 1000;
            if (tmp_time < 0)
                tmp_time = 0;

            /* Choose the smallest of the 2 */
            if (tmp_time < timeout.tv_usec)
                timeout.tv_usec = (u_int)tmp_time;
        }
    }

        /*
         * now, the proxified sockets
         */
        proxy_manager_select_fill(&nfds, readfds, writefds, xfds);

        *pnfds = nfds;
}

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds)
{
    struct socket *so, *so_next;
    int ret;

    global_readfds = readfds;
    global_writefds = writefds;
    global_xfds = xfds;

	/* Update time */
	updtime();

	/*
	 * See if anything has timed out
	 */
	if (link_up) {
		if (time_fasttimo && ((curtime - time_fasttimo) >= 2)) {
			tcp_fasttimo();
			time_fasttimo = 0;
		}
		if (do_slowtimo && ((curtime - last_slowtimo) >= 499)) {
			ip_slowtimo();
			tcp_slowtimo();
			last_slowtimo = curtime;
		}
	}

	/*
	 * Check sockets
	 */
	if (link_up) {
		/*
		 * Check TCP sockets
		 */
		for (so = tcb.so_next; so != &tcb; so = so_next) {
			so_next = so->so_next;

			/*
			 * FD_ISSET is meaningless on these sockets
			 * (and they can crash the program)
			 */
			if (so->so_state & SS_NOFDREF || so->s == -1)
			   continue;

                        /*
                         * proxified sockets are polled later in this
                         * function.
                         */
                        if ((so->so_state & SS_PROXIFIED) != 0)
                            continue;

			/*
			 * Check for URG data
			 * This will soread as well, so no need to
			 * test for readfds below if this succeeds
			 */
			if (FD_ISSET(so->s, xfds))
			   sorecvoob(so);
			/*
			 * Check sockets for reading
			 */
			else if (FD_ISSET(so->s, readfds)) {
				/*
				 * Check for incoming connections
				 */
				if (so->so_state & SS_FACCEPTCONN) {
					tcp_connect(so);
					continue;
				} /* else */
				ret = soread(so);

				/* Output it if we read something */
				if (ret > 0)
				   tcp_output(sototcpcb(so));
			}

			/*
			 * Check sockets for writing
			 */
			if (FD_ISSET(so->s, writefds)) {
			  /*
			   * Check for non-blocking, still-connecting sockets
			   */
			  if (so->so_state & SS_ISFCONNECTING) {
			    /* Connected */
			    so->so_state &= ~SS_ISFCONNECTING;

			    ret = socket_send(so->s, (char*)&ret, 0);
			    if (ret < 0) {
			      /* XXXXX Must fix, zero bytes is a NOP */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;

			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    }
			    /* else so->so_state &= ~SS_ISFCONNECTING; */

			    /*
			     * Continue tcp_input
			     */
			    tcp_input((MBuf )NULL, sizeof(struct ip), so);
			    /* continue; */
			  } else
			    ret = sowrite(so);
			  /*
			   * XXXXX If we wrote something (a lot), there
			   * could be a need for a window update.
			   * In the worst case, the remote will send
			   * a window probe to get things going again
			   */
			}

			/*
			 * Probe a still-connecting, non-blocking socket
			 * to check if it's still alive
	 	 	 */
#ifdef PROBE_CONN
			if (so->so_state & SS_ISFCONNECTING) {
			  ret = socket_recv(so->s, (char *)&ret, 0);

			  if (ret < 0) {
			    /* XXX */
			    if (errno == EAGAIN || errno == EWOULDBLOCK ||
				errno == EINPROGRESS || errno == ENOTCONN)
			      continue; /* Still connecting, continue */

			    /* else failed */
			    so->so_state = SS_NOFDREF;

			    /* tcp_input will take care of it */
			  } else {
			    ret = socket_send(so->s, &ret, 0,0);
			    if (ret < 0) {
			      /* XXX */
			      if (errno == EAGAIN || errno == EWOULDBLOCK ||
				  errno == EINPROGRESS || errno == ENOTCONN)
				continue;
			      /* else failed */
			      so->so_state = SS_NOFDREF;
			    } else
			      so->so_state &= ~SS_ISFCONNECTING;

			  }
			  tcp_input((MBuf )NULL, sizeof(struct ip),so);
			} /* SS_ISFCONNECTING */
#endif
		}

		/*
		 * Now UDP sockets.
		 * Incoming packets are sent straight away, they're not buffered.
		 * Incoming UDP data isn't buffered either.
		 */
		for (so = udb.so_next; so != &udb; so = so_next) {
			so_next = so->so_next;

                        if ((so->so_state & SS_PROXIFIED) != 0)
                            continue;

			if (so->s != -1 && FD_ISSET(so->s, readfds)) {
                            sorecvfrom(so);
                        }
		}
	}

        /*
         * Now the proxified sockets
         */
        proxy_manager_poll(readfds, writefds, xfds);

	/*
	 * See if we can start outputting
	 */
	if (if_queued && link_up)
	   if_start();

	/* clear global file descriptor sets.
	 * these reside on the stack in vl.c
	 * so they're unusable if we're not in
	 * slirp_select_fill or slirp_select_poll.
	 */
	 global_readfds = NULL;
	 global_writefds = NULL;
	 global_xfds = NULL;
}

#define ETH_ALEN 6
#define ETH_HLEN 14

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/

#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/

struct ethhdr
{
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	unsigned short	h_proto;		/* packet type ID field	*/
};

struct arphdr
{
	unsigned short	ar_hrd;		/* format of hardware address	*/
	unsigned short	ar_pro;		/* format of protocol address	*/
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/
	unsigned short	ar_op;		/* ARP opcode (command)		*/

	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	unsigned char		ar_sip[4];		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	unsigned char		ar_tip[4];		/* target IP address		*/
};

void arp_input(const uint8_t *pkt, int pkt_len)
{
    struct ethhdr *eh = (struct ethhdr *)pkt;
    struct arphdr *ah = (struct arphdr *)(pkt + ETH_HLEN);
    uint8_t arp_reply[ETH_HLEN + sizeof(struct arphdr)];
    struct ethhdr *reh = (struct ethhdr *)arp_reply;
    struct arphdr *rah = (struct arphdr *)(arp_reply + ETH_HLEN);
    int ar_op;

    ar_op = ntohs(ah->ar_op);
    switch(ar_op) {
        uint32_t   ar_tip_ip;
    case ARPOP_REQUEST:
        ar_tip_ip = (ah->ar_tip[0] << 24) | (ah->ar_tip[1] << 16) | (ah->ar_tip[2] << 8);
        if (ar_tip_ip == special_addr_ip) {
            if ( CTL_IS_DNS(ah->ar_tip[3]) || ah->ar_tip[3] == CTL_ALIAS)
                goto arp_ok;
            return;
        arp_ok:
            /* XXX: make an ARP request to have the client address */
            memcpy(client_ethaddr, eh->h_source, ETH_ALEN);

            /* ARP request for alias/dns mac address */
            memcpy(reh->h_dest, pkt + ETH_ALEN, ETH_ALEN);
            memcpy(reh->h_source, special_ethaddr, ETH_ALEN - 1);
            reh->h_source[5] = ah->ar_tip[3];
            reh->h_proto = htons(ETH_P_ARP);

            rah->ar_hrd = htons(1);
            rah->ar_pro = htons(ETH_P_IP);
            rah->ar_hln = ETH_ALEN;
            rah->ar_pln = 4;
            rah->ar_op = htons(ARPOP_REPLY);
            memcpy(rah->ar_sha, reh->h_source, ETH_ALEN);
            memcpy(rah->ar_sip, ah->ar_tip, 4);
            memcpy(rah->ar_tha, ah->ar_sha, ETH_ALEN);
            memcpy(rah->ar_tip, ah->ar_sip, 4);
            slirp_output(arp_reply, sizeof(arp_reply));
        }
        break;
    default:
        break;
    }
}

void slirp_input(const uint8_t *pkt, int pkt_len)
{
    MBuf m;
    int proto;

    if (pkt_len < ETH_HLEN)
        return;

    proto = ntohs(*(uint16_t *)(pkt + 12));
    switch(proto) {
    case ETH_P_ARP:
        arp_input(pkt, pkt_len);
        break;
    case ETH_P_IP:
        m = mbuf_alloc();
        if (!m)
            return;
        /* Note: we add to align the IP header */
        m->m_len = pkt_len + 2;
        memcpy(m->m_data + 2, pkt, pkt_len);

        m->m_data += 2 + ETH_HLEN;
        m->m_len -= 2 + ETH_HLEN;

        ip_input(m);
        break;
    default:
        break;
    }
}

/* output the IP packet to the ethernet device */
void if_encap(const uint8_t *ip_data, int ip_data_len)
{
    uint8_t buf[1600];
    struct ethhdr *eh = (struct ethhdr *)buf;

    if (ip_data_len + ETH_HLEN > sizeof(buf))
        return;

    memcpy(eh->h_dest, client_ethaddr, ETH_ALEN);
    memcpy(eh->h_source, special_ethaddr, ETH_ALEN - 1);
    /* XXX: not correct */
    eh->h_source[5] = CTL_ALIAS;
    eh->h_proto = htons(ETH_P_IP);
    memcpy(buf + sizeof(struct ethhdr), ip_data, ip_data_len);
    slirp_output(buf, ip_data_len + ETH_HLEN);
}

int slirp_redir(int is_udp, int host_port,
                uint32_t  guest_ip, int guest_port)
{
    if (is_udp) {
        if (!udp_listen(host_port,
                        guest_ip,
                        guest_port, 0))
            return -1;
    } else {
        if (!solisten(host_port, guest_ip, guest_port, 0))
            return -1;
    }
    return 0;
}

int  slirp_unredir(int  is_udp, int  host_port)
{
    if (is_udp)
        return udp_unlisten( host_port );
    else
        return sounlisten( host_port );
}

