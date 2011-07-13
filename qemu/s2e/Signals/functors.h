
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

#if 0
template <class T, typename RET, typename P1>
class functor1 : public functor_base <RET, P1, P1>
{
public:
    typedef RET (T::*func_t)(P1);
private:
    func_t m_func;
    T* m_obj;

public:
    functor1(T* obj, func_t f) {
        m_obj = obj;
        m_func = f;
    };

    virtual ~functor1() {}

    virtual RET operator()(P1 p1) {
        return (*m_obj.*m_func)(p1);
    };
};

template <class T, typename RET, typename P1>
inline functor_base<RET, P1, P1>*
mem_fun(T &obj, RET (T::*f)(P1)) {
    return new functor1<T, RET, P1>(&obj, f);
}
#endif

#if 0
template <class T, typename RET, typename P1, typename P2>
class functor2 : public functor_base <RET, P1, P2>
{
public:
    typedef RET (T::*func_t)(P1, P2);
private:
    func_t m_func;
    T* m_obj;

public:
    functor2(T* obj, func_t f) {
        m_obj = obj;
        m_func = f;
    };

    virtual ~functor2() {}

    virtual RET operator()(P1 p1, P2 p2) {
        return (*m_obj.*m_func)(p1, p2);
    };
};

template <class T, typename RET, typename P1, typename P2>
inline functor_base<RET, P1, P2>*
mem_fun(T &obj, RET (T::*f)(P1, P2)) {
    return new functor2<T, RET, P1, P2>(&obj, f);
}
#endif
