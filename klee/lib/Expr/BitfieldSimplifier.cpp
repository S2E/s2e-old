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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include "klee/BitfieldSimplifier.h"

#include <klee/Common.h>
#include "llvm/Support/CommandLine.h"

using namespace klee;
using namespace llvm;

namespace {
    inline uint64_t zeroMask(uint64_t w) {
        if(w < 64)
            return (((uint64_t) (int64_t) -1) << w);
        else
            return 0;
    }

    cl::opt<bool>
    DebugSimplifier("debug-expr-simplifier",
                cl::init(false));

    cl::opt<bool>
    PrintSimplifier("print-expr-simplifier",
                cl::init(false));
}

ref<Expr> BitfieldSimplifier::replaceWithConstant(ref<Expr> e, uint64_t value)
{
    ConstantExpr *ce = dyn_cast<ConstantExpr>(e);
    if(ce && ce->getZExtValue() == value)
        return e;

    // Remove kids from cache
    unsigned numKids = e->getNumKids();
    for(unsigned i = 0; i < numKids; ++i)
        m_bitsInfoCache.erase(e->getKid(i));

    // Remove e from cache
    m_bitsInfoCache.erase(e);

    return ConstantExpr::create(value & ~zeroMask(e->getWidth()), e->getWidth());
}

