#ifndef _S2E_SIGNALS_

#define _S2E_SIGNALS_

#include <cassert>
#include <stdlib.h>

namespace fsigc {

class trackable{};

struct nil {};

class mysignal_base;

class connection {
private:
    void *m_functor;
    mysignal_base *m_sig;
    bool m_connected;
public:
    connection() {
        m_functor = NULL;
        m_sig = NULL;
        m_connected = false;
    }

    connection(mysignal_base *sig, void *func);
    inline bool connected() const {
        return m_connected;
    }
    void disconnect();
};


class mysignal_base
{
public:
    virtual void disconnect(void *functor) = 0;
};

//*************************************************
//*************************************************
//*************************************************


template <typename RET, typename P1, typename P2, typename P3,
        typename P4, typename P5, typename P6, typename P7>
class functor_base
{
private:
    unsigned m_refcount;
public:
    void incref() { ++m_refcount; }
    unsigned decref() { assert(m_refcount > 0); return --m_refcount; }
    virtual ~functor_base() {}
    virtual RET operator()() {};
    virtual RET operator()(P1 p1) {};
    virtual RET operator()(P1 p1, P2 p2) {};
    virtual RET operator()(P1 p1, P2 p2, P3 p3) {};
    virtual RET operator()(P1 p1, P2 p2, P3 p3, P4 p4) {};
    virtual RET operator()(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {};
    virtual RET operator()(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) {};
    virtual RET operator()(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {};
};


//*************************************************
//Stateless function pointers
//0 parameter
//*************************************************
template <typename RET>
class ptrfun0 : public functor_base <RET, nil, nil, nil, nil, nil, nil, nil>
{
public:
    typedef RET (*func_t)();
protected:
    func_t m_func;

public:
    ptrfun0(func_t f) {
        m_func = f;
    };

    virtual ~ptrfun0() {}

    virtual RET operator()() {
        return (*m_func)();
    };
};

template <typename RET>
inline functor_base<RET, nil, nil, nil, nil, nil, nil, nil>*
ptr_fun(RET (*f)()) {
    return new ptrfun0<RET>(f);
}

//*************************************************
//*************************************************
//*************************************************



//*************************************************
//0 parameter
//*************************************************
template <class T, typename RET>
class functor0 : public functor_base <RET, nil, nil, nil, nil, nil, nil, nil>
{
public:
    typedef RET (T::*func_t)();
protected:
    func_t m_func;
    T* m_obj;

public:
    functor0(T* obj, func_t f) {
        m_obj = obj;
        m_func = f;
    };

    virtual ~functor0() {}

    virtual RET operator()() {
        return (*m_obj.*m_func)();
    };
};

template <class T, typename RET>
inline functor_base<RET, nil, nil, nil, nil, nil, nil, nil>*
mem_fun(T &obj, RET (T::*f)()) {
    return new functor0<T, RET>(&obj, f);
}


#define SIGNAL_CLASS        signal0
#define OPERATOR_PARAM_DECL
#define CALL_PARAMS

template <typename RET>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, nil, nil, nil, nil, nil, nil, nil>* func_t;
    #include "sig-template.h"
};

#undef CALL_PARAMS
#undef OPERATOR_PARAM_DECL
#undef SIGNAL_CLASS

//*************************************************
//1 parameter
//*************************************************

#define FUNCTOR_NAME        functor1
#define TYPENAMES           typename P1
#define BASE_CLASS_INST     P1, nil, nil, nil, nil, nil, nil
#define FUNCT_DECL          P1
#define OPERATOR_PARAM_DECL P1 p1
#define CALL_PARAMS         p1
#define SIGNAL_CLASS        signal1

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};

//*************************************************
//2 parameters
//*************************************************

#define FUNCTOR_NAME        functor2
#define TYPENAMES           typename P1, typename P2
#define BASE_CLASS_INST     P1, P2, nil, nil, nil, nil, nil
#define FUNCT_DECL          P1, P2
#define OPERATOR_PARAM_DECL P1 p1, P2 p2
#define CALL_PARAMS         p1, p2
#define SIGNAL_CLASS        signal2

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};

//*************************************************
//3 parameters
//*************************************************

#define FUNCTOR_NAME        functor3
#define TYPENAMES           typename P1, typename P2, typename P3
#define BASE_CLASS_INST     P1, P2, P3, nil, nil, nil, nil
#define FUNCT_DECL          P1, P2, P3
#define OPERATOR_PARAM_DECL P1 p1, P2 p2, P3 p3
#define CALL_PARAMS         p1, p2, p3
#define SIGNAL_CLASS        signal3

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};

