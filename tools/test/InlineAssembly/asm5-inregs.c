//Testing many input registers spilled on the stack
#include <stdio.h>

int main(int argc, char **argv) 
{

   int arr[8] = {1,2,3,4,5,6,7,8};
   int res = 0;
   asm("mov %1, %0\n"
       "add %2, %0\n"
       "add %3, %0\n"
       "add %4, %0\n"
       "add %5, %0\n"
       "add %6, %0\n"
       "add %7, %0\n"
       "add %8, %0\n"
       "add %9, %0\n":
       "=r"(res) : "0"(res), 
       "r"(arr[0]), "r"(arr[1]),"r"(arr[2]),"r"(arr[3]),
       "r"(arr[4]), "r"(arr[5]),"r"(arr[6]),"r"(arr[7]));

   return printf("%d\n", res);
}
