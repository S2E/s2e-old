#include "BitfieldSimplifier.h"

#include <map>

namespace {
    inline uint64_t zeroMask(uint64_t w) {
        if(w < 64)
            return (((uint64_t) (int64_t) -1) << w);
        else
            return 0;
    }
}

namespace klee {
namespace BitfieldSimplifier {

/* This function serves two purposses:
   - calculate which bits of e is known to be one or zero
   - simplify expression taking by ignoring all bits from ignoredBits

   Note: value of ignoredBits does not affect known[One|Zero]Bits results
*/
ref<Expr> doSimplifyBits(ref<Expr> e, uint64_t ignoredBits,
                         uint64_t* knownOneBits, uint64_t *knownZeroBits)
{
    assert(e->getNumKids() <= 8);

    ref<Expr> kids[8];

    uint64_t oneBits0 = 0, zeroBits0 = 0;
    uint64_t oneBits1 = 0, zeroBits1 = 0;
    uint64_t oneBits2 = 0, zeroBits2 = 0;

    switch(e->getKind()) {
    // TODO: Concat, Extract, Read, ZExt, SExt, And, Or, Xor, Not, Shifts

    case Expr::And:
        kids[1] = doSimplifyBits(e->getKid(1), ignoredBits, &oneBits1, &zeroBits1);
        kids[0] = doSimplifyBits(e->getKid(0), ignoredBits | zeroBits1, &oneBits0, &zeroBits0);

        // Simplify again to take into account zeroBits0
        kids[1] = doSimplifyBits(kids[1], ignoredBits | zeroBits0 | zeroBits1,
                                 &oneBits1, &zeroBits1);

        *knownOneBits = oneBits0 & oneBits1;
        *knownZeroBits = zeroBits0 | zeroBits1;
        break;

    case Expr::Or:
        kids[1] = doSimplifyBits(e->getKid(1), ignoredBits, &oneBits1, &zeroBits1);
        kids[0] = doSimplifyBits(e->getKid(0), ignoredBits | oneBits1, &oneBits0, &zeroBits0);

        // Simplify again to take into account oneBits0
        kids[1] = doSimplifyBits(kids[1], ignoredBits | oneBits0 | oneBits1,
                                 &oneBits1, &zeroBits1);

        *knownOneBits = oneBits0 | oneBits1;
        *knownZeroBits = zeroBits0 & zeroBits1;
        break;

    case Expr::Shl:
        if(isa<ConstantExpr>(e->getKid(1))) {
            // We can simplify only is the shift is known
            uint64_t shift = cast<ConstantExpr>(e->getKid(1))->getZExtValue();
            kids[0] = doSimplifyBits(e->getKid(0),
                                (ignoredBits >> shift) | zeroMask(64-shift),
                                &oneBits0, &zeroBits0);

            *knownOneBits = (oneBits0 << shift) & ~zeroMask(e->getWidth());
            *knownZeroBits = (zeroBits0 << shift) | zeroMask(e->getWidth())
                                | ~zeroMask(shift);
        } else {
            // This is the most general assumption
            *knownOneBits = 0;
            *knownZeroBits = zeroMask(e->getWidth());
        }
        break;

    case Expr::Extract:
        {
            ExtractExpr* ee = cast<ExtractExpr>(e);
            uint64_t mask = (~zeroMask(ee->width)) << ee->offset;
            kids[0] = doSimplifyBits(e->getKid(0), ignoredBits | ~mask,
                                     &oneBits0, &zeroBits0);
            *knownOneBits = oneBits0 & mask;
            *knownZeroBits = zeroBits0 | ~mask;
        }
        break;

    // Note: the following cases do not add new information to the ignoredBits
    // of children expressions

    case Expr::Select:
        kids[1] = doSimplifyBits(e->getKid(1), ignoredBits, &oneBits1, &zeroBits1);
        kids[2] = doSimplifyBits(e->getKid(2), ignoredBits, &oneBits2, &zeroBits2);
        *knownOneBits = oneBits1 & oneBits2;
        *knownZeroBits = zeroBits1 & zeroBits2 & zeroMask(e->getWidth());

        break;

    case Expr::ZExt:
        kids[0] = doSimplifyBits(e->getKid(0), ignoredBits, &oneBits0, &zeroBits0);
        *knownOneBits = oneBits0;
        *knownZeroBits = zeroBits0; // zeroMask of e is less restrictive
        break;

    case Expr::SExt:
        {
            kids[0] = doSimplifyBits(e->getKid(0), ignoredBits, &oneBits0, &zeroBits0);

            // Mask of bits determined by the sign
            uint64_t bits = zeroMask(kids[0]->getWidth()) & ~zeroMask(e->getWidth());

            *knownOneBits = oneBits0;
            *knownZeroBits = zeroBits0 & ~bits;

            if(oneBits0 & (1UL<<(kids[0]->getWidth()-1))) {
                // kid[0] is negative
                *knownOneBits = oneBits0 | bits;
            } else if(zeroBits0 & (1UL<<(kids[0]->getWidth()-1))) {
                // kid[0] is positive
                *knownZeroBits = zeroBits0 | bits;
            }
        }
        break;

    case Expr::Constant:
        *knownOneBits = cast<ConstantExpr>(e)->getZExtValue();
        *knownZeroBits = ~*knownOneBits;
        break;

    default:
        // This is the most general assumption
        *knownOneBits = 0;
        *knownZeroBits = zeroMask(e->getWidth());

        // Note: we do not propagate optimizations through unknown nodes
        // because we do not know how to relate ignoredBits on the node
        // and its child. It means we can not add any new information compared
        // to what was already known on creation time
        break;
    }

    assert((*knownOneBits & *knownZeroBits) == 0);
    assert((*knownOneBits & zeroMask(e->getWidth())) == 0);
    assert((*knownZeroBits & zeroMask(e->getWidth())) == zeroMask(e->getWidth()));

    if(~(*knownOneBits | *knownZeroBits) == 0) {
        // All bits are known, expression is constant
        return ConstantExpr::create(*knownOneBits, e->getWidth());
    }

    for(int i = 0; i < e->getNumKids(); ++i) {
        if(!kids[i].isNull() && kids[i] != e->getKid(i)) {
            // Rebuild expression if any kid was changed
            return e->rebuild(kids);
        }
    }

    return e;
}

ref<Expr> simplifyBits(ref<Expr> e)
{
    uint64_t knownOneBits = 0, knownZeroBits = 0;
    //return e;
    return doSimplifyBits(e, 0, &knownOneBits, &knownZeroBits);
}

} // namespace BitfieldSimplifier
} // namespace klee
