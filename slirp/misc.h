/*
 * Copyright (c) 1995 Danny Gasparovski.
 * 
 * Please read the file COPYRIGHT for the 
 * terms and conditions of the copyright.
 */

#ifndef _MISC_H_
#define _MISC_H_

extern u_int curtime, time_fasttimo, last_slowtimo, detach_time, detach_wait;

extern int (*lprint_print) _P((void *, const char *, va_list));
extern char *lprint_ptr, *lprint_ptr2, **lprint_arg;
//extern SBuf lprint_sb;

#ifndef HAVE_STRDUP
char *strdup _P((const char *));
#endif

void do_wait _P((int));

#define EMU_NONE 0x0

/* TCP emulations */
#define EMU_CTL 0x1
#define EMU_FTP 0x2
#define EMU_KSH 0x3
#define EMU_IRC 0x4
#define EMU_REALAUDIO 0x5
#define EMU_RLOGIN 0x6
#define EMU_IDENT 0x7
#define EMU_RSH 0x8

#define EMU_NOCONNECT 0x10	/* Don't connect */

/* UDP emulations */
#define EMU_TALK	0x1
#define EMU_NTALK	0x2
#define EMU_CUSEEME	0x3

struct tos_t {
	u_int16_t lport;
	u_int16_t fport;
	u_int8_t tos;
	u_int8_t emu;
};

struct emu_t {
	u_int16_t lport;
	u_int16_t fport;
	u_int8_t tos;
	u_int8_t emu;
	struct emu_t *next;
};

extern struct emu_t *tcpemu;

void getouraddr _P((void));
inline  void slirp_insque  _P((void *, void *));
inline  void slirp_remque  _P((void *));
void add_emu _P((char *));

#endif
