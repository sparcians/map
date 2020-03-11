// -*- C++ -*-


#include <iostream>
#include <inttypes.h>

#include "sparta/resources/Buffer.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/report/Report.hpp"

#include <boost/timer/timer.hpp>
#include <vector>
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/CycleCounter.hpp"
TEST_INIT;

#define PIPEOUT_GEN

#define QUICK_PRINT(x) \
    std::cout << x << std::endl

void testConstIterator();

struct dummy_struct
{
    uint16_t int16_field;
    uint32_t int32_field;
    std::string s_field;

    dummy_struct() = default;
    dummy_struct(const uint16_t int16_field, const uint32_t int32_field, const std::string &s_field) : 
        int16_field{int16_field},
        int32_field{int32_field},
        s_field{s_field} {}
};
std::ostream &operator<<(std::ostream &os, const dummy_struct &obj)
{
    os << obj.int16_field << " " << obj.int32_field << obj.s_field << "\n";
    return os;
}

int main()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler sched;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    sparta::StatisticSet buf10_stats(&rtn);

    sparta::Buffer<double> buf10("buf10_test", 10,
                               root_clk.get(),
                               &buf10_stats);

    sparta::Buffer<double> buf_inf("buf_inf_test", 1,
                               root_clk.get(),
                               &buf10_stats);

    sparta::Buffer<dummy_struct> buf_dummy("buf_pf_test", 4,
                               root_clk.get(),
                               &buf10_stats);

    rtn.setClock(root_clk.get());
#ifdef PIPEOUT_GEN
    buf10.enableCollection(&rtn);
#endif

    rtn.enterConfiguring();
    rtn.enterFinalized();

    // Get info messages from the scheduler node and send them to this file
    sparta::log::Tap t2(root_clk.get()->getScheduler(), "debug", "scheduler.log.debug");

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("testBuffer", 1000000,
                                           root_clk.get(), &rtn);
