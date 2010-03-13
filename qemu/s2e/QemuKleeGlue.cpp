#include "QemuKleeGlue.h"

#include <s2e/s2e.h>

extern "C"
{
  int cpu_memory_rw_debug_se(uint64_t addr,
                        uint8_t *buf, int len, int is_write);
  uint64_t cpu_get_phys_page_debug_se(uint64_t addr);

#include "config.h"
#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"

}

using namespace std;

void QEMU::DumpVirtualMemory(uint64_t Addr, unsigned Length)
{
 unsigned int i, j;
 uint8_t Data;
 
	for (i=0; i<Length; i++)
	{
		if (!(i % 16)) {
			printf("\n");
      printf("%I64x ", Addr + i);
		}

    bool IsValid = ReadVirtualMemory(Addr + i, &Data, 1);
		
    if (IsValid) {
      printf("%02x ", Data);
    }else {
      printf("-- ");
    }

		if ((i%16) == 15)
		{
      printf(" ");
        for (j=0; j<16; j++) {
					IsValid = ReadVirtualMemory(Addr + i-15+j, &Data, 1);	
          if (IsValid) {
            if (Data < 32)
            {
              printf(".");
              continue;
            }

            printf("%c", Data);
          }else {
            printf("-");
          }
				}
		}

	}
  printf("\n");

}

bool QEMU::GetAsciiz(uint64_t base, std::string &ret)
{
	char c;
	unsigned i=0;

  ret = "";
	do {
		if (cpu_memory_rw_debug_se(base+i, (uint8_t*)&c, sizeof(char), 0)<0) {
			DPRINTF("Could not load asciiz at %#I64x\n", base+i);
			return false;
		}
		if (c) {
			ret = ret + c;
		}else {
			break;
		}
		i++;
	}while(i<256);

	return true;
}

std::string QEMU::GetUnicode(uint64_t base, unsigned size)
{
  string ret;
	uint16_t c;
	for (unsigned i=0; i<size; i++) {
		if (cpu_memory_rw_debug_se(base+i*sizeof(c), (uint8_t*)&c, sizeof(c), 0)<0) {
			DPRINTF("Could not load unicode char at %#I64x\n", base+i);
			break;
		}
		if (c) {
			ret += (char)c;
		}else {
			break;
		}
	}
	return ret;
}



bool QEMU::ReadVirtualMemory(uint64_t Addr, void *Buffer, unsigned Length)
{
  return cpu_memory_rw_debug_se(Addr, (uint8_t*)Buffer, Length, 0 )  == 0;
}

uint64_t QEMU::GetPhysAddr(uint64_t va)
{
  uint64_t a = cpu_get_phys_page_debug_se(va);
  if (a == (uint64_t)-1)
    return (uint64_t)-1;
  a |= va & 0xFFF;
  return a;
}

/** 
 *  Reads an integer from the guest's VM.
 *  XXX:Take into account the indianness of the host and of the target.
 */
bool QEMU::ReadInteger(uint64_t Addr, unsigned Size, uint64_t &Result)
{
  bool Status;

  switch(Size) {
    case 1:
      {
        uint8_t Val;
        Status = ReadVirtualMemory(Addr, &Val, Size);
        Result = Val;
        break;
      }
    case 2:
      {
        uint16_t Val;
        Status = ReadVirtualMemory(Addr, &Val, Size);
        Result = Val;
        break;
      }
    case 4:
      {
        uint32_t Val;
        Status = ReadVirtualMemory(Addr, &Val, Size);
        Result = Val;
        break;
      }
    case 8:
      {
        uint64_t Val;
        Status = ReadVirtualMemory(Addr, &Val, Size);
        Result = Val;
        break;
      }
  }


  
  return Status;
}