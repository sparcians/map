
#include <iostream>
#include <inttypes.h>
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/resources/Queue.hpp"
#include "sparta/report/Report.hpp"

#include <boost/timer/timer.hpp>
#include <vector>
#include <string>
#include <memory>
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
TEST_INIT

#define PIPEOUT_GEN

void testIteratorValidity();
void testIteratorValidity2();
void testPushClearAccess();
void testStatsOutput();
void testPopBack();
void testIteratorOperations();
void testDecrementWraparoundBug();

struct dummy_struct
{
    uint16_t int16_field;
    uint32_t int32_field;
    std::string s_field;

    dummy_struct(const uint16_t int16_field, const uint32_t int32_field, const std::string &s_field) : int16_field{int16_field},
                                                                                                       int32_field{int32_field},
                                                                                                       s_field{s_field} {}
};
using dummy_struct_ptr = sparta::SpartaSharedPointer<dummy_struct>;
sparta::SpartaSharedPointerAllocator<dummy_struct> dummy_struct_allocator(6, 3);

std::ostream &operator<<(std::ostream &os, const dummy_struct &obj)
{
    os << obj.int16_field << " " << obj.int32_field << obj.s_field << "\n";
    return os;
}

int main()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    sparta::StatisticSet queue10_stats(&rtn);

    sparta::Queue<double> queue10_untimed("queue10_untimed", 10, root_clk.get(), &queue10_stats);
    sparta::Queue<dummy_struct *> dummy_struct_queue("dummy_struct_queue", 3, root_clk.get(), &queue10_stats);
    sparta::Queue<dummy_struct> dummy_struct_queue_up("dummy_struct_queue_up", 3, root_clk.get(), &queue10_stats);
    sparta::Queue<dummy_struct_ptr> dummy_struct_queue_alloc("dummy_struct_queue_alloc", 5, root_clk.get(), &queue10_stats);

    rtn.setClock(root_clk.get());

#ifdef PIPEOUT_GEN
    queue10_untimed.enableCollection(&rtn);
#endif

    rtn.enterConfiguring();
    rtn.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("testPipe", 1000000, root_clk.get(), &rtn);
