#include "def-helper.h"

#define _M_CF    ((uint64_t) 1) <<29
#define _M_VF   ((uint64_t) 1) <<30
#define _M_NF   ((uint64_t) 1) <<31
#define _M_ZF   ((uint64_t) 1) <<32
#define _M_R0   ((uint64_t) 1) <<33
#define _M_R1   ((uint64_t) 1) <<34
#define _M_R2   ((uint64_t) 1) <<35
#define _M_R3   ((uint64_t) 1) <<36
#define _M_R4   ((uint64_t) 1) <<37
#define _M_R5   ((uint64_t) 1) <<38
#define _M_R6   ((uint64_t) 1) <<39
#define _M_R7   ((uint64_t) 1) <<40
#define _M_R8   ((uint64_t) 1) <<41
#define _M_R9   ((uint64_t) 1) <<42
#define _M_R10  ((uint64_t) 1) << 43
#define _M_R11  ((uint64_t) 1) << 44
#define _M_R12  ((uint64_t) 1) << 45
#define _M_R13  ((uint64_t) 1) << 46
#define _M_R14  ((uint64_t) 1) << 47
#define _M_SPSR    				  	(1 << 0 )
#define _M_BANKED_SPSR	((uint64_t) 63) 	<< 1
#define _M_BANKED_R13	((uint64_t) 63) 	<< 7
#define _M_BANKED_R14   ((uint64_t) 63) 	<< 13
#define	_M_USR_REGS		((uint64_t) 31) 	<< 19
#define _M_REGS 		((uint64_t) 0x7FFF)<< 33
#define _M_ALL		~((uint64_t)(0)<<39)
#define _M_HIGHREGS     ((uint64_t) 0x7F) << 41
#define CPSR_SPECIAL (_M_HIGHREGS | _M_USR_REGS | _M_BANKED_R13 | _M_BANKED_R14 | _M_SPSR | _M_BANKED_SPSR )

#define _RM_EXCP    (_M_CF | _M_VF | _M_NF | _M_ZF)
#define _WM_EXCP    (_M_CF | _M_VF | _M_NF | _M_ZF)
#define _AM_EXCP    0


DEF_HELPER_2_M(cpsr_write, void, i32, i32, CPSR_SPECIAL, ( CPSR_SPECIAL | _WM_EXCP ) , 0)
DEF_HELPER_0_M(cpsr_read, i32, ( CPSR_SPECIAL | _RM_EXCP ), CPSR_SPECIAL, 0)

DEF_HELPER_1_M(get_user_reg, i32, i32, (_M_USR_REGS | _M_BANKED_R14 | _M_BANKED_R13 | _M_REGS), 0, 0)
DEF_HELPER_2_M(set_user_reg, void, i32, i32, 0, (_M_USR_REGS | _M_BANKED_R14 | _M_BANKED_R13 | _M_REGS) ,0)

DEF_HELPER_2_M(add_cc, i32, i32, i32, 0, _WM_EXCP, 0)
DEF_HELPER_2_M(adc_cc, i32, i32, i32, 0, _WM_EXCP, 0)
DEF_HELPER_2_M(sub_cc, i32, i32, i32, 0, _WM_EXCP, 0)
DEF_HELPER_2_M(sbc_cc, i32, i32, i32, _M_CF, _WM_EXCP, 0)
DEF_HELPER_2_M(shl_cc, i32, i32, i32, 0, _M_CF, 0)
DEF_HELPER_2_M(shr_cc, i32, i32, i32, 0, _M_CF, 0)
DEF_HELPER_2_M(sar_cc, i32, i32, i32, 0, _M_CF, 0)
DEF_HELPER_2_M(ror_cc, i32, i32, i32, 0, _M_CF, 0)

