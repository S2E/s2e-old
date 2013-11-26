/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef __S2E_OPCODES__

#define __S2E_OPCODES__

/** Custom opcodes

    ARM: 0xFF 0xXX 0xYY 0x00
	I386/AMD64: 0x0F 0x3F 0x00 0xXX 0xYY 0x00 0x00 0x00 0x00 0x00

    0xXX = main opcode
    0xYY = subfunction operand, used by some plugins
 */

//Central opcode repository for plugins that implement micro-operations
#define RAW_MONITOR_OPCODE   0xAA
#define MEMORY_TRACER_OPCODE 0xAC
#define STATE_MANAGER_OPCODE 0xAD
#define CODE_SELECTOR_OPCODE 0xAE
#define MODULE_EXECUTION_DETECTOR_OPCODE 0xAF

#ifdef TARGET_I386

#define OPSHIFT 8
#define SUBOPSHIFT 16
#define OPCODE_SIZE (2 + 8)
#define OPCODE_CHECK(operand, opcode) ((((operand)>> OPSHIFT) & 0xFF) == (opcode))
#define OPCODE_GETSUBFUNCTION(operand) (((operand) >> SUBOPSHIFT) & 0xFF)

#elif defined(TARGET_ARM)

#define OPSHIFT 16
#define SUBOPSHIFT 8
#define OPCODE_SIZE (2 + 2)
#define OPCODE_CHECK(operand, opcode) ((((operand)>> OPSHIFT) & 0xFF) == (opcode))
#define OPCODE_GETSUBFUNCTION(operand) (((operand) >> SUBOPSHIFT) & 0xFF)

#endif

#endif
