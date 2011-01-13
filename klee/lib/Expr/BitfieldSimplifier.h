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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef KLEE_BITFIELDSIMPLIFIER_H
#define KLEE_BITFIELDSIMPLIFIER_H

#include "klee/Expr.h"
#include "klee/util/ExprHashMap.h"

namespace klee {

class BitfieldSimplifier {
protected:
    struct BitsInfo {
        uint64_t ignoredBits;   ///< Bits that can be ignored because they
                                ///< are not used by higher-level expressions
                                ///< (passed top-down)
        uint64_t knownOneBits;  ///< Bits known to be one (passed bottom-up)
        uint64_t knownZeroBits; ///< Bits known to be zero (passed bottom-up)
    };

    /// XXX: this cache will probably grow too large with time
    ExprHashMap<BitsInfo> m_bitsInfoCache;

    ref<Expr> replaceWithConstant(ref<Expr> e, uint64_t value);

    typedef std::pair<ref<Expr>, BitsInfo> ExprBitsInfo;
    ExprBitsInfo doSimplifyBits(ref<Expr> e, uint64_t ignoredBits);

public:
    ref<Expr> simplify(ref<Expr> e);
};

} // namespace klee

#endif // BITFIELDSIMPLIFIER_H
