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
#include <llvm/Analysis/Verifier.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/IRBuilder.h>

#include <iostream>
#include <sstream>

using namespace llvm;

struct TCGLLVMContext {
    TCGContext* tcgContext;
    LLVMContext context;
    IRBuilder<> builder;

    /* Shortcuts */
    const Type *i32Ty, *i32PtrTy;
    const Type *i64Ty, *i64PtrTy;
    const Type *wordTy, *wordPtrTy;

    /* Current module */
    Module *module;
    ModuleProvider *moduleProvider;

    /* Function for current translation block */
    Function *tbFunction;

    /* Current temp values */
    Value* values[TCG_MAX_TEMPS];

    /* Pointers to in-memory versions of globals */
    Value* globalsPtr[TCG_MAX_TEMPS];

    /* For reg-based globals, store argument number,
     * for mem-based globals, store base value index */
    int globalsIdx[TCG_MAX_TEMPS];

    /* Count of generated translation blocks */
    int tbCount;

    /* Function pass manager (used for optimizing the code) */
    FunctionPassManager *functionPassManager;

    /* JIT engine */
    ExecutionEngine *executionEngine;

    TCGLLVMContext(TCGContext* _tcgContext):
        tcgContext(_tcgContext),
        context(),
        builder(context),
        i32Ty   (Type::getInt32Ty(context)),
        i32PtrTy(PointerType::get(i32Ty, 0)),
        i64Ty   (Type::getInt64Ty(context)),
        i64PtrTy(PointerType::get(i64Ty, 0)),
        wordTy     (TCG_TARGET_REG_BITS == 64 ? i64Ty : i32Ty),
        wordPtrTy  (TCG_TARGET_REG_BITS == 64 ? i64PtrTy : i32PtrTy),
        tbFunction(NULL),
        tbCount(0)
    {
        std::memset(values, 0, sizeof(values));
        std::memset(globalsPtr, 0, sizeof(globalsPtr));
        std::memset(globalsIdx, 0, sizeof(globalsIdx));

        InitializeNativeTarget();

        module = new Module("tcg-llvm", context);
        executionEngine = EngineBuilder(module).create();
        assert(executionEngine != NULL);

        moduleProvider = new ExistingModuleProvider(module);
        functionPassManager = new FunctionPassManager(moduleProvider);
        functionPassManager->add(
                new TargetData(*executionEngine->getTargetData()));
        /*
        functionPassManager->add(createInstructionCombiningPass());
        functionPassManager->add(createReassociatePass());
        functionPassManager->add(createGVNPass());
        functionPassManager->add(createCFGSimplificationPass());
        */

	functionPassManager->add(createReassociatePass());
	functionPassManager->add(createConstantPropagationPass());
	functionPassManager->add(createInstructionCombiningPass());
	functionPassManager->add(createGVNPass());
	functionPassManager->add(createDeadStoreEliminationPass());
	functionPassManager->add(createCFGSimplificationPass());
	functionPassManager->add(createPromoteMemoryToRegisterPass());

        functionPassManager->doInitialization();
    }

    ~TCGLLVMContext()
    {
        delete functionPassManager;
        delete moduleProvider;

        // the following line will also delete module and all its functions
        delete executionEngine;
    }
};

struct TCGLLVMTranslationBlock {
    TCGLLVMContext *tcgLLVMContext;
    Function *tbFunction;

    TCGLLVMTranslationBlock(TCGLLVMContext *_tcgLLVMContext,
                                Function *_tbFunction)
        : tcgLLVMContext(_tcgLLVMContext), tbFunction(_tbFunction)
    {
    }

    ~TCGLLVMTranslationBlock()
    {
        tbFunction->eraseFromParent();
    }
};

TCGLLVMContext* tcg_llvm_context_new(TCGContext *s)
{
    return new TCGLLVMContext(s);
}

void tcg_llvm_context_free(TCGLLVMContext *l)
{
    delete l;
}

