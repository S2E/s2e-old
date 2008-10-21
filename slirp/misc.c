/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define WANT_SYS_IOCTL_H
#include <slirp.h>
#define  SLIRP_COMPILATION
#include "sockets.h"

u_int curtime, time_fasttimo, last_slowtimo, detach_time;
u_int detach_wait = 600000;	/* 10 minutes */
struct emu_t *tcpemu;

#ifndef HAVE_INET_ATON
int
inet_aton(const char* cp, struct in_addr* ia)
{
	u_int32_t addr = inet_addr(cp);
	if (addr == 0xffffffff)
		return 0;
	ia->s_addr = addr;
	return 1;
}
#endif

/*
 * Get our IP address and put it in our_addr
 */
void
getouraddr()
{
	char buff[256];
	struct hostent *he = NULL;

	if (gethostname(buff,256) == 0)
            he = gethostbyname(buff);
        if (he)
            our_addr = *(struct in_addr *)he->h_addr;
        if (our_addr.s_addr == 0)
            our_addr.s_addr = loopback_addr.s_addr;
}

#if SIZEOF_CHAR_P == 8

struct quehead_32 {
	u_int32_t qh_link;
	u_int32_t qh_rlink;
};

inline void
insque_32(void* a, void* b)
{
	register struct quehead_32 *element = (struct quehead_32 *) a;
	register struct quehead_32 *head = (struct quehead_32 *) b;
	element->qh_link = head->qh_link;
	head->qh_link = (u_int32_t)element;
	element->qh_rlink = (u_int32_t)head;
	((struct quehead_32 *)(element->qh_link))->qh_rlink
	= (u_int32_t)element;
}

inline void
remque_32(void* a)
{
	register struct quehead_32 *element = (struct quehead_32 *) a;
	((struct quehead_32 *)(element->qh_link))->qh_rlink = element->qh_rlink;
	((struct quehead_32 *)(element->qh_rlink))->qh_link = element->qh_link;
	element->qh_rlink = 0;
}

#endif /* SIZEOF_CHAR_P == 8 */

struct quehead {
	struct quehead *qh_link;
	struct quehead *qh_rlink;
};

inline void
insque(void*  a, void*  b)
{
	register struct quehead *element = (struct quehead *) a;
	register struct quehead *head = (struct quehead *) b;
	element->qh_link = head->qh_link;
	head->qh_link = (struct quehead *)element;
	element->qh_rlink = (struct quehead *)head;
	((struct quehead *)(element->qh_link))->qh_rlink
	= (struct quehead *)element;
}

inline void
remque(void*  a)
{
  register struct quehead *element = (struct quehead *) a;
  ((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
  ((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
  element->qh_rlink = NULL;
  /*  element->qh_link = NULL;  TCP FIN1 crashes if you do this.  Why ? */
}

/* #endif */


#ifndef HAVE_STRERROR

/*
 * For systems with no strerror
 */

extern int sys_nerr;
extern char *sys_errlist[];

char *
strerror(int  error)
{
	if (error < sys_nerr)
	   return sys_errlist[error];
	else
	   return "Unknown error.";
}

#endif



#ifndef HAVE_STRDUP
char *
strdup(const char*  str)
{
	char *bptr;
	int   len = strlen(str);

	bptr = (char *)malloc(len+1);
	memcpy(bptr, str, len+1);

	return bptr;
}
#endif


int (*lprint_print) _P((void *, const char *, va_list));
char *lprint_ptr, *lprint_ptr2, **lprint_arg;

void
lprint(const char *format, ...)
{
	va_list args;

    va_start(args, format);
	if (lprint_print)
	   lprint_ptr += (*lprint_print)(*lprint_arg, format, args);

	/* Check if they want output to be logged to file as well */
	if (lfd) {
		/*
		 * Remove \r's
		 * otherwise you'll get ^M all over the file
		 */
		int len = strlen(format);
		char *bptr1, *bptr2;

		bptr1 = bptr2 = strdup(format);

		while (len--) {
			if (*bptr1 == '\r')
			   memcpy(bptr1, bptr1+1, len+1);
			else
			   bptr1++;
		}
		vfprintf(lfd, bptr2, args);
		free(bptr2);
	}
	va_end(args);
}

void
add_emu(char* buff)
{
	u_int lport, fport;
	u_int8_t tos = 0, emu = 0;
	char buff1[256], buff2[256], buff4[128];
	char *buff3 = buff4;
	struct emu_t *emup;
	struct socket *so;

	if (sscanf(buff, "%256s %256s", buff2, buff1) != 2) {
		lprint("Error: Bad arguments\r\n");
		return;
	}

	if (sscanf(buff1, "%d:%d", &lport, &fport) != 2) {
		lport = 0;
		if (sscanf(buff1, "%d", &fport) != 1) {
			lprint("Error: Bad first argument\r\n");
			return;
		}
	}

	if (sscanf(buff2, "%128[^:]:%128s", buff1, buff3) != 2) {
		buff3 = 0;
		if (sscanf(buff2, "%256s", buff1) != 1) {
			lprint("Error: Bad second argument\r\n");
			return;
		}
	}

	if (buff3) {
		if (strcmp(buff3, "lowdelay") == 0)
		   tos = IPTOS_LOWDELAY;
		else if (strcmp(buff3, "throughput") == 0)
		   tos = IPTOS_THROUGHPUT;
		else {
			lprint("Error: Expecting \"lowdelay\"/\"throughput\"\r\n");
			return;
		}
	}

	if (strcmp(buff1, "ftp") == 0)
	   emu = EMU_FTP;
	else if (strcmp(buff1, "irc") == 0)
	   emu = EMU_IRC;
	else if (strcmp(buff1, "none") == 0)
	   emu = EMU_NONE; /* ie: no emulation */
	else {
		lprint("Error: Unknown service\r\n");
		return;
	}

	/* First, check that it isn't already emulated */
	for (emup = tcpemu; emup; emup = emup->next) {
		if (emup->lport == lport && emup->fport == fport) {
			lprint("Error: port already emulated\r\n");
			return;
		}
	}

	/* link it */
	emup = (struct emu_t *)malloc(sizeof (struct emu_t));
	emup->lport = (u_int16_t)lport;
	emup->fport = (u_int16_t)fport;
	emup->tos = tos;
	emup->emu = emu;
	emup->next = tcpemu;
	tcpemu = emup;

	/* And finally, mark all current sessions, if any, as being emulated */
	for (so = tcb.so_next; so != &tcb; so = so->so_next) {
		if ((lport && lport == ntohs(so->so_lport)) ||
		    (fport && fport == ntohs(so->so_fport))) {
			if (emu)
			   so->so_emu = emu;
			if (tos)
			   so->so_iptos = tos;
		}
	}

	lprint("Adding emulation for %s to port %d/%d\r\n", buff1, emup->lport, emup->fport);
}

#ifdef BAD_SPRINTF

#undef vsprintf
#undef sprintf

/*
 * Some BSD-derived systems have a sprintf which returns char *
 */

int
vsprintf_len(char* string, const char* format, va_list args)
{
	vsprintf(string, format, args);
	return strlen(string);
}

int
sprintf_len(char *string, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsprintf(string, format, args);
	return strlen(string);
}

#endif

