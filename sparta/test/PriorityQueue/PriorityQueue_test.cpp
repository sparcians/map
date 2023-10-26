
#include <cinttypes>
#include <iostream>

#include "sparta/resources/PriorityQueue.hpp"
#include "sparta/utils/SpartaTester.hpp"


void test_defafult_pq()
{
    sparta::PriorityQueue<uint32_t> pqueue;

    for(auto i : {1,3,2,5,6,4,8,7}) {
        pqueue.insert(i);
    }

    EXPECT_EQUAL(pqueue.size(), 8);

    EXPECT_EQUAL(pqueue.top(), 1);
    pqueue.pop();

    EXPECT_FALSE(pqueue.empty());

    EXPECT_EQUAL(pqueue.top(), 2);

    pqueue.insert(100);

    EXPECT_EQUAL(pqueue.top(), 2);

    pqueue.remove(5);
    pqueue.pop(); // 2
    pqueue.pop(); // 3
    pqueue.pop(); // 4
    EXPECT_EQUAL(pqueue.top(), 6);

    pqueue.forceFront(500);
    EXPECT_EQUAL(pqueue.top(), 500);
    pqueue.pop(); // 500

    EXPECT_EQUAL(pqueue.top(), 6);

    while(!pqueue.empty()) {
        pqueue.pop();
    }

    EXPECT_THROW(pqueue.pop());

    pqueue.insert(100);
    EXPECT_EQUAL(pqueue.top(), 100);

    pqueue.clear();
    EXPECT_THROW(pqueue.top());
    EXPECT_THROW(pqueue.back());
    EXPECT_THROW(pqueue.pop());

    // Removing from an empty queue does nothing
    EXPECT_NOTHROW(pqueue.remove(10));

    pqueue.insert(100);
    EXPECT_EQUAL(pqueue.top(), 100);
    EXPECT_EQUAL(pqueue.size(), 1);

    auto it = pqueue.begin();
    EXPECT_EQUAL(*it, 100);
    pqueue.erase(it);
    EXPECT_EQUAL(pqueue.size(), 0);

    pqueue.insert(100);
    EXPECT_EQUAL(pqueue.top(), 100);
    EXPECT_EQUAL(pqueue.size(),  1);

    const auto pqueue_const_ptr = static_cast<const sparta::PriorityQueue<uint32_t> *>(&pqueue);
    static_assert(std::is_const_v<decltype(pqueue_const_ptr)>, "Why not const?");

    auto cit = pqueue_const_ptr->begin();
    static_assert(std::is_same_v<decltype(cit), typename sparta::PriorityQueue<uint32_t>::const_iterator >,
                  "Why iterator not const?");
    pqueue.erase(cit);
    EXPECT_EQUAL(pqueue.size(), 0);

}

class DynamicSorter
{
public:

    DynamicSorter(DynamicSorter * sorter) :
        dyn_sorter_(sorter)
    {}

    bool operator()(const int32_t existing, const int32_t to_be_inserted) const
    {
        return dyn_sorter_->choose(existing, to_be_inserted);
    }

    bool choose(const int32_t existing, const int32_t to_be_inserted) {
        if(smaller_first_) {
            return existing > to_be_inserted;
        }
        else {
            return existing < to_be_inserted;
        }
    }

    void toggleSmallerFirst() {
        smaller_first_ = !smaller_first_;
    }

private:
    DynamicSorter * dyn_sorter_ = this;
    bool smaller_first_ = false;
};

void test_custom_order_pq()
{
    DynamicSorter dyn_sorter(nullptr);

    sparta::PriorityQueue<int32_t, DynamicSorter> pqueue({DynamicSorter(&dyn_sorter)});

    for(auto i : {1,3,2,-5,6,4,-8,7,-3,8,5,-7}) {
        pqueue.insert(i);
    }

    // for(auto i : pqueue) {
    //     std::cout << i << std::endl;
    // }

    EXPECT_EQUAL(pqueue.top(), -8);
    pqueue.pop();
    EXPECT_EQUAL(pqueue.top(), -7);

    pqueue.insert(10);
    EXPECT_EQUAL(pqueue.top(), -7);

    dyn_sorter.toggleSmallerFirst();

    pqueue.insert(11);
    EXPECT_EQUAL(pqueue.top(), 11);

    // for(auto i : pqueue) {
    //     std::cout << i << std::endl;
    // }
}

int main()
{
    test_defafult_pq();
    test_custom_order_pq();

    REPORT_ERROR;
    return ERROR_CODE;
}