//*************************************************
//4 parameters
//*************************************************

#define FUNCTOR_NAME        functor4
#define TYPENAMES           typename P1, typename P2, typename P3, typename P4
#define BASE_CLASS_INST     P1, P2, P3, P4, nil, nil, nil
#define FUNCT_DECL          P1, P2, P3, P4
#define OPERATOR_PARAM_DECL P1 p1, P2 p2, P3 p3, P4 p4
#define CALL_PARAMS         p1, p2, p3, p4
#define SIGNAL_CLASS        signal4

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};


//*************************************************
//5 parameters
//*************************************************

#define FUNCTOR_NAME        functor5
#define TYPENAMES           typename P1, typename P2, typename P3, typename P4, typename P5
#define BASE_CLASS_INST     P1, P2, P3, P4, P5, nil, nil
#define FUNCT_DECL          P1, P2, P3, P4, P5
#define OPERATOR_PARAM_DECL P1 p1, P2 p2, P3 p3, P4 p4, P5 p5
#define CALL_PARAMS         p1, p2, p3, p4, p5
#define SIGNAL_CLASS        signal5

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};


//*************************************************
//6 parameters
//*************************************************

#define FUNCTOR_NAME        functor6
#define TYPENAMES           typename P1, typename P2, typename P3, typename P4, typename P5, typename P6
#define BASE_CLASS_INST     P1, P2, P3, P4, P5, P6, nil
#define FUNCT_DECL          P1, P2, P3, P4, P5, P6
#define OPERATOR_PARAM_DECL P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6
#define CALL_PARAMS         p1, p2, p3, p4, p5, p6
#define SIGNAL_CLASS        signal6

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};


//*************************************************
//7 parameters
//*************************************************

#define FUNCTOR_NAME        functor7
#define TYPENAMES           typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7
#define BASE_CLASS_INST     P1, P2, P3, P4, P5, P6, P7
#define FUNCT_DECL          P1, P2, P3, P4, P5, P6, P7
#define OPERATOR_PARAM_DECL P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7
#define CALL_PARAMS         p1, p2, p3, p4, p5, p6, p7
#define SIGNAL_CLASS        signal7

#include "functors.h"

template <typename RET, TYPENAMES>
class SIGNAL_CLASS: public mysignal_base
{
public:
    typedef functor_base<RET, BASE_CLASS_INST>* func_t;
    #include "sig-template.h"
};


//*************************************************
//*************************************************
//*************************************************

//Binders

//*************************************************
//0 arguments base event - 1 extra argument
template <typename RET, typename A1, typename B1>
class functor0_1 : public functor_base<RET, nil, nil, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, A1, nil, nil, nil, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1;
public:

    functor0_1(functor_t *fb, A1 _a1) : a1(_a1) {
        this->m_fb = fb;
    }
    virtual ~functor0_1() {delete this->m_fb;}
    virtual RET operator()() {
        return m_fb->operator ()(a1);
    };

};

template <typename RET, typename A1, typename B1>
inline functor_base<RET, nil, nil, nil, nil, nil, nil, nil>*
bind(functor_base<RET, A1, nil, nil, nil, nil, nil, nil> *f, B1 a1) {
    return new functor0_1<RET, A1, B1>(f, a1);
}

//0 arguments base event - 2 extra argument
template <typename RET, typename A1, typename A2>
class functor0_2 : public functor_base<RET, nil, nil, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, A1, A2, nil, nil, nil, nil, nil> functor_t;

private:
    A1 a1; A2 a2;
    functor_t *m_fb;
public:

    functor0_2(functor_t *fb, A1 a1, A2 a2) {
        this->m_fb = fb;
        this->a1 = a1;
        this->a2 = a2;
    }
    virtual ~functor0_2() {delete this->m_fb;}
    virtual RET operator()() {
        return m_fb->operator ()(a1, a2);
    };
};

template <typename RET, typename A1, typename B1, typename A2, typename B2>
inline functor_base<RET, nil, nil, nil, nil, nil, nil, nil>*
bind(functor_base<RET, A1, A2, nil, nil, nil, nil, nil> *f, B1 a1, B2 a2) {
    return new functor0_2<RET, A1, A2>(f, a1, a2);
}

//*************************************************
//1 arguments base event - 1 extra argument
template <typename RET, typename BE1, typename A1>
class functor1_1 : public functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, A1, nil, nil, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1;
public:

    functor1_1(functor_t *fb, A1 a1_):m_fb(fb), a1(a1_) {
    }
    virtual ~functor1_1() {delete this->m_fb;}
    virtual RET operator()(BE1 be1) {
        return m_fb->operator ()(be1, a1);
    };

};

