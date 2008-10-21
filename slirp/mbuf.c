/*
 * Copyright (c) 1995 Danny Gasparovski
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

/*
 * mbuf's in SLiRP are much simpler than the real mbufs in
 * FreeBSD.  They are fixed size, determined by the MTU,
 * so that one whole packet can fit.  Mbuf's cannot be
 * chained together.  If there's more data than the mbuf
 * could hold, an external malloced buffer is pointed to
 * by m_ext (and the data pointers) and M_EXT is set in
 * the flags
 */

#include <slirp.h>

static int      mbuf_alloced = 0;
static MBufRec  m_freelist, m_usedlist;
static int      mbuf_thresh = 30;
static int      mbuf_max = 0;
static int      msize;

/* 
 * How much room is in the mbuf, from m_data to the end of the mbuf
 */
#define M_ROOM(m) ((m->m_flags & M_EXT)? \
			(((m)->m_ext + (m)->m_size) - (m)->m_data) \
		   : \
			(((m)->m_dat + (m)->m_size) - (m)->m_data))

/*
 * How much free room there is
 */
#define M_FREEROOM(m) (M_ROOM(m) - (m)->m_len)


void
mbuf_init()
{
	m_freelist.m_next = m_freelist.m_prev = &m_freelist;
	m_usedlist.m_next = m_usedlist.m_prev = &m_usedlist;
	msize_init();
}

void
msize_init()
{
	/*
	 * Find a nice value for msize
	 * XXX if_maxlinkhdr already in mtu
	 */
	msize = (if_mtu > if_mru ? if_mtu : if_mru) + 
			if_maxlinkhdr + sizeof(struct m_hdr ) + 6;
}

static void
mbuf_insque(MBuf  m, MBuf  head)
{
	m->m_next         = head->m_next;
	m->m_prev         = head;
	head->m_next      = m;
	m->m_next->m_prev = m;
}

static void
mbuf_remque(MBuf  m)
{
	m->m_prev->m_next = m->m_next;
	m->m_next->m_prev = m->m_prev;
	m->m_next = m->m_prev = m;
}

/*
 * Get an mbuf from the free list, if there are none
 * malloc one
 * 
 * Because fragmentation can occur if we alloc new mbufs and
 * free old mbufs, we mark all mbufs above mbuf_thresh as M_DOFREE,
 * which tells m_free to actually free() it
 */
MBuf
mbuf_alloc(void)
{
	register MBuf m;
	int flags = 0;
	
	DEBUG_CALL("mbuf_alloc");
	
	if (m_freelist.m_next == &m_freelist) {
		m = (MBuf) malloc(msize);
		if (m == NULL) goto end_error;
		mbuf_alloced++;
		if (mbuf_alloced > mbuf_thresh)
			flags = M_DOFREE;
		if (mbuf_alloced > mbuf_max)
			mbuf_max = mbuf_alloced;
	} else {
		m = m_freelist.m_next;
		mbuf_remque(m);
	}
	
	/* Insert it in the used list */
	mbuf_insque(m,&m_usedlist);
	m->m_flags = (flags | M_USEDLIST);
	
	/* Initialise it */
	m->m_size  = msize - sizeof(struct m_hdr);
	m->m_data  = m->m_dat;
	m->m_len   = 0;
	m->m_next2 = NULL;
	m->m_prev2 = NULL;
end_error:
	DEBUG_ARG("m = %lx", (long )m);
	return m;
}

void
mbuf_free(MBuf  m)
{
	
  DEBUG_CALL("mbuf_free");
  DEBUG_ARG("m = %lx", (long )m);
	
  if(m) {
	/* Remove from m_usedlist */
	if (m->m_flags & M_USEDLIST)
	   mbuf_remque(m);
	
	/* If it's M_EXT, free() it */
	if (m->m_flags & M_EXT)
	   free(m->m_ext);

	/*
	 * Either free() it or put it on the free list
	 */
	if (m->m_flags & M_DOFREE) {
		free(m);
		mbuf_alloced--;
	} else if ((m->m_flags & M_FREELIST) == 0) {
		mbuf_insque(m,&m_freelist);
		m->m_flags = M_FREELIST; /* Clobber other flags */
	}
  } /* if(m) */
}

