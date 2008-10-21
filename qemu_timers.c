#include "vl.h"

#include <sys/time.h>
#include <time.h>
#include "android_utils.h"
#include "android.h"

#include <signal.h>

#define  D(...)  VERBOSE_PRINT(init,__VA_ARGS__)


#ifdef _WIN32
#include <sys/timeb.h>
#else
#include <sys/ioctl.h>
#endif

#ifdef __linux__
#include <linux/rtc.h>
#include "hpet.h"
#endif

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)

QEMUClock *rt_clock;
QEMUClock *vm_clock;

/***********************************************************/
/* real time host monotonic timer */

/* digit: the following two variables are used to implement high-resolution
 * poll-based interrupts. the idea is to be able to generate an emulated
 * interrupt every millisecond, even on non-Linux platforms which don't have
 * a /dev/rtc
 */
static int64_t  milli_last_ns;
static int64_t  milli_next_ns;

/* digit: the following global boolean is set when we need to perform
 * high-resolution polling. If not, we'll use host interrupts instead
 */
int  qemu_milli_needed = 0;

#ifdef WIN32

static int64_t clock_freq;

static void init_get_clock(void)
{
    LARGE_INTEGER freq;
    int ret;
    ret = QueryPerformanceFrequency(&freq);
    if (ret == 0) {
        fprintf(stderr, "Could not calibrate ticks\n");
        exit(1);
    }
    clock_freq = freq.QuadPart;

    {
        LARGE_INTEGER  ti;
        QueryPerformanceCounter(&ti);
        milli_last_ns = muldiv64(ti.QuadPart,QEMU_TIMER_BASE,clock_freq);
        milli_next_ns = milli_last_ns + 1000000;
    }
}

static int64_t get_clock(void)
{
    LARGE_INTEGER ti;
    QueryPerformanceCounter(&ti);
    return muldiv64(ti.QuadPart, QEMU_TIMER_BASE, clock_freq);
}



#elif defined(__APPLE__)

#include <mach/mach_time.h>

static mach_timebase_info_data_t   _mach_timebase_info;
static int64_t                     _mach_timebase;

static int64_t get_clock(void)
{
    int64_t   elapsed    = mach_absolute_time() - _mach_timebase;
    int64_t   numer      = _mach_timebase_info.numer;
    int64_t   denom      = _mach_timebase_info.denom;

    /* return elapsed time in nanoseconds */
    return elapsed * numer / denom;
}

static void init_get_clock(void)
{
    mach_timebase_info( &_mach_timebase_info );
    _mach_timebase = mach_absolute_time();

    milli_last_ns = get_clock();
    milli_next_ns = milli_last_ns + 1000000;
}


#else

static int use_rt_clock;

static int64_t get_clock(void)
{
#if defined(__linux__)
    if (use_rt_clock) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    } else
#endif
    {
        /* XXX: using gettimeofday leads to problems if the date
           changes, so it should be avoided. */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000000LL + (tv.tv_usec * 1000);
    }
}

static void init_get_clock(void)
{
    use_rt_clock = 0;
#if defined(__linux__)
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            use_rt_clock = 1;
        }
    }
#endif
    milli_last_ns = get_clock();
    milli_next_ns = milli_last_ns + 1000000;
}


#endif

int64_t  qemu_get_elapsed_ns(void)
{
    return get_clock();
}

void  qemu_polling_enable( void )
{
    if (++qemu_milli_needed == 1) {
      milli_last_ns = get_clock();
      milli_next_ns = milli_last_ns + 1000000;
    }
}

void  qemu_polling_disable( void )
{
    --qemu_milli_needed;
}

int  qemu_milli_check(void)
{
    int       result = 0;
    int64_t   now_ns = get_clock();

    //fprintf(stderr,"MilliCheck: now=%.4fms next=%.4fms\n", (double)now_ns/1000000.0, (double)milli_next_ns/1000000.0);
    if (now_ns < milli_last_ns) {  /* software suspend ? */
        milli_last_ns = now_ns;
        milli_next_ns = now_ns + 1000000;
        return 1;
    }

    while (now_ns >= milli_next_ns)
    {
        milli_last_ns = milli_next_ns;
        milli_next_ns = milli_last_ns + 1000000;
        result  += 1;
    }
    return result;
}