#endif

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&rtn);
#endif

    ////////////////////////////////////////////////////////////
    sched.run(1);

    // Test perfect forwarding Buffer move
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        buf_dummy.push_back(std::move(dummy_1));
        EXPECT_TRUE(dummy_1.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "ABC");
        buf_dummy.insert(0, std::move(dummy_2));
        EXPECT_TRUE(dummy_2.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "DEF");
        auto itr = buf_dummy.begin();
        buf_dummy.insert(itr, std::move(dummy_3));
        EXPECT_TRUE(dummy_3.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "GHI");
        auto ritr = buf_dummy.rbegin();
        buf_dummy.insert(++ritr, std::move(dummy_4));
        EXPECT_TRUE(dummy_4.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy.read(2).s_field == "JKL");
    }

    // Test perfect forwarding Buffer copy
    {
        buf_dummy.clear();
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        buf_dummy.push_back(dummy_1);
        EXPECT_TRUE(dummy_1.int16_field == 1);
        EXPECT_TRUE(dummy_1.int32_field == 2);
        EXPECT_TRUE(dummy_1.s_field == "ABC");
        EXPECT_TRUE(buf_dummy.read(0).int16_field == 1);
        EXPECT_TRUE(buf_dummy.read(0).int32_field == 2);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "ABC");
        buf_dummy.insert(0, dummy_2);
        EXPECT_TRUE(dummy_2.int16_field == 3);
        EXPECT_TRUE(dummy_2.int32_field == 4);
        EXPECT_TRUE(dummy_2.s_field == "DEF");
        EXPECT_TRUE(buf_dummy.read(0).int16_field == 3);
        EXPECT_TRUE(buf_dummy.read(0).int32_field == 4);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "DEF");
        auto itr = buf_dummy.begin();
        buf_dummy.insert(itr, dummy_3);
        EXPECT_TRUE(dummy_3.int16_field == 5);
        EXPECT_TRUE(dummy_3.int32_field == 6);
        EXPECT_TRUE(dummy_3.s_field == "GHI");
        EXPECT_TRUE(buf_dummy.read(0).int16_field == 5);
        EXPECT_TRUE(buf_dummy.read(0).int32_field == 6);
        EXPECT_TRUE(buf_dummy.read(0).s_field == "GHI");
        auto ritr = buf_dummy.rbegin();
        buf_dummy.insert(++ritr, dummy_4);
        EXPECT_TRUE(dummy_4.int16_field == 7);
        EXPECT_TRUE(dummy_4.int32_field == 8);
        EXPECT_TRUE(dummy_4.s_field == "JKL");
        EXPECT_TRUE(buf_dummy.read(2).int16_field == 7);
        EXPECT_TRUE(buf_dummy.read(2).int32_field == 8);
        EXPECT_TRUE(buf_dummy.read(2).s_field == "JKL");
    }

    // Test an empty buffer
    uint32_t i = 0;
    for(sparta::Buffer<double>::iterator buf10_iter = buf10.begin();
        buf10_iter != buf10.end(); buf10_iter++)
    {
        i++;
    }

    // Testing the Infinite Buffer in this scope.
    {
        // Insert the only value this Finite Buffer can hold.
        EXPECT_NOTHROW(buf_inf.push_back(0));

        // Normal Finite Buffer should throw on any subsequent push_back.
        for(size_t i = 1; i < 10000; ++i) {
            EXPECT_THROW(buf_inf.push_back(i));
        }

        // Make Buffer Infinite with a resize factor of 3, which
        // means, the underlying vector should resize itself to
        // hold atleast three additional entries.
        buf_inf.makeInfinite(3);

        // As long as there is enough contigous memory in free store,
        // any sort of insertion is acceptable. Users would get a
        // std::bad_alloc exception thrown if free store cannot allocate
        // a continous chunk of memory of requested size.
        for(size_t i = 1; i < 10000; ++i) {
            EXPECT_NOTHROW(buf_inf.push_back(i));
        }

        // Verify all the values are correct.
        for(size_t i = 0; i < 10000; ++i) {
            EXPECT_EQUAL(buf_inf.read(i), i);
        }

        // Clear out the buffer.
        buf_inf.clear();
        EXPECT_EQUAL(buf_inf.size(), 0);

        // Use push_back method to push entries.
        for(size_t i = 0; i < 10; ++i) {
            EXPECT_NOTHROW(buf_inf.push_back(i));
        }

        // Use insert method with integral index.
        EXPECT_NOTHROW(buf_inf.insert(2, 17));
        EXPECT_NOTHROW(buf_inf.insert(10, 23));
        auto buf_inf_iter = buf_inf.begin();

        // Use insert method with iterators.
        EXPECT_NOTHROW(buf_inf.insert(buf_inf_iter, 18));
        auto buf_inf_iter_nx = std::next(buf_inf_iter, 5);
        EXPECT_NOTHROW(buf_inf.insert(buf_inf_iter_nx, 79));
        EXPECT_NOTHROW(buf_inf.push_back(51));
        std::vector<double> expected_res {18, 0, 1, 17, 2, 3, 79, 4, 5, 6, 7, 8, 23, 9, 51};
        for(size_t i = 0; i < buf_inf.size(); ++i) {
            EXPECT_EQUAL(buf_inf.read(i), expected_res[i]);
        }

        // Test erase method.
        buf_inf_iter = buf_inf.begin();
        // Erase zeroeth element.
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter));

        buf_inf_iter = buf_inf.begin();
        buf_inf_iter_nx = std::next(buf_inf_iter, 4);
        // Erase fourth element.
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        buf_inf_iter = buf_inf.begin();
        ++buf_inf_iter;
        // Erase first element.
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter));

        buf_inf_iter = buf_inf.begin();
        buf_inf_iter_nx = std::next(buf_inf_iter, 2);
        // Erase second element.
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        buf_inf_iter = buf_inf.begin();
        // Erase fourth element.
        buf_inf_iter_nx = std::next(buf_inf_iter, 4);
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        buf_inf_iter = buf_inf.begin();
        // Erase first element.
        buf_inf_iter_nx = std::next(buf_inf_iter, 1);
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        buf_inf_iter = buf_inf.begin();
        // Erase zeroeth element.
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter));

        buf_inf_iter = buf_inf.begin();
        // Erase sixth element.
        buf_inf_iter_nx = std::next(buf_inf_iter, 6);
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        buf_inf_iter = buf_inf.begin();
        // Erase fifth element.
        buf_inf_iter_nx = std::next(buf_inf_iter, 5);
        EXPECT_NOTHROW(buf_inf.erase(buf_inf_iter_nx));

        std::vector<double> expected_res_2 {79, 4, 6, 7, 8, 51};
        for(size_t i = 0; i < buf_inf.size(); ++i) {
            EXPECT_EQUAL(buf_inf.read(i), expected_res_2[i]);
        }
        std::cout << "\n";

        // Test iterator walk of buffer.
        std::size_t i {0};
        for(auto it = buf_inf.begin(); it != buf_inf.end(); ++it){
            EXPECT_EQUAL(*it, expected_res_2[i++]);
        }

        // Test rerverse iterator walk of buffer.
        i = 5;
        for(auto it = buf_inf.rbegin(); it != buf_inf.rend(); ++it){
            EXPECT_EQUAL(*it, expected_res_2[i--]);
        }

        // Test isValid().
        EXPECT_EQUAL(buf_inf.isValid(0), true);
        EXPECT_EQUAL(buf_inf.isValid(8), false);

        // Test accessBack().
        EXPECT_EQUAL(buf_inf.accessBack(), 51);

        // Test read() with iterator.
        auto itr = buf_inf.begin();
        EXPECT_EQUAL(*itr, buf_inf.read(itr));

        // Test rbegin() and accessBack().
        auto ritr = buf_inf.rbegin();
        EXPECT_EQUAL(*ritr, buf_inf.accessBack());

        // Clear out the buffer.
        buf_inf.clear();
        EXPECT_EQUAL(buf_inf.size(), 0);
    }

    EXPECT_EQUAL(i, 0);

    buf10.push_back(1234.5);
    EXPECT_TRUE(buf10.size() == 1);

    sched.run(1);

    EXPECT_TRUE(buf10.size() == 1);

    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(0.5 + i);
    }
    EXPECT_EQUAL(buf10.size(), 10);

    // Append one more
    //EXPECT_THROW_MSG_SHORT(buf10.push_back(1), "numFree() > 0");
    EXPECT_THROW(buf10.push_back(1));
    EXPECT_THROW(buf10.insert(0, 1));

    EXPECT_EQUAL(buf10.size(), 10);

    sched.run(1);
    EXPECT_EQUAL(buf10.size(), 10);

    sparta::Buffer<double>::iterator buf10_iter= buf10.begin();
    EXPECT_EQUAL(*buf10_iter, 1234.5);
    buf10_iter++;
    for(; buf10_iter < buf10.end();buf10_iter++){
        EXPECT_EQUAL(*buf10_iter, i + 0.5);
        i++;
    }

    buf10_iter = buf10.begin();
    EXPECT_NOTHROW(
                   *buf10_iter  = 1234.51;
                   EXPECT_EQUAL(*buf10_iter, 1234.51);
                   *buf10_iter  = 1234.5;
                   );

    sparta::Buffer<double>::const_iterator buf10_const_iter = buf10.begin();
    EXPECT_EQUAL(*buf10_const_iter, 1234.5);
    auto post_fix_iter = buf10_const_iter++;
    EXPECT_EQUAL(*post_fix_iter, 1234.5);


    i = 0;
    for(; buf10_const_iter < buf10.end();buf10_const_iter++){
        EXPECT_EQUAL(*buf10_const_iter, i + 0.5);
        i++;
    }
    buf10_const_iter = buf10.begin();


    uint32_t half = buf10.size()/2;
    for(uint32_t i = 0; i < half; ++i) {
        buf10.erase(0);
    }
    EXPECT_EQUAL(buf10.size(), 5);
    for(uint32_t i = 0; i < half; ++i) {
        EXPECT_EQUAL(buf10.read(i), 4.5 + i);
    }

    sched.run(1);

    buf10.erase(3);
    EXPECT_EQUAL(buf10.size(), 4);
    EXPECT_EQUAL(buf10.read(0), 4.5);
    EXPECT_EQUAL(buf10.read(1), 5.5);
    EXPECT_EQUAL(buf10.read(2), 6.5);
    EXPECT_EQUAL(buf10.read(3), 8.5);
    sched.run(1);

    while(buf10.size() != 0) {
        buf10.erase(0);
    }
    EXPECT_EQUAL(buf10.size(), 0);
    sched.run(1);
    EXPECT_EQUAL(buf10.size(), 0);

    //////////////////////////////////////////////////////////////////////
    // Test clearing
    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(1.5 + i);
    }
    EXPECT_EQUAL(buf10.size(), 9);
    sched.run(1);

    buf10.clear();
    EXPECT_EQUAL(buf10.size(), 0);
    sched.run(1);

    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(20.5 + i);
    }
    EXPECT_EQUAL(buf10.size(), 9);
    sched.run(1);


    //////////////////////////////////////////////////////////////////////
    // ITERATOR tests

    buf10.clear();
    EXPECT_EQUAL(buf10.size(), 0);
    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(20.5 + i);
    }
    buf10.push_back(1234.5);
    // Check to see if the iterator errors out when unconnected to a sparta::Buffer
    // Although this behavior is allowed in the
    sparta::Buffer<double>::iterator unconnected_itr;
    EXPECT_THROW_MSG_CONTAINS(--unconnected_itr, "attached_buffer_: The iterator is not attached to a buffer. Was it initialized?");
    EXPECT_THROW_MSG_CONTAINS(++unconnected_itr, "attached_buffer_: The iterator is not attached to a buffer. Was it initialized?");

    // Check for correct response when using the decrement operator
    auto itr = buf10.end();
    --itr;
    EXPECT_EQUAL(buf10.read(itr), 1234.5);

    itr = buf10.begin();
    EXPECT_THROW_MSG_CONTAINS(--itr, "Decrementing the iterator results in buffer underrun");

    itr = buf10.begin();
    ++itr;
    EXPECT_EQUAL(buf10.read(itr), 21.5);
    --itr;
    EXPECT_EQUAL(buf10.read(itr), 20.5);

    itr = buf10.end();
    --itr;
    EXPECT_EQUAL(buf10.read(itr), 1234.5);

    buf10.erase(9);
    buf10.erase(8);
    buf10.erase(7);
    EXPECT_EQUAL(buf10.size(), 7);
    EXPECT_THROW_MSG_SHORT(buf10.read(itr),"isValid(idx)");
    --itr; // should point to 6
    EXPECT_EQUAL(buf10.read(itr), 26.5);

    sched.run(1);

    // Check for correct response when using the increment operator
    buf10.clear();
    EXPECT_EQUAL(buf10.size(), 0);
    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(20.5 + i);
    }
    buf10.push_back(1234.5);

    EXPECT_EQUAL(buf10.accessBack(), 1234.5);
    EXPECT_EQUAL(buf10.access(9), 1234.5);


    itr = buf10.end();
    EXPECT_THROW_MSG_CONTAINS(++itr, "Incrementing the iterator to entry that is not valid");

    itr = buf10.end();
    --itr;
    EXPECT_EQUAL(buf10.read(itr), 1234.5);
    EXPECT_EQUAL(buf10.access(itr), 1234.5);
    ++itr;

    // Attempt range-based for loop
    // for(auto & it : buf10) {
    //     EXPECT_TRUE(it.isValid());
    // }


    sched.run(1);

    //////////////////////////////////////////////////////////////////////
    // REVERSE_ITERATOR tests

    buf10.clear();
    EXPECT_EQUAL(buf10.size(), 0);
    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(20.5 + i);
    }
    buf10.push_back(1234.5);

    // Check for correct response when using the increment operator
    auto ritr = buf10.rbegin();
    ++ritr;
    EXPECT_EQUAL(buf10.read(ritr), 1234.5);

    ritr = buf10.rend();
    EXPECT_THROW_MSG_CONTAINS(++ritr, "Decrementing the iterator results in buffer underrun");

    ritr = buf10.rend();
    --ritr;
    EXPECT_EQUAL(buf10.read(ritr), 21.5);
    ++ritr;
    EXPECT_EQUAL(buf10.read(ritr), 20.5);

    ritr = buf10.rbegin();
    EXPECT_EQUAL(*ritr, 1234.5);
    ++ritr;
    // This looks wrong...
    EXPECT_EQUAL(buf10.access(ritr), 1234.5);
    EXPECT_EQUAL(buf10.read(ritr), 1234.5);

    buf10.erase(9);
    buf10.erase(8);
    buf10.erase(7);
    EXPECT_EQUAL(buf10.size(), 7);
    EXPECT_THROW_MSG_SHORT(buf10.read(ritr),"isValid(idx)");
    ++ritr; // should point to 6
    EXPECT_EQUAL(buf10.read(ritr), 26.5);
    --ritr; // What should this do?

    sched.run(1);

    // Check for correct response when using the increment operator
    buf10.clear();
    EXPECT_EQUAL(buf10.size(), 0);
    for(uint32_t i = 0; i < 9; ++i) {
        buf10.push_back(20.5 + i);
    }
    buf10.push_back(1234.5);

    ritr = buf10.rbegin();
    EXPECT_THROW_MSG_CONTAINS(--ritr, "Incrementing the iterator to entry that is not valid");

    ritr = buf10.rbegin();
    ++ritr;
    EXPECT_EQUAL(buf10.read(ritr), 1234.5);
    --ritr;

    sched.run(5);

    testConstIterator();

    rtn.enterTeardown();