BitfieldSimplifier::ExprBitsInfo BitfieldSimplifier::doSimplifyBits(
                                    ref<Expr> e, uint64_t ignoredBits)
{
    ExprHashMap<BitsInfo>::iterator it = m_bitsInfoCache.find(e);
    if(it != m_bitsInfoCache.end()) {
        /* This expression was already visited before */
        if((ignoredBits & ~it->second.ignoredBits) == 0) {
            /* ignoredBits is not more restrictive then before,
               there is no point in reoptimizing the expression */
            return *it;
        }
    }

    ref<Expr> kids[8];
    BitsInfo bits[8];
    uint64_t oldIgnoredBits[8];

    BitsInfo rbits;
    rbits.ignoredBits = ignoredBits;

    /* Call doSimplifyBits recursively to obtain knownBits for each kid */
    unsigned numKids = e->getNumKids();
    for(unsigned i = 0; i < numKids; ++i) {
        /* By setting ignoredBits to zero we disable any ignoredBits-related
           optimization. Only optimizations based on knownBits will be done */
        ExprBitsInfo r = doSimplifyBits(e->getKid(i), 0);
        kids[i] = r.first;
        bits[i] = r.second;

        /* Save current value of ignoredBits. If we find more bits that are
           ignored we rerun doSimplifyBits for this kid */
        oldIgnoredBits[i] = bits[i].ignoredBits;
    }

    if (DebugSimplifier)
        *klee_message_stream << "Considering " << e << '\n';

    /* Apply kind-specific knowledge to obtain knownBits for e and
       ignoredBits for kids of e, then to optimize e */
    switch(e->getKind()) {
    // TODO: Concat, Read, AShr


    case Expr::And:
        rbits.knownOneBits = bits[0].knownOneBits & bits[1].knownOneBits;
        rbits.knownZeroBits = bits[0].knownZeroBits | bits[1].knownZeroBits;

        bits[0].ignoredBits = ignoredBits | bits[1].knownZeroBits;
        bits[1].ignoredBits = ignoredBits |
                        (bits[0].knownZeroBits & ~bits[1].knownZeroBits);

        /* Check if we can replace some kids by 1 */
        for(unsigned i = 0; i < 2; ++i) {
            if(~(bits[i].knownOneBits | bits[i].ignoredBits) == 0) {
                /* All bits of this kid is either one or ignored */
                bits[i].knownOneBits = (uint64_t) -1;
                bits[i].knownZeroBits = 0;
            }
        }

        break;

    case Expr::Or:
        rbits.knownOneBits = bits[0].knownOneBits | bits[1].knownOneBits;
        rbits.knownZeroBits = bits[0].knownZeroBits & bits[1].knownZeroBits;

        bits[0].ignoredBits = ignoredBits | bits[1].knownOneBits;
        bits[1].ignoredBits = ignoredBits |
                        (bits[0].knownOneBits & ~bits[1].knownOneBits);

        /* Check if we can replace some kids by 0 */
        for(unsigned i = 0; i < 2; ++i) {
            if(~(bits[i].knownZeroBits | bits[i].ignoredBits) == 0) {
                /* All bits of this kid is either zero or ignored */
                bits[i].knownOneBits = 0;
                bits[i].knownZeroBits = (uint64_t) -1;
            }
        }

        break;

    case Expr::Xor:
        rbits.knownOneBits = (bits[0].knownZeroBits & bits[1].knownOneBits) |
                             (bits[0].knownOneBits & bits[1].knownZeroBits);
        rbits.knownZeroBits = (bits[0].knownOneBits & bits[1].knownOneBits) |
                              (bits[0].knownZeroBits & bits[1].knownZeroBits);

        bits[0].ignoredBits = ignoredBits;
        bits[1].ignoredBits = ignoredBits;

        break;

    case Expr::Not:
        rbits.knownOneBits = bits[0].knownZeroBits & ~zeroMask(e->getWidth());
        rbits.knownZeroBits = bits[0].knownOneBits | zeroMask(e->getWidth());

        bits[0].ignoredBits = ignoredBits;

        break;

    case Expr::Shl:
        if(ConstantExpr *c1 = dyn_cast<ConstantExpr>(kids[1])) {
            // We can simplify only if the shift is known
            uint64_t shift = c1->getZExtValue();
            uint64_t width = e->getWidth();
            assert(width == kids[0]->getWidth());

            if(shift < width) {
                rbits.knownOneBits = (bits[0].knownOneBits << shift)
                                     & ~zeroMask(width);
                rbits.knownZeroBits = (bits[0].knownZeroBits << shift)
                                      | zeroMask(width)
                                      | ~zeroMask(shift);

                bits[0].ignoredBits =
                        ((ignoredBits & ~zeroMask(width)) >> shift)
                        | zeroMask(e->getWidth() - shift);
            } else {
                rbits.knownOneBits = 0;
                rbits.knownZeroBits = (uint64_t) -1;
                bits[0].ignoredBits = (uint64_t) -1;
            }
        } else {
            // This is the most general assumption
            rbits.knownOneBits = 0;
            rbits.knownZeroBits = zeroMask(e->getWidth());
        }
        break;



    case Expr::LShr:
        if(ConstantExpr *c1 = dyn_cast<ConstantExpr>(kids[1])) {
            // We can simplify only if the shift is known
            uint64_t shift = c1->getZExtValue();
            uint64_t width = e->getWidth();
            assert(width == kids[0]->getWidth());

            if(shift < width) {
                rbits.knownOneBits = bits[0].knownOneBits >> shift;
                rbits.knownZeroBits = (bits[0].knownZeroBits >> shift)
                                      | zeroMask(width - shift);

                bits[0].ignoredBits = (ignoredBits << shift)
                                      | ~zeroMask(shift)
                                      | zeroMask(width);
            } else {
                rbits.knownOneBits = 0;
                rbits.knownZeroBits = (uint64_t) -1;
                bits[0].ignoredBits = (uint64_t) -1;
            }
        } else {
            // This is the most general assumption
            rbits.knownOneBits = 0;
            rbits.knownZeroBits = zeroMask(e->getWidth());
        }
        break;


    case Expr::Extract:
        {
            ExtractExpr* ee = cast<ExtractExpr>(e);

            // Calculate mask - bits that are kept by Extract
            uint64_t mask = zeroMask(ee->width);
            rbits.knownOneBits = (bits[0].knownOneBits >> ee->offset) & ~mask;
            rbits.knownZeroBits = (bits[0].knownZeroBits >> ee->offset) | mask;

            bits[0].ignoredBits = (ignoredBits << ee->offset) |
                                  ~((~mask) << ee->offset);
        }
        break;

    case Expr::Concat:
        {
            uint64_t shift = kids[1]->getWidth();
            rbits.knownOneBits = (bits[0].knownOneBits << shift) |
                                 bits[1].knownOneBits;
            rbits.knownZeroBits = (bits[0].knownZeroBits << shift)
                                  | (bits[1].knownZeroBits &
                                     ~zeroMask(kids[1]->getWidth()));

            bits[0].ignoredBits = (ignoredBits >> shift)
                                  | zeroMask(kids[0]->getWidth());
            bits[1].ignoredBits = ignoredBits
                                  | zeroMask(kids[1]->getWidth());
        }
        break;

    case Expr::Select:
        rbits.knownOneBits = bits[1].knownOneBits & bits[2].knownOneBits;
        rbits.knownZeroBits = (bits[1].knownZeroBits & bits[2].knownZeroBits)
                               | zeroMask(e->getWidth());

        bits[1].ignoredBits = ignoredBits;
        bits[2].ignoredBits = ignoredBits;
        break;

    case Expr::ZExt:
        rbits.knownOneBits = bits[0].knownOneBits;
        // zeroMask of e is less restrictive
        rbits.knownZeroBits = bits[0].knownZeroBits;

        bits[0].ignoredBits = ignoredBits;

        break;

    case Expr::SExt:
        {
            // Mask of bits determined by the sign
            uint64_t mask = zeroMask(kids[0]->getWidth())
                            & ~zeroMask(e->getWidth());

            rbits.knownOneBits = bits[0].knownOneBits;
            rbits.knownZeroBits = bits[0].knownZeroBits & ~mask;

            if(bits[0].knownOneBits & (1UL<<(kids[0]->getWidth()-1))) {
                // kid[0] is negative
                rbits.knownOneBits = bits[0].knownOneBits | mask;
            } else if(bits[0].knownZeroBits & (1UL<<(kids[0]->getWidth()-1))) {
                // kid[0] is positive
                rbits.knownZeroBits = bits[0].knownZeroBits | mask;
            }

            bits[0].ignoredBits = ignoredBits;
            if(mask & ~ignoredBits) {
                /* Some of sign-dependend bits are not ignored */
                bits[0].ignoredBits &= ~(1UL<<(kids[0]->getWidth()-1));
            }
        }
        break;

    case Expr::Constant:
        rbits.knownOneBits = cast<ConstantExpr>(e)->getZExtValue();
        rbits.knownZeroBits = ~rbits.knownOneBits;
        break;

    default:
        // This is the most general assumption
        rbits.knownOneBits = 0;
        rbits.knownZeroBits = zeroMask(e->getWidth());
        break;
    }

    if ((rbits.knownOneBits&rbits.knownZeroBits)!=0)
    {
        return std::make_pair(e, rbits);
    }

    assert((rbits.knownOneBits & rbits.knownZeroBits) == 0);
    assert((rbits.knownOneBits & zeroMask(e->getWidth())) == 0);
    assert((rbits.knownZeroBits & zeroMask(e->getWidth())) ==
                zeroMask(e->getWidth()));

    if(!isa<ConstantExpr>(e) &&
          (~(rbits.knownOneBits | rbits.knownZeroBits | ignoredBits)) == 0) {

        if (DebugSimplifier) {
            *klee_message_stream << "CS Replacing " << e << " with constant " << hexval(rbits.knownOneBits) << '\n';
        }

        e = replaceWithConstant(e, rbits.knownOneBits);
    } else {
        // Check wether we want to reoptimize or replace kids
        for(unsigned i = 0; i < e->getNumKids(); ++i) {
            if((~(bits[i].knownOneBits | bits[i].knownZeroBits
                  | bits[i].ignoredBits)) == 0) {
                // All bits are known or ignored, replace expression by const
                // NOTE: we do it here on order to take into account
                //       kind-specific adjustements to knownBits
                if (DebugSimplifier) {
                    *klee_message_stream << "NC Replacing " << kids[i] << " with constant 0x" << hexval(bits[i].knownOneBits) << '\n';
                }

                kids[i] = replaceWithConstant(kids[i], bits[i].knownOneBits);

            } else if(bits[i].ignoredBits & ~oldIgnoredBits[i]) {
                /* We have new information about ignoredBits */
                kids[i] = doSimplifyBits(kids[i], bits[i].ignoredBits).first;
            }
        }

        // Check wheter any kid was changed
        for(unsigned i = 0; i < e->getNumKids(); ++i) {
            if(kids[i] != e->getKid(i)) {
                // Kid was changed, we must rebuild the expression
                e = e->rebuild(kids);
                break;
            }
        }
    }

    /* Cache knownBits information, but only for complex expressions */
    if(e->getNumKids() > 1)
        m_bitsInfoCache.insert(std::make_pair(e, rbits));

    return std::make_pair(e, rbits);
}

ref<Expr> BitfieldSimplifier::simplify(ref<Expr> e, uint64_t *knownZeroBits)
{
    bool cste = isa<ConstantExpr>(e);
    if (PrintSimplifier && !cste && klee_message_stream)
        *klee_message_stream << "BEFORE SIMPL: " << e << '\n';

    if (cste) {
        return e;
    }

    ExprBitsInfo ret = doSimplifyBits(e, 0);

    if (PrintSimplifier && !cste && klee_message_stream)
        *klee_message_stream << "AFTER  SIMPL: " << ret.first << '\n';

    if (knownZeroBits) {
        *knownZeroBits = ret.second.knownZeroBits;
    }

    return ret.first;
}