/***********************************************************/
/* guest cycle counter */

static int64_t cpu_ticks_prev;
static int64_t cpu_ticks_offset;
static int64_t cpu_clock_offset;
static int cpu_ticks_enabled;

/* return the host CPU cycle counter and handle stop/restart */
int64_t cpu_get_ticks(void)
{
    if (!cpu_ticks_enabled) {
        return cpu_ticks_offset;
    } else {
        int64_t ticks;
        ticks = cpu_get_real_ticks();
        if (cpu_ticks_prev > ticks) {
            /* Note: non increasing ticks may happen if the host uses
               software suspend */
            cpu_ticks_offset += cpu_ticks_prev - ticks;
        }
        cpu_ticks_prev = ticks;
        return ticks + cpu_ticks_offset;
    }
}

/* return the host CPU monotonic timer and handle stop/restart */
static int64_t cpu_get_clock(void)
{
    int64_t ti;
    if (!cpu_ticks_enabled) {
        return cpu_clock_offset;
    } else {
        ti = get_clock();
        return ti + cpu_clock_offset;
    }
}

/* enable cpu_get_ticks() */
void cpu_enable_ticks(void)
{
    if (!cpu_ticks_enabled) {
        cpu_ticks_offset -= cpu_get_real_ticks();
        cpu_clock_offset -= get_clock();
        cpu_ticks_enabled = 1;
    }
}

/* disable cpu_get_ticks() : the clock is stopped. You must not call
   cpu_get_ticks() after that.  */
void cpu_disable_ticks(void)
{
    if (cpu_ticks_enabled) {
        cpu_ticks_offset = cpu_get_ticks();
        cpu_clock_offset = cpu_get_clock();
        cpu_ticks_enabled = 0;
    }
}

/***********************************************************/
/* timers */

#define QEMU_TIMER_REALTIME 0
#define QEMU_TIMER_VIRTUAL  1

struct QEMUClock {
    int type;
    /* XXX: add frequency */
};

struct QEMUTimer {
    QEMUClock *clock;
    int64_t expire_time;
    QEMUTimerCB *cb;
    void *opaque;
    struct QEMUTimer *next;
};

struct qemu_alarm_timer {
    char const *name;
    unsigned int flags;

    int (*start)(struct qemu_alarm_timer *t);
    void (*stop)(struct qemu_alarm_timer *t);
    void (*rearm)(struct qemu_alarm_timer *t);
    void (*signal)(void);
    void *priv;
};

#define ALARM_FLAG_DYNTICKS  0x1
#define ALARM_FLAG_EXPIRED   0x2

static inline int alarm_timer_has_dynticks(struct qemu_alarm_timer *t)
{
    return t && t->flags & ALARM_FLAG_DYNTICKS;
}

static void alarm_timer_rearm(struct qemu_alarm_timer *t)
{
    if (!alarm_timer_has_dynticks(t))
        return;

    t->rearm(t);
}

/* TODO: MIN_TIMER_REARM_US should be optimized */
#define MIN_TIMER_REARM_US 250

static struct qemu_alarm_timer *alarm_timer;

#ifdef _WIN32
#define  USE_WIN32_TIMER           1
#define  USE_WIN32_DYNTICKS_TIMER  1
#else
#define  USE_UNIX_TIMER   1
#endif

#ifdef __linux__
#define  DONT_USE_DYNTICKS_TIMER  1  /* it freezes the Android emulator hard */
#define  USE_HPET_TIMER      1
#define  USE_RTC_TIMER       1
#endif


