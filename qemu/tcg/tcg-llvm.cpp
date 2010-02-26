/*
 * LLVM backend for Tiny Code Generator for QEMU
 *
 * Copyright (c) 2010 Volodymyr Kuznetsov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "tcg-llvm.h"

extern "C" {
#include "config.h"
#include "qemu-common.h"
#include "disas.h"
}

#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/ModuleProvider.h>
#include <llvm/PassManager.h>
#include <llvm/Intrinsics.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/IRBuilder.h>

#include <iostream>
#include <sstream>

using namespace llvm;

struct TCGLLVMTranslationBlock {
    TCGLLVMContext *tcgLLVMContext;
    Function *m_tbFunction;

    TCGLLVMTranslationBlock(
        TCGLLVMContext *_tcgLLVMContext, Function *_tbFunction);
    ~TCGLLVMTranslationBlock();
};

struct TCGLLVMContext {
    TCGContext* m_tcgContext;
    LLVMContext m_context;
    IRBuilder<> m_builder;

    /* Current m_module */
    Module *m_module;
    ModuleProvider *moduleProvider;

    /* Function for current translation block */
    Function *m_tbFunction;

    /* Current temp m_values */
    Value* m_values[TCG_MAX_TEMPS];

    /* Pointers to in-memory versions of globals */
    Value* m_globalsPtr[TCG_MAX_TEMPS];

    /* For reg-based globals, store argument number,
     * for mem-based globals, store base value index */
    int m_globalsIdx[TCG_MAX_TEMPS];

    /* Function pass manager (used for optimizing the code) */
    FunctionPassManager *m_functionPassManager;

    /* JIT engine */
    ExecutionEngine *m_executionEngine;

    /* Count of generated translation blocks */
    int m_tbCount;

public:
    TCGLLVMContext(TCGContext* _tcgContext);
    ~TCGLLVMContext();

    /* Shortcuts */
    const Type* intType(int w) { return IntegerType::get(m_context, w); }
    const Type* intPtrType(int w) { return PointerType::get(intType(w), 0); }
    const Type* wordType() { return intType(TCG_TARGET_REG_BITS); }
    const Type* wordPtrType() { return intPtrType(TCG_TARGET_REG_BITS); }

    const Type* tcgType(int type) {
        return type == TCG_TYPE_I64 ? intType(64) : intType(32);
    }

    const Type* tcgPtrType(int type) {
        return type == TCG_TYPE_I64 ? intPtrType(64) : intPtrType(32);
    }

    /* Helpers */
    Value* getValue(int idx);
    void setValue(int idx, Value *v);
    void delValue(int idx);

    Value* getPtrForGlobal(int idx);
    void delPtrForGlobal(int idx);

    void initGlobals();

    /* Code generation */
    int generateOperation(int opc, const TCGArg *args);
    TCGLLVMTranslationBlock* generateCode();
};

inline TCGLLVMTranslationBlock::TCGLLVMTranslationBlock(
        TCGLLVMContext *_tcgLLVMContext, Function *_tbFunction)
    : tcgLLVMContext(_tcgLLVMContext), m_tbFunction(_tbFunction)
{
}

inline TCGLLVMTranslationBlock::~TCGLLVMTranslationBlock()
{
    m_tbFunction->eraseFromParent();
}

