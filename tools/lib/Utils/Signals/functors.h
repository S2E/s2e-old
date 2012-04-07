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
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

template <class T, typename RET, TYPENAMES>
class FUNCTOR_NAME : public functor_base <RET, BASE_CLASS_INST>
{
public:
    typedef RET (T::*func_t)(FUNCT_DECL);
protected:
    func_t m_func;
    T* m_obj;

public:
    FUNCTOR_NAME(T* obj, func_t f) {
        m_obj = obj;
        m_func = f;
    };

    virtual ~FUNCTOR_NAME() {}

    virtual RET operator()(OPERATOR_PARAM_DECL) {
        FASSERT(this->m_refcount > 0);
        return (*m_obj.*m_func)(CALL_PARAMS);
    };
};

template <class T, typename RET, TYPENAMES>
inline functor_base<RET, BASE_CLASS_INST>*
mem_fun(T &obj, RET (T::*f)(FUNCT_DECL)) {
    return new FUNCTOR_NAME<T, RET, FUNCT_DECL>(&obj, f);
}


/*** Stateless functors ***/
#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)


template <typename RET, TYPENAMES>
class glue(FUNCTOR_NAME, _sl) : public functor_base <RET, BASE_CLASS_INST>
{
public:
    typedef RET (*func_t)(FUNCT_DECL);
protected:
    func_t m_func;

public:
    glue(FUNCTOR_NAME, _sl)(func_t f) {
        m_func = f;
    };

    virtual ~glue(FUNCTOR_NAME, _sl)() {}

    virtual RET operator()(OPERATOR_PARAM_DECL) {
        FASSERT(this->m_refcount > 0);
        return (*m_func)(CALL_PARAMS);
    };
};

template <typename RET, TYPENAMES>
inline functor_base<RET, BASE_CLASS_INST>*
ptr_fun(RET (*f)(FUNCT_DECL)) {
    return new glue(FUNCTOR_NAME, _sl)<RET, FUNCT_DECL>(f);
}

#undef glue
#undef xglue
