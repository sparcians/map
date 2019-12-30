

#include <cstdio>
#include <iostream>
#include "cache/HybridPLRU_8_Replacement.hpp"

static const uint32_t NUM_WAYS=8;

int main()
{
    sparta::cache::HybridPLRU_8_Replacement rep;

    rep.touchMRU(0);
    rep.touchMRU(1);
    rep.touchMRU(2);
    rep.touchMRU(3);
    rep.touchMRU(4);
    rep.touchMRU(5);
    rep.touchMRU(6);
    rep.touchMRU(7);

    // This test is hand coded and the expects are from pen&paper

    // LRU               MRU
    //  0     1     2     3         <-- top4 way in rank order
    // 0 1   2 3   4 5   6 7        <-- bottom 8 way in rank order
    uint32_t mru = rep.getMRUWay();
    uint32_t lru = rep.getLRUWay();
    std::cout << "MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==7);
    sparta_assert(lru==0);

    rep.touchMRU(5);
    //  0     1     3     2         <-- top4 way in rank order
    // 0 1   2 3   6 7   4 5        <-- bottom 8 way in rank order
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==5);
    sparta_assert(lru==0);

    rep.touchMRU(0);
    //  1     3     2     0         <-- top4 way in rank order
    // 2 3   6 7   4 5   1 0        <-- bottom 8 way in rank order
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==2);

    rep.touchLRU(0);
    //  0    1     3     2          <-- top4 way in rank order
    // 0 1  2 3   6 7   4 5         <-- bottom 8 way in rank order
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==5);
    sparta_assert(lru==0);

    rep.touchLRU(7);
    //   3    0    1     2          <-- top4 way in rank order
    //  7 6  1 0  2 3   4 5         <-- bottom 8 way in rank order
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==5);
    sparta_assert(lru==7);

    std::cout << "Testing touchMRU ....";
    for (uint32_t i=0; i<100; ++i) {
    uint32_t way = random() & (NUM_WAYS-1);
    rep.touchMRU(way);
    sparta_assert( way == rep.getMRUWay() );
    }
    std::cout << "PASSED" << std::endl;

    std::cout << "Testing touchLRU ....";
    for (uint32_t i=0; i<100; ++i) {
    uint32_t way = random() & (NUM_WAYS-1);
    rep.touchLRU(way);
    sparta_assert( way == rep.getLRUWay() );
    }
    std::cout << "PASSED" << std::endl;

    std::cout << "TEST PASSED" << std::endl;
    return 0;
}
