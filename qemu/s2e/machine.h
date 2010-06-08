//This headers contains declarations of functions
//implemented using assembly.

#ifndef _S2E_MACHINE_H_

#define _S2E_MACHINE_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);

#ifdef __cplusplus
}
#endif


#endif
