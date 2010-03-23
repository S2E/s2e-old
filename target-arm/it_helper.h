/* Copyright (C) 2007-2010 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * Contains implementation of helper routines for IT block support.
 */

#ifndef QEMU_TARGET_ARM_IT_HELPER_H
#define QEMU_TARGET_ARM_IT_HELPER_H

/* Gets condition bits from ITSTATE.
 * Return:
 *  ITSTATE condition bits in the low 4 bits of the returned value.
 */
static inline uint8_t
itstate_cond(uint8_t itstate)
{
    return (itstate >> 4) & 0xf;
}

/* Checks if ITSTATE defines the last instruction in an IT block.
 * Param:
 *  itstate - ITSTATE for an instruction in an IT block.
 * Return:
 *  boolean: 1 if an instruction is the last instruction in its IT block,
 *  or zero if there are more instruction in the IT block.
 */
static inline int
itstate_is_last_it_insn(uint8_t itstate)
{
    return (itstate & 0x7) == 0;
}

/* Checks if ITSTATE suggests that an IT block instruction is being translated.
 * Return:
 *  boolean: 1 if an IT block instruction is being translated, or zero if
 *  a "normal" instruction is being translated.
 */
static inline int
itstate_is_in_it_block(uint8_t itstate)
{
    return (itstate & 0xff) != 0;
}

/* Advances ITSTATE to the next instruction in an IT block.
 * Param:
 *  itstate - ITSTATE of the currently executing instruction.
 * Retunn:
 *  ITSTATE for the next instruction in an IT block, or zero is currently
 *  executing instruction was the last IT block instruction.
 */
static inline uint8_t
itstate_advance(uint8_t itstate)
{
    if (itstate_is_last_it_insn(itstate)) {
        return 0;
    } else {
        return (itstate & 0xe0) | ((itstate & 0xf) << 1);
    }
}

/* Checks if given THUMB instruction is an IT instruction.
 * Param:
 *  insn - THUMB instruction to check.
 * Return:
 *  boolean: 1 if instruction is IT instruction, or zero otherwise.
 */
static inline int
is_it_insn(uint16_t insn)
{
    return (insn & 0xff00) == 0xbf00 && (insn & 0x000f) != 0;
}

#endif  // QEMU_TARGET_ARM_IT_HELPER_H
