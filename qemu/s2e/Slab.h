/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef SLAB_H

#define SLAB_H


#include <inttypes.h>
#include <assert.h>
#include <memory.h>

#include <map>
#include <vector>
#include <set>

#include "machine.h"

namespace s2e
{

#define containing_record(address, type, field) ((type *)( \
  (uint8_t*)(address) - \
  (uintptr_t)(&((type *)0)->field)))


struct list_t
{
    list_t *next;
    list_t *prev;
};

static inline void list_init_head(list_t *lh)
{
    lh->prev = lh->next = lh;
}

#define list_empty(head) ((head)->next == (head))

static inline void list_remove_entry(list_t *entry)
{
    list_t *prev;
    list_t *next;

    next = entry->next;
    prev = entry->prev;
    prev->next = next;
    next->prev = prev;
}

static inline void list_insert_head(list_t *list_head, list_t* entry)
{
    list_t *next;

    next = list_head->next;
    entry->next = next;
    entry->prev = list_head;
    next->prev = entry;
    list_head->next = entry;
}

static inline void list_insert_tail(list_t* list_head, list_t* entry)
{
    list_t* prev;

    prev = list_head->prev;
    entry->next = list_head;
    entry->prev = prev;
    prev->next = entry;
    list_head->prev = entry;
}

static inline list_t* list_remove_tail(list_t* list_head)
{
    list_t* prev;
    list_t* entry;

    entry = list_head->prev;
    prev = entry->prev;
    list_head->prev = prev;
    prev->next = list_head;
    return entry;
}


//Allocates chunks of 256KB from the system
#define REGION_SIZE (256*1024)
class PageAllocator
{
private:
    struct RegCmp {
        bool operator() (uintptr_t r1, uintptr_t r2) const {
            return r1 + REGION_SIZE <= r2;
        }
    };

    //region offset to bitmap
    typedef std::map<uintptr_t, uintptr_t, RegCmp> RegionMap;
    typedef std::set<uintptr_t, RegCmp> RegionSet;
    RegionMap m_regions;
    RegionSet m_busyRegions;

private:
    inline uintptr_t getRegionSize() const {
        return REGION_SIZE;
    }

    uintptr_t osAlloc();
    void osFree(uintptr_t region);

public:
    PageAllocator();
    ~PageAllocator();

    uintptr_t allocPage();
    void freePage(uintptr_t page);

    uintptr_t getPageSize() const {
        return 0x1000;
    }

    bool belongsToUs(uintptr_t addr) const;
};


#define BLOCK_HDR_SIGNATURE 0x11AABB00

//This is 128 bytes long
struct BlockAllocatorHdr
{
    list_t link;
    uint32_t signature;
    uint32_t freeCount;
    uint64_t mask[8]; //Max 512 blocks of 8 bytes per page
    uint8_t padding[40];

    void init(BlockAllocatorHdr *a, uint32_t count) {
        a->freeCount = count;
        memset(a->mask, (unsigned)-1, sizeof(a->mask));
    }

    inline unsigned findFree(unsigned maxBlocks) const{
        assert(freeCount > 0);
        unsigned c = maxBlocks/(sizeof(mask[0])*8);
        if (maxBlocks%(sizeof(mask[0])*8)) {
            c++;
        }
        //std::cout << std::dec << freeCount << std::endl;
        unsigned i = 0;
        do {
            if (mask[i]) {
                uint64_t index = 0;
                int r = bit_scan_forward_64(&index, mask[i]);
                assert(r);
                return index + i * sizeof(mask[0])*8;
            }
            i++;
        }while(i<c);
        assert(false);
        return 0;
    }

    inline bool isFree(unsigned b) {
        uint64_t d = b / (sizeof(mask[0])*8);
        uint64_t r = b % (sizeof(mask[0])*8);
        return mask[d] & (1LL << r);
    }

    inline void alloc(unsigned b) {
        assert(freeCount > 0);
        assert(isFree(b));

        uint64_t d = b / (sizeof(mask[0])*8);
        uint64_t r = b % (sizeof(mask[0])*8);

        mask[d] &= ~(1LL << r);
        freeCount--;
    }

    inline void free(unsigned b) {
        assert(!isFree(b));

        uint64_t d = b / (sizeof(mask[0])*8);
        uint64_t r = b % (sizeof(mask[0])*8);

        mask[d] |= (1LL << r);

        freeCount++;
    }

}__attribute__((packed));


class BlockAllocator
{
private:
    list_t m_totallyFreeList;
    list_t m_freeList;
    list_t m_busyList;

    PageAllocator *m_pa;
    uintptr_t m_pageSize;
    uintptr_t m_blockSize;
    uintptr_t m_blocksPerPage;

    uint64_t m_freePagesCount;
    uint64_t m_busyPagesCount;
    uint64_t m_freeBlocksCount;

    uint64_t m_allocatedBlocksCount;
    uint8_t m_magic;

public:
    BlockAllocator(PageAllocator *pa, unsigned blockSizePo2, uint8_t magic = 0);

    ~BlockAllocator()
    {
        //Not necessary to free anything, the page allocator will do it
    }

    uintptr_t expand();
    void shrink();

    uintptr_t alloc();
    void free(uintptr_t b);

    uint64_t getAllocatedBlocksCount() const {
        return m_allocatedBlocksCount;
    }
};


class SlabAllocator
{
private:
    PageAllocator *m_pa;
    BlockAllocator **m_bas;

    unsigned m_minPo2, m_maxPo2;

    BlockAllocator *getSlab(uintptr_t addr) const;
    unsigned log(size_t s) const;
public:
    SlabAllocator(unsigned minPo2, unsigned maxPo2);
    ~SlabAllocator();

    uintptr_t alloc(size_t s);
    bool free(uintptr_t addr);
    bool isValid(uintptr_t addr) const;

    void printStats(std::ostream &os) const;

    const PageAllocator *getPageAllocator() const {
        return m_pa;
    }
};

}


#endif
