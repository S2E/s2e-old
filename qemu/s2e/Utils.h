#ifndef S2E_UTILS_H
#define S2E_UTILS_H

#include <cstdio>
#include <cassert>

#ifdef NDEBUG
#define DPRINTF(...)
#else
#define DPRINTF(...) printf(__VA_ARGS__)
#endif

/* The following is GCC-specific implementation of foreach.
   Should handle correctly all crazy C++ corner cases */

template <typename T>
class _S2EForeachContainer {
public:
    inline _S2EForeachContainer(const T& t) : c(t), brk(0), i(c.begin()), e(c.end()) { }
    const T c; /* Compiler will remove the copying here */
    int brk;
    typename T::const_iterator i, e;
};

#define foreach(variable, container) \
for (_S2EForeachContainer<__typeof__(container)> _container_(container); \
     !_container_.brk && _container_.i != _container_.e; \
     __extension__  ({ ++_container_.brk; ++_container_.i; })) \
    for (variable = *_container_.i;; __extension__ ({--_container_.brk; break;}))

#endif // S2E_UTILS_H
