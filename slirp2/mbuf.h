/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mbuf.h	8.3 (Berkeley) 1/21/94
 * mbuf.h,v 1.9 1994/11/14 13:54:20 bde Exp
 */

#ifndef _MBUF_H_
#define _MBUF_H_

#define MINCSIZE 4096	/* Amount to increase mbuf if too small */

/* flags for the mh_flags field */
#define M_EXT			0x01	/* m_ext points to more (malloced) data */
#define M_FREELIST		0x02	/* mbuf is on free list */
#define M_USEDLIST		0x04	/* XXX mbuf is on used list (for dtom()) */
#define M_DOFREE		0x08	/* when mbuf_free is called on the mbuf, free()
					                                  * it rather than putting it on the free list */


/* XXX About mbufs for slirp:
 * Only one mbuf is ever used in a chain, for each "cell" of data.
 * m_nextpkt points to the next packet, if fragmented.
 * If the data is too large, the M_EXT is used, and a larger block
 * is alloced.  Therefore, mbuf_free[m] must check for M_EXT and if set
 * free the m_ext.  This is inefficient memory-wise, but who cares.
 */

/* XXX should union some of these! */
/* header at beginning of each mbuf: */

/**
 *  m_next, m_prev   :: used to place the MBuf in free/used linked lists
 *  m_next2, m_prev2 :: used to place the same MBuf in other linked lists
 *  m_flags :: bit flags describing this MBuf
 *  m_size  :: total size of MBuf buffer
 *  m_so    :: socket this MBuf is attached to
 *  m_data  :: pointer to current cursor in MBuf buffer
 *  m_len   :: amount of data recorded in this MBuf
 */
#define  MBUF_HEADER           \
	struct mbuf*   m_next;     \
	struct mbuf*   m_prev;     \
	struct mbuf*   m_next2;    \
	struct mbuf*   m_prev2;    \
	int            m_flags;    \
	int            m_size;     \
	struct socket* m_so;       \
	caddr_t        m_data;     \
	int            m_len;

struct m_hdr {
	MBUF_HEADER
};

typedef struct mbuf {
	MBUF_HEADER
	union M_dat {
		char   m_dat_[1]; /* ANSI doesn't like 0 sized arrays */
		char*  m_ext_;
	} M_dat;
} MBufRec, *MBuf;

#define m_nextpkt	m_next2
#define m_prevpkt	m_prev2
#define m_dat		M_dat.m_dat_
#define m_ext		M_dat.m_ext_

#define ifq_prev m_prev
#define ifq_next m_next

#define ifs_prev m_prev2
#define ifs_next m_next2

#define ifq_so m_so

void mbuf_init  (void);
void msize_init (void);
MBuf mbuf_alloc (void);
void mbuf_free  (MBuf  m);
void mbuf_append(MBuf  m1, MBuf  m2);
void mbuf_ensure(MBuf  m, int  size);
void mbuf_trim  (MBuf  m, int  len);
int  mbuf_copy  (MBuf  m, MBuf  n, int  n_offset, int  n_length);

#define MBUF_TO(m,t)  ((t)(m)->m_data)
#define MBUF_FROM(d)  mbuf_from(d)
MBuf  mbuf_from (void *);

int   mbuf_freeroom( MBuf  m );

#endif
