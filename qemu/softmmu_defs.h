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

#ifndef SOFTMMU_DEFS_H
#define SOFTMMU_DEFS_H

uint8_t REGPARM __ldb_mmu(target_ulong addr, int mmu_idx);
void REGPARM __stb_mmu(target_ulong addr, uint8_t val, int mmu_idx);
uint16_t REGPARM __ldw_mmu(target_ulong addr, int mmu_idx);
void REGPARM __stw_mmu(target_ulong addr, uint16_t val, int mmu_idx);
uint32_t REGPARM __ldl_mmu(target_ulong addr, int mmu_idx);
void REGPARM __stl_mmu(target_ulong addr, uint32_t val, int mmu_idx);
uint64_t REGPARM __ldq_mmu(target_ulong addr, int mmu_idx);
void REGPARM __stq_mmu(target_ulong addr, uint64_t val, int mmu_idx);

uint8_t REGPARM __ldb_cmmu(target_ulong addr, int mmu_idx);
void REGPARM __stb_cmmu(target_ulong addr, uint8_t val, int mmu_idx);
uint16_t REGPARM __ldw_cmmu(target_ulong addr, int mmu_idx);
void REGPARM __stw_cmmu(target_ulong addr, uint16_t val, int mmu_idx);
uint32_t REGPARM __ldl_cmmu(target_ulong addr, int mmu_idx);
void REGPARM __stl_cmmu(target_ulong addr, uint32_t val, int mmu_idx);
uint64_t REGPARM __ldq_cmmu(target_ulong addr, int mmu_idx);
void REGPARM __stq_cmmu(target_ulong addr, uint64_t val, int mmu_idx);

uint8_t REGPARM io_readb_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeb_mmu(target_phys_addr_t physaddr, uint8_t val, target_ulong addr, void *retaddr);
uint16_t REGPARM io_readw_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writew_mmu(target_phys_addr_t physaddr, uint16_t val, target_ulong addr, void *retaddr);
uint32_t REGPARM io_readl_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writel_mmu(target_phys_addr_t physaddr, uint32_t val, target_ulong addr, void *retaddr);
uint64_t REGPARM io_readq_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeq_mmu(target_phys_addr_t physaddr, uint64_t val, target_ulong addr, void *retaddr);

uint8_t REGPARM io_readb_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeb_cmmu(target_phys_addr_t physaddr, uint8_t val, target_ulong addr, void *retaddr);
uint16_t REGPARM io_readw_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writew_cmmu(target_phys_addr_t physaddr, uint16_t val, target_ulong addr, void *retaddr);
uint32_t REGPARM io_readl_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writel_cmmu(target_phys_addr_t physaddr, uint32_t val, target_ulong addr, void *retaddr);
uint64_t REGPARM io_readq_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeq_cmmu(target_phys_addr_t physaddr, uint64_t val, target_ulong addr, void *retaddr);

void io_write_chkb_mmu_s2e_trace(target_phys_addr_t physaddr, uint8_t val, target_ulong addr, void *retaddr);
void io_write_chkw_mmu_s2e_trace(target_phys_addr_t physaddr, uint16_t val, target_ulong addr, void *retaddr);
void io_write_chkl_mmu_s2e_trace(target_phys_addr_t physaddr, uint32_t val, target_ulong addr, void *retaddr);
void io_write_chkq_mmu_s2e_trace(target_phys_addr_t physaddr, uint64_t val, target_ulong addr, void *retaddr);

void io_write_chkb_mmu(target_phys_addr_t physaddr, uint8_t val, target_ulong addr, void *retaddr);
void io_write_chkw_mmu(target_phys_addr_t physaddr, uint16_t val, target_ulong addr, void *retaddr);
void io_write_chkl_mmu(target_phys_addr_t physaddr, uint32_t val, target_ulong addr, void *retaddr);
void io_write_chkq_mmu(target_phys_addr_t physaddr, uint64_t val, target_ulong addr, void *retaddr);

uint8_t io_read_chkb_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint16_t io_read_chkw_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint32_t io_read_chkl_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint64_t io_read_chkq_mmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);

uint8_t  io_read_chkb_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint16_t io_read_chkw_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint32_t io_read_chkl_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint64_t io_read_chkq_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);


uint8_t  io_read_chkb_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint16_t  io_read_chkw_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint32_t  io_read_chkl_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
uint64_t  io_read_chkq_cmmu(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);

uint8_t io_make_symbolicb_mmu(const char *name);
uint16_t io_make_symbolicw_mmu(const char *name);
uint32_t io_make_symbolicl_mmu(const char *name);
uint64_t io_make_symbolicq_mmu(const char *name);

uint8_t  io_read_chk_symb_b_mmu(const char *label, target_ulong physaddr, uintptr_t pa);
uint16_t  io_read_chk_symb_w_mmu(const char *label, target_ulong physaddr, uintptr_t pa);
uint32_t  io_read_chk_symb_l_mmu(const char *label, target_ulong physaddr, uintptr_t pa);
uint64_t  io_read_chk_symb_q_mmu(const char *label, target_ulong physaddr, uintptr_t pa);


#ifdef CONFIG_S2E

uint8_t REGPARM io_readb_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeb_mmu_s2e_trace(target_phys_addr_t physaddr, uint8_t val, target_ulong addr, void *retaddr);
uint16_t REGPARM io_readw_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writew_mmu_s2e_trace(target_phys_addr_t physaddr, uint16_t val, target_ulong addr, void *retaddr);
uint32_t REGPARM io_readl_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writel_mmu_s2e_trace(target_phys_addr_t physaddr, uint32_t val, target_ulong addr, void *retaddr);
uint64_t REGPARM io_readq_mmu_s2e_trace(target_phys_addr_t physaddr, target_ulong addr, void *retaddr);
void REGPARM io_writeq_mmu_s2e_trace(target_phys_addr_t physaddr, uint64_t val, target_ulong addr, void *retaddr);

uintptr_t s2e_notdirty_mem_write(target_phys_addr_t ram_addr);
int s2e_ismemfunc(void *f);

uint8_t REGPARM __ldb_mmu_s2e_trace(target_ulong addr, int mmu_idx);
void REGPARM __stb_mmu_s2e_trace(target_ulong addr, uint8_t val, int mmu_idx);
uint16_t REGPARM __ldw_mmu_s2e_trace(target_ulong addr, int mmu_idx);
void REGPARM __stw_mmu_s2e_trace(target_ulong addr, uint16_t val, int mmu_idx);
uint32_t REGPARM __ldl_mmu_s2e_trace(target_ulong addr, int mmu_idx);
void REGPARM __stl_mmu_s2e_trace(target_ulong addr, uint32_t val, int mmu_idx);
uint64_t REGPARM __ldq_mmu_s2e_trace(target_ulong addr, int mmu_idx);
void REGPARM __stq_mmu_s2e_trace(target_ulong addr, uint64_t val, int mmu_idx);

#endif

#endif
