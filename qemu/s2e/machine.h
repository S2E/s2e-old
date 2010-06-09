//This headers contains declarations of functions
//implemented using assembly.

#ifndef _S2E_MACHINE_H_

#define _S2E_MACHINE_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

//XXX: ugly hack to support underscores
#ifndef _WIN32
#define bit_scan_forward_64 bit_scan_forward_64_posix
int bit_scan_forward_64_posix(uint64_t *SetIndex, uint64_t Mask);

#else

int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);
#endif
#ifdef __cplusplus
}
#endif


#endif