#if defined(USE_WIN32_TIMER) || defined(USE_WIN32_DYNTICKS_TIMER)
struct qemu_alarm_win32 {
    MMRESULT timerId;
    HANDLE host_alarm;
    unsigned int period;
} alarm_win32_data = {0, NULL, -1};

static int win32_start_timer(struct qemu_alarm_timer *t);
static void win32_stop_timer(struct qemu_alarm_timer *t);
#endif

#ifdef USE_WIN32_DYNTICKS_TIMER
static void win32_rearm_timer(struct qemu_alarm_timer *t);
#endif

#ifdef USE_UNIX_TIMER
static int unix_start_timer(struct qemu_alarm_timer *t);
static void unix_stop_timer(struct qemu_alarm_timer *t);
#endif

#ifdef USE_DYNTICKS_TIMER
static int dynticks_start_timer(struct qemu_alarm_timer *t);
static void dynticks_stop_timer(struct qemu_alarm_timer *t);
static void dynticks_rearm_timer(struct qemu_alarm_timer *t);
#endif

#ifdef USE_HPET_TIMER
static int hpet_start_timer(struct qemu_alarm_timer *t);
static void hpet_stop_timer(struct qemu_alarm_timer *t);
#endif

#ifdef USE_RTC_TIMER
static int rtc_start_timer(struct qemu_alarm_timer *t);
static void rtc_stop_timer(struct qemu_alarm_timer *t);
#endif


static struct qemu_alarm_timer alarm_timers[] = {
#ifdef USE_DYNTICKS_TIMER
    {"dynticks", ALARM_FLAG_DYNTICKS, dynticks_start_timer,
     dynticks_stop_timer, dynticks_rearm_timer, NULL, NULL},
#endif
#ifdef USE_HPET_TIMER
    /* HPET - if available - is preferred */
    {"hpet", 0, hpet_start_timer, hpet_stop_timer, NULL, NULL, NULL},
#endif
#ifdef USE_RTC_TIMER
    /* ...otherwise try RTC */
    {"rtc", 0, rtc_start_timer, rtc_stop_timer, NULL, NULL, NULL},
#endif
#ifdef USE_UNIX_TIMER
    {"unix", 0, unix_start_timer, unix_stop_timer, NULL, NULL, NULL},
#endif
#ifdef USE_WIN32_DYNTICKS_TIMER
    {"dynticks", ALARM_FLAG_DYNTICKS, win32_start_timer,
     win32_stop_timer, win32_rearm_timer, NULL, &alarm_win32_data},
#endif
#ifdef USE_WIN32_TIMER
    {"win32", 0, win32_start_timer,
     win32_stop_timer, NULL, NULL, &alarm_win32_data},
#endif
    {NULL, }
};

static void show_available_alarms()
{
    int i;

    printf("Available alarm timers, in order of precedence:\n");
    for (i = 0; alarm_timers[i].name; i++)
        printf("%s\n", alarm_timers[i].name);
}

void qemu_configure_alarms(char const *opt)
{
    int i;
    int cur = 0;
    int count = (sizeof(alarm_timers) / sizeof(*alarm_timers)) - 1;
    char *arg;
    char *name;

    if (!strcmp(opt, "help")) {
        show_available_alarms();
        exit(0);
    }

    arg = strdup(opt);

    /* Reorder the array */
    name = strtok(arg, ",");
    while (name) {
        struct qemu_alarm_timer tmp;

        for (i = 0; i < count && alarm_timers[i].name; i++) {
            if (!strcmp(alarm_timers[i].name, name))
                break;
        }

        if (i == count) {
            fprintf(stderr, "Unknown clock %s\n", name);
            goto next;
        }

        if (i < cur)
            /* Ignore */
            goto next;

    /* Swap */
        tmp = alarm_timers[i];
        alarm_timers[i] = alarm_timers[cur];
        alarm_timers[cur] = tmp;

        cur++;
next:
        name = strtok(NULL, ",");
    }

    free(arg);

    if (cur) {
    /* Disable remaining timers */
        for (i = cur; i < count; i++)
            alarm_timers[i].name = NULL;
    }

    /* debug */
    show_available_alarms();
}

