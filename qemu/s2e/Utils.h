#ifndef S2E_UTILS_H
#define S2E_UTILS_H

#include <cstdio>
#include <cassert>
#include <ostream>
#include <iomanip>
#include <inttypes.h>

namespace s2e {

struct hexval {
    uint64_t value;
    int width;

    hexval(uint64_t _value, int _width=0) : value(_value), width(_width) {}
    hexval(void* _value): value((uint64_t)_value), width(sizeof(uintptr_t)*2) {}
};

inline std::ostream& operator<<(std::ostream& out, const hexval& h)
{
    out << "0x";

    std::ios_base::fmtflags oldf = out.flags();
    out.setf(std::ios::hex, std::ios_base::basefield);
    std::streamsize oldw = out.width(); out.width(h.width);
    char oldfill = out.fill(); out.fill('0');

    out << h.value;

    out.flags(oldf); out.width(oldw); out.fill(oldfill);
    return out;
}

/** A macro used to escape "," in an argument to another macro */
#define S2E_NOOP(...) __VA_ARGS__

#ifdef NDEBUG
#define DPRINTF(...)
#define TRACE(...) 
#else
#define DPRINTF(...) printf(__VA_ARGS__)
#define TRACE(...) { printf("%s - ", __FUNCTION__); printf(__VA_ARGS__); }
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

#define foreach2(_i, _b, _e) \
      for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i) 

} // namespace s2e

#endif // S2E_UTILS_H