#endif

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&rtn);
#endif

    ////////////////////////////////////////////////////////////
    sched.run(1);

    // Test Queue with SpartaSharedPointerAllocator
    {
        dummy_struct_ptr dummy_1 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 1, 2, "ABC");
        dummy_struct_queue_alloc.push(std::move(dummy_1));
        dummy_struct_ptr dummy_2 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 2, 3, "DEF");
        dummy_struct_queue_alloc.push(std::move(dummy_2));
        dummy_struct_ptr dummy_3 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 3, 4, "GHI");
        dummy_struct_queue_alloc.push(std::move(dummy_3));
        dummy_struct_ptr dummy_4 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 4, 5, "JKL");
        dummy_struct_queue_alloc.push(std::move(dummy_4));
        dummy_struct_ptr dummy_5 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "MNO");
        dummy_struct_queue_alloc.push(std::move(dummy_5));
        dummy_struct_queue_alloc.pop();
        dummy_struct_queue_alloc.pop();
        dummy_struct_queue_alloc.pop();
        dummy_struct_ptr dummy_6 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ASD");
        dummy_struct_ptr dummy_7 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ZXC");
        dummy_struct_ptr dummy_8 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "RTY");
        dummy_struct_queue_alloc.push(std::move(dummy_6));
        dummy_struct_queue_alloc.push(std::move(dummy_7));
        dummy_struct_queue_alloc.push(std::move(dummy_8));
        dummy_struct_queue_alloc.pop_back();
        dummy_struct_queue_alloc.pop_back();
        dummy_struct_ptr dummy_9 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ASD");
        dummy_struct_ptr dummy_10 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ZXC");
        dummy_struct_queue_alloc.push(std::move(dummy_9));
        dummy_struct_queue_alloc.push(std::move(dummy_10));
        dummy_struct_queue_alloc.pop();
        dummy_struct_queue_alloc.pop_back();
        dummy_struct_queue_alloc.pop();
        dummy_struct_queue_alloc.pop_back();
        dummy_struct_ptr dummy_11 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ASD");
        dummy_struct_ptr dummy_12 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "ZXC");
        dummy_struct_ptr dummy_13 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "RTY");
        dummy_struct_ptr dummy_14 = sparta::allocate_sparta_shared_pointer<dummy_struct>(dummy_struct_allocator, 5, 6, "RTY");
        dummy_struct_queue_alloc.push(std::move(dummy_11));
        dummy_struct_queue_alloc.push(std::move(dummy_12));
        dummy_struct_queue_alloc.push(std::move(dummy_13));
        dummy_struct_queue_alloc.push(std::move(dummy_14));
        dummy_struct_queue_alloc.clear();
    }

    dummy_struct_queue.push(new dummy_struct{16, 314, "dummy struct 1"});
    EXPECT_TRUE(dummy_struct_queue.size() == 1);

    // Test perfect forwarding queue
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        dummy_struct_queue_up.push(std::move(dummy_1));
        EXPECT_TRUE(dummy_1.s_field.size() == 0);
        EXPECT_TRUE(dummy_struct_queue_up.back().int16_field == 1);
        EXPECT_TRUE(dummy_struct_queue_up.back().int32_field == 2);
        EXPECT_TRUE(dummy_struct_queue_up.back().s_field == "ABC");
        dummy_struct_queue_up.push(dummy_2);
        EXPECT_TRUE(dummy_2.s_field == "DEF");
        EXPECT_TRUE(dummy_struct_queue_up.back().int16_field == 3);
        EXPECT_TRUE(dummy_struct_queue_up.back().int32_field == 4);
        EXPECT_TRUE(dummy_struct_queue_up.back().s_field == "DEF");
        dummy_struct_queue_up.push(std::move(dummy_3));
        EXPECT_TRUE(dummy_3.s_field.size() == 0);
        EXPECT_TRUE(dummy_struct_queue_up.back().int16_field == 5);
        EXPECT_TRUE(dummy_struct_queue_up.back().int32_field == 6);
        EXPECT_TRUE(dummy_struct_queue_up.back().s_field == "GHI");
    }

    queue10_untimed.push(1234.5);
    EXPECT_TRUE(queue10_untimed.size() == 1);

    sched.run(1);

    EXPECT_TRUE(queue10_untimed.size() == 1);
    EXPECT_TRUE(dummy_struct_queue.size() == 1);

    EXPECT_EQUAL(queue10_untimed.front(), 1234.5);
    EXPECT_EQUAL(queue10_untimed.back(), 1234.5);

    for (uint32_t i = 0; i < 9; ++i)
    {
        const double val = 0.5 + i;
        queue10_untimed.push(val);
        EXPECT_EQUAL(queue10_untimed.back(), val);
    }

    dummy_struct_queue.push(new dummy_struct{32, 123, "dummy struct 2"});
    EXPECT_TRUE(dummy_struct_queue.size() == 2);
    dummy_struct_queue.push(new dummy_struct{64, 109934, "dummy struct 3"});
    EXPECT_TRUE(dummy_struct_queue.size() == 3);

    // Test pointer to member operator
    EXPECT_TRUE(dummy_struct_queue.read(0)->int16_field == 16);
    EXPECT_TRUE(dummy_struct_queue.read(1)->int16_field == 32);
    EXPECT_TRUE(dummy_struct_queue.read(2)->int16_field == 64);
    EXPECT_TRUE(dummy_struct_queue.read(0)->int32_field == 314);
    EXPECT_TRUE(dummy_struct_queue.read(1)->int32_field == 123);
    EXPECT_TRUE(dummy_struct_queue.read(2)->int32_field == 109934);
    EXPECT_TRUE(dummy_struct_queue.read(0)->s_field == "dummy struct 1");
    EXPECT_TRUE(dummy_struct_queue.read(1)->s_field == "dummy struct 2");
    EXPECT_TRUE(dummy_struct_queue.read(2)->s_field == "dummy struct 3");

    // Test dereference operator
    EXPECT_TRUE((*(dummy_struct_queue.read(0))).int16_field == 16);
    EXPECT_TRUE((*(dummy_struct_queue.read(1))).int16_field == 32);
    EXPECT_TRUE((*(dummy_struct_queue.read(2))).int16_field == 64);
    EXPECT_TRUE((*(dummy_struct_queue.read(0))).int32_field == 314);
    EXPECT_TRUE((*(dummy_struct_queue.read(1))).int32_field == 123);
    EXPECT_TRUE((*(dummy_struct_queue.read(2))).int32_field == 109934);
    EXPECT_TRUE((*(dummy_struct_queue.read(0))).s_field == "dummy struct 1");
    EXPECT_TRUE((*(dummy_struct_queue.read(1))).s_field == "dummy struct 2");
    EXPECT_TRUE((*(dummy_struct_queue.read(2))).s_field == "dummy struct 3");

    delete dummy_struct_queue.read(0);
    delete dummy_struct_queue.read(1);
    delete dummy_struct_queue.read(2);

    sparta::Queue<double>::iterator queue10_untimes_iter = queue10_untimed.begin();

    EXPECT_EQUAL(*queue10_untimes_iter, 1234.5);
    queue10_untimes_iter++;

    uint32_t i = 0;
    for (; queue10_untimes_iter < queue10_untimed.end(); queue10_untimes_iter++)
    {
        EXPECT_EQUAL(*queue10_untimes_iter, i + 0.5);
        i++;
    }
    queue10_untimes_iter = queue10_untimed.begin();
    EXPECT_NOTHROW(
            *queue10_untimes_iter = 1234.51;
            EXPECT_EQUAL(*queue10_untimes_iter, 1234.51);
            *queue10_untimes_iter = 1234.5;);

    sparta::Queue<double>::const_iterator queue10_untimed_const_iter = queue10_untimed.begin();
    EXPECT_EQUAL(*queue10_untimed_const_iter, 1234.5);
    queue10_untimed_const_iter++;

    i = 0;
    for (; queue10_untimed_const_iter < queue10_untimed.end(); queue10_untimed_const_iter++)
    {
        EXPECT_EQUAL(*queue10_untimed_const_iter, i + 0.5);
        i++;
    }
    queue10_untimed_const_iter = queue10_untimed.begin();