QEMUClock *rt_clock;
QEMUClock *vm_clock;

static QEMUTimer *active_timers[2];
#if defined(USE_WIN32_TIMER) || defined(USE_WIN32_DYNTICKS_TIMER)
static MMRESULT timerID;
static HANDLE host_alarm = NULL;
static unsigned int period = 1;
#endif

QEMUClock *qemu_new_clock(int type)
{
    QEMUClock *clock;
    clock = qemu_mallocz(sizeof(QEMUClock));
    if (!clock)
        return NULL;
    clock->type = type;
    return clock;
}

QEMUTimer *qemu_new_timer(QEMUClock *clock, QEMUTimerCB *cb, void *opaque)
{
    QEMUTimer *ts;

    ts = qemu_mallocz(sizeof(QEMUTimer));
    ts->clock = clock;
    ts->cb = cb;
    ts->opaque = opaque;
    return ts;
}

void qemu_free_timer(QEMUTimer *ts)
{
    qemu_free(ts);
}

/* stop a timer, but do not dealloc it */
void qemu_del_timer(QEMUTimer *ts)
{
    QEMUTimer **pt, *t;

    /* NOTE: this code must be signal safe because
       qemu_timer_expired() can be called from a signal. */
    pt = &active_timers[ts->clock->type];
    for(;;) {
        t = *pt;
        if (!t)
            break;
        if (t == ts) {
            *pt = t->next;
            break;
        }
        pt = &t->next;
    }
}

/* modify the current timer so that it will be fired when current_time
   >= expire_time. The corresponding callback will be called. */
void qemu_mod_timer(QEMUTimer *ts, int64_t expire_time)
{
    QEMUTimer **pt, *t;

    qemu_del_timer(ts);

    /* add the timer in the sorted list */
    /* NOTE: this code must be signal safe because
       qemu_timer_expired() can be called from a signal. */
    pt = &active_timers[ts->clock->type];
    for(;;) {
        t = *pt;
        if (!t)
            break;
        if (t->expire_time > expire_time)
            break;
        pt = &t->next;
    }
    ts->expire_time = expire_time;
    ts->next = *pt;
    *pt = ts;

    /* Rearm if necessary  */
    if ((alarm_timer->flags & ALARM_FLAG_EXPIRED) == 0 &&
        pt == &active_timers[ts->clock->type])
        alarm_timer_rearm(alarm_timer);
}

int qemu_timer_pending(QEMUTimer *ts)
{
    QEMUTimer *t;
    for(t = active_timers[ts->clock->type]; t != NULL; t = t->next) {
        if (t == ts)
            return 1;
    }
    return 0;
}

static inline int qemu_timer_expired(QEMUTimer *timer_head, int64_t current_time)
{
    if (!timer_head)
        return 0;
    return (timer_head->expire_time <= current_time);
}

#define  QEMU_TIMER_MAX_TIMEOUT  10

static inline int  qemu_timer_next_timeout(QEMUTimer*  timer_head)
{
    int64_t  now, diff;

    if (!timer_head)
        return QEMU_TIMER_MAX_TIMEOUT;

    now  = qemu_get_clock(timer_head->clock);
    diff = timer_head->expire_time - now;
    if (timer_head->clock == vm_clock)
        diff /= 1000000;

    if (diff > QEMU_TIMER_MAX_TIMEOUT)
        return QEMU_TIMER_MAX_TIMEOUT;
    if (diff < 1)
        diff = 1;
    return (int) diff;
}


static void qemu_run_timers(QEMUTimer **ptimer_head, int64_t current_time)
{
    QEMUTimer *ts;

    for(;;) {
        ts = *ptimer_head;
        if (!ts || ts->expire_time > current_time)
            break;
        /* remove timer from the list before calling the callback */
        *ptimer_head = ts->next;
        ts->next = NULL;

        /* run the callback (the timer list can be modified) */
        ts->cb(ts->opaque);
    }
}

