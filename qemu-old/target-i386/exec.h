/*
 *  i386 execution defines
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The file was modified for S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include "config.h"
#include "dyngen-exec.h"

#ifdef CONFIG_S2E
#include <s2e/s2e_qemu.h>
#endif

/* XXX: factorize this mess */
#ifdef TARGET_X86_64
#define TARGET_LONG_BITS 64
#else
#define TARGET_LONG_BITS 32
#endif

#include "cpu-defs.h"

#ifdef CONFIG_S2E
extern struct CPUX86State *env;

#undef EAX
#define EAX (RR_cpu(env, regs[R_EAX]))
#define EAX_W(v) (WR_cpu(env, regs[R_EAX], v))
#undef ECX
#define ECX (RR_cpu(env, regs[R_ECX]))
#define ECX_W(v) (WR_cpu(env, regs[R_ECX], v))
#undef EDX
#define EDX (RR_cpu(env, regs[R_EDX]))
#define EDX_W(v) (WR_cpu(env, regs[R_EDX], v))
#undef EBX
#define EBX (RR_cpu(env, regs[R_EBX]))
#define EBX_W(v) (WR_cpu(env, regs[R_EBX], v))
#undef ESP
#define ESP (RR_cpu(env, regs[R_ESP]))
#define ESP_W(v) (WR_cpu(env, regs[R_ESP], v))
#undef EBP
#define EBP (RR_cpu(env, regs[R_EBP]))
#define EBP_W(v) (WR_cpu(env, regs[R_EBP], v))
#undef ESI
#define ESI (RR_cpu(env, regs[R_ESI]))
#define ESI_W(v) (WR_cpu(env, regs[R_ESI], v))
#undef EDI
#define EDI (RR_cpu(env, regs[R_EDI]))
#define EDI_W(v) (WR_cpu(env, regs[R_EDI], v))

#define CC_SRC (RR_cpu(env, cc_src))
#define CC_DST (RR_cpu(env, cc_dst))
#define CC_OP  (RR_cpu(env, cc_op))

#define CC_SRC_W(v) (WR_cpu(env, cc_src, v))
#define CC_DST_W(v) (WR_cpu(env, cc_dst, v))
#define CC_OP_W(v)  (WR_cpu(env, cc_op, v))

#define DF  (env->df)
#define DF_W(v)  (env->df = (v))

#undef EIP
#define EIP (env->eip)

/* float macros */
#define FT0    (env->ft0)
#define ST0    (env->fpregs[env->fpstt].d)
#define ST(n)  (env->fpregs[(env->fpstt + (n)) & 7].d)
#define ST1    ST(1)

#else /* CONFIG_S2E */

extern struct CPUX86State *env;

#undef EAX
#define EAX (env->regs[R_EAX])
#define EAX_W(v) (env->regs[R_EAX] = v)
#undef ECX
#define ECX (env->regs[R_ECX])
#define ECX_W(v) (env->regs[R_ECX] = v)
#undef EDX
#define EDX (env->regs[R_EDX])
#define EDX_W(v) (env->regs[R_EDX] = v)
#undef EBX
#define EBX (env->regs[R_EBX])
#define EBX_W(v) (env->regs[R_EBX] = v)
#undef ESP
#define ESP (env->regs[R_ESP])
#define ESP_W(v) (env->regs[R_ESP] = v)
#undef EBP
#define EBP (env->regs[R_EBP])
#define EBP_W(v) (env->regs[R_EBP] = v)
#undef ESI
#define ESI (env->regs[R_ESI])
#define ESI_W(v) (env->regs[R_ESI] = v)
#undef EDI
#define EDI (env->regs[R_EDI])
#define EDI_W(v) (env->regs[R_EDI] = v)

#define DF  (env->df)
#define CC_SRC (env->cc_src)
#define CC_DST (env->cc_dst)
#define CC_OP  (env->cc_op)

#define DF_W(v) (env->df = v)
#define CC_SRC_W(v) (env->cc_src = v)
#define CC_DST_W(v) (env->cc_dst = v)
#define CC_OP_W(v) (env->cc_op = v)

#undef EIP
#define EIP (env->eip)

/* float macros */
#define FT0    (env->ft0)
#define ST0    (env->fpregs[env->fpstt].d)
#define ST(n)  (env->fpregs[(env->fpstt + (n)) & 7].d)
#define ST1    ST(1)

#endif

#include "qemu-common.h"
#include "qemu-log.h"

#include "cpu.h"
#include "exec-all.h"

/* op_helper.c */
void do_interrupt(int intno, int is_int, int error_code,
                  target_ulong next_eip, int is_hw);
void do_interrupt_user(int intno, int is_int, int error_code,
                       target_ulong next_eip);
uint64_t helper_do_interrupt(int intno, int is_int, int error_code,
                  target_ulong next_eip, int is_hw);

void QEMU_NORETURN raise_exception_err(int exception_index, int error_code);
void QEMU_NORETURN raise_exception(int exception_index);
void do_smm_enter(void);

uint64_t helper_set_cc_op_eflags(void);

