#ifndef S2ELJLJ_H

#define S2ELJLJ_H

#include <config.h>
#if defined(CONFIG_WIN32) && defined (CONFIG_S2E)
#include <inttypes.h>


struct _s2e_jmpbuf_t
{
    uint64_t gpregs[16];
    uint64_t rip;
};

typedef struct _s2e_jmpbuf_t s2e_jmp_buf[1];

#ifdef __cplusplus
extern "C" {
#endif

int s2e_setjmp(s2e_jmp_buf buf);
int s2e_longjmp(s2e_jmp_buf buf, int value);

#ifdef __cplusplus
}
#endif

#else

#include <setjmp.h>

#define s2e_setjmp setjmp
#define s2e_longjmp longjmp
#define s2e_jmp_buf jmp_buf

#endif

#endif