template <typename RET, typename BE1, typename A1, typename B1>
inline functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, A1, nil, nil, nil, nil, nil> *f, B1 a1) {
    return new functor1_1<RET, BE1, A1>(f, a1);
}

//1 arguments base event - 2 extra argument
template <typename RET, typename BE1, typename A1, typename A2>
class functor1_2 : public functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, A1, A2, nil, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2;
public:

    functor1_2(functor_t *fb, A1 a1_, A2 a2_):m_fb(fb), a1(a1_), a2(a2_) {}
    virtual ~functor1_2() {delete this->m_fb;}
    virtual RET operator()(BE1 be1) {
        return m_fb->operator ()(be1, a1, a2);
    };

};

template <typename RET, typename BE1, typename A1, typename B1, typename A2, typename B2>
inline functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, A1, A2, nil, nil, nil, nil> *f, B1 a1, B2 a2) {
    return new functor1_2<RET, BE1, A1, A2>(f, a1, a2);
}

//1 arguments base event - 3 extra argument
template <typename RET, typename BE1, typename A1, typename A2, typename A3>
class functor1_3 : public functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, A1, A2, A3, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2; A3 a3;

public:

    functor1_3(functor_t *fb, A1 a1_, A2 a2_, A3 a3_):m_fb(fb), a1(a1_), a2(a2_), a3(a3_) {}
    virtual ~functor1_3() {delete this->m_fb;}
    virtual RET operator()(BE1 be1) {
        return m_fb->operator ()(be1, a1, a2, a3);
    };

};

template <typename RET, typename BE1, typename A1, typename B1, typename A2, typename B2, typename A3, typename B3>
inline functor_base<RET, BE1, nil, nil, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, A1, A2, A3, nil, nil, nil> *f, B1 a1, B2 a2, B3 a3) {
    return new functor1_3<RET, BE1, A1, A2, A3>(f, a1, a2, a3);
}

//*************************************************
//2 arguments base event - 1 extra argument
template <typename RET, typename BE1, typename BE2, typename A1>
class functor2_1 : public functor_base<RET, BE1, BE2, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, A1, nil, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1;

public:

    functor2_1(functor_t *fb, A1 a1_):m_fb(fb), a1(a1_) {}
    virtual ~functor2_1() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2) {
        return m_fb->operator ()(be1, be2, a1);
    };
};

template <typename RET, typename BE1, typename BE2, typename A1, typename B1>
inline functor_base<RET, BE1, BE2, nil, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, A1, nil, nil, nil, nil> *f, B1 a1) {
    return new functor2_1<RET, BE1, BE2, A1>(f, a1);
}

//2 arguments base event - 2 extra argument
template <typename RET, typename BE1, typename BE2, typename A1, typename A2>
class functor2_2 : public functor_base<RET, BE1, BE2, nil, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, A1, A2, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2;

public:

    functor2_2(functor_t *fb, A1 a1_, A2 a2_):m_fb(fb), a1(a1_), a2(a2_) {}
    virtual ~functor2_2() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2) {
        return m_fb->operator ()(be1, be2, a1, a2);
    };

};

template <typename RET, typename BE1, typename BE2, typename A1, typename B1, typename A2, typename B2>
inline functor_base<RET, BE1, BE2, nil, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, A1, A2, nil, nil, nil> *f, B1 a1, B2 a2) {
    return new functor2_2<RET, BE1, BE2, A1, A2>(f, a1, a2);
}


//*************************************************
//3 arguments base event - 1 extra argument
template <typename RET, typename BE1, typename BE2, typename BE3, typename A1>
class functor3_1 : public functor_base<RET, BE1, BE2, BE3, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, BE3, A1, nil, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1;

public:

    functor3_1(functor_t *fb, A1 a1_):m_fb(fb), a1(a1_) {}
    virtual ~functor3_1() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2, BE3 be3) {
        return m_fb->operator ()(be1, be2, be3, a1);
    };
};

template <typename RET, typename BE1, typename BE2, typename BE3, typename A1, typename B1>
inline functor_base<RET, BE1, BE2, BE3, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, BE3, A1, nil, nil, nil> *f, B1 a1) {
    return new functor3_1<RET, BE1, BE2, BE3, A1>(f, a1);
}

//3 arguments base event - 2 extra argument
template <typename RET, typename BE1, typename BE2, typename BE3, typename A1, typename A2>
class functor3_2 : public functor_base<RET, BE1, BE2, BE3, nil, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, BE3, A1, A2, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2;