inline TCGLLVMContext::TCGLLVMContext(TCGContext* _tcgContext)
    : m_tcgContext(_tcgContext), m_context(), m_builder(m_context),
      m_tbFunction(NULL), m_tbCount(0)
{
    std::memset(m_values, 0, sizeof(m_values));
    std::memset(m_globalsPtr, 0, sizeof(m_globalsPtr));
    std::memset(m_globalsIdx, 0, sizeof(m_globalsIdx));

    InitializeNativeTarget();

    m_module = new Module("tcg-llvm", m_context);
    m_executionEngine = EngineBuilder(m_module).create();
    assert(m_executionEngine != NULL);

    moduleProvider = new ExistingModuleProvider(m_module);
    m_functionPassManager = new FunctionPassManager(moduleProvider);
    m_functionPassManager->add(
            new TargetData(*m_executionEngine->getTargetData()));

    m_functionPassManager->add(createReassociatePass());
    m_functionPassManager->add(createConstantPropagationPass());
    m_functionPassManager->add(createInstructionCombiningPass());
    m_functionPassManager->add(createGVNPass());
    m_functionPassManager->add(createDeadStoreEliminationPass());
    m_functionPassManager->add(createCFGSimplificationPass());
    m_functionPassManager->add(createPromoteMemoryToRegisterPass());

    m_functionPassManager->doInitialization();
}

inline TCGLLVMContext::~TCGLLVMContext()
{
    delete m_functionPassManager;
    delete moduleProvider;

    // the following line will also delete
    // m_module and all its functions
    delete m_executionEngine;
}

inline Value* TCGLLVMContext::getPtrForGlobal(int idx)
{
    TCGContext *s = m_tcgContext;
    TCGTemp &temp = s->temps[idx];

    assert(idx < s->nb_globals);
    
    if(m_globalsPtr[idx] == NULL) {
        if(temp.fixed_reg) {
            Value *v = m_builder.CreateConstGEP1_32(
                    m_tbFunction->arg_begin(), m_globalsIdx[idx]);
            m_globalsPtr[idx] = m_builder.CreatePointerCast(
                    v, tcgPtrType(temp.type),
                    StringRef(temp.name) + "_ptr");

        } else {
            Value *v = getValue(m_globalsIdx[idx]);
            assert(v->getType() == wordType());

            v = m_builder.CreateAdd(v, ConstantInt::get(
                            wordType(), temp.mem_offset));
            m_globalsPtr[idx] =
                m_builder.CreateIntToPtr(v, tcgPtrType(temp.type),
                        StringRef(temp.name) + "_ptr");
        }
    }

    return m_globalsPtr[idx];
}

inline void TCGLLVMContext::delValue(int idx)
{
    if(m_values[idx] && m_values[idx]->use_empty()) {
        if(!isa<Instruction>(m_values[idx]) ||
                !cast<Instruction>(m_values[idx])->getParent())
            delete m_values[idx];
    }
    m_values[idx] = NULL;
}

inline void TCGLLVMContext::delPtrForGlobal(int idx)
{
    assert(idx < m_tcgContext->nb_globals);

    if(m_globalsPtr[idx] && m_globalsPtr[idx]->use_empty()) {
        if(!isa<Instruction>(m_globalsPtr[idx]) ||
                !cast<Instruction>(m_globalsPtr[idx])->getParent())
            delete m_globalsPtr[idx];
    }
    m_globalsPtr[idx] = NULL;
}

inline Value* TCGLLVMContext::getValue(int idx)
{
    if(m_values[idx] == NULL) {
        if(idx < m_tcgContext->nb_globals) {
            m_values[idx] = m_builder.CreateLoad(
                    getPtrForGlobal(idx),
                    m_tcgContext->temps[idx].name);
        } else {
            // Temp value was not previousely assigned
            assert(false); // XXX: or return zero constant ?
        }
    }

    return m_values[idx];
}

inline void TCGLLVMContext::setValue(int idx, Value *v)
{
    delValue(idx);
    m_values[idx] = v;

    if(!v->hasName()) {
        if(idx < m_tcgContext->nb_globals)
            v->setName(m_tcgContext->temps[idx].name);
    }

    if(idx < m_tcgContext->nb_globals) {
        // We need to save a global copy of a value
        m_builder.CreateStore(v, getPtrForGlobal(idx));

        if(m_tcgContext->temps[idx].fixed_reg) {
            /* Invalidate all dependent global vals and pointers */
            for(int i=0; i<m_tcgContext->nb_globals; ++i) {
                if(i != idx && !m_tcgContext->temps[idx].fixed_reg &&
                                    m_globalsIdx[i] == idx) {
                    delValue(i);
                    delPtrForGlobal(i);
                }
            }
        }
    }
}

