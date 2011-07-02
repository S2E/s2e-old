//Testing registers spilled on the stack
#include <stdio.h>
#include <inttypes.h>

typedef struct {
  char c1, c2;
  short s1, s2;
  uint32_t l1, l2;
  uint64_t ll1, ll2;
}data;

int main(int argc, char **argv) 
{
   data d;

   asm("mov $0xbb, %0\n"
       "mov $0x12, %1\n"
       "mov $0xddbb, %2\n"
       "mov $0xeebb, %3\n"
       "mov $0xffbbaaff, %4\n"
       "mov $0x11bbaa00, %5\n"
       "mov $0x2334aa2222bb, %6\n"
       "mov $0x0011aabbaabb, %7\n"
       :"=r"(d.c1), "=r"(d.c2), 
        "=r"(d.s1), "=r"(d.s2),
        "=r"(d.l1), "=r"(d.l2),
        "=r"(d.ll1), "=r"(d.ll2));

   return printf("%x %x %x %x %x %x %"PRIx64" %"PRIx64"\n", d.c1, d.c2, d.s1, d.s2, d.l1, d.l1, d.ll1, d.ll2);
}
