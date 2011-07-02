//Testing multiple output operands
#include <stdio.h>

int rol(int *val, int count, int *dummy) 
{
  int res;
  asm("mov (%%rdi), %%eax\n"
      "rol %%cl, %%eax\n"
      "mov $0xdeadbeef, %%edx":"=a" (res), "=d" (*dummy) :  "d"(val), "c" (count) );
  return res;
}

int main(int argc, char **argv) 
{
   int res, dummy;
   res = rol(&argc, argc, &dummy);
   return printf("%x %x\n", res, dummy);
}
