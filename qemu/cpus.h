#ifndef QEMU_CPUS_H
#define QEMU_CPUS_H

/* cpus.c */
void qemu_init_cpu_loop(void);
void resume_all_vcpus(void);
void pause_all_vcpus(void);
void cpu_stop_current(void);

void cpu_synchronize_all_states(void);
void cpu_synchronize_all_post_reset(void);
void cpu_synchronize_all_post_init(void);

void qtest_clock_warp(int64_t dest);

/* vl.c */
extern int smp_cores;
extern int smp_threads;
void set_numa_modes(void);
void set_cpu_log(const char *optarg);
void set_cpu_log_filename(const char *optarg);
void list_cpus(FILE *f, fprintf_function cpu_fprintf, const char *optarg);

typedef struct TimersState {
    int64_t cpu_ticks_prev;
    int64_t cpu_ticks_offset;
    int64_t cpu_clock_offset;
    int32_t cpu_ticks_enabled;
    int64_t dummy;

    /* S2E: tell the vm clock to go slower by a factor x */
    int32_t clock_scale_enable;
    int32_t clock_scale;
    int64_t cpu_clock_prev;
    int64_t cpu_clock_prev_scaled;

} TimersState;

void cpu_enable_scaling(int scale);

extern TimersState timers_state;

#endif
