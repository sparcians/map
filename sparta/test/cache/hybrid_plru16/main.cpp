

#include <cstdio>
#include <iostream>
#include "cache/HybridPLRU_16_Replacement.hpp"

static const uint32_t NUM_WAYS=16;

int main()
{
    sparta::cache::HybridPLRU_16_Replacement rep;

    rep.touchMRU(0);
    rep.touchMRU(1);
    rep.touchMRU(2);
    rep.touchMRU(3);
    rep.touchMRU(4);
    rep.touchMRU(5);
    rep.touchMRU(6);
    rep.touchMRU(7);
    rep.touchMRU(8);
    rep.touchMRU(9);
    rep.touchMRU(10);
    rep.touchMRU(11);
    rep.touchMRU(12);
    rep.touchMRU(13);
    rep.touchMRU(14);
    rep.touchMRU(15);

    // This test is hand coded and the expects are from pen&paper
    std::cout << "Initial: " << std::endl;
    // LRU                                         MRU
    //  0     1     2     3   |   4     5     6     7        <-- top8 ways in rank order
    // 0 1   2 3   4 5   6 7  |  8 9   a b   c d   e f       <-- bottom 16 ways in rank order
    uint32_t mru = rep.getMRUWay();
    uint32_t lru = rep.getLRUWay();
    std::cout << "  MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==15);
    sparta_assert(lru==0);

    std::cout << "touchMRU(5): " << std::endl;
    rep.touchMRU(5);
    // LRU                                           MRU
    //   4     5     6     7    |   0     1      3    2          <-- top8 ways
    //  8 9   a b   c d   e f   |  0 1   2 3    6 7  4 5        <-- bottom 16 ways
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "  MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==5);
    sparta_assert(lru==8);

    std::cout << "touchMRU(0): " << std::endl;
    rep.touchMRU(0);
    // LRU                                           MRU
    //   4     5     6     7    |    1      3    2     0         <-- top8 ways
    //  8 9   a b   c d   e f   |   2 3    6 7  4 5   1 0        <-- bottom 16 ways
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "  MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==8);

    std::cout << "touchLRU(0): " << std::endl;
    rep.touchLRU(0);
    // LRU                                         MRU
    //    0    1      3    2   |   4     5     6     7         <-- top8 ways
    //   0 1  2 3    6 7  4 5  |  8 9   a b   c d   e f       <-- bottom 16 ways
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "  MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==15);
    sparta_assert(lru==0);

    std::cout << "touchLRU(5): " << std::endl;
    rep.touchLRU(5);
    //  LRU                                           MRU
    //    2    0    1      3   |   4     5     6     7         <-- top8 ways
    //   5 4  0 1  2 3    6 7  |  8 9   a b   c d   e f       <-- bottom 16 ways
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "  MRU=" << mru << " LRU=" << lru << std::endl;
    sparta_assert(mru==15);
    sparta_assert(lru==5);

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
    rep.touchMRU(way);
    sparta_assert( way == rep.getMRUWay() );
    }
    std::cout << "PASSED" << std::endl;

    std::cout << "TEST PASSED" << std::endl;
    return 0;
}
