#include <inttypes.h>
int bitScanReverse(uint64_t x) {
   asm ("out %%ax,%%dx; bsrl %0, %0" : "=r" (x) : "0" (x));
   return (int) x;
}


int main(int argc, char **argv)
{
	uint32_t x = 0x7FFFFFFF;
	asm ("bsrl %0, %0" : "=r" (x) : "0" (x));
        return (int) x;
}