inline void TCGLLVMContext::initGlobals()
{
    TCGContext *s = m_tcgContext;

    int reg_to_idx[TCG_TARGET_NB_REGS];
    for(int i=0; i<TCG_TARGET_NB_REGS; ++i)
        reg_to_idx[i] = -1;

    int argNumber = 0;
    for(int i=0; i<s->nb_globals; ++i) {
        if(s->temps[i].fixed_reg) {
            // This global is in fixed host register. We are
            // mapping such registers to function arguments
            m_globalsIdx[i] = argNumber++;
            reg_to_idx[s->temps[i].reg] = i;

        } else {
            // This global is in memory at (mem_reg + mem_offset).
            // Base value is not known yet, so just store mem_reg
            m_globalsIdx[i] = s->temps[i].mem_reg;
        }
    }

    // Map mem_reg to index for memory-based globals
    for(int i=0; i<s->nb_globals; ++i) {
        if(!s->temps[i].fixed_reg) {
            assert(reg_to_idx[m_globalsIdx[i]] >= 0);
            m_globalsIdx[i] = reg_to_idx[m_globalsIdx[i]];
        }
    }
}

inline int TCGLLVMContext::generateOperation(int opc, const TCGArg *args)
{
    Value *v;
    TCGOpDef &def = tcg_op_defs[opc];
    int nb_args = def.nb_args;

    switch(opc) {
    case INDEX_op_debug_insn_start:
        break;

    /* predefined ops */
    case INDEX_op_nop:
    case INDEX_op_nop1:
    case INDEX_op_nop2:
    case INDEX_op_nop3:
        break;

    case INDEX_op_nopn:
        nb_args = args[0];
        break;

    case INDEX_op_discard:
        delValue(args[0]);
        break;

    case INDEX_op_call:
        {
            int nb_oargs = args[0] >> 16;
            int nb_iargs = args[0] & 0xffff;
            nb_args = nb_oargs + nb_iargs + def.nb_cargs + 1;

            int flags = args[nb_oargs + nb_iargs + 1];
            assert((flags & TCG_CALL_TYPE_MASK) == TCG_CALL_TYPE_STD);

            std::vector<Value*> argValues;
            std::vector<const Type*> argTypes;
            argValues.reserve(nb_iargs-1);
            argTypes.reserve(nb_iargs-1);
            for(int i=0; i < nb_iargs-1; ++i) {
                TCGArg arg = args[nb_oargs + i + 1];
                if(arg != TCG_CALL_DUMMY_ARG) {
                    Value *v = getValue(arg);
                    argValues.push_back(v);
                    argTypes.push_back(v->getType());
                }
            }

            assert(nb_oargs == 0 || nb_oargs == 1);
            const Type* retType = nb_oargs == 0 ?
                Type::getVoidTy(m_context) : wordType(); // XXX?

            Value* funcAddr = getValue(args[nb_oargs + nb_iargs]);
            funcAddr = m_builder.CreateIntToPtr(funcAddr, 
                    PointerType::get(
                        FunctionType::get(retType, argTypes, false), 0));

            Value* result = m_builder.CreateCall(funcAddr,
                                argValues.begin(), argValues.end());

            /* Invalidate all globals since call might have changed them */
            for(int i=0; i<m_tcgContext->nb_globals; ++i) {
                delValue(i);
                if(!m_tcgContext->temps[i].fixed_reg)
                    delPtrForGlobal(i);
            }

            if(nb_oargs == 1)
                setValue(args[1], result);
        }
        break;

    case INDEX_op_movi_i32:
        setValue(args[0], ConstantInt::get(intType(32), args[1]));
        break;

    case INDEX_op_mov_i32:
        // Truncation is silently accepted
        assert(getValue(args[1])->getType() == intType(32) ||
                getValue(args[1])->getType() == intType(64));
        setValue(args[0], getValue(args[1]));
        break;

#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_movi_i64:
        setValue(args[0], ConstantInt::get(intType(64), args[1]));
        break;

    case INDEX_op_mov_i64:
        assert(getValue(args[1])->getType() == intType(64));
        setValue(args[0], getValue(args[1]));
        break;
#endif

    /* size extensions */
#define __EXT_OP(opc_name, truncBits, opBits, signE )               \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(opBits) ||   \
               getValue(args[1])->getType() == intType(truncBits)); \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                m_builder.CreateTrunc(                              \
                    getValue(args[1]), intType(truncBits)),         \
                intType(opBits)));                                  \
        break;

    __EXT_OP(INDEX_op_ext8s_i32,   8, 32, S)
    __EXT_OP(INDEX_op_ext8u_i32,   8, 32, Z)
    __EXT_OP(INDEX_op_ext16s_i32, 16, 32, S)
    __EXT_OP(INDEX_op_ext16u_i32, 16, 32, Z)