int64_t qemu_get_clock(QEMUClock *clock)
{
    switch(clock->type) {
    case QEMU_TIMER_REALTIME:
        return get_clock() / 1000000;
    default:
    case QEMU_TIMER_VIRTUAL:
        return cpu_get_clock();
    }
}

void init_timers(void)
{
    static int inited;
    if (!inited) {
        init_get_clock();
        ticks_per_sec = QEMU_TIMER_BASE;
        rt_clock = qemu_new_clock(QEMU_TIMER_REALTIME);
        vm_clock = qemu_new_clock(QEMU_TIMER_VIRTUAL);
        inited = 1;
    }
}

/* save a timer */
void qemu_put_timer(QEMUFile *f, QEMUTimer *ts)
{
    uint64_t expire_time;

    if (qemu_timer_pending(ts)) {
        expire_time = ts->expire_time;
    } else {
        expire_time = -1;
    }
    qemu_put_be64(f, expire_time);
}

void qemu_get_timer(QEMUFile *f, QEMUTimer *ts)
{
    uint64_t expire_time;

    expire_time = qemu_get_be64(f);
    if (expire_time != -1) {
        qemu_mod_timer(ts, expire_time);
    } else {
        qemu_del_timer(ts);
    }
}

void timer_save(QEMUFile *f, void *opaque)
{
    if (cpu_ticks_enabled) {
        hw_error("cannot save state if virtual timers are running");
    }
    qemu_put_be64s(f, (uint64_t*)&cpu_ticks_offset);
    qemu_put_be64s(f, (uint64_t*)&ticks_per_sec);
}

int timer_load(QEMUFile *f, void *opaque, int version_id)
{
    if (version_id != 1)
        return -EINVAL;
    if (cpu_ticks_enabled) {
        return -EINVAL;
    }
    qemu_get_be64s(f, (uint64_t*)&cpu_ticks_offset);
    qemu_get_be64s(f, (uint64_t*)&ticks_per_sec);
    return 0;
}