/* n must be a constant to be efficient */
static inline target_long lshift(target_long x, int n)
{
    if (n >= 0)
        return x << n;
    else
        return x >> (-n);
}

#include "helper.h"

static inline void svm_check_intercept(uint32_t type)
{
    helper_svm_check_intercept_param(type, 0);
}

#if !defined(CONFIG_USER_ONLY)

#include "softmmu_exec.h"

#endif /* !defined(CONFIG_USER_ONLY) */

#ifdef USE_X86LDOUBLE
/* use long double functions */
#define floatx_to_int32 floatx80_to_int32
#define floatx_to_int64 floatx80_to_int64
#define floatx_to_int32_round_to_zero floatx80_to_int32_round_to_zero
#define floatx_to_int64_round_to_zero floatx80_to_int64_round_to_zero
#define int32_to_floatx int32_to_floatx80
#define int64_to_floatx int64_to_floatx80
#define float32_to_floatx float32_to_floatx80
#define float64_to_floatx float64_to_floatx80
#define floatx_to_float32 floatx80_to_float32
#define floatx_to_float64 floatx80_to_float64
#define floatx_abs floatx80_abs
#define floatx_chs floatx80_chs
#define floatx_round_to_int floatx80_round_to_int
#define floatx_compare floatx80_compare
#define floatx_compare_quiet floatx80_compare_quiet
#else
#define floatx_to_int32 float64_to_int32
#define floatx_to_int64 float64_to_int64
#define floatx_to_int32_round_to_zero float64_to_int32_round_to_zero
#define floatx_to_int64_round_to_zero float64_to_int64_round_to_zero
#define int32_to_floatx int32_to_float64
#define int64_to_floatx int64_to_float64
#define float32_to_floatx float32_to_float64
#define float64_to_floatx(x, e) (x)
#define floatx_to_float32 float64_to_float32
#define floatx_to_float64(x, e) (x)
#define floatx_abs float64_abs
#define floatx_chs float64_chs
#define floatx_round_to_int float64_round_to_int
#define floatx_compare float64_compare
#define floatx_compare_quiet float64_compare_quiet
#endif

#define RC_MASK         0xc00
#define RC_NEAR		0x000
#define RC_DOWN		0x400
#define RC_UP		0x800
#define RC_CHOP		0xc00

#define MAXTAN 9223372036854775808.0

#ifdef USE_X86LDOUBLE

/* only for x86 */
typedef union {
    long double d;
    struct {
        unsigned long long lower;
        unsigned short upper;
    } l;
} CPU86_LDoubleU;

/* the following deal with x86 long double-precision numbers */
#define MAXEXPD 0x7fff
#define EXPBIAS 16383
#define EXPD(fp)	(fp.l.upper & 0x7fff)
#define SIGND(fp)	((fp.l.upper) & 0x8000)
#define MANTD(fp)       (fp.l.lower)
#define BIASEXPONENT(fp) fp.l.upper = (fp.l.upper & ~(0x7fff)) | EXPBIAS

#else

/* NOTE: arm is horrible as double 32 bit words are stored in big endian ! */
typedef union {
    double d;
#if !defined(HOST_WORDS_BIGENDIAN) && !defined(__arm__)
    struct {
        uint32_t lower;
        int32_t upper;
    } l;
#else
    struct {
        int32_t upper;
        uint32_t lower;
    } l;
#endif
#ifndef __arm__
    int64_t ll;
#endif
} CPU86_LDoubleU;

/* the following deal with IEEE double-precision numbers */
#define MAXEXPD 0x7ff
#define EXPBIAS 1023
#define EXPD(fp)	(((fp.l.upper) >> 20) & 0x7FF)
#define SIGND(fp)	((fp.l.upper) & 0x80000000)
#ifdef __arm__
#define MANTD(fp)	(fp.l.lower | ((uint64_t)(fp.l.upper & ((1 << 20) - 1)) << 32))
#else
#define MANTD(fp)	(fp.ll & ((1LL << 52) - 1))
#endif
#define BIASEXPONENT(fp) fp.l.upper = (fp.l.upper & ~(0x7ff << 20)) | (EXPBIAS << 20)
#endif

static inline void fpush(void)
{
    env->fpstt = (env->fpstt - 1) & 7;
    env->fptags[env->fpstt] = 0; /* validate stack entry */
}

static inline void fpop(void)
{
    env->fptags[env->fpstt] = 1; /* invvalidate stack entry */
    env->fpstt = (env->fpstt + 1) & 7;
}

#ifndef USE_X86LDOUBLE
static inline CPU86_LDouble helper_fldt(target_ulong ptr)
{
    CPU86_LDoubleU temp;
    int upper, e;
    uint64_t ll;

    /* mantissa */
    upper = lduw(ptr + 8);
    /* XXX: handle overflow ? */
    e = (upper & 0x7fff) - 16383 + EXPBIAS; /* exponent */
    e |= (upper >> 4) & 0x800; /* sign */
    ll = (ldq(ptr) >> 11) & ((1LL << 52) - 1);
#ifdef __arm__
    temp.l.upper = (e << 20) | (ll >> 32);
    temp.l.lower = ll;
#else
    temp.ll = ll | ((uint64_t)e << 52);
#endif
    return temp.d;
}