#if TCG_TARGET_REG_BITS == 64
    __EXT_OP(INDEX_op_ext8s_i64,   8, 64, S)
    __EXT_OP(INDEX_op_ext8u_i64,   8, 64, Z)
    __EXT_OP(INDEX_op_ext16s_i64, 16, 64, S)
    __EXT_OP(INDEX_op_ext16u_i64, 16, 64, Z)
    __EXT_OP(INDEX_op_ext32s_i64, 32, 64, S)
    __EXT_OP(INDEX_op_ext32u_i64, 32, 64, Z)
#endif

#undef __EXT_OP

    /* load/store */
#define __LD_OP(opc_name, srcBits, dstBits, signE)                  \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == wordType());         \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(srcBits));       \
        v = m_builder.CreateLoad(v);                                \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                    v, intPtrType(dstBits)));                       \
        break;

#define __ST_OP(opc_name, srcBits, dstBits)                         \
    case opc_name:                                                  \
        assert(getValue(args[0])->getType() == intType(srcBits));   \
        assert(getValue(args[1])->getType() == wordType());         \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(dstBits));       \
        m_builder.CreateStore(m_builder.CreateTrunc(                \
                getValue(args[0]), intType(dstBits)), v);           \
        break;

    __LD_OP(INDEX_op_ld8u_i32,   8, 32, Z)
    __LD_OP(INDEX_op_ld8s_i32,   8, 32, S)
    __LD_OP(INDEX_op_ld16u_i32, 16, 32, Z)
    __LD_OP(INDEX_op_ld16s_i32, 16, 32, S)
    __LD_OP(INDEX_op_ld_i32,    32, 32, Z)

    __ST_OP(INDEX_op_st8_i32,   8, 32)
    __ST_OP(INDEX_op_st16_i32, 16, 32)
    __ST_OP(INDEX_op_st_i32,   32, 32)

#if TCG_TARGET_REG_BITS == 64
    __LD_OP(INDEX_op_ld8u_i64,   8, 64, Z)
    __LD_OP(INDEX_op_ld8s_i64,   8, 64, S)
    __LD_OP(INDEX_op_ld16u_i64, 16, 64, Z)
    __LD_OP(INDEX_op_ld16s_i64, 16, 64, S)
    __LD_OP(INDEX_op_ld32u_i64, 32, 64, Z)
    __LD_OP(INDEX_op_ld32s_i64, 32, 64, S)
    __LD_OP(INDEX_op_ld_i64,    64, 64, Z)

    __ST_OP(INDEX_op_st8_i64,   8, 64)
    __ST_OP(INDEX_op_st16_i64, 16, 64)
    __ST_OP(INDEX_op_st32_i64, 32, 64)
    __ST_OP(INDEX_op_st_i64,   64, 64)