#if 0
    try{
        *queue10_untimed_const_iter  = 1234.51;
        sparta_assert(EXPECT_EQUAL(*queue10_untimed_const_iter, 1234.51), "Const Iterator just modified the data!!");
    }
    catch(...){

    }
#endif

    EXPECT_EQUAL(queue10_untimed.size(), 10);
    sched.run(1);
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    uint32_t half = queue10_untimed.size() / 2;
    for (uint32_t i = 0; i < half; ++i)
    {
        queue10_untimed.pop();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 5);
    sched.run(1);

    while (queue10_untimed.size() != 0)
    {
        queue10_untimed.pop();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 0);
    sched.run(1);
    EXPECT_EQUAL(queue10_untimed.size(), 0);

    // Test clear()
    for (uint32_t i = 0; i < queue10_untimed.capacity(); ++i)
    {
        queue10_untimed.push(i);
        EXPECT_EQUAL(queue10_untimed.back(), i);
        EXPECT_EQUAL(queue10_untimed.front(), 0);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    queue10_untimed.clear();

    // Do it again.
    for (uint32_t i = 0; i < queue10_untimed.capacity(); ++i)
    {
        queue10_untimed.push(i);
        EXPECT_EQUAL(queue10_untimed.back(), i);
        EXPECT_EQUAL(queue10_untimed.front(), 0);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);
    auto bit = queue10_untimed.begin();

    EXPECT_EQUAL(queue10_untimed.read(0), 0);
    EXPECT_EQUAL(queue10_untimed.access(0), 0);

    queue10_untimed.clear();

    EXPECT_EQUAL(queue10_untimed.size(), 0);
    EXPECT_TRUE(queue10_untimed.begin() == queue10_untimed.end());
    EXPECT_FALSE(bit.isValid());
    EXPECT_NOTHROW(bit++);
    EXPECT_FALSE(bit.isValid());
    EXPECT_THROW(*bit);

    for (uint32_t i = 0; i < queue10_untimed.capacity(); ++i)
    {
        queue10_untimed.push(i);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    auto eit = queue10_untimed.end();
    EXPECT_NOTHROW(eit--);
    EXPECT_EQUAL(*eit, 9);

    // Test pop_back(), oldest (front -> 0,1,2,3,4,5,6,7,8,9 <- newest (back)
    for (uint32_t i = (queue10_untimed.capacity() - 1); i != 0; --i)
    {
        EXPECT_EQUAL(queue10_untimed.back(), i);
        queue10_untimed.pop_back();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 1);
    EXPECT_EQUAL(queue10_untimed.front(), 0);
    EXPECT_EQUAL(queue10_untimed.back(), 0);

    queue10_untimed.pop_back();
    EXPECT_EQUAL(queue10_untimed.size(), 0);

    for (uint32_t i = 0; i < queue10_untimed.capacity(); ++i)
    {
        queue10_untimed.push(i);
    }
    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;

    for (uint32_t i = (queue10_untimed.capacity() / 2); i != 0; --i)
    {
        queue10_untimed.pop_back();
    }

    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;
    for (uint32_t i = 0; i < queue10_untimed.capacity() / 2; ++i)
    {
        queue10_untimed.push(i + 5);
    }

    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;

    auto it = std::begin(queue10_untimed);
    EXPECT_EQUAL(*it, 0);
    ++it;
    EXPECT_EQUAL(*it, 1);
    ++it;
    EXPECT_EQUAL(*it, 2);
    ++it;
    EXPECT_EQUAL(*it, 3);
    ++it;
    EXPECT_EQUAL(*it, 4);
    ++it;
    EXPECT_EQUAL(*it, 5);
    ++it;
    EXPECT_EQUAL(*it, 6);
    ++it;
    EXPECT_EQUAL(*it, 7);
    ++it;
    EXPECT_EQUAL(*it, 8);
    ++it;
    EXPECT_EQUAL(*it, 9);

    ////////////////////////////////////////////////////////////////////////////////
    // Test dead iterators
    sparta::Queue<dummy_struct>::iterator dead_it;
    EXPECT_FALSE(dead_it.isValid());
    EXPECT_THROW(*dead_it);
    EXPECT_THROW(dead_it++);
    EXPECT_THROW(++dead_it);
    EXPECT_THROW((void)dead_it->s_field);
    EXPECT_THROW(dead_it.getIndex());

    EXPECT_TRUE(dummy_struct_queue_up.size() > 0);
    dead_it = dummy_struct_queue_up.begin();
    EXPECT_TRUE(dead_it.isValid());
    EXPECT_NOTHROW(*dead_it);
    EXPECT_NOTHROW(dead_it++);
    EXPECT_NOTHROW(++dead_it);
    EXPECT_NOTHROW((void)dead_it->s_field);
    EXPECT_NOTHROW(dead_it.getIndex());

    testIteratorValidity();
    testIteratorValidity2();
    testPushClearAccess();
    testStatsOutput();
    testPopBack();
    testIteratorOperations();
    testDecrementWraparoundBug();

    rtn.enterTeardown();
#ifdef PIPEOUT_GEN
    pc.destroy();
#endif

    REPORT_ERROR;
    return ERROR_CODE;
}
void testIteratorValidity()
{
    sparta::Queue<uint32_t> queue_test("iterator_test", 6, nullptr);
    auto itr1 = queue_test.push(1);
    auto itr2 = queue_test.push(2);
    auto itr3 = queue_test.push(3);

    EXPECT_TRUE(itr1.isValid());
    EXPECT_TRUE(itr2.isValid());
    EXPECT_TRUE(itr3.isValid());

    queue_test.pop();

    EXPECT_FALSE(itr1.isValid());
    EXPECT_TRUE(itr2.isValid());
    EXPECT_TRUE(itr3.isValid());

    auto itr4 = queue_test.push(4);
    EXPECT_FALSE(itr1.isValid());
    EXPECT_TRUE(itr2.isValid());
    EXPECT_TRUE(itr3.isValid());
    EXPECT_TRUE(itr4.isValid());

    EXPECT_EQUAL(queue_test.access(itr4.getIndex()), 4);

    auto itr5 = queue_test.push(5);
    EXPECT_FALSE(itr1.isValid());
    EXPECT_TRUE(itr2.isValid());
    EXPECT_TRUE(itr3.isValid());
    EXPECT_TRUE(itr4.isValid());

    auto itr6 = queue_test.push(6);
    auto itr7 = queue_test.push(7);
    EXPECT_FALSE(itr1.isValid());
    EXPECT_TRUE(itr2.isValid());
    EXPECT_TRUE(itr3.isValid());
    EXPECT_TRUE(itr4.isValid());
    EXPECT_TRUE(itr5.isValid());
    EXPECT_TRUE(itr6.isValid());
    EXPECT_TRUE(itr7.isValid());

    queue_test.pop();
    queue_test.pop();
    queue_test.pop();
    EXPECT_FALSE(itr1.isValid());
    EXPECT_FALSE(itr2.isValid());
    EXPECT_FALSE(itr3.isValid());
    EXPECT_FALSE(itr4.isValid());
    EXPECT_TRUE(itr5.isValid());
    EXPECT_TRUE(itr6.isValid());
    EXPECT_TRUE(itr7.isValid());

    EXPECT_EQUAL(queue_test.access(itr5.getIndex()), 5);
    EXPECT_EQUAL(queue_test.access(itr6.getIndex()), 6);
    EXPECT_EQUAL(queue_test.access(itr7.getIndex()), 7);

    auto itr8 = queue_test.push(8);
    auto itr9 = queue_test.push(9);
    auto itr10 = queue_test.push(10);

    EXPECT_EQUAL(queue_test.size(), 6);

    EXPECT_FALSE(itr1.isValid());
    EXPECT_FALSE(itr2.isValid());
    EXPECT_FALSE(itr3.isValid());
    EXPECT_FALSE(itr4.isValid());
    EXPECT_TRUE(itr5.isValid());
    EXPECT_TRUE(itr6.isValid());
    EXPECT_TRUE(itr7.isValid());
    EXPECT_TRUE(itr8.isValid());
    EXPECT_TRUE(itr9.isValid());
    EXPECT_TRUE(itr10.isValid());

    EXPECT_EQUAL(queue_test.access(itr5.getIndex()), 5);
    EXPECT_EQUAL(queue_test.access(itr6.getIndex()), 6);
    EXPECT_EQUAL(queue_test.access(itr7.getIndex()), 7);
    EXPECT_EQUAL(queue_test.access(itr8.getIndex()), 8);
    EXPECT_EQUAL(queue_test.access(itr9.getIndex()), 9);
    EXPECT_EQUAL(queue_test.access(itr10.getIndex()), 10);

    queue_test.pop();
    queue_test.pop();
    queue_test.pop();
    auto itr11 = queue_test.push(11);
    auto itr12 = queue_test.push(12);
    auto itr13 = queue_test.push(13);

    EXPECT_FALSE(itr1.isValid());
    EXPECT_FALSE(itr2.isValid());
    EXPECT_FALSE(itr3.isValid());
    EXPECT_FALSE(itr4.isValid());
    EXPECT_FALSE(itr5.isValid());
    EXPECT_FALSE(itr6.isValid());
    EXPECT_FALSE(itr7.isValid());
    EXPECT_TRUE(itr8.isValid());
    EXPECT_TRUE(itr9.isValid());
    EXPECT_TRUE(itr10.isValid());
    EXPECT_TRUE(itr11.isValid());
    EXPECT_TRUE(itr12.isValid());
    EXPECT_TRUE(itr13.isValid());

    EXPECT_EQUAL(queue_test.access(itr8.getIndex()), 8);
    EXPECT_EQUAL(queue_test.access(itr9.getIndex()), 9);
    EXPECT_EQUAL(queue_test.access(itr10.getIndex()), 10);
    EXPECT_EQUAL(queue_test.access(itr11.getIndex()), 11);
    EXPECT_EQUAL(queue_test.access(itr12.getIndex()), 12);
    EXPECT_EQUAL(queue_test.access(itr13.getIndex()), 13);

    queue_test.clear();

    // Force the queue to wrap around
    for(uint32_t i = 0; i < 13; ++i) {
        queue_test.push(i);
        queue_test.pop();
    }

    auto itr100 = queue_test.push(100);
    auto itr101 = queue_test.push(101);
    auto itr102 = queue_test.push(102);
    auto itr103 = queue_test.push(103);
    auto itr104 = queue_test.push(104);
    auto itr105 = queue_test.push(105);
    EXPECT_TRUE(itr100.isValid());
    EXPECT_TRUE(itr101.isValid());
    EXPECT_TRUE(itr102.isValid());
    EXPECT_TRUE(itr103.isValid());
    EXPECT_TRUE(itr104.isValid());
    EXPECT_TRUE(itr105.isValid());
    EXPECT_EQUAL(queue_test.access(itr100.getIndex()), 100);
    EXPECT_EQUAL(queue_test.access(itr101.getIndex()), 101);
    EXPECT_EQUAL(queue_test.access(itr102.getIndex()), 102);
    EXPECT_EQUAL(queue_test.access(itr103.getIndex()), 103);
    EXPECT_EQUAL(queue_test.access(itr104.getIndex()), 104);
    EXPECT_EQUAL(queue_test.access(itr105.getIndex()), 105);
}


void testIteratorValidity2()
{
    sparta::Queue<uint32_t> queue_test("iterator_test", 16, nullptr);
    for(uint32_t i = 0; i < queue_test.capacity(); ++i)
    {
        queue_test.push(i);
    }
    auto itr = queue_test.begin();

    for(uint32_t i = 0; i < queue_test.capacity(); ++i)
    {
        EXPECT_TRUE(itr.isValid());
        (void)itr.getIndex();
        ++itr;
    }
}


void testPushClearAccess()
{
    sparta::Queue<uint32_t> queue_test("push_clear_test", 6, nullptr);
    for(uint32_t i = 0; i < queue_test.capacity(); ++i) {
        queue_test.push(i);
    }
    uint32_t access = queue_test.access(0);
    EXPECT_EQUAL(access, 0);
    queue_test.pop();
    access = queue_test.access(0);
    EXPECT_EQUAL(access, 1);
    queue_test.pop();
    access = queue_test.access(0);
    EXPECT_EQUAL(access, 2);

    // This will force a "wrap around" in the queue
    queue_test.push(10);
    access = queue_test.access(0);
    EXPECT_EQUAL(access, 2);

    access = queue_test.access(queue_test.size() - 1);
    EXPECT_EQUAL(access, 10);
}

void testPopBack()
{
    sparta::Queue<uint32_t> pop_backer("pop_back_test", 100, nullptr);
    std::vector<sparta::Queue<uint32_t>::iterator> iters;
    iters.reserve(pop_backer.capacity());
    EXPECT_EQUAL(iters.capacity(), pop_backer.capacity());

    for(uint32_t i = 0; i < pop_backer.capacity(); ++i) {
        iters.emplace_back(pop_backer.push(i));
    }
    EXPECT_EQUAL(pop_backer.size(), pop_backer.capacity());
    uint32_t i = 0;
    for(auto & itr : iters) {
        EXPECT_EQUAL(*itr, i);
        ++i;
    }

    const uint32_t invalidate_count = 10;
    std::vector<sparta::Queue<uint32_t>::iterator> invalid_iters;
    invalid_iters.reserve(invalidate_count);

    // Pop 99 -> 89
    for(uint32_t i = 0; i < invalidate_count; ++i) {
        invalid_iters.emplace_back(iters.back());
        pop_backer.pop_back();
        iters.pop_back();
    }

    EXPECT_EQUAL(pop_backer.size(), 90);
    EXPECT_EQUAL(iters.size(), 90);
    EXPECT_EQUAL(pop_backer.back(), 89);

    i = 0;
    for(auto & itr : iters) {
        if(!itr.isValid()) {
            EXPECT_TRUE(itr.isValid());
            std::cout << "Error: " << i << " is not valid" << std::endl;
        }
        ++i;
    }

    // These iterators were cut from the queue.  They should be invalidated
    for(auto & itr : invalid_iters) {
        EXPECT_FALSE(itr.isValid());
    }

    const uint32_t sz = pop_backer.size();
    for(uint32_t i = 0; i < sz; ++i) {
        pop_backer.pop_back();
    }

    EXPECT_THROW(pop_backer.pop_back());

    for(auto & itr : iters) {
        EXPECT_TRUE(!itr.isValid());
    }

    // Rebuild the queue
    for(uint32_t i = 0; i < pop_backer.capacity(); ++i) {
        pop_backer.push(i);
    }

    // The iterators should still remain invalid -- they are old
    for(auto & itr : iters) {
        EXPECT_TRUE(!itr.isValid());
    }

    // for(uint32_t i = 0; i < pop_backer.size(); ++i) {
    //     std::cout << pop_backer.access(i) << std::endl;
    // }

}

void testStatsOutput()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet stats(&rtn);
    sparta::Queue<uint32_t> b("buf_const_test", 10,
                              root_clk.get(),
                              &stats);
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

    std::cout << r1 << std::endl;

    rtn.enterTeardown();
}

