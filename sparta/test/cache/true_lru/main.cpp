

#include <cstdio>
#include <iostream>
#include "cache/TrueLRU_4_Replacement.hpp"
#include "cache/TrueLRUReplacement.hpp"


void test1_touchMRU_touchLRU()
{
    sparta::cache::TrueLRUReplacement rep1(4);
    sparta::cache::TrueLRU_4_Replacement rep2;

    rep1.touchMRU(0);
    rep1.touchMRU(1);
    rep1.touchMRU(2);
    rep1.touchMRU(3);

    rep2.touchMRU(0);
    rep2.touchMRU(1);
    rep2.touchMRU(2);
    rep2.touchMRU(3);

    std::cout << "Testing touch MRU" << std::endl;
    for (uint32_t i=0; i<50; i++) {
        uint32_t way = random() & 0x3;
        std::cout << "  " << way ;
        rep1.touchMRU(way);
        rep2.touchMRU(way);
        if ( rep1.getMRUWay() != rep2.getMRUWay() ) {
            std::cout << "   Error:  rep1.getMRUWay=" << rep1.getMRUWay() << " while rep2.getMRUWay=" << rep2.getMRUWay() << std::endl;
            sparta_assert(0);
        }
        if ( rep1.getLRUWay() != rep2.getLRUWay() ) {
            std::cout << "   Error:  rep1.getLRUWay=" << rep1.getLRUWay() << " while rep2.getLRUWay=" << rep2.getLRUWay() << std::endl;
            sparta_assert(0);
        }
    }
    std::cout << std::endl;

    std::cout << "Testing touch LRU" << std::endl;
    for (uint32_t i=0; i<50; i++) {
        uint32_t way = random() & 0x3;
        std::cout << "  " << way ;
        rep1.touchLRU(way);
        rep2.touchLRU(way);
        if ( rep1.getMRUWay() != rep2.getMRUWay() ) {
            std::cout << "   Error:  rep1.getMRUWay=" << rep1.getMRUWay() << " while rep2.getMRUWay=" << rep2.getMRUWay() << std::endl;
            sparta_assert(0);
        }
        if ( rep1.getLRUWay() != rep2.getLRUWay() ) {
            std::cout << "   Error:  rep1.getLRUWay=" << rep1.getLRUWay() << " while rep2.getLRUWay=" << rep2.getLRUWay() << std::endl;
            sparta_assert(0);
        }
    }

    std::cout << std::endl;

}

void test2_replacement()
{
    uint32_t replaced_ways=0;
    uint32_t NUM_WAYS=8;
    sparta::cache::TrueLRUReplacement rep(NUM_WAYS);

    std::cout <<"Testing replacement . . . .";
    for (uint32_t i=0; i<NUM_WAYS; i++)
    {
        replaced_ways  |= ( 1<<rep.getLRUWay() );
        rep.touchMRU( rep.getLRUWay() );
    }

    // After 8 MRU updates, we should cycle back to way0
    std::cout << " LRU=" << rep.getLRUWay() << " replaced_ways=0x" << std::hex << replaced_ways << std::endl;
    sparta_assert( rep.getLRUWay() == 0 );

    // Expect all 8 ways get replaced
    sparta_assert( replaced_ways  == 0xFF);

    rep.reset();

    std::cout <<"Testing replacement after reset . . . .";
    for (uint32_t i=0; i<NUM_WAYS; i++)
    {
        replaced_ways  |= ( 1<<rep.getLRUWay() );
        rep.touchMRU( rep.getLRUWay() );
    }

    // After 8 MRU updates, we should cycle back to way0
    std::cout << " LRU=" << rep.getLRUWay() << " replaced_ways=0x" << std::hex << replaced_ways << std::endl;
    sparta_assert( rep.getLRUWay() == 0 );

    // Expect all 8 ways get replaced
    sparta_assert( replaced_ways  == 0xFF);

    std::cout << "TEST PASSED" << std::endl;
}

int main()
{
    test1_touchMRU_touchLRU();
    test2_replacement();

    std::cout << std::endl;
    std::cout << "TESTS PASSED" << std::endl;
    return 0;
}
