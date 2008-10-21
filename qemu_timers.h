#ifndef _QEMU_TIMERS_H
#define _QEMU_TIMERS_H

#include "qemu_file.h"

#define QEMU_TIMER_BASE 1000000000LL

/* timers */

typedef struct QEMUClock QEMUClock;
typedef struct QEMUTimer QEMUTimer;

typedef void QEMUTimerCB(void *opaque);

/* The real time clock should be used only for stuff which does not
   change the virtual machine state, as it is run even if the virtual
   machine is stopped. The real time clock has a frequency of 1000
   Hz. */
extern QEMUClock *rt_clock;

/* The virtual clock is only run during the emulation. It is stopped
   when the virtual machine is stopped. Virtual timers use a high
   precision clock, usually cpu cycles (use ticks_per_sec). */
extern QEMUClock *vm_clock;

/* return the elapsed host time in nanoseconds */
int64_t qemu_get_elapsed_ns(void);

int64_t qemu_get_clock(QEMUClock *clock);

QEMUTimer *qemu_new_timer(QEMUClock *clock, QEMUTimerCB *cb, void *opaque);

void qemu_free_timer(QEMUTimer *ts);
void qemu_del_timer(QEMUTimer *ts);
void qemu_mod_timer(QEMUTimer *ts, int64_t expire_time);
int  qemu_timer_pending(QEMUTimer *ts);

extern int64_t ticks_per_sec;
extern int pit_min_timer_count;

int64_t cpu_get_ticks(void);
void    cpu_enable_ticks(void);
void    cpu_disable_ticks(void);

void    init_timers(void);

void    init_alarm_timer(void (*cb)(void));

void timer_save(QEMUFile *f, void *opaque);
int  timer_load(QEMUFile *f, void *opaque, int version_id);

void    qemu_run_virtual_timers(void);
void    qemu_run_realtime_timers(void);

void    qemu_init_alarm_timer( void(*cb)(void) );
void    qemu_start_alarm_timer(void);
void    qemu_rearm_alarm_timer(void);
void    qemu_stop_alarm_timer(void);

void    qemu_configure_alarms(char const *opt);

#endif /* _QEMU_TIMERS_H */
