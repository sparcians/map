
#include "sparta/utils/FastList.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <list>
#include <chrono>


void testConst(const sparta::utils::FastList<int> & fl)
{
    size_t i = 9;
    for(auto val : fl) {
        EXPECT_EQUAL(val, i);
        --i;
    }
}


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

    const size_t num_elems = fl.max_size();
    for(size_t i = 0; i < num_elems; ++i) {
        fl.emplace_back(i);
    }
    EXPECT_THROW(fl.emplace_back(100));

    size_t i = 9;
    for(const auto val : fl) {
        EXPECT_EQUAL(val, i);
        --i;
    }
    testConst(fl);
}

#define PERF_TEST 100000000
template<class ListType>
void testListPerf()
{
    ListType fl(10);
    const int num_elems = 10;
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

    testFastList();

    auto start = std::chrono::system_clock::system_clock::now();
    testListPerf<sparta::utils::FastList<int>>();
    auto end = std::chrono::system_clock::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Raw time (seconds) fast list : " << dur / 1000000.0 << std::endl;

    start = std::chrono::system_clock::system_clock::now();
    testListPerf<std::list<int>>();
    end = std::chrono::system_clock::system_clock::now();
    dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Raw time (seconds) old list : " << dur / 1000000.0 << std::endl;

    // Done
    REPORT_ERROR;

    return ERROR_CODE;
}
