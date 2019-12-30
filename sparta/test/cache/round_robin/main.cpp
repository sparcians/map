#include <cstdio>
#include <iostream>
#include "cache/RoundRobinReplacement.hpp"



void test1_round_robin()
{
    sparta::cache::RoundRobinReplacement rep1(16);
    uint32_t last_lru_way = 0;

    std::cout << "Testing if LRU way is consistent in round-robin policy" << std::endl;
    for (uint32_t i=0; i<20; i++) {
        uint32_t new_lru = rep1.getLRUWay();

        if(new_lru != last_lru_way) {
            std::cout << " new_lru = " << new_lru << " is NOT the same as last_lru_way = " << last_lru_way << std::endl;
            sparta_assert(0);
        }

        last_lru_way = new_lru;
    }
    std::cout << "LRU Way consistency check passed " <<  std::endl;

    std::cout << "Testing touch LRU" << std::endl;
    last_lru_way = 0;
    bool first_time = true;
    for (uint32_t i=0; i<30; i++) {

        if(! first_time) {
            if(last_lru_way != rep1.getLRUWay()) {
                std::cout << " last_lru_way = " << last_lru_way <<
                    " does not match current LRU way = " << rep1.getLRUWay() << std::endl;
                sparta_assert(0);
            }
        } else {
            first_time = false;
        }

        uint32_t way = random() & 15;
        std::cout << "  " << way ;
        rep1.touchLRU(way);
        last_lru_way = way;
    }
    std::cout << std::endl << " touch LRU check passed " << std::endl;

    std::cout << "Testing round robin LRU" << std::endl;
    last_lru_way = 0;
    rep1.touchLRU(0);
    for (uint32_t i=0; i<100; i++) {
        uint32_t curr_lru = rep1.getLRUWay();
        if(curr_lru != last_lru_way) {
            std::cout << " curr_lru = " << curr_lru << " doesn't match last_lru_way % 16 = "
                      << (last_lru_way % 16) << std::endl;
            sparta_assert(0);

        }
        last_lru_way = (i + 1) % 16;
        rep1.touchLRU(last_lru_way);
    }

    std::cout << "Testing round robin MRU" << std::endl;
    uint32_t last_mru_way = 0;
    // this moves the RR pointer to way 1
    rep1.touchMRU(0);
    for (uint32_t i=0; i<100; i++) {
        uint32_t curr_mru = rep1.getMRUWay();
        if(curr_mru != last_mru_way) {
            std::cout << " curr_mru = " << curr_mru << " doesn't match last_mru_way % 16 = "
                      << (last_mru_way % 16) << std::endl;
            sparta_assert(0);

        }
        last_mru_way = random() & 15;
        rep1.touchMRU(last_mru_way);
    }

    std::cout << std::endl;

}


int main()
{
    test1_round_robin();

    std::cout << std::endl;
    std::cout << "TESTS PASSED" << std::endl;
    return 0;
}