namespace {

inline const Type* get_type(TCGLLVMContext *l, int type)
{
    return type == TCG_TYPE_I64 ? l->i64Ty : l->i32Ty;
}

inline const Type* get_ptr_type(TCGLLVMContext *l, int type)
{
    return type == TCG_TYPE_I64 ? l->i64PtrTy : l->i32PtrTy;
}

inline Value* get_val(TCGLLVMContext *l, int idx);
inline void set_val(TCGLLVMContext *l, int idx, Value *v);

inline Value* get_ptr_for_global(TCGLLVMContext *l, int idx)
{
    TCGContext *s = l->tcgContext;
    TCGTemp &temp = s->temps[idx];

    assert(idx < s->nb_globals);
    
    if(l->globalsPtr[idx] == NULL) {
        if(temp.fixed_reg) {
            Value *v = l->builder.CreateConstGEP1_32(
                    l->tbFunction->arg_begin(), l->globalsIdx[idx]);
            l->globalsPtr[idx] = l->builder.CreatePointerCast(
                    v, get_ptr_type(l, temp.type),
                    StringRef(temp.name) + "_ptr");

        } else {
            Value *v = get_val(l, l->globalsIdx[idx]);
            assert(v->getType() == l->wordTy);

            v = l->builder.CreateAdd(v, ConstantInt::get(
                            l->wordTy, temp.mem_offset));
            l->globalsPtr[idx] =
                l->builder.CreateIntToPtr(v,
                        get_ptr_type(l, temp.type),
                        StringRef(temp.name) + "_ptr");
        }
    }

    return l->globalsPtr[idx];
}

inline void del_val(TCGLLVMContext *l, int idx)
{
    if(l->values[idx] && l->values[idx]->use_empty())
        delete l->values[idx];
    l->values[idx] = NULL;
}

inline void del_ptr_for_global(TCGLLVMContext *l, int idx)
{
    assert(idx < l->tcgContext->nb_globals);

    if(l->globalsPtr[idx] && l->globalsPtr[idx]->use_empty())
        delete l->globalsPtr[idx];
    l->globalsPtr[idx] = NULL;
}

inline Value* get_val(TCGLLVMContext *l, int idx)
{
    if(l->values[idx] == NULL) {
        if(idx < l->tcgContext->nb_globals) {
            l->values[idx] = l->builder.CreateLoad(
                    get_ptr_for_global(l, idx),
                    l->tcgContext->temps[idx].name);
        } else {
            // Temp value was not previousely assigned
            assert(false); // XXX: or return zero constant ?
        }
    }

    return l->values[idx];
}

inline void set_val(TCGLLVMContext *l, int idx, Value *v)
{
    del_val(l, idx);
    l->values[idx] = v;

    if(!v->hasName()) {
        if(idx < l->tcgContext->nb_globals)
            v->setName(l->tcgContext->temps[idx].name);
    }

    if(idx < l->tcgContext->nb_globals) {
        // We need to save a global copy of a value
        l->builder.CreateStore(v, get_ptr_for_global(l, idx));

        if(l->tcgContext->temps[idx].fixed_reg) {
            /* Invalidate all dependent global vals and pointers */
            for(int i=0; i<l->tcgContext->nb_globals; ++i) {
                if(i != idx && !l->tcgContext->temps[idx].fixed_reg &&
                                    l->globalsIdx[i] == idx) {
                    del_val(l, i);
                    del_ptr_for_global(l, i);
                }
            }
        }
    }
}

inline int tcg_llvm_out_op(TCGLLVMContext *l, int opc, const TCGArg *args)
{
    TCGOpDef &def = tcg_op_defs[opc];
    int nb_args = def.nb_args;

    switch(opc) {
    case INDEX_op_debug_insn_start:
    case INDEX_op_nop:
    case INDEX_op_nop1:
    case INDEX_op_nop2:
    case INDEX_op_nop3:
        break;

    case INDEX_op_nopn:
        nb_args += args[0];
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
                    Value *v = get_val(l, arg);
                    argValues.push_back(v);
                    argTypes.push_back(v->getType());
                }
            }

            assert(nb_oargs == 0 || nb_oargs == 1);
            const Type* retType = nb_oargs == 0 ?
                Type::getVoidTy(l->context) : l->wordTy; // XXX?

            Value* funcAddr = get_val(l, args[nb_oargs + nb_iargs]);
            funcAddr = l->builder.CreateIntToPtr(funcAddr, 
                    PointerType::get(
                        FunctionType::get(retType, argTypes, false), 0));

            Value* result = l->builder.CreateCall(funcAddr,
                                argValues.begin(), argValues.end());

            /* Invalidate all globals since call might have changed them */
            for(int i=0; i<l->tcgContext->nb_globals; ++i) {
                del_val(l, i);
                if(!l->tcgContext->temps[i].fixed_reg)
                    del_ptr_for_global(l, i);
            }

