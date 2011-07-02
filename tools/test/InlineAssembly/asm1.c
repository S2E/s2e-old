#include <stdio.h>

int rol(int val, int count) 
{
  int res;
  asm("rol %%cl, %%eax\n"
      "mov %%eax, %%ecx\n":"=c" (res) : "a"(val), "c" (count) );
  return res;
}

int main(int argc, char **argv) 
{
   return printf("%x\n", rol(argc, argc));
}