#ifdef _WIN32
void CALLBACK host_alarm_handler(UINT uTimerID, UINT uMsg,
                                 DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
#else
static void host_alarm_handler(int host_signum)
#endif
{
#if 0
    //write( 2, "|", 1 );
#define DISP_FREQ 1000
    {
        static int64_t delta_min = INT64_MAX;
        static int64_t delta_max, delta_cum, last_clock, delta, ti;
        static int count;
        ti = qemu_get_clock(vm_clock);
        if (last_clock != 0) {
            delta = ti - last_clock;
            if (delta < delta_min)
                delta_min = delta;
            if (delta > delta_max)
                delta_max = delta;
            delta_cum += delta;
            if (++count == DISP_FREQ) {
                printf("timer: min=%" PRId64 " us max=%" PRId64 " us avg=%" PRId64 " us avg_freq=%0.3f Hz\n",
                       muldiv64(delta_min, 1000000, ticks_per_sec),
                       muldiv64(delta_max, 1000000, ticks_per_sec),
                       muldiv64(delta_cum, 1000000 / DISP_FREQ, ticks_per_sec),
                       (double)ticks_per_sec / ((double)delta_cum / DISP_FREQ));
                count = 0;
                delta_min = INT64_MAX;
                delta_max = 0;
                delta_cum = 0;
            }
        }
        last_clock = ti;
    }
#endif
    if (alarm_timer_has_dynticks(alarm_timer) ||
        qemu_timer_expired(active_timers[QEMU_TIMER_VIRTUAL],
                           qemu_get_clock(vm_clock)) ||
        qemu_timer_expired(active_timers[QEMU_TIMER_REALTIME],
                           qemu_get_clock(rt_clock))) {
#ifdef _WIN32
        struct qemu_alarm_win32 *data = ((struct qemu_alarm_timer*)dwUser)->priv;
        SetEvent(data->host_alarm);
#endif
        alarm_timer->flags |= ALARM_FLAG_EXPIRED;

        if (alarm_timer->signal)
            alarm_timer->signal();
    }
}

static uint64_t qemu_next_deadline(void)
{
    int64_t nearest_delta_us = INT64_MAX;
    int64_t vmdelta_us;

    if (active_timers[QEMU_TIMER_REALTIME])
        nearest_delta_us = (active_timers[QEMU_TIMER_REALTIME]->expire_time -
                            qemu_get_clock(rt_clock))*1000;

    if (active_timers[QEMU_TIMER_VIRTUAL]) {
        /* round up */
        vmdelta_us = (active_timers[QEMU_TIMER_VIRTUAL]->expire_time -
                      qemu_get_clock(vm_clock)+999)/1000;
        if (vmdelta_us < nearest_delta_us)
            nearest_delta_us = vmdelta_us;
    }

    /* Avoid arming the timer to negative, zero, or too low values */
    if (nearest_delta_us <= MIN_TIMER_REARM_US)
        nearest_delta_us = MIN_TIMER_REARM_US;

    return nearest_delta_us;
}

#define RTC_FREQ 1024

#if defined(USE_HPET_TIMER) || defined(USE_RTC_TIMER)
static void enable_sigio_timer(int fd)
{
    struct sigaction act;

    /* timer signal */
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = host_alarm_handler;

    sigaction(SIGIO, &act, NULL);
    fcntl(fd, F_SETFL, O_ASYNC);
    fcntl(fd, F_SETOWN, getpid());
}
#endif

#ifdef USE_HPET_TIMER
static int hpet_start_timer(struct qemu_alarm_timer *t)
{
    struct hpet_info info;
    int r, fd;

    fd = open("/dev/hpet", O_RDONLY);
    if (fd < 0)
        return -1;

    /* Set frequency */
    r = ioctl(fd, HPET_IRQFREQ, RTC_FREQ);
    if (r < 0) {
        fprintf(stderr, "Could not configure '/dev/hpet' to have a 1024Hz timer. This is not a fatal\n"
                "error, but for better emulation accuracy type:\n"
                "'echo 1024 > /proc/sys/dev/hpet/max-user-freq' as root.\n");
        goto fail;
    }

    /* Check capabilities */
    r = ioctl(fd, HPET_INFO, &info);
    if (r < 0)
        goto fail;

    /* Enable periodic mode */
    r = ioctl(fd, HPET_EPI, 0);
    if (info.hi_flags && (r < 0))
        goto fail;

    /* Enable interrupt */
    r = ioctl(fd, HPET_IE_ON, 0);
    if (r < 0)
        goto fail;

    enable_sigio_timer(fd);
    t->priv = (void *)(long)fd;

    return 0;
fail:
    close(fd);
    return -1;
}

static void hpet_stop_timer(struct qemu_alarm_timer *t)
{
    int fd = (long)t->priv;

    close(fd);
}
#endif

#ifdef USE_RTC_TIMER
static int rtc_start_timer(struct qemu_alarm_timer *t)
{
    int rtc_fd;
    unsigned long current_rtc_freq = 0;

    TFR(rtc_fd = open("/dev/rtc", O_RDONLY));
    if (rtc_fd < 0)
        return -1;
    ioctl(rtc_fd, RTC_IRQP_READ, &current_rtc_freq);
    if (current_rtc_freq != RTC_FREQ &&
        ioctl(rtc_fd, RTC_IRQP_SET, RTC_FREQ) < 0) {
        fprintf(stderr, "Could not configure '/dev/rtc' to have a 1024 Hz timer. This is not a fatal\n"
                "error, but for better emulation accuracy either use a 2.6 host Linux kernel or\n"
                "type 'echo 1024 > /proc/sys/dev/rtc/max-user-freq' as root.\n");
        goto fail;
    }
    if (ioctl(rtc_fd, RTC_PIE_ON, 0) < 0) {
    fail:
        close(rtc_fd);
        return -1;
    }

    enable_sigio_timer(rtc_fd);

    t->priv = (void *)(long)rtc_fd;

    return 0;
}

static void rtc_stop_timer(struct qemu_alarm_timer *t)
{
    int rtc_fd = (long)t->priv;

    close(rtc_fd);
}
#endif

#ifdef USE_DYNTICKS_TIMER
static int dynticks_start_timer(struct qemu_alarm_timer *t)
{
    struct sigevent ev;
    timer_t host_timer;
    struct sigaction act;

    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = host_alarm_handler;

    sigaction(SIGALRM, &act, NULL);

    ev.sigev_value.sival_int = 0;
    ev.sigev_notify = SIGEV_SIGNAL;
    ev.sigev_signo = SIGALRM;

    if (timer_create(CLOCK_REALTIME, &ev, &host_timer)) {
        perror("timer_create");

        /* disable dynticks */
        fprintf(stderr, "Dynamic Ticks disabled\n");

        return -1;
    }

    t->priv = (void *)host_timer;

    return 0;
}

static void dynticks_stop_timer(struct qemu_alarm_timer *t)
{
    timer_t host_timer = (timer_t)t->priv;

    timer_delete(host_timer);
}

static void dynticks_rearm_timer(struct qemu_alarm_timer *t)
{
    timer_t host_timer = (timer_t)t->priv;
    struct itimerspec timeout;
    int64_t nearest_delta_us = INT64_MAX;
    int64_t current_us;

    if (!active_timers[QEMU_TIMER_REALTIME] &&
        !active_timers[QEMU_TIMER_VIRTUAL]) {
        return;
    }
    nearest_delta_us = qemu_next_deadline();

    /* check whether a timer is already running */
    if (timer_gettime(host_timer, &timeout)) {
        perror("gettime");
        fprintf(stderr, "Internal timer error: aborting\n");
        exit(1);
    }
    current_us = timeout.it_value.tv_sec * 1000000 + timeout.it_value.tv_nsec/1000;
    if (current_us && current_us <= nearest_delta_us)
        return;

    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_nsec = 0; /* 0 for one-shot timer */
    timeout.it_value.tv_sec =  nearest_delta_us / 1000000;
    timeout.it_value.tv_nsec = (nearest_delta_us % 1000000) * 1000;
    if (timer_settime(host_timer, 0 /* RELATIVE */, &timeout, NULL)) {
        perror("settime");
        fprintf(stderr, "Internal timer error: aborting\n");
        exit(1);
    }
}
#endif /* USE_DYNTICKS_TIMER */

#ifdef USE_UNIX_TIMER
static int unix_start_timer(struct qemu_alarm_timer *t)
{
    struct sigaction act;
    struct itimerval itv;
    int err;

    /* timer signal */
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = host_alarm_handler;

    sigaction(SIGALRM, &act, NULL);

    itv.it_interval.tv_sec = 0;
    /* for i386 kernel 2.6 to get 1 ms */
    itv.it_interval.tv_usec = 999;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 10 * 1000;

    err = setitimer(ITIMER_REAL, &itv, NULL);
    if (err)
        return -1;

    return 0;
}

static void unix_stop_timer(struct qemu_alarm_timer *t)
{
    struct itimerval itv;

    memset(&itv, 0, sizeof(itv));
    setitimer(ITIMER_REAL, &itv, NULL);
}

#endif /* USE_UNIX_TIMER */

#if defined(USE_WIN32_TIMER) || defined(USE_WIN32_DYNTICKS_TIMER)
static int win32_start_timer(struct qemu_alarm_timer *t)
{
    TIMECAPS tc;
    struct qemu_alarm_win32 *data = t->priv;
    UINT flags;

    data->host_alarm = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!data->host_alarm) {
        perror("Failed CreateEvent");
        return -1;
    }

    memset(&tc, 0, sizeof(tc));
    timeGetDevCaps(&tc, sizeof(tc));

    if (data->period < tc.wPeriodMin)
        data->period = tc.wPeriodMin;

    timeBeginPeriod(data->period);

    flags = TIME_CALLBACK_FUNCTION;
    if (alarm_timer_has_dynticks(t))
        flags |= TIME_ONESHOT;
    else
        flags |= TIME_PERIODIC;

    data->timerId = timeSetEvent(1,         // interval (ms)
                        data->period,       // resolution
                        host_alarm_handler, // function
                        (DWORD)t,           // parameter
                        flags);

    if (!data->timerId) {
        perror("Failed to initialize win32 alarm timer");

        timeEndPeriod(data->period);
        CloseHandle(data->host_alarm);
        return -1;
    }

    qemu_add_wait_object(data->host_alarm, NULL, NULL);

    return 0;
}

