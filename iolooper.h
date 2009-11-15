#ifndef IOLOOPER_H
#define IOLOOPER_H

#include <stdint.h>

/* An IOLooper is an abstraction for select() */

typedef struct IoLooper  IoLooper;

IoLooper*  iolooper_new(void);
void       iolooper_free( IoLooper*  iol );
void       iolooper_reset( IoLooper*  iol );

void       iolooper_add_read( IoLooper*  iol, int  fd );
void       iolooper_add_write( IoLooper*  iol, int  fd );
void       iolooper_del_read( IoLooper*  iol, int  fd );
void       iolooper_del_write( IoLooper*  iol, int  fd );

int        iolooper_poll( IoLooper*  iol );
int        iolooper_wait( IoLooper*  iol, int64_t  duration );

int        iolooper_is_read( IoLooper*  iol, int  fd );
int        iolooper_is_write( IoLooper*  iol, int  fd );

#endif /* IOLOOPER_H */