public:

    functor3_2(functor_t *fb, A1 a1_, A2 a2_):m_fb(fb), a1(a1_), a2(a2_) {}
    virtual ~functor3_2() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2, BE3 be3) {
        return m_fb->operator ()(be1, be2, be3, a1, a2);
    };

};

template <typename RET, typename BE1, typename BE2, typename BE3, typename A1, typename B1, typename A2, typename B2>
inline functor_base<RET, BE1, BE2, BE3, nil, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, BE3, A1, A2, nil, nil> *f, B1 a1, B2 a2) {
    return new functor3_2<RET, BE1, BE2, BE3, A1, A2>(f, a1, a2);
}

//*************************************************
//4 arguments base event - 1 extra argument
template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4, typename A1>
class functor4_1 : public functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, BE3, BE4, A1, nil, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1;

public:

    functor4_1(functor_t *fb, A1 a1_):m_fb(fb), a1(a1_) {}
    virtual ~functor4_1() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2, BE3 be3, BE4 be4) {
        return m_fb->operator ()(be1, be2, be3, be4, a1);
    };
};

template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4, typename A1, typename B1>
inline functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, BE3, BE4, A1, nil, nil> *f, B1 a1) {
    return new functor4_1<RET, BE1, BE2, BE3, BE4, A1>(f, a1);
}

//4 arguments base event - 2 extra argument
template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4, typename A1, typename A2>
class functor4_2 : public functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, BE3, BE4, A1, A2, nil> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2;

public:

    functor4_2(functor_t *fb, A1 a1_, A2 a2_):m_fb(fb), a1(a1_), a2(a2_) {}
    virtual ~functor4_2() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2, BE3 be3, BE4 be4) {
        return m_fb->operator ()(be1, be2, be3, be4, a1, a2);
    };
};

template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4, typename A1, typename B1, typename A2, typename B2>
inline functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, BE3, BE4, A1, A2, nil> *f, B1 a1, B2 a2) {
    return new functor4_2<RET, BE1, BE2, BE3, BE4, A1, A2>(f, a1, a2);
}

//4 arguments base event - 3 extra argument
template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4, typename A1, typename A2, typename A3>
class functor4_3 : public functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>
{
public:
    typedef functor_base<RET, BE1, BE2, BE3, BE4, A1, A2, A3> functor_t;

private:
    functor_t *m_fb;
    A1 a1; A2 a2; A3 a3;

public:

    functor4_3(functor_t *fb, A1 a1_, A2 a2_, A3 a3_):m_fb(fb), a1(a1_), a2(a2_), a3(a3_) {}
    virtual ~functor4_3() {delete this->m_fb;}
    virtual RET operator()(BE1 be1, BE2 be2, BE3 be3, BE4 be4) {
        return m_fb->operator ()(be1, be2, be3, be4, a1, a2, a3);
    };
};

template <typename RET, typename BE1, typename BE2, typename BE3, typename BE4,
          typename A1, typename B1, typename A2, typename B2, typename A3, typename B3>
inline functor_base<RET, BE1, BE2, BE3, BE4, nil, nil, nil>*
bind(functor_base<RET, BE1, BE2, BE3, BE4, A1, A2, A3> *f, B1 a1, B2 a2, B3 a3) {
    return new functor4_3<RET, BE1, BE2, BE3, BE4, A1, A2, A3>(f, a1, a2, a3);
}

//*************************************************
//*************************************************
//*************************************************


template <typename RET, typename P1 = nil, typename P2 = nil, typename P3 = nil,
                        typename P4 = nil, typename P5 = nil, typename P6 = nil, typename P7 = nil>
class signal: public signal7<RET, P1, P2, P3, P4, P5, P6, P7> { };


template <typename RET, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
class signal<RET, P1, P2, P3, P4, P5, P6>: public signal6<RET, P1, P2, P3, P4, P5, P6>
{};


template <typename RET, typename P1, typename P2, typename P3, typename P4, typename P5>
class signal<RET, P1, P2, P3, P4, P5>: public signal5<RET, P1, P2, P3, P4, P5>
{};


template <typename RET, typename P1, typename P2, typename P3, typename P4>
class signal<RET, P1, P2, P3, P4>: public signal4<RET, P1, P2, P3, P4>
{};


template <typename RET, typename P1, typename P2, typename P3>
class signal<RET, P1, P2, P3>: public signal3<RET, P1, P2, P3>
{};


template <typename RET, typename P1, typename P2>
class signal<RET, P1, P2>: public signal2<RET, P1, P2>
{};

template <typename RET, typename P1>
class signal<RET, P1>: public signal1<RET, P1>
{};

template <typename RET>
class signal<RET>: public signal0<RET>
{};



}

#endif