void testIteratorOperations()
{
    sparta::Queue<uint32_t> queue_test("iterator_test", 5, nullptr);

    // Fill the queue
    for(uint32_t i = 0; i < 5; ++i) {
        queue_test.push(i);
    }

    // Test basic increment/decrement on valid iterators
    auto it = queue_test.begin();
    EXPECT_TRUE(it.isValid());
    EXPECT_EQUAL(*it, 0);

    // Increment should work
    ++it;
    EXPECT_TRUE(it.isValid());
    EXPECT_EQUAL(*it, 1);

    // Decrement should work
    --it;
    EXPECT_TRUE(it.isValid());
    EXPECT_EQUAL(*it, 0);

    // Test decrement from end() - should work
    auto end_it = queue_test.end();
    EXPECT_FALSE(end_it.isValid()); // end() is not valid, but can be decremented

    --end_it; // This should work and go to last element
    EXPECT_TRUE(end_it.isValid());
    EXPECT_EQUAL(*end_it, 4);

    // Test increment from end() - should throw
    auto end_it2 = queue_test.end();
    EXPECT_THROW(++end_it2); // Should throw when trying to increment end()

    // Test decrement from beginning - should throw
    auto begin_it = queue_test.begin();
    EXPECT_TRUE(begin_it.isValid());
    EXPECT_EQUAL(*begin_it, 0);

    EXPECT_THROW(--begin_it); // Should throw (can't go before first element)

    // Test increment from beginning - should work
    auto begin_it2 = queue_test.begin();
    ++begin_it2;
    EXPECT_TRUE(begin_it2.isValid());
    EXPECT_EQUAL(*begin_it2, 1);

    // Test increment from last element should go to end()
    auto last_it = queue_test.begin();
    ++last_it; ++last_it; ++last_it; ++last_it; // Go to last element
    EXPECT_TRUE(last_it.isValid());
    EXPECT_EQUAL(*last_it, 4);

    ++last_it; // Should go to end()
    EXPECT_FALSE(last_it.isValid());
    EXPECT_TRUE(last_it == queue_test.end()); // Should compare equal to end()

    // Test decrement from end() should go back to last element
    --last_it;
    EXPECT_TRUE(last_it.isValid());
    EXPECT_EQUAL(*last_it, 4);

    // Test both should fail on detached iterators
    sparta::Queue<uint32_t>::iterator detached_it;
    EXPECT_FALSE(detached_it.isValid());

    // Both increment and decrement should throw
    EXPECT_THROW(++detached_it);
    EXPECT_THROW(--detached_it);
}

