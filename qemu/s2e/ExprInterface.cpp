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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include <klee/Expr.h>
#include <llvm/ADT/SmallVector.h>
#include <inttypes.h>
#include "Utils.h"
#include "ExprInterface.h"
#include "S2E.h"
#include "S2EExecutor.h"
#include "S2EExecutionState.h"
#include "s2e_qemu.h"

/**
 * The expression interface allows the QEMU emulation code to manipulate
 * symbolic values as simply as possible, even in concrete mode.
 * This avoids the need of invoking the LLVM interpreter for the simple
 * common case where the symbolic values only affect the data flow
 * (i.e., no need for forking).
 *
 * Eventually, we'll need an LLVM pass that automatically instruments
 * the data flow with symbolic-aware operations in order to avoid
 * the messy manual part.
 *
 * The interface encapsulates klee expressions in an opaque object
 * that the C code can pass around. ExprManager keeps track of all these
 * objects in order to avoid memory leaks.
 */


using namespace klee;

struct ExprBox {
    bool constant;
    uint64_t value;
    klee::ref<Expr> expr;

    ExprBox() {
        constant = false;
    }
};

class ExprManager {
    llvm::SmallVector<ExprBox*, 4> expressions;

public:

    ~ExprManager() {
        foreach2(it, expressions.begin(), expressions.end()) {
            delete *it;
        }
    }

    inline void add(ExprBox *box) {
        expressions.push_back(box);
    }

    ExprBox *create() {
        ExprBox *ret = new ExprBox();
        expressions.push_back(ret);
        return ret;
    }
};

void* s2e_expr_mgr()
{
    return new ExprManager;
}

void s2e_expr_clear(void *_mgr) {
    ExprManager *mgr = static_cast<ExprManager*>(_mgr);
    delete mgr;
}

void s2e_expr_set(void *expr, uint64_t constant)
{
    ExprBox *box = static_cast<ExprBox*>(expr);
    box->value = constant;
    box->constant = true;
}

void *s2e_expr_and(void *_mgr, void *_lhs, uint64_t constant)
{
    ExprManager *mgr = static_cast<ExprManager*>(_mgr);
    ExprBox *box = static_cast<ExprBox*>(_lhs);
    ExprBox *retbox = mgr->create();

    if (box->constant) {
        retbox->value = box->value & constant;
        retbox->constant = true;
    } else {
        ConstantExpr *cste = dyn_cast<ConstantExpr>(box->expr);
        if (cste) {
            retbox->value = cste->getZExtValue() & constant;
            retbox->constant = true;
        } else {
            retbox->expr = AndExpr::create(box->expr, ConstantExpr::create(constant, box->expr->getWidth()));
            retbox->constant = false;
        }
    }
    return retbox;
}

uint64_t s2e_expr_to_constant(void *_expr)
{
    ExprBox *box = static_cast<ExprBox*>(_expr);
    if (box->constant) {
        return box->value;
    } else {
        ref<Expr> expr = g_s2e->getExecutor()->toConstant(*g_s2e_state, box->expr, "klee_expr_to_constant");
        ConstantExpr *cste = dyn_cast<ConstantExpr>(expr);
        return cste->getZExtValue();
    }
}

void s2e_expr_write_cpu(void *expr, unsigned offset, unsigned size)
{
    ExprBox *box = static_cast<ExprBox*>(expr);
    if (box->constant) {
        g_s2e_state->writeCpuRegister(offset, ConstantExpr::create(box->value, size*8));
    } else {
        unsigned exprSizeInBytes = box->expr->getWidth() / 8;
        if (exprSizeInBytes == size) {
            g_s2e_state->writeCpuRegisterSymbolic(offset, box->expr);
        } else if (exprSizeInBytes > size) {
            g_s2e_state->writeCpuRegisterSymbolic(offset, ExtractExpr::create(box->expr, 0, size * 8));
        } else {
            g_s2e_state->writeCpuRegisterSymbolic(offset, ZExtExpr::create(box->expr, size * 8));
        }
    }
}

void *s2e_expr_read_cpu(void *_mgr, unsigned offset, unsigned size)
{
    ExprManager *mgr = static_cast<ExprManager*>(_mgr);
    ExprBox *retbox = mgr->create();

    retbox->expr = g_s2e_state->readCpuRegister(offset, size * 8);
    ConstantExpr *constant = dyn_cast<ConstantExpr>(retbox->expr);
    if (constant) {
        retbox->constant = true;
        retbox->value = constant->getZExtValue();
    }

    return retbox;
}

void *s2e_expr_read_mem_l(void *_mgr, uint64_t virtual_address)
{
    ExprManager *mgr = static_cast<ExprManager*>(_mgr);
    ExprBox *retbox = mgr->create();

    //XXX: This may be slow... fast path for concrete values required.
    retbox->expr = g_s2e_state->readMemory(virtual_address, Expr::Int32);

    //XXX: What do we do if the result is NULL?
    //For now we call this function from iret-type of handlers where
    //some checks must have been done before accessing the memory
    assert(!retbox->expr.isNull() && "Failed memory access");

    ConstantExpr *constant = dyn_cast<ConstantExpr>(retbox->expr);
    if (constant) {
        retbox->constant = true;
        retbox->value = constant->getZExtValue();
    }

    return retbox;
}
