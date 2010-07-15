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

    /// XXX: this cache will probably grew too large with time
    ExprHashMap<BitsInfo> m_bitsInfoCache;

    ref<Expr> replaceWithConstant(ref<Expr> e, uint64_t value);

    typedef std::pair<ref<Expr>, BitsInfo> ExprBitsInfo;
    ExprBitsInfo doSimplifyBits(ref<Expr> e, uint64_t ignoredBits);

public:
    ref<Expr> simplify(ref<Expr> e);
};

} // namespace klee

#endif // BITFIELDSIMPLIFIER_H
