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

unsigned m_activeSignals;
unsigned m_size;
private:
func_t *m_funcs;

public:

SIGNAL_CLASS() { m_size = 0; m_funcs = 0; m_activeSignals = 0;}

SIGNAL_CLASS(const SIGNAL_CLASS &one) {
    m_activeSignals = one.m_activeSignals;
    m_size = one.m_size;
    m_funcs = new func_t[m_size];
    for (unsigned i=0; i<m_size; ++i) {
        m_funcs[i] = one.m_funcs[i];
        m_funcs[i]->incref();
    }
}

virtual ~SIGNAL_CLASS() {
    disconnectAll();
    if (m_funcs) {
        delete [] m_funcs;
    }
}

void disconnectAll()
{
    for (unsigned i=0; i<m_size; ++i) {
        if (m_funcs[i] && !m_funcs[i]->decref()) {
            delete m_funcs[i];
        }
        m_funcs[i] = NULL;
    }
}

virtual void disconnect(void *functor, unsigned index) {
    assert(m_activeSignals > 0);
    assert(m_size > index);

    if (m_funcs[index] == functor) {
        if (!m_funcs[index]->decref()) {
            delete m_funcs[index];
        }
        --m_activeSignals;
        m_funcs[index] = NULL;
    }
}

connection connect(func_t fcn) {
    fcn->incref();
    ++m_activeSignals;
    for (unsigned i=0; i<m_size; ++i) {
        if (!m_funcs[i]) {
            m_funcs[i] = fcn;
            return connection(this, fcn, i);
        }
    }
    ++m_size;
    func_t *nf = new func_t[m_size];

    if (m_funcs) {
        memcpy(nf, m_funcs, sizeof(func_t)*(m_size-1));
        delete [] m_funcs;
    }
    m_funcs = nf;

    m_funcs[m_size-1] = fcn;
    return connection(this, fcn, m_size-1);
}

bool empty() const{
    return m_activeSignals == 0;
}

void emit(OPERATOR_PARAM_DECL) {
    for (unsigned i=0; i<m_size; ++i) {
        if (m_funcs[i]) {
            m_funcs[i]->operator ()(CALL_PARAMS);
        }
    }
}


#undef SIGNAL_CLASS
#undef FUNCTOR_NAME
#undef TYPENAMES
#undef BASE_CLASS_INST
#undef FUNCT_DECL
#undef OPERATOR_PARAM_DECL
#undef CALL_PARAMS
