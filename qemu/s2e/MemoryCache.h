#ifndef _S2E_MEMORY_CACHE_

#define _S2E_MEMORY_CACHE_

#include <vector>
#include <inttypes.h>

namespace s2e {

template <class T, unsigned OBJSIZE, unsigned PAGESIZE, unsigned SUPERPAGESIZE>
class MemoryCache
{
private:
    struct ThirdLevel {
        T level3[PAGESIZE/OBJSIZE];
        ThirdLevel() {
            for (unsigned i=0; i<PAGESIZE/OBJSIZE; ++i) {
                level3[i] = T();
            }
        }        
    };

    struct SecondLevel {
        ThirdLevel* level2[SUPERPAGESIZE/PAGESIZE];

        SecondLevel() {
            for (unsigned i=0; i<SUPERPAGESIZE/PAGESIZE; ++i) {
                level2[i] = NULL;
            }
        }

        ~SecondLevel() {
            for (unsigned i=0; i<SUPERPAGESIZE/PAGESIZE; ++i) {
                if (level2[i])
                    delete level2[i];
            }
        }
    };

    std::vector<SecondLevel*> m_level1;
    uint64_t m_hostAddrStart;
    uint64_t m_size;


public:
    MemoryCache(uint64_t hostAddrStart, uint64_t size)
    {
        assert((size & ((1<<20)-1)) == 0);
        m_hostAddrStart = hostAddrStart;
        m_size = size;

        m_level1.resize(size / SUPERPAGESIZE, NULL);
    }

    //XXX: Clone an empty cache for now
    MemoryCache(const MemoryCache &one) {
        m_hostAddrStart = one.m_hostAddrStart;
        m_size = one.m_size;
        m_level1.resize(m_size / SUPERPAGESIZE, NULL);
    }

    ~MemoryCache() {
        flushCache();
    }

    void flushCache() {
        for (unsigned i=0; i<m_level1.size(); ++i) {
            if (m_level1[i]) {
                delete m_level1[i];
            }
        }
    }

    bool contains(uint64_t hostAddress) {
        return (hostAddress >= m_hostAddrStart) && (hostAddress < m_hostAddrStart + m_size);
    }

    void put(uint64_t hostAddress, const T &obj)
    {
        if (!contains(hostAddress)) {
            return;
        }
        uint64_t offset = hostAddress - m_hostAddrStart;
        uint64_t level1 = offset / SUPERPAGESIZE;
        uint64_t level2 = (offset & (SUPERPAGESIZE-1)) / PAGESIZE;
        uint64_t level3 = (offset / OBJSIZE) & ((PAGESIZE/OBJSIZE)-1);

        SecondLevel *ptrLevel2;
        if (!(ptrLevel2 = m_level1[level1])) {
            ptrLevel2 = new SecondLevel();
            m_level1[level1] = ptrLevel2;
        }

        ThirdLevel *ptrLevel3;
        if (!(ptrLevel3 = ptrLevel2->level2[level2])) {
            ptrLevel3 = new ThirdLevel();
            ptrLevel2->level2[level2] = ptrLevel3;
        }

        ptrLevel3->level3[level3] = obj;
    }

    T get(uint64_t hostAddress)
    {
        if (!contains(hostAddress)) {
            return T();
        }

        uint64_t offset = hostAddress - m_hostAddrStart;
        uint64_t level1 = offset / SUPERPAGESIZE;
        uint64_t level2 = (offset & (SUPERPAGESIZE-1)) / PAGESIZE;
        uint64_t level3 = (offset / OBJSIZE) & ((PAGESIZE/OBJSIZE)-1);

        SecondLevel *ptrLevel2;
        if (!(ptrLevel2 = m_level1[level1])) {
            return T();
        }

        ThirdLevel *ptrLevel3;
        if (!(ptrLevel3 = ptrLevel2->level2[level2])) {
            return T();
        }

        return ptrLevel3->level3[level3];
    }
};

}

#endif