static void win32_stop_timer(struct qemu_alarm_timer *t)
{
    struct qemu_alarm_win32 *data = t->priv;

    timeKillEvent(data->timerId);
    timeEndPeriod(data->period);

    CloseHandle(data->host_alarm);
}
#endif /* USE_WIN32_TIMER || USE_WIN32_DYNTICKS_TIMER */

#ifdef USE_WIN32_DYNTICKS_TIMER
static void win32_rearm_timer(struct qemu_alarm_timer *t)
{
    struct qemu_alarm_win32 *data = t->priv;
    uint64_t nearest_delta_us;

    if (!active_timers[QEMU_TIMER_REALTIME] &&
                !active_timers[QEMU_TIMER_VIRTUAL])
        return;

    nearest_delta_us = qemu_next_deadline();
    nearest_delta_us /= 1000;

    timeKillEvent(data->timerId);

    data->timerId = timeSetEvent(1,
                        data->period,
                        host_alarm_handler,
                        (DWORD)t,
                        TIME_ONESHOT | TIME_PERIODIC);

    if (!data->timerId) {
        perror("Failed to re-arm win32 alarm timer");

        timeEndPeriod(data->period);
        CloseHandle(data->host_alarm);
        exit(1);
    }
}
#endif /* USE_WIN32_DYNTICKS_TIMER */