#endif

#undef __LD_OP
#undef __ST_OP

    /* arith */
#define __ARITH_OP(opc_name, op, bits)                              \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        assert(getValue(args[2])->getType() == intType(bits));      \
        setValue(args[0], m_builder.Create ## op(                   \
                getValue(args[1]), getValue(args[2])));             \
        break;

#define __ARITH_OP_DIV2(opc_name, signE, bits)                      \
    case opc_name:                                                  \
        assert(getValue(args[2])->getType() == intType(bits));      \
        assert(getValue(args[3])->getType() == intType(bits));      \
        assert(getValue(args[4])->getType() == intType(bits));      \
        v = m_builder.CreateShl(                                    \
                m_builder.CreateZExt(                               \
                    getValue(args[3]), intType(bits*2)),            \
                ConstantInt::get(intType(bits*2), bits));           \
        v = m_builder.CreateOr(v,                                   \
                m_builder.CreateZExt(                               \
                    getValue(args[2]), intType(bits*2)));           \
        setValue(args[0], m_builder.Create ## signE ## Div(         \
                v, getValue(args[4])));                             \
        setValue(args[1], m_builder.Create ## signE ## Rem(         \
                v, getValue(args[4])));                             \
        break;

#define __ARITH_OP_ROT(opc_name, op1, op2, bits)                    \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        assert(getValue(args[2])->getType() == intType(bits));      \
        v = m_builder.CreateSub(                                    \
                ConstantInt::get(intType(bits), bits),              \
                getValue(args[2]));                                 \
        setValue(args[0], m_builder.CreateOr(                       \
                m_builder.Create ## op1 (                           \
                    getValue(args[1]), getValue(args[2])),          \
                m_builder.Create ## op2 (                           \
                    getValue(args[1]), v)));                        \
        break;

#define __ARITH_OP_I(opc_name, op, i, bits)                         \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        setValue(args[0], m_builder.Create ## op(                   \
                    ConstantInt::get(intType(bits), i),             \
                    getValue(args[1])));                            \
        break;

#define __ARITH_OP_BSWAP(opc_name, sBits, bits)                     \
    case opc_name: {                                                \
        assert(getValue(args[1])->getType() == intType(bits));      \
        const Type* Tys[] = { intType(sBits) };                     \
        Function *bswap = Intrinsic::getDeclaration(m_module,       \
                Intrinsic::bswap, Tys, 1);                          \
        v = m_builder.CreateTrunc(getValue(args[1]),intType(sBits));\
        setValue(args[0], m_builder.CreateZExt(                     \
                m_builder.CreateCall(bswap, v), intType(bits)));    \
        } break;


    __ARITH_OP(INDEX_op_add_i32, Add, 32)
    __ARITH_OP(INDEX_op_sub_i32, Sub, 32)
    __ARITH_OP(INDEX_op_mul_i32, Mul, 32)

#ifdef TCG_TARGET_HAS_div_i32
    __ARITH_OP(INDEX_op_div_i32,  SDiv, 32)
    __ARITH_OP(INDEX_op_divu_i32, UDiv, 32)
    __ARITH_OP(INDEX_op_rem_i32,  SRem, 32)
    __ARITH_OP(INDEX_op_remu_i32, URem, 32)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i32,  S, 32)
    __ARITH_OP_DIV2(INDEX_op_divu2_i32, U, 32)
#endif

    __ARITH_OP(INDEX_op_and_i32, And, 32)
    __ARITH_OP(INDEX_op_or_i32,   Or, 32)
    __ARITH_OP(INDEX_op_xor_i32, Xor, 32)

    __ARITH_OP(INDEX_op_shl_i32,  Shl, 32)
    __ARITH_OP(INDEX_op_shr_i32, LShr, 32)
    __ARITH_OP(INDEX_op_sar_i32, AShr, 32)

    __ARITH_OP_ROT(INDEX_op_rotl_i32, Shl, LShr, 32)
    __ARITH_OP_ROT(INDEX_op_rotr_i32, LShr, Shl, 32)

    __ARITH_OP_I(INDEX_op_not_i32, Xor, (uint64_t) -1, 32)
    __ARITH_OP_I(INDEX_op_neg_i32, Sub, 0, 32)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i32, 16, 32)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i32, 32, 32)

