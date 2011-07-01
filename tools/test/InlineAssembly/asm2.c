//Testing pointers as operands
#include <stdio.h>

int rol(int *val, int count) 
{
  int res;
  asm("mov (%%rdi), %%eax\n"
      "rol %%cl, %%eax":"=a" (res) : "d"(val), "c" (count) );
  return res;
}

int main(int argc, char **argv) 
{
   return printf("%x\n", rol(&argc, argc));
}