            if(nb_oargs == 1)
                set_val(l, args[1], result);
        }
        break;

    case INDEX_op_exit_tb:
        l->builder.CreateRet(ConstantInt::get(l->wordTy, args[0]));
        break;

    case INDEX_op_movi_i32:
        set_val(l, args[0], ConstantInt::get(l->i32Ty, args[1]));
        break;

    case INDEX_op_movi_i64:
        set_val(l, args[0], ConstantInt::get(l->i64Ty, args[1]));
        break;

    case INDEX_op_mov_i32:
        assert(get_val(l, args[1])->getType() == l->i32Ty);
        set_val(l, args[0], get_val(l, args[1]));
        break;

    case INDEX_op_mov_i64:
        assert(get_val(l, args[1])->getType() == l->i64Ty);
        set_val(l, args[0], get_val(l, args[1]));
        break;

    case INDEX_op_st_i64:
        assert(get_val(l, args[0])->getType() == l->i64Ty);
        assert(get_val(l, args[1])->getType() == l->wordTy);

        {
            Value *addr = get_val(l, args[1]);
            addr = l->builder.CreateAdd(addr,
                        ConstantInt::get(l->wordTy, args[2]));
            addr = l->builder.CreateIntToPtr(addr, l->i64PtrTy);
            l->builder.CreateStore(get_val(l, args[0]), addr);
        }
        break;

    default:
        std::cerr << "ERROR: unknown TCG micro operation '"
                  << def.name << "'" << std::endl;
        tcg_abort();
        break;
    }

    return nb_args;
}

inline void tcg_llvm_init_globals(TCGLLVMContext *l)
{
    TCGContext *s = l->tcgContext;

    int reg_to_idx[TCG_TARGET_NB_REGS];
    for(int i=0; i<TCG_TARGET_NB_REGS; ++i)
        reg_to_idx[i] = -1;

    int argNumber = 0;
    for(int i=0; i<s->nb_globals; ++i) {
        if(s->temps[i].fixed_reg) {
            // This global is in fixed host register. We are
            // mapping such registers to function arguments
            l->globalsIdx[i] = argNumber++;
            reg_to_idx[s->temps[i].reg] = i;

        } else {
            // This global is in memory at (mem_reg + mem_offset).
            // Base value is not known yet, so just store mem_reg
            l->globalsIdx[i] = s->temps[i].mem_reg;
        }
    }

    // Map mem_reg to index for memory-based globals
    for(int i=0; i<s->nb_globals; ++i) {
        if(!s->temps[i].fixed_reg) {
            assert(reg_to_idx[l->globalsIdx[i]] >= 0);
            l->globalsIdx[i] = reg_to_idx[l->globalsIdx[i]];
        }
    }
}

} // namespace

TCGLLVMTranslationBlock* tcg_llvm_gen_code(TCGLLVMContext *l)
{
    /* Prepare globals and temps information */
    std::memset(l->values, 0, sizeof(l->values));
    std::memset(l->globalsPtr, 0, sizeof(l->globalsPtr));
    tcg_llvm_init_globals(l);

    /* Create new function for current translation block */
    std::ostringstream fName;
    fName << "tcg-llvm-tb-" << (l->tbCount++);

    /*
    if(l->tbFunction)
        l->tbFunction->eraseFromParent();
    */

    FunctionType *tbFunctionType = FunctionType::get(
            l->wordTy, std::vector<const Type*>(1, l->i64PtrTy), false);
    l->tbFunction = Function::Create(tbFunctionType,
            Function::PrivateLinkage, fName.str(), l->module);
    BasicBlock *basicBlock = BasicBlock::Create(l->context,
            "entry", l->tbFunction);
    l->builder.SetInsertPoint(basicBlock);


    /* Generate code for each opc */
    const TCGArg *args = gen_opparam_buf;
    for(int op_index=0; ;++op_index) {
        int opc = gen_opc_buf[op_index];

        if(opc == INDEX_op_end)
            break;

        args += tcg_llvm_out_op(l, opc, args);
    }

    /* Finalize function */
    if(!isa<ReturnInst>(l->tbFunction->back().back()))
        l->builder.CreateRet(ConstantInt::get(l->wordTy, 0));

    /* Clean up unused values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i) {
        del_val(l, i);
        if(i < l->tcgContext->nb_globals)
            del_ptr_for_global(l, i);
    }

#ifndef NDEBUG
    verifyFunction(*l->tbFunction);
#endif

    l->functionPassManager->run(*l->tbFunction);

    std::cout << *l->tbFunction << std::endl;

    return new TCGLLVMTranslationBlock(l, l->tbFunction);
}

void tcg_llvm_tb_free(TCGLLVMTranslationBlock *tb)
{
    delete tb;
}

uintptr_t tcg_llvm_qemu_tb_exec(TCGLLVMTranslationBlock *tb, void* volatile *args)
{
    TCGLLVMContext *l = tb->tcgLLVMContext;
    uintptr_t (*fPtr)(void* volatile*) = (uintptr_t (*)(void* volatile*))
        l->executionEngine->getPointerToFunction(tb->tbFunction);
    
    qemu_log("OUT(LLVM):\n");
    log_disas((void*)fPtr, 0x100);
    qemu_log("\n");
    qemu_log_flush();

    std::cerr << "Executing JITed code..." << std::endl;
    return fPtr(args);
}