#if TCG_TARGET_REG_BITS == 64
    __ARITH_OP(INDEX_op_add_i64, Add, 64)
    __ARITH_OP(INDEX_op_sub_i64, Sub, 64)
    __ARITH_OP(INDEX_op_mul_i64, Mul, 64)

#ifdef TCG_TARGET_HAS_div_i64
    __ARITH_OP(INDEX_op_div_i64,  SDiv, 64)
    __ARITH_OP(INDEX_op_divu_i64, UDiv, 64)
    __ARITH_OP(INDEX_op_rem_i64,  SRem, 64)
    __ARITH_OP(INDEX_op_remu_i64, URem, 64)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i64,  S, 64)
    __ARITH_OP_DIV2(INDEX_op_divu2_i64, U, 64)
#endif

    __ARITH_OP(INDEX_op_and_i64, And, 64)
    __ARITH_OP(INDEX_op_or_i64,   Or, 64)
    __ARITH_OP(INDEX_op_xor_i64, Xor, 64)

    __ARITH_OP(INDEX_op_shl_i64,  Shl, 64)
    __ARITH_OP(INDEX_op_shr_i64, LShr, 64)
    __ARITH_OP(INDEX_op_sar_i64, AShr, 64)

    __ARITH_OP_ROT(INDEX_op_rotl_i64, Shl, LShr, 64)
    __ARITH_OP_ROT(INDEX_op_rotr_i64, LShr, Shl, 64)

    __ARITH_OP_I(INDEX_op_not_i64, Xor, (uint64_t) -1, 64)
    __ARITH_OP_I(INDEX_op_neg_i64, Sub, 0, 64)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i64, 16, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i64, 32, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap64_i64, 64, 64)
#endif

#undef __ARITH_OP_BSWAP
#undef __ARITH_OP_I
#undef __ARITH_OP_ROT
#undef __ARITH_OP_DIV2
#undef __ARITH_OP

    case INDEX_op_exit_tb:
        m_builder.CreateRet(ConstantInt::get(wordType(), args[0]));
        break;

    /* QEMU specific */
#if TCG_TARGET_REG_BITS == 64
#ifndef CONFIG_SOFTMMU

#define __OP_QEMU_ST(opc_name, bits)                                \
    case opc_name:                                                  \
        v = m_builder.CreateIntCast(                                \
                getValue(args[1]), wordType(), false);              \
        v = m_builder.CreateAdd(v,                                  \
                ConstantInt::get(wordType(), GUEST_BASE));          \
        m_builder.CreateStore(                                      \
                m_builder.CreateTrunc(                              \
                    getValue(args[0]), intType(bits)),              \
                m_builder.CreateIntToPtr(v, intPtrType(bits)));     \
        break;

#define __OP_QEMU_LD(opc_name, bits, signE)                         \
    case opc_name:                                                  \
        v = m_builder.CreateIntCast(                                \
                getValue(args[1]), wordType(), false);              \
        v = m_builder.CreateAdd(v,                                  \
                ConstantInt::get(wordType(), GUEST_BASE));          \
        v = m_builder.CreateLoad(                                   \
                m_builder.CreateIntToPtr(v, intPtrType(bits)));     \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                v, intType(64)));                                   \
        break;

    __OP_QEMU_ST(INDEX_op_qemu_st8,   8)
    __OP_QEMU_ST(INDEX_op_qemu_st16, 16)
    __OP_QEMU_ST(INDEX_op_qemu_st32, 32)
    __OP_QEMU_ST(INDEX_op_qemu_st64, 64)

    __OP_QEMU_LD(INDEX_op_qemu_ld8s,   8, S)
    __OP_QEMU_LD(INDEX_op_qemu_ld8u,   8, Z)
    __OP_QEMU_LD(INDEX_op_qemu_ld16s, 16, S)
    __OP_QEMU_LD(INDEX_op_qemu_ld16u, 16, Z)
    __OP_QEMU_LD(INDEX_op_qemu_ld32s, 32, S)
    __OP_QEMU_LD(INDEX_op_qemu_ld32u, 32, Z)
    __OP_QEMU_LD(INDEX_op_qemu_ld64,  64, Z)

