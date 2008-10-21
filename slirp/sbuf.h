/*
 * Copyright (c) 1995 Danny Gasparovski.
 * 
 * Please read the file COPYRIGHT for the 
 * terms and conditions of the copyright.
 */

#ifndef _SBUF_H_
#define _SBUF_H_

#include "mbuf.h"
#include <stddef.h>

/* a SBuf is a simple circular buffer used to hold RX and TX data in a struct socket
 */

typedef struct sbuf {
	unsigned  sb_cc;      /* actual chars in buffer */
	unsigned  sb_datalen; /* Length of data  */
	char*     sb_wptr;    /* write pointer. points to where the next
			                                       * bytes should be written in the sbuf */
	char*     sb_rptr;    /* read pointer. points to where the next
                                                                       * byte should be read from the sbuf */
	char*     sb_data;	/* Actual data */
} SBufRec, *SBuf;

void sbuf_free    (SBuf  sb);
void sbuf_drop    (SBuf  sb, int  num);
void sbuf_reserve (SBuf  sb, int  count);
void sbuf_append  (struct socket *so, MBuf  m);
void sbuf_appendsb(SBuf  sb, MBuf  m);
void sbuf_copy    (SBuf  sb, int  offset, int  length, char *to);

#define sbuf_flush(sb) sbuf_drop((sb),(sb)->sb_cc)
#define sbuf_space(sb) ((sb)->sb_datalen - (sb)->sb_cc)

#endif
