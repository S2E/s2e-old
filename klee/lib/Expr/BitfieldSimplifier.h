#ifndef KLEE_BITFIELDSIMPLIFIER_H
#define KLEE_BITFIELDSIMPLIFIER_H

#include "klee/Expr.h"

namespace klee {

namespace BitfieldSimplifier {
    // This function simplified the expression according to BitField theory.
    // It should be called when creating any expression that can ignore some
    // bits of its argument
    ref<Expr> simplifyBits(ref<Expr> e);
}
}

#endif // BITFIELDSIMPLIFIER_H