/*
 * Copy data from one mbuf to the end of
 * the other.. if result is too big for one mbuf, malloc()
 * an M_EXT data segment
 */
void
mbuf_append(MBuf  m, MBuf  n)
{
	/*
	 * If there's no room, realloc
	 */
	if (M_FREEROOM(m) < n->m_len)
		mbuf_ensure(m, m->m_size+MINCSIZE);
	
	memcpy(m->m_data+m->m_len, n->m_data, n->m_len);
	m->m_len += n->m_len;

	mbuf_free(n);
}


/* make m size bytes large */
void
mbuf_ensure(MBuf  m, int  size)
{
	int datasize;

	/* some compiles throw up on gotos.  This one we can fake. */
    if(m->m_size > size) return;

    if (m->m_flags & M_EXT) {
        datasize = m->m_data - m->m_ext;
        m->m_ext = (char *)realloc(m->m_ext,size);
        m->m_data = m->m_ext + datasize;
    } else {
        char *dat;
        datasize = m->m_data - m->m_dat;
        dat      = (char *)malloc(size);
        memcpy(dat, m->m_dat, m->m_size);

        m->m_ext    = dat;
        m->m_data   = m->m_ext + datasize;
        m->m_flags |= M_EXT;
    }
 
    m->m_size = size;
}



void
mbuf_trim(MBuf  m, int  len)
{
	if (m == NULL)
		return;
	if (len >= 0) {
		/* Trim from head */
		m->m_data += len;
		m->m_len  -= len;
	} else {
		/* Trim from tail */
		len       = -len;
		m->m_len -= len;
	}
}


/*
 * Copy len bytes from m, starting off bytes into n
 */
int
mbuf_copy(MBuf  n, MBuf  m, int  off, int  len)
{
	if (len > M_FREEROOM(n))
		return -1;

	memcpy((n->m_data + n->m_len), (m->m_data + off), len);
	n->m_len += len;
	return 0;
}

int
mbuf_freeroom( MBuf  m )
{
	return  M_FREEROOM(m);
}

/*
 * Given a pointer into an mbuf, return the mbuf
 * XXX This is a kludge, I should eliminate the need for it
 * Fortunately, it's not used often
 */
MBuf
mbuf_from(void*  dat)
{
	MBuf  m;
	
	DEBUG_CALL("mbuf_from");
	DEBUG_ARG("dat = %lx", (long )dat);

	/* bug corrected for M_EXT buffers */
	for (m = m_usedlist.m_next; m != &m_usedlist; m = m->m_next) {
	  if (m->m_flags & M_EXT) {
	    if( (unsigned)((char*)dat - m->m_ext) < (unsigned)m->m_size )
			goto Exit;
	  } else {
	    if( (unsigned)((char *)dat - m->m_dat) < (unsigned)m->m_size )
	      goto Exit;
	  }
	}
	m  = NULL;
	DEBUG_ERROR((dfd, "mbuf_from failed"));
Exit:	
	return m;
}

void
mbufstats()
{
	MBuf m;
	int i;
	
    lprint(" \r\n");
	
	lprint("Mbuf stats:\r\n");

	lprint("  %6d mbufs allocated (%d max)\r\n", mbuf_alloced, mbuf_max);
	
	i = 0;
	for (m = m_freelist.m_next; m != &m_freelist; m = m->m_next)
		i++;
	lprint("  %6d mbufs on free list\r\n",  i);
	
	i = 0;
	for (m = m_usedlist.m_next; m != &m_usedlist; m = m->m_next)
		i++;
	lprint("  %6d mbufs on used list\r\n",  i);
        lprint("  %6d mbufs queued as packets\r\n\r\n", if_queued);
}
