

#include <cstdio>
#include <iostream>
#include "cache/TreePLRUReplacement.hpp"


void test1_touchMRU_touchLRU()
{
    uint32_t NUM_WAYS=4;
    sparta::cache::TreePLRUReplacement rep(NUM_WAYS);

    // This test is hand coded with expects generated with pen & paper

    rep.touchMRU(0);
    rep.touchMRU(1);
    rep.touchMRU(2);
    rep.touchMRU(3);
    uint32_t mru = rep.getMRUWay();
    uint32_t lru = rep.getLRUWay();
    std::cout << "    Initial: " << rep.getDisplayString() << std::endl;
    sparta_assert(mru==3);
    sparta_assert(lru==0);

    rep.touchMRU(2);
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "touchMRU(2): " << rep.getDisplayString() << std::endl;
    sparta_assert(mru==2);
    sparta_assert(lru==0);

    rep.touchMRU(0);
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "touchMRU(0): " << rep.getDisplayString() << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==3);

    rep.touchMRU(2);
     mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "touchMRU(2): " << rep.getDisplayString() << std::endl;
    sparta_assert(mru==2);
    sparta_assert(lru==1);


    rep.touchLRU(2);
    mru = rep.getMRUWay();
    lru = rep.getLRUWay();
    std::cout << "touchLRU(2): " << rep.getDisplayString() << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==2);


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





    NUM_WAYS=64;
    sparta::cache::TreePLRUReplacement rep2(NUM_WAYS);

    for (uint32_t i=0; i<NUM_WAYS; ++i) {
    rep2.touchMRU(i);
    }

    // LRU               MRU
    //   0,1  2,3  ...  62,63    :Initial way order
    mru = rep2.getMRUWay();
    lru = rep2.getLRUWay();
    std::cout << "    Initial: " << rep2.getDisplayString() << std::endl;
    sparta_assert(mru==63);
    sparta_assert(lru==0);

    rep2.touchMRU(0);
    mru = rep2.getMRUWay();
    lru = rep2.getLRUWay();
    std::cout << "touchmMRU(0): " << rep2.getDisplayString() << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==32);

    rep2.touchLRU(35);
    mru = rep2.getMRUWay();
    lru = rep2.getLRUWay();
    std::cout << "touchmLRU(35): " << rep2.getDisplayString() << std::endl;
    sparta_assert(mru==0);
    sparta_assert(lru==35);

    rep2.touchMRU(35);
    mru = rep2.getMRUWay();
    lru = rep2.getLRUWay();
    std::cout << "touchmMRU(35): " << rep2.getDisplayString() << std::endl;
    sparta_assert(mru==35);
    sparta_assert(lru==16);

    std::cout << "Testing touchMRU ....";
    for (uint32_t i=0; i<100; ++i) {
    uint32_t way = random() & (NUM_WAYS-1);
    rep2.touchMRU(way);
    sparta_assert( way == rep2.getMRUWay() );
    }
    std::cout << "PASSED" << std::endl;

    std::cout << "Testing touchLRU ....";
    for (uint32_t i=0; i<100; ++i) {
    uint32_t way = random() & (NUM_WAYS-1);
    rep2.touchMRU(way);
    sparta_assert( way == rep2.getMRUWay() );
    }
    std::cout << "PASSED" << std::endl;

}



void test2_replacement()
{
    uint32_t replaced_ways=0;
    uint32_t NUM_WAYS=8;
    sparta::cache::TreePLRUReplacement rep(NUM_WAYS);

    for (uint32_t i=0; i<NUM_WAYS; i++)
    {
        replaced_ways  |= ( 1<<rep.getLRUWay() );
        rep.touchMRU( rep.getLRUWay() );
    }

    // After 8 MRU updates, we should cycle back to way0
    sparta_assert( rep.getLRUWay() == 0 );

    // Expect all 8 ways get replaced
    sparta_assert( replaced_ways  == 0xFF);
}


int main()
{
    test1_touchMRU_touchLRU();
    test2_replacement();

    std::cout << "TESTS PASSED" << std::endl;
    return 0;
}