#undef __OP_QEMU_LD
#undef __OP_QEMU_ST

#endif
#endif
        
    default:
        std::cerr << "ERROR: unknown TCG micro operation '"
                  << def.name << "'" << std::endl;
        tcg_abort();
        break;
    }

    return nb_args;
}

inline TCGLLVMTranslationBlock* TCGLLVMContext::generateCode()
{
    /* Prepare globals and temps information */
    std::memset(m_values, 0, sizeof(m_values));
    std::memset(m_globalsPtr, 0, sizeof(m_globalsPtr));
    initGlobals();

    /* Create new function for current translation block */
    std::ostringstream fName;
    fName << "tcg-llvm-tb-" << (m_tbCount++);

    /*
    if(m_tbFunction)
        m_tbFunction->eraseFromParent();
    */

    FunctionType *tbFunctionType = FunctionType::get(
            wordType(),
            std::vector<const Type*>(1, intPtrType(64)), false);
    m_tbFunction = Function::Create(tbFunctionType,
            Function::PrivateLinkage, fName.str(), m_module);
    BasicBlock *basicBlock = BasicBlock::Create(m_context,
            "entry", m_tbFunction);
    m_builder.SetInsertPoint(basicBlock);


    /* Generate code for each opc */
    const TCGArg *args = gen_opparam_buf;
    for(int op_index=0; ;++op_index) {
        int opc = gen_opc_buf[op_index];

        if(opc == INDEX_op_end)
            break;

        args += generateOperation(opc, args);
    }

    /* Finalize function */
    if(!isa<ReturnInst>(m_tbFunction->back().back()))
        m_builder.CreateRet(ConstantInt::get(wordType(), 0));

    /* Clean up unused m_values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i) {
        delValue(i);
        if(i < m_tcgContext->nb_globals)
            delPtrForGlobal(i);
    }

#ifndef NDEBUG
    verifyFunction(*m_tbFunction);
#endif

    m_functionPassManager->run(*m_tbFunction);

    std::cout << *m_tbFunction << std::endl;

    return new TCGLLVMTranslationBlock(this, m_tbFunction);
}

TCGLLVMContext* tcg_llvm_context_new(TCGContext *s)
{
    return new TCGLLVMContext(s);
}

void tcg_llvm_context_free(TCGLLVMContext *l)
{
    delete l;
}

TCGLLVMTranslationBlock* tcg_llvm_gen_code(TCGLLVMContext *l)
{
    return l->generateCode();
}

void tcg_llvm_tb_free(TCGLLVMTranslationBlock *tb)
{
    delete tb;
}

uintptr_t tcg_llvm_qemu_tb_exec(
        TCGLLVMTranslationBlock *tb, void* volatile *args)
{
    TCGLLVMContext *l = tb->tcgLLVMContext;
    uintptr_t (*fPtr)(void* volatile*) = (uintptr_t (*)(void* volatile*))
        l->m_executionEngine->getPointerToFunction(tb->m_tbFunction);
    
    qemu_log("OUT(LLVM):\n");
    log_disas((void*)fPtr, 0x100);
    qemu_log("\n");
    qemu_log_flush();

    std::cerr << "Executing JITed code..." << std::endl;
    return fPtr(args);
}

