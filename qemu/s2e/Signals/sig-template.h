
unsigned m_size;
private:
func_t *m_funcs;

public:

SIGNAL_CLASS() { m_size = 0; m_funcs = 0;}

SIGNAL_CLASS(const SIGNAL_CLASS &one) {
    m_size = one.m_size;
    m_funcs = new func_t[m_size];
    for (unsigned i=0; i<m_size; ++i) {
        m_funcs[i] = one.m_funcs[i];
        one.m_funcs[i]->incref();
    }
}

~SIGNAL_CLASS() {
    for (unsigned i=0; i<m_size; ++i) {
        if (!m_funcs[i]->decref()) {
            delete m_funcs[i];
        }
    }
    if (m_funcs) {
        delete [] m_funcs;
    }
}

virtual void disconnect(void *functor) {
    for (unsigned i=0; i<m_size; ++i) {
        if (m_funcs[i] == functor) {
            if (!m_funcs[i]->decref()) {
                delete m_funcs[i];
            }
            m_funcs[i] = NULL;
            return;
        }
    }
    assert(false);
}

connection connect(func_t fcn) {
    fcn->incref();
    for (unsigned i=0; i<m_size; ++i) {
        if (!m_funcs[i]) {
            m_funcs[i] = fcn;
            return connection(this, fcn);
        }
    }
    ++m_size;
    func_t *nf = new func_t[m_size];
    memcpy(nf, m_funcs, sizeof(func_t)*(m_size-1));
    nf[m_size-1] = fcn;

    if (m_funcs)
        delete [] m_funcs;

    m_funcs = nf;
    return connection(this, fcn);
}

bool empty() {
    return m_size == 0;
}

void emit(OPERATOR_PARAM_DECL) {
    for (unsigned i=0; i<m_size; ++i) {
        if (m_funcs[i])
            m_funcs[i]->operator ()(CALL_PARAMS);
    }
}


#undef SIGNAL_CLASS
#undef FUNCTOR_NAME
#undef TYPENAMES
#undef BASE_CLASS_INST
#undef FUNCT_DECL
#undef OPERATOR_PARAM_DECL
#undef CALL_PARAMS


#if 0
template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;

    //--Paste template here
};

#endif


#if 0
template <typename RET, typename P1>
class signal1: public mysignal_base
{
public:
    typedef functor_base<RET, P1, P1>* func_t;
    unsigned m_size;
private:
    func_t *m_funcs;

public:

    signal1() { m_size = 0; m_funcs = 0;}

    ~signal1() {
        for (unsigned i=0; i<m_size; ++i) {
            delete m_funcs[i];
        }
        delete [] m_funcs;
    }

    virtual void disconnect(void *functor) {
        for (unsigned i=0; i<m_size; ++i) {
            if (m_funcs[i] == functor) {
                delete m_funcs[i];
                m_funcs[i] = NULL;
                return;
            }
        }
        assert(false);
    }

    connection connect(func_t fcn) {
        for (unsigned i=0; i<m_size; ++i) {
            if (!m_funcs[i]) {
                m_funcs[i] = fcn;
                return connection(this, fcn);
            }
        }
        ++m_size;
        func_t *nf = new func_t[m_size];
        memcpy(nf, m_funcs, sizeof(func_t)*(m_size-1));
        nf[m_size-1] = fcn;

        if (m_funcs)
            delete [] m_funcs;

        m_funcs = nf;
        return connection(this, fcn);
    }

    void emit(P1 p1) {
        for (unsigned i=0; i<m_size; ++i) {
            m_funcs[i]->operator ()(p1);
        }
    }

    bool empty() {
        return m_size == 0;
    }

};

//*****

template <typename RET, typename P1, typename P2>
class signal2: public mysignal_base
{
public:
    typedef functor_base<RET, P1, P2>* func_t;
    unsigned m_size;
private:
    func_t *m_funcs;

public:

    signal2() { m_size = 0; m_funcs = NULL;}

    ~signal2() {
        for (unsigned i=0; i<m_size; ++i) {
            delete m_funcs[i];
        }
        delete [] m_funcs;
    }

    virtual void disconnect(void *functor) {
        for (unsigned i=0; i<m_size; ++i) {
            if (m_funcs[i] == functor) {
                delete m_funcs[i];
                m_funcs[i] = NULL;
                return;
            }
        }
        assert(false);
    }

    connection connect(func_t fcn) {
        for (unsigned i=0; i<m_size; ++i) {
            if (!m_funcs[i]) {
                m_funcs[i] = fcn;
                return connection(this, fcn);
            }
        }
        ++m_size;
        func_t *nf = new func_t[m_size];
        memcpy(nf, m_funcs, sizeof(func_t)*(m_size-1));
        nf[m_size-1] = fcn;

        if (m_funcs)
            delete [] m_funcs;

        m_funcs = nf;
        return connection(this, fcn);
    }

    void emit(P1 p1, P2 p2) {
        for (unsigned i=0; i<m_size; ++i) {
            m_funcs[i]->operator ()(p1, p2);
        }
    }

    bool empty() {
        return m_size == 0;
    }

};
#endif