#ifdef PIPEOUT_GEN
    pc.destroy();
#endif

    REPORT_ERROR;
    return ERROR_CODE;
}


struct B {
    uint32_t val = 5;
};

std::ostream & operator<<(std::ostream & os, const B &) {
    return os;
}

class A
{
public:
    A(sparta::Buffer<B> & b) :
        it(b.begin()),
        cit(b.begin())
    {
    }

    void foo() const {
        std::cout << (*it).val << std::endl;
        std::cout << (*cit).val << std::endl;
    }

    void bar() {
        std::cout << (*it).val << std::endl;
        std::cout << (*cit).val << std::endl;
        (*it).val = 6;
    }

private:
    sparta::Buffer<B>::iterator it;
    sparta::Buffer<B>::const_iterator cit;

};

void testConstIterator()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet buf_stats(&rtn);
    sparta::Buffer<B> b("buf_const_test", 10,
                      root_clk.get(),
                      &buf_stats);
    const std::string report_def =
        R"(name: "String-based report Autopopulation Test"
style:
    decimal_places: 3
content:
    top:
        subreport:
            name: All stats
            style:
                collapsible_children: no
            content:
                autopopulate:
                    attributes: "!=vis:hidden && !=vis:summary"
                    max_report_depth: 1
        subreport:
            name: Hidden stats
            style:
                collapsible_children: no
            content:
                autopopulate:
                    attributes: "==vis:hidden"
                    max_report_depth: 1
        )";

    r1.setContext(rtn.getSearchScope());
    r1.addDefinitionString(report_def);

    rtn.enterConfiguring();
    rtn.enterFinalized();

    b.push_back(B());
    A a(b);
    a.foo();

    std::cout << r1 << std::endl;

    rtn.enterTeardown();
}