void testDecrementWraparoundBug()
{
    // Test decrementing from iterator with physical_index_ = 0 after wraparound
    // This should work correctly with the proper fix
    sparta::Queue<uint32_t> queue_test("wraparound_test", 2, nullptr);  // Size 2 -> physical size 4

    // Fill to capacity (2 elements)
    queue_test.push(100);  // physical index 0, logical index 0
    queue_test.push(200);  // physical index 1, logical index 1

    // Pop all elements to move head to physical index 2
    queue_test.pop();  // head now at physical index 1
    queue_test.pop();  // head now at physical index 2

    // Push new elements to fill the queue again
    queue_test.push(300);  // physical index 2, logical index 0
    queue_test.push(400);  // physical index 3, logical index 1

    // Pop elements to move head forward
    queue_test.pop();  // head now at physical index 3

    // Push new element - this will wrap around to physical index 0
    auto it = queue_test.push(500);

    // Verify the iterator is at logical index 1 (second position)
    // but at physical index 0 due to wraparound
    EXPECT_EQUAL(it.getIndex(), 1);
    EXPECT_EQUAL(*it, 500);

    // Decrementing from physical index 0 should work correctly
    --it;
    EXPECT_TRUE(it.isValid());
    EXPECT_EQUAL(*it, 400);
    EXPECT_EQUAL(it.getIndex(), 0);
}