void qemu_init_alarm_timer(void (*cb)(void))
{
    struct qemu_alarm_timer *t;
    int i, err = -1;

    /* on UNIX, ensure that SIGALRM is not blocked for some reason */
#ifndef _WIN32
    {
        sigset_t  ss;
        sigemptyset( &ss );
        sigaddset( &ss, SIGALRM );
        sigprocmask( SIG_UNBLOCK, &ss, NULL );
    }
#endif

    for (i = 0; alarm_timers[i].name; i++) {
        t = &alarm_timers[i];

        err = t->start(t);
        if (!err)
            break;
    }

    if (err) {
        fprintf(stderr, "Unable to find any suitable alarm timer.\n");
        fprintf(stderr, "Terminating\n");
        exit(1);
    }

    t->signal = cb;
    D( "using '%s' alarm timer", t->name );
    alarm_timer = t;
}

#if 0
static void quit_timers(void)
{
    alarm_timer->stop(alarm_timer);
    alarm_timer = NULL;
}
#endif

void
qemu_run_virtual_timers(void)
{
    qemu_run_timers(&active_timers[QEMU_TIMER_VIRTUAL], qemu_get_clock(vm_clock));
}

void
qemu_run_realtime_timers(void)
{
    qemu_run_timers(&active_timers[QEMU_TIMER_REALTIME], qemu_get_clock(rt_clock));
}

void
qemu_start_alarm_timer(void)
{
    alarm_timer_rearm(alarm_timer);
}


void
qemu_rearm_alarm_timer(void)
{
    if (alarm_timer->flags & ALARM_FLAG_EXPIRED) {
        alarm_timer->flags &= ~(ALARM_FLAG_EXPIRED);
        alarm_timer_rearm(alarm_timer);
    }
}

void
qemu_stop_alarm_timer(void)
{
    alarm_timer->stop(alarm_timer);
}
