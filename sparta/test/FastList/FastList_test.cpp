
#include "sparta/utils/FastList.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <list>
#include <chrono>

#define PERF_TEST 100000000
void testFastList()
{
    sparta::utils::FastList<int> fl(10);
    std::cout << fl;

    auto itr = fl.emplace_back(0);
    std::cout << "Added one: \n" << fl;
    EXPECT_TRUE(*itr == 0);

    auto itr1 = fl.emplace_back(1);
    std::cout << "Added another: \n" << fl;
    EXPECT_TRUE(*itr1 == 1);

    auto itr2 = fl.emplace_back(2);
    std::cout << "Added another: \n" << fl;
    EXPECT_TRUE(*itr2 == 2);
    EXPECT_TRUE(fl.size() == 3);
    EXPECT_TRUE(fl.max_size() == 10);

    fl.erase(itr1);
    std::cout << "Erased: \n" << fl;
    EXPECT_TRUE(fl.size() == 2);

    fl.erase(itr2);
    std::cout << "Erased: \n" << fl;
    EXPECT_TRUE(fl.size() == 1);

    fl.erase(itr);
    std::cout << "Erased: \n" << fl;
    EXPECT_TRUE(fl.size() == 0);

    //EXPECT_THROW(fl.erase(itr));

    const uint32_t cap = fl.max_size();
    for(int i = 0; i < PERF_TEST; ++i) {
        for(size_t i = 0; i < cap; ++i) {
            fl.emplace_back(i);
        }

        const auto end = fl.end();
        for(auto it = fl.begin(); it != end;) {
            fl.erase(it++);
        }
    }
}

void testNormalList()
{
    const int num_elems = 10;
    std::list<int> fl;
    for(int i = 0; i < PERF_TEST; ++i) {
        for(size_t i = 0; i < num_elems; ++i) {
            fl.emplace_back(i);
        }

        const auto end = fl.end();
        for(auto it = fl.begin(); it != end;) {
            fl.erase(it++);
        }
    }
}

int main()
{
    std::locale::global(std::locale(""));
    std::cout.imbue(std::locale());
    std::cout.precision(12);

    auto start = std::chrono::system_clock::system_clock::now();
    testFastList();
    auto end = std::chrono::system_clock::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Raw time (seconds) fast list : " << dur / 1000000.0 << std::endl;

    start = std::chrono::system_clock::system_clock::now();
    testNormalList();
    end = std::chrono::system_clock::system_clock::now();
    dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Raw time (seconds) old list : " << dur / 1000000.0 << std::endl;

    // Done
    REPORT_ERROR;

    return ERROR_CODE;
}