static inline void helper_fstt(CPU86_LDouble f, target_ulong ptr)
{
    CPU86_LDoubleU temp;
    int e;

    temp.d = f;
    /* mantissa */
    stq(ptr, (MANTD(temp) << 11) | (1LL << 63));
    /* exponent + sign */
    e = EXPD(temp) - EXPBIAS + 16383;
    e |= SIGND(temp) >> 16;
    stw(ptr + 8, e);
}
#else

/* we use memory access macros */

static inline CPU86_LDouble helper_fldt(target_ulong ptr)
{
    CPU86_LDoubleU temp;

    temp.l.lower = ldq(ptr);
    temp.l.upper = lduw(ptr + 8);
    return temp.d;
}

static inline void helper_fstt(CPU86_LDouble f, target_ulong ptr)
{
    CPU86_LDoubleU temp;

    temp.d = f;
    stq(ptr, temp.l.lower);
    stw(ptr + 8, temp.l.upper);
}

#endif /* USE_X86LDOUBLE */

#define FPUS_IE (1 << 0)
#define FPUS_DE (1 << 1)
#define FPUS_ZE (1 << 2)
#define FPUS_OE (1 << 3)
#define FPUS_UE (1 << 4)
#define FPUS_PE (1 << 5)
#define FPUS_SF (1 << 6)
#define FPUS_SE (1 << 7)
#define FPUS_B  (1 << 15)

#define FPUC_EM 0x3f

static inline uint32_t compute_eflags(void)
{
    return env->mflags | helper_cc_compute_all(CC_OP) | (DF & DF_MASK);
}

/* NOTE: CC_OP must be modified manually to CC_OP_EFLAGS */
static inline void load_eflags(int eflags, int update_mask)
{
    CC_SRC_W(eflags & CFLAGS_MASK);
    DF_W((eflags & DF_MASK) ? -1 : 1);
    /*
    WR_cpu(env, eflags, (RR_cpu(env, eflags) & ~update_mask) |
        (eflags & update_mask) | 0x2);
    */
    env->mflags = (env->mflags & ~update_mask) |
                    (eflags & MFLAGS_MASK & update_mask);
}

static inline void env_to_regs(void)
{
#ifdef reg_EAX
    EAX = env->regs[R_EAX];
#endif
#ifdef reg_ECX
    ECX = env->regs[R_ECX];
#endif
#ifdef reg_EDX
    EDX = env->regs[R_EDX];
#endif
#ifdef reg_EBX
    EBX = env->regs[R_EBX];
#endif
#ifdef reg_ESP
    ESP = env->regs[R_ESP];
#endif
#ifdef reg_EBP
    EBP = env->regs[R_EBP];
#endif
#ifdef reg_ESI
    ESI = env->regs[R_ESI];
#endif
#ifdef reg_EDI
    EDI = env->regs[R_EDI];
#endif
}

static inline void regs_to_env(void)
{
#ifdef reg_EAX
    env->regs[R_EAX] = EAX;
#endif
#ifdef reg_ECX
    env->regs[R_ECX] = ECX;
#endif
#ifdef reg_EDX
    env->regs[R_EDX] = EDX;
#endif
#ifdef reg_EBX
    env->regs[R_EBX] = EBX;
#endif
#ifdef reg_ESP
    env->regs[R_ESP] = ESP;
#endif
#ifdef reg_EBP
    env->regs[R_EBP] = EBP;
#endif
#ifdef reg_ESI
    env->regs[R_ESI] = ESI;
#endif
#ifdef reg_EDI
    env->regs[R_EDI] = EDI;
#endif
}

static inline int cpu_has_work(CPUState *env)
{
    int work;

    work = (env->interrupt_request & CPU_INTERRUPT_HARD) &&
           (env->mflags & IF_MASK);
    work |= env->interrupt_request & CPU_INTERRUPT_NMI;
    work |= env->interrupt_request & CPU_INTERRUPT_INIT;
    work |= env->interrupt_request & CPU_INTERRUPT_SIPI;

    return work;
}

static inline int cpu_halted(CPUState *env) {
    /* handle exit of HALTED state */
    if (!env->halted)
        return 0;
    /* disable halt condition */
    if (cpu_has_work(env)) {
        env->halted = 0;
        return 0;
    }
    return EXCP_HALTED;
}

/* load efer and update the corresponding hflags. XXX: do consistency
   checks with cpuid bits ? */
static inline void cpu_load_efer(CPUState *env, uint64_t val)
{
    env->efer = val;
    env->hflags &= ~(HF_LMA_MASK | HF_SVME_MASK);
    if (env->efer & MSR_EFER_LMA)
        env->hflags |= HF_LMA_MASK;
    if (env->efer & MSR_EFER_SVME)
        env->hflags |= HF_SVME_MASK;
}