DEF_HELPER_1_M(clz, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(sxtb16, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(uxtb16, i32, i32, 0, 0, 0)

DEF_HELPER_2_M(add_setq, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(add_saturate, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(sub_saturate, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(add_usaturate, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(sub_usaturate, i32, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(double_saturate, i32, s32, 0, 0, 0)
DEF_HELPER_2_M(sdiv, s32, s32, s32, 0, 0, 0)
DEF_HELPER_2_M(udiv, i32, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(rbit, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(abs, i32, i32, 0, 0, 0)

#ifdef CONFIG_TRACE
DEF_HELPER_1_M(traceTicks, void, i32, 0, 0, 0)
DEF_HELPER_0_M(traceInsn, void, 0, 0, 0)
#if HOST_LONG_BITS == 32
DEF_HELPER_2_M(traceBB32, void, i64, i32, 0, 0, 0)
#endif
#if HOST_LONG_BITS == 64
DEF_HELPER_2_M(traceBB64, void, i64, i64, 0, 0, 0)
#endif
#endif

#define PAS_OP(pfx)  \
    DEF_HELPER_3_M(pfx ## add8, i32, i32, i32, ptr, 0, 0, 0) \
    DEF_HELPER_3_M(pfx ## sub8, i32, i32, i32, ptr, 0, 0, 0) \
    DEF_HELPER_3_M(pfx ## sub16, i32, i32, i32, ptr, 0, 0, 0) \
    DEF_HELPER_3_M(pfx ## add16, i32, i32, i32, ptr, 0, 0, 0) \
    DEF_HELPER_3_M(pfx ## addsubx, i32, i32, i32, ptr, 0, 0, 0) \
    DEF_HELPER_3_M(pfx ## subaddx, i32, i32, i32, ptr, 0, 0, 0)

PAS_OP(s)
PAS_OP(u)
#undef PAS_OP

#define PAS_OP(pfx)  \
    DEF_HELPER_2_M(pfx ## add8, i32, i32, i32, 0, 0, 0) \
    DEF_HELPER_2_M(pfx ## sub8, i32, i32, i32, 0, 0, 0) \
    DEF_HELPER_2_M(pfx ## sub16, i32, i32, i32, 0, 0, 0) \
    DEF_HELPER_2_M(pfx ## add16, i32, i32, i32, 0, 0, 0) \
    DEF_HELPER_2_M(pfx ## addsubx, i32, i32, i32, 0, 0, 0) \
    DEF_HELPER_2_M(pfx ## subaddx, i32, i32, i32, 0, 0, 0)
PAS_OP(q)
PAS_OP(sh)
PAS_OP(uq)
PAS_OP(uh)
#undef PAS_OP

DEF_HELPER_2_M(ssat, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(usat, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(ssat16, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(usat16, i32, i32, i32, 0, 0 ,0)

DEF_HELPER_2_M(usad8, i32, i32, i32, 0, 0, 0)

DEF_HELPER_1_M(logicq_cc, i32, i64, 0, 0, 0)

DEF_HELPER_3_M(sel_flags, i32, i32, i32, i32, 0, 0, 0)
DEF_HELPER_1_M(exception, void, i32, 0, 0, 0)
DEF_HELPER_0_M(wfi, void, 0, 0, 0)

DEF_HELPER_2_M(get_r13_banked, i32, env, i32, _M_BANKED_R13, 0, 0)
DEF_HELPER_3_M(set_r13_banked, void, env, i32, i32, 0, _M_BANKED_R13, 0)

DEF_HELPER_2(mark_exclusive, void, env, i32)
DEF_HELPER_2(test_exclusive, i32, env, i32)
DEF_HELPER_1(clrex, void, env)

DEF_HELPER_3(v7m_msr, void, env, i32, i32)
DEF_HELPER_2(v7m_mrs, i32, env, i32)

DEF_HELPER_3_M(set_cp15, void, env, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(get_cp15, i32, env, i32, 0, _M_ZF, 0)

DEF_HELPER_3_M(set_cp, void, env, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(get_cp, i32, env, i32, 0, 0, 0)

DEF_HELPER_1(vfp_get_fpscr, i32, env)
DEF_HELPER_2(vfp_set_fpscr, void, env, i32)

DEF_HELPER_3(vfp_adds, f32, f32, f32, env)
DEF_HELPER_3(vfp_addd, f64, f64, f64, env)
DEF_HELPER_3(vfp_subs, f32, f32, f32, env)
DEF_HELPER_3(vfp_subd, f64, f64, f64, env)
DEF_HELPER_3(vfp_muls, f32, f32, f32, env)
DEF_HELPER_3(vfp_muld, f64, f64, f64, env)
DEF_HELPER_3(vfp_divs, f32, f32, f32, env)
DEF_HELPER_3(vfp_divd, f64, f64, f64, env)
DEF_HELPER_1(vfp_negs, f32, f32)
DEF_HELPER_1(vfp_negd, f64, f64)
DEF_HELPER_1(vfp_abss, f32, f32)
DEF_HELPER_1(vfp_absd, f64, f64)
DEF_HELPER_2(vfp_sqrts, f32, f32, env)
DEF_HELPER_2(vfp_sqrtd, f64, f64, env)
DEF_HELPER_3(vfp_cmps, void, f32, f32, env)
DEF_HELPER_3(vfp_cmpd, void, f64, f64, env)
DEF_HELPER_3(vfp_cmpes, void, f32, f32, env)
DEF_HELPER_3(vfp_cmped, void, f64, f64, env)

DEF_HELPER_2(vfp_fcvtds, f64, f32, env)
DEF_HELPER_2(vfp_fcvtsd, f32, f64, env)

DEF_HELPER_2(vfp_uitos, f32, f32, env)
DEF_HELPER_2(vfp_uitod, f64, f32, env)
DEF_HELPER_2(vfp_sitos, f32, f32, env)
DEF_HELPER_2(vfp_sitod, f64, f32, env)

DEF_HELPER_2(vfp_touis, f32, f32, env)
DEF_HELPER_2(vfp_touid, f32, f64, env)
DEF_HELPER_2(vfp_touizs, f32, f32, env)
DEF_HELPER_2(vfp_touizd, f32, f64, env)
DEF_HELPER_2(vfp_tosis, f32, f32, env)
DEF_HELPER_2(vfp_tosid, f32, f64, env)
DEF_HELPER_2(vfp_tosizs, f32, f32, env)
DEF_HELPER_2(vfp_tosizd, f32, f64, env)

DEF_HELPER_3(vfp_toshs, f32, f32, i32, env)
DEF_HELPER_3(vfp_tosls, f32, f32, i32, env)
DEF_HELPER_3(vfp_touhs, f32, f32, i32, env)
DEF_HELPER_3(vfp_touls, f32, f32, i32, env)
DEF_HELPER_3(vfp_toshd, f64, f64, i32, env)
DEF_HELPER_3(vfp_tosld, f64, f64, i32, env)
DEF_HELPER_3(vfp_touhd, f64, f64, i32, env)
DEF_HELPER_3(vfp_tould, f64, f64, i32, env)
DEF_HELPER_3(vfp_shtos, f32, f32, i32, env)
DEF_HELPER_3(vfp_sltos, f32, f32, i32, env)
DEF_HELPER_3(vfp_uhtos, f32, f32, i32, env)
DEF_HELPER_3(vfp_ultos, f32, f32, i32, env)
DEF_HELPER_3(vfp_shtod, f64, f64, i32, env)
DEF_HELPER_3(vfp_sltod, f64, f64, i32, env)
DEF_HELPER_3(vfp_uhtod, f64, f64, i32, env)
DEF_HELPER_3(vfp_ultod, f64, f64, i32, env)

DEF_HELPER_3(recps_f32, f32, f32, f32, env)
DEF_HELPER_3(rsqrts_f32, f32, f32, f32, env)
DEF_HELPER_2(recpe_f32, f32, f32, env)
DEF_HELPER_2(rsqrte_f32, f32, f32, env)
DEF_HELPER_2(recpe_u32, i32, i32, env)
DEF_HELPER_2(rsqrte_u32, i32, i32, env)
DEF_HELPER_4(neon_tbl, i32, i32, i32, i32, i32)
DEF_HELPER_2(neon_add_saturate_u64, i64, i64, i64)
DEF_HELPER_2(neon_add_saturate_s64, i64, i64, i64)
DEF_HELPER_2(neon_sub_saturate_u64, i64, i64, i64)
DEF_HELPER_2(neon_sub_saturate_s64, i64, i64, i64)

DEF_HELPER_2_M(shl, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(shr, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(sar, i32, i32, i32, 0, 0, 0)
DEF_HELPER_2_M(ror, i32, i32, i32, 0, 0, 0)

/* neon_helper.c */
DEF_HELPER_3(neon_qadd_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_s16, i32, env, i32, i32)

DEF_HELPER_2(neon_hadd_s8, i32, i32, i32)
DEF_HELPER_2(neon_hadd_u8, i32, i32, i32)
DEF_HELPER_2(neon_hadd_s16, i32, i32, i32)
DEF_HELPER_2(neon_hadd_u16, i32, i32, i32)
DEF_HELPER_2(neon_hadd_s32, s32, s32, s32)
DEF_HELPER_2(neon_hadd_u32, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s8, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_u8, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s16, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_u16, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s32, s32, s32, s32)
DEF_HELPER_2(neon_rhadd_u32, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s8, i32, i32, i32)
DEF_HELPER_2(neon_hsub_u8, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s16, i32, i32, i32)
DEF_HELPER_2(neon_hsub_u16, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s32, s32, s32, s32)
DEF_HELPER_2(neon_hsub_u32, i32, i32, i32)

DEF_HELPER_2(neon_cgt_u8, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s8, i32, i32, i32)
DEF_HELPER_2(neon_cgt_u16, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s16, i32, i32, i32)
DEF_HELPER_2(neon_cgt_u32, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s32, i32, i32, i32)
DEF_HELPER_2(neon_cge_u8, i32, i32, i32)
DEF_HELPER_2(neon_cge_s8, i32, i32, i32)
DEF_HELPER_2(neon_cge_u16, i32, i32, i32)
DEF_HELPER_2(neon_cge_s16, i32, i32, i32)
DEF_HELPER_2(neon_cge_u32, i32, i32, i32)
DEF_HELPER_2(neon_cge_s32, i32, i32, i32)

DEF_HELPER_2(neon_min_u8, i32, i32, i32)
DEF_HELPER_2(neon_min_s8, i32, i32, i32)
DEF_HELPER_2(neon_min_u16, i32, i32, i32)
DEF_HELPER_2(neon_min_s16, i32, i32, i32)
DEF_HELPER_2(neon_min_u32, i32, i32, i32)
DEF_HELPER_2(neon_min_s32, i32, i32, i32)
DEF_HELPER_2(neon_max_u8, i32, i32, i32)
DEF_HELPER_2(neon_max_s8, i32, i32, i32)
DEF_HELPER_2(neon_max_u16, i32, i32, i32)
DEF_HELPER_2(neon_max_s16, i32, i32, i32)
DEF_HELPER_2(neon_max_u32, i32, i32, i32)
DEF_HELPER_2(neon_max_s32, i32, i32, i32)
DEF_HELPER_2(neon_pmin_u8, i32, i32, i32)
DEF_HELPER_2(neon_pmin_s8, i32, i32, i32)
DEF_HELPER_2(neon_pmin_u16, i32, i32, i32)
DEF_HELPER_2(neon_pmin_s16, i32, i32, i32)
DEF_HELPER_2(neon_pmax_u8, i32, i32, i32)
DEF_HELPER_2(neon_pmax_s8, i32, i32, i32)
DEF_HELPER_2(neon_pmax_u16, i32, i32, i32)
DEF_HELPER_2(neon_pmax_s16, i32, i32, i32)

DEF_HELPER_2(neon_abd_u8, i32, i32, i32)
DEF_HELPER_2(neon_abd_s8, i32, i32, i32)
DEF_HELPER_2(neon_abd_u16, i32, i32, i32)
DEF_HELPER_2(neon_abd_s16, i32, i32, i32)
DEF_HELPER_2(neon_abd_u32, i32, i32, i32)
DEF_HELPER_2(neon_abd_s32, i32, i32, i32)

DEF_HELPER_2(neon_shl_u8, i32, i32, i32)
DEF_HELPER_2(neon_shl_s8, i32, i32, i32)
DEF_HELPER_2(neon_shl_u16, i32, i32, i32)
DEF_HELPER_2(neon_shl_s16, i32, i32, i32)
DEF_HELPER_2(neon_shl_u32, i32, i32, i32)
DEF_HELPER_2(neon_shl_s32, i32, i32, i32)
DEF_HELPER_2(neon_shl_u64, i64, i64, i64)
DEF_HELPER_2(neon_shl_s64, i64, i64, i64)
DEF_HELPER_2(neon_rshl_u8, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s8, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u16, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s16, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u32, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s32, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u64, i64, i64, i64)
DEF_HELPER_2(neon_rshl_s64, i64, i64, i64)
DEF_HELPER_3(neon_qshl_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qshl_s64, i64, env, i64, i64)
DEF_HELPER_3(neon_qrshl_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qrshl_s64, i64, env, i64, i64)

DEF_HELPER_2(neon_add_u8, i32, i32, i32)
DEF_HELPER_2(neon_add_u16, i32, i32, i32)
DEF_HELPER_2(neon_padd_u8, i32, i32, i32)
DEF_HELPER_2(neon_padd_u16, i32, i32, i32)
DEF_HELPER_2(neon_sub_u8, i32, i32, i32)
DEF_HELPER_2(neon_sub_u16, i32, i32, i32)
DEF_HELPER_2(neon_mul_u8, i32, i32, i32)
DEF_HELPER_2(neon_mul_u16, i32, i32, i32)
DEF_HELPER_2(neon_mul_p8, i32, i32, i32)

DEF_HELPER_2(neon_tst_u8, i32, i32, i32)
DEF_HELPER_2(neon_tst_u16, i32, i32, i32)
DEF_HELPER_2(neon_tst_u32, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u8, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u16, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u32, i32, i32, i32)

DEF_HELPER_1(neon_abs_s8, i32, i32)
DEF_HELPER_1(neon_abs_s16, i32, i32)
DEF_HELPER_1(neon_clz_u8, i32, i32)
DEF_HELPER_1(neon_clz_u16, i32, i32)
DEF_HELPER_1(neon_cls_s8, i32, i32)
DEF_HELPER_1(neon_cls_s16, i32, i32)
DEF_HELPER_1(neon_cls_s32, i32, i32)
DEF_HELPER_1(neon_cnt_u8, i32, i32)

DEF_HELPER_3(neon_qdmulh_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrdmulh_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qdmulh_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrdmulh_s32, i32, env, i32, i32)

DEF_HELPER_1(neon_narrow_u8, i32, i64)
DEF_HELPER_1(neon_narrow_u16, i32, i64)
DEF_HELPER_2(neon_narrow_sat_u8, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s8, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_u16, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s16, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_u32, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s32, i32, env, i64)
DEF_HELPER_1(neon_narrow_high_u8, i32, i64)
DEF_HELPER_1(neon_narrow_high_u16, i32, i64)
DEF_HELPER_1(neon_narrow_round_high_u8, i32, i64)
DEF_HELPER_1(neon_narrow_round_high_u16, i32, i64)
DEF_HELPER_1(neon_widen_u8, i64, i32)
DEF_HELPER_1(neon_widen_s8, i64, i32)
DEF_HELPER_1(neon_widen_u16, i64, i32)
DEF_HELPER_1(neon_widen_s16, i64, i32)

DEF_HELPER_2(neon_addl_u16, i64, i64, i64)
DEF_HELPER_2(neon_addl_u32, i64, i64, i64)
DEF_HELPER_2(neon_paddl_u16, i64, i64, i64)
DEF_HELPER_2(neon_paddl_u32, i64, i64, i64)
DEF_HELPER_2(neon_subl_u16, i64, i64, i64)
DEF_HELPER_2(neon_subl_u32, i64, i64, i64)
DEF_HELPER_3(neon_addl_saturate_s32, i64, env, i64, i64)
DEF_HELPER_3(neon_addl_saturate_s64, i64, env, i64, i64)
DEF_HELPER_2(neon_abdl_u16, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s16, i64, i32, i32)
DEF_HELPER_2(neon_abdl_u32, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s32, i64, i32, i32)
DEF_HELPER_2(neon_abdl_u64, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s64, i64, i32, i32)
DEF_HELPER_2(neon_mull_u8, i64, i32, i32)
DEF_HELPER_2(neon_mull_s8, i64, i32, i32)
DEF_HELPER_2(neon_mull_u16, i64, i32, i32)
DEF_HELPER_2(neon_mull_s16, i64, i32, i32)

DEF_HELPER_1(neon_negl_u16, i64, i64)
DEF_HELPER_1(neon_negl_u32, i64, i64)
DEF_HELPER_1(neon_negl_u64, i64, i64)

DEF_HELPER_2(neon_qabs_s8, i32, env, i32)
DEF_HELPER_2(neon_qabs_s16, i32, env, i32)
DEF_HELPER_2(neon_qabs_s32, i32, env, i32)
DEF_HELPER_2(neon_qneg_s8, i32, env, i32)
DEF_HELPER_2(neon_qneg_s16, i32, env, i32)
DEF_HELPER_2(neon_qneg_s32, i32, env, i32)

DEF_HELPER_0(neon_trn_u8, void)
DEF_HELPER_0(neon_trn_u16, void)
DEF_HELPER_0(neon_unzip_u8, void)
DEF_HELPER_0(neon_zip_u8, void)
DEF_HELPER_0(neon_zip_u16, void)

DEF_HELPER_2(neon_min_f32, i32, i32, i32)
DEF_HELPER_2(neon_max_f32, i32, i32, i32)
DEF_HELPER_2(neon_abd_f32, i32, i32, i32)
DEF_HELPER_2(neon_add_f32, i32, i32, i32)
DEF_HELPER_2(neon_sub_f32, i32, i32, i32)
DEF_HELPER_2(neon_mul_f32, i32, i32, i32)
DEF_HELPER_2(neon_ceq_f32, i32, i32, i32)
DEF_HELPER_2(neon_cge_f32, i32, i32, i32)
DEF_HELPER_2(neon_cgt_f32, i32, i32, i32)
DEF_HELPER_2(neon_acge_f32, i32, i32, i32)
DEF_HELPER_2(neon_acgt_f32, i32, i32, i32)

/* iwmmxt_helper.c */
DEF_HELPER_2(iwmmxt_maddsq, i64, i64, i64)
DEF_HELPER_2(iwmmxt_madduq, i64, i64, i64)
DEF_HELPER_2(iwmmxt_sadb, i64, i64, i64)
DEF_HELPER_2(iwmmxt_sadw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mulslw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mulshw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mululw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_muluhw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_macsw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_macuw, i64, i64, i64)
DEF_HELPER_1(iwmmxt_setpsr_nz, i32, i64)

#define DEF_IWMMXT_HELPER_SIZE_ENV(name) \
DEF_HELPER_3(iwmmxt_##name##b, i64, env, i64, i64) \
DEF_HELPER_3(iwmmxt_##name##w, i64, env, i64, i64) \
DEF_HELPER_3(iwmmxt_##name##l, i64, env, i64, i64) \

DEF_IWMMXT_HELPER_SIZE_ENV(unpackl)
DEF_IWMMXT_HELPER_SIZE_ENV(unpackh)

DEF_HELPER_2(iwmmxt_unpacklub, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackluw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklul, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhub, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhuw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhul, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsb, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsl, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsb, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsl, i64, env, i64)

DEF_IWMMXT_HELPER_SIZE_ENV(cmpeq)
DEF_IWMMXT_HELPER_SIZE_ENV(cmpgtu)
DEF_IWMMXT_HELPER_SIZE_ENV(cmpgts)

DEF_IWMMXT_HELPER_SIZE_ENV(mins)
DEF_IWMMXT_HELPER_SIZE_ENV(minu)
DEF_IWMMXT_HELPER_SIZE_ENV(maxs)
DEF_IWMMXT_HELPER_SIZE_ENV(maxu)

DEF_IWMMXT_HELPER_SIZE_ENV(subn)
DEF_IWMMXT_HELPER_SIZE_ENV(addn)
DEF_IWMMXT_HELPER_SIZE_ENV(subu)
DEF_IWMMXT_HELPER_SIZE_ENV(addu)
DEF_IWMMXT_HELPER_SIZE_ENV(subs)
DEF_IWMMXT_HELPER_SIZE_ENV(adds)

DEF_HELPER_3(iwmmxt_avgb0, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgb1, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgw0, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgw1, i64, env, i64, i64)

DEF_HELPER_2(iwmmxt_msadb, i64, i64, i64)

DEF_HELPER_3(iwmmxt_align, i64, i64, i64, i32)
DEF_HELPER_4(iwmmxt_insr, i64, i64, i32, i32, i32)

DEF_HELPER_1(iwmmxt_bcstb, i64, i32)
DEF_HELPER_1(iwmmxt_bcstw, i64, i32)
DEF_HELPER_1(iwmmxt_bcstl, i64, i32)

DEF_HELPER_1(iwmmxt_addcb, i64, i64)
DEF_HELPER_1(iwmmxt_addcw, i64, i64)
DEF_HELPER_1(iwmmxt_addcl, i64, i64)

DEF_HELPER_1(iwmmxt_msbb, i32, i64)
DEF_HELPER_1(iwmmxt_msbw, i32, i64)
DEF_HELPER_1(iwmmxt_msbl, i32, i64)

DEF_HELPER_3(iwmmxt_srlw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_srll, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_srlq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sllw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_slll, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sllq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sraw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sral, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sraq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorl, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_shufh, i64, env, i64, i32)

DEF_HELPER_3(iwmmxt_packuw, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packul, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packuq, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsw, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsl, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsq, i64, env, i64, i64)

DEF_HELPER_3(iwmmxt_muladdsl, i64, i64, i32, i32)
DEF_HELPER_3(iwmmxt_muladdsw, i64, i64, i32, i32)
DEF_HELPER_3(iwmmxt_muladdswl, i64, i64, i32, i32)

DEF_HELPER_2(set_teecr, void, env, i32)

#ifdef CONFIG_MEMCHECK
/* Hooks to translated BL/BLX. This callback is used to build thread's
 * calling stack.
 * Param:
 *  First pointer contains guest PC where BL/BLX has been found.
 *  Second pointer contains guest PC where BL/BLX will return.
 */
DEF_HELPER_2(on_call, void, i32, i32)
/* Hooks to return from translated BL/BLX. This callback is used to build
 * thread's calling stack.
 * Param:
 *  Pointer contains guest PC where BL/BLX will return.
 */
DEF_HELPER_1(on_ret, void, i32)
#endif  // CONFIG_MEMCHECK
#include "def-helper.h"
