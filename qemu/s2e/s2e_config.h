#ifndef S2E_CONFIG_H
#define S2E_CONFIG_H

/** This defines the size of each MemoryObject that represents physical RAM.
    Larget values save some memory, smaller (exponentially) decrease solving
    time for constraints with symbolic addresses */
#define S2E_RAM_OBJECT_BITS 12
#define S2E_RAM_OBJECT_SIZE (1 << S2E_RAM_OBJECT_BITS)
#define S2E_RAM_OBJECT_MASK (~(S2E_RAM_OBJECT_SIZE - 1))

/** Enables S2E TLB to speed-up concrete memory accesses */
#define S2E_ENABLE_S2E_TLB

/** Defines whether all memory addresses should be concretized by
    forking state for all possible values */
#define S2E_FORK_ON_SYMBOLIC_ADDRESS

/** Enables simple memory debugging support */
//#define S2E_DEBUG_MEMORY

#endif // S2E_CONFIG_H
