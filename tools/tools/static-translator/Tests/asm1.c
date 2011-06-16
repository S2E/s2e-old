#include <stdio.h>

int rol(int val, int count) 
{
  int res;
  asm("rol %%cl, %%eax":"=a" (res) : "a"(val), "c" (count) );
  return res;
}

int main(int argc, char **argv) 
{
   return printf("%x\n", rol(argc, argc));
}
