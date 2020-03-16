
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <array>
#include <chrono>
#include <memory>

#ifdef __linux__
#include <ext/pool_allocator.h>
#endif

uint32_t my_type_deleted = 0;
class MyType
{
public:
    MyType() : a(10) {}
    MyType(uint32_t _a) : a(_a) {}

    ~MyType() {
        ++my_type_deleted;
    }

    uint32_t a;

};

class MyNonTrivialType
{
public:
    MyNonTrivialType(uint32_t _a) : a(_a) {}

    uint32_t a;
};

std::ostream& operator<<(std::ostream&os, const MyType & t) {
    os << t.a;
    return os;
}

std::ostream& operator<<(std::ostream&os, const sparta::SpartaSharedPointer<MyType> & t) {
    os << *t;
    return os;
}

sparta::SpartaSharedPointer<MyType>::SpartaSharedPointerAllocator           trivial_type_allocator(11000, 10000);
sparta::SpartaSharedPointer<MyNonTrivialType>::SpartaSharedPointerAllocator non_trivial_type_allocator(11000, 10000);


TEST_INIT;

void func(const sparta::SpartaSharedPointer<MyType> & my_ref, const unsigned int val)
{
    sparta::SpartaSharedPointer<MyType> another_ref(my_ref);
    sparta::SpartaSharedPointer<MyType> another_ref2;
    EXPECT_TRUE(another_ref2 == nullptr);
    another_ref2 = my_ref;
    EXPECT_TRUE(another_ref2 != nullptr);

    EXPECT_TRUE(another_ref->a == val);
    EXPECT_TRUE(another_ref2->a == val);
    another_ref2->a = val + 10;

    EXPECT_TRUE(another_ref2 == another_ref);
    EXPECT_FALSE(another_ref2 != another_ref);
}

void testBasicSpartaSharedPointer()
{
    sparta::SpartaSharedPointer<MyType> ptr(new MyType);
    MyType * t = new MyType;
    sparta::SpartaSharedPointer<MyType> ptr2(t);
    sparta::SpartaSharedPointer<MyType> ptr3;

    ptr3 = ptr;
    ptr->a = 5;
    EXPECT_TRUE(ptr3->a == 5);

    EXPECT_TRUE((*ptr).a == 5);

    ptr2 = ptr;
    EXPECT_TRUE(ptr2->a == 5);

    const sparta::SpartaSharedPointer<MyType> ptr4(ptr3);

    EXPECT_TRUE(ptr4->a == 5);
    EXPECT_TRUE((*ptr4).a == 5);

    sparta::SpartaSharedPointer<MyType> ptr5;
    EXPECT_TRUE(ptr5 == nullptr);

    MyType * t2 = new MyType;
    ptr5.reset(t2);
    EXPECT_FALSE(ptr5 == nullptr);
    EXPECT_TRUE(ptr5->a == 10);
    std::cout << "Printed ptr: " << ptr5 << std::endl;

    // modifies a += 10
    func(ptr5, ptr5->a);
    EXPECT_FALSE(ptr5 == nullptr);
    EXPECT_TRUE(ptr5->a == 20);

    sparta::SpartaSharedPointer<int> int_ptr;
    int_ptr.reset(new int(0));
    *int_ptr = 5;

    EXPECT_TRUE(*int_ptr == 5);


    sparta::SpartaSharedPointer<MyType> ptr6;
    {
        sparta::SpartaSharedPointer<MyType> ptr7(ptr6);
    }

}

void testMoveSupport()
{
    MyType * orig_type;
    sparta::SpartaSharedPointer<MyType> ptr(orig_type = new MyType);
    sparta::SpartaSharedPointer<MyType> ptr2(ptr);
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_EQUAL(ptr.use_count(), 2);
    EXPECT_TRUE(ptr.get() == orig_type);

    // Move construct with the first pointer.  The first pointer
    // should be null after the move
    sparta::SpartaSharedPointer<MyType> ptr3(std::move(ptr));

    EXPECT_THROW(ptr.use_count());
    EXPECT_TRUE(ptr3.get() == orig_type);
    EXPECT_EQUAL(ptr3.use_count(), 2);

    // Move assign with the third pointer.  The third pointer
    // should be null after the move
    MyType * untouched_ptr;
    sparta::SpartaSharedPointer<MyType> untouched(untouched_ptr = new MyType);
    sparta::SpartaSharedPointer<MyType> ptr4(untouched);
    EXPECT_EQUAL(ptr4.use_count(), 2);
    EXPECT_TRUE(ptr4.get() == untouched.get());
    EXPECT_TRUE(ptr4 == untouched);
    EXPECT_EQUAL(untouched.use_count(), 2);

    // now, move ptr3.  Ptr4 should leave untouched alone and nullify
    // ptr3.
    ptr4 = std::move(ptr3);

    EXPECT_THROW(ptr3.use_count());
    EXPECT_TRUE(ptr4.get() == orig_type);
    EXPECT_EQUAL(ptr4.use_count(), 2);

    EXPECT_EQUAL(untouched.use_count(), 1);
    EXPECT_EQUAL(untouched.get(), untouched_ptr);

    // Try moving bogus ones
    sparta::SpartaSharedPointer<MyType> ptr5 = nullptr;
    sparta::SpartaSharedPointer<MyType> ptr6(std::move(ptr5));

    sparta::SpartaSharedPointer<MyType> ptr7 = nullptr;
    sparta::SpartaSharedPointer<MyType> ptr8;
    ptr8 = std::move(ptr7);

    EXPECT_THROW(ptr5.use_count());
    EXPECT_EQUAL(ptr6.use_count(), 0);
    EXPECT_THROW(ptr7.use_count());
    EXPECT_EQUAL(ptr8.use_count(), 0);

    EXPECT_THROW(ptr5.get());
    EXPECT_EQUAL(ptr6.get(), nullptr);
    EXPECT_THROW(ptr7.get());
    EXPECT_EQUAL(ptr8.get(), nullptr);

    EXPECT_THROW(ptr5 = ptr8);
    EXPECT_THROW(ptr5 = std::move(ptr8));

}

#define COUNT 10

bool warning_issued = false;
void waterMarkCallback(const decltype(trivial_type_allocator) & allocator)
{
    std::cout << __PRETTY_FUNCTION__ << ": watermark hit" << std::endl;
    warning_issued = true;
}

void testMemoryAllocation(const bool test_warning,
                          const bool test_error)
{
    my_type_deleted = 0;

    const uint32_t count = COUNT;
    std::array<sparta::SpartaSharedPointer<MyType>, COUNT> ptrs;
    for(uint32_t i = 0; i < count; ++i) {
        sparta::SpartaSharedPointer<MyType> ptr2 =
            sparta::allocate_sparta_shared_pointer<MyType>(trivial_type_allocator, 30);
        ptr2->a = i;
        ptrs[i] = ptr2;
        EXPECT_TRUE(ptrs[i]->a == i);
    }
    EXPECT_EQUAL(trivial_type_allocator.getNumFree(), 0);
    EXPECT_EQUAL(trivial_type_allocator.getNumAllocated(), count);

    std::vector<sparta::SpartaSharedPointer<MyType>> v_ptrs(std::begin(ptrs), std::end(ptrs));

    for(uint32_t i = 0; i < count; ++i) {
        EXPECT_TRUE(ptrs[i]->a == i);
        ptrs[i].reset();
    }
    v_ptrs.clear();

    EXPECT_EQUAL(trivial_type_allocator.getNumFree(), trivial_type_allocator.getNumAllocated());

    // Get a vector of outstanding allocated objects.  This should be 0
    auto outstand_objects = trivial_type_allocator.getOutstandingAllocatedObjects();
    EXPECT_EQUAL(outstand_objects.size(), 0);

    // Do it again
    for(uint32_t i = 0; i < count; ++i) {
        sparta::SpartaSharedPointer<MyType> ptr2 =
            sparta::allocate_sparta_shared_pointer<MyType>(trivial_type_allocator, 30);
        ptr2->a = i;
        ptrs[i] = ptr2;
    }

    for(uint32_t i = 0; i < count; ++i) {
        EXPECT_TRUE(ptrs[i]->a == i);
        ptrs[i].reset();
    }

    // Get a vector of outstanding allocated objects.  This should be 0
    outstand_objects = trivial_type_allocator.getOutstandingAllocatedObjects();
    EXPECT_EQUAL(outstand_objects.size(), 0);


    if(test_warning)
    {
        // Test the watermark
        const size_t max = 10;
        const size_t water = 8;
        sparta::SpartaSharedPointer<MyType>::SpartaSharedPointerAllocator limited_allocator(max, water);
        limited_allocator.registerCustomWaterMarkCallback(waterMarkCallback);
        for(uint32_t i = 0; i < max; ++i) {
            sparta::SpartaSharedPointer<MyType> ptr =
                sparta::allocate_sparta_shared_pointer<MyType>(limited_allocator, 30);
            ptrs[i] = ptr;
        }

        for(auto & p : ptrs) {
            p.reset();
        }
        EXPECT_TRUE(warning_issued);
    }

    if(test_error) {
        // Test over allocation
        const size_t max = 10;
        const size_t water = 8;
        sparta::SpartaSharedPointer<MyType>::SpartaSharedPointerAllocator limited_allocator(max, water);
        bool error_caught = false;
        for(uint32_t i = 0; i < max + 1; ++i) {
            try {
                sparta::SpartaSharedPointer<MyType> ptr =
                    sparta::allocate_sparta_shared_pointer<MyType>(limited_allocator, 30);
                ptrs[i] = ptr;
            }
            catch(sparta::SpartaException & e) {
                std::cout << "Successfully caught overallocation error: " << e.what() << std::endl;
                error_caught = true;
            }
        }
        EXPECT_TRUE(error_caught);

        for(auto & p : ptrs) {
            p.reset();
        }
    }
}

void testBasicAllocationSupport()
{
    // Test non-trival class creation
    sparta::SpartaSharedPointer<MyNonTrivialType> ptr =
        sparta::allocate_sparta_shared_pointer<MyNonTrivialType>(non_trivial_type_allocator, 10);
    EXPECT_TRUE(ptr != nullptr);
    ptr.reset();

    // Test trivial
    sparta::SpartaSharedPointer<MyType> ptr2 =
        sparta::allocate_sparta_shared_pointer<MyType>(trivial_type_allocator);
    EXPECT_TRUE(ptr2 != nullptr);

    // Get a vector of outstanding allocated objects.  This should be 1
    auto outstand_objects = trivial_type_allocator.getOutstandingAllocatedObjects();
    EXPECT_EQUAL(outstand_objects.size(), 1);
    if(outstand_objects.size() > 0) {
        EXPECT_TRUE(outstand_objects[0]->a == 10);
    }

    ptr2.reset();

    // Get a vector of outstanding allocated objects.  This should be 0
    outstand_objects = trivial_type_allocator.getOutstandingAllocatedObjects();
    EXPECT_EQUAL(outstand_objects.size(), 0);
    EXPECT_TRUE(trivial_type_allocator.hasOutstandingObjects() == false);

}

void testMemoryAllocationPerformance(const bool old_way)
{
    // const uint32_t num_ptrs = 100;
    // const uint32_t iterations = 50;
    const uint32_t num_ptrs = 10000;
    const uint32_t iterations = 5000;
    if(old_way)
    {
        std::vector<std::shared_ptr<MyType>> ptrs(num_ptrs);
#ifdef __linux__
        __gnu_cxx::__pool_alloc<MyType> pool_alloc;
#else
        std::allocator<MyType> reg_alloc;
#endif
        for(uint32_t j = 0; j < iterations; ++j)
        {
            for(uint32_t i = 0; i < num_ptrs; ++i) {
#ifdef __linux__
                ptrs[i] = std::allocate_shared<MyType>(pool_alloc, i);
#else
                ptrs[i] = std::allocate_shared<MyType>(reg_alloc, i);
#endif
                EXPECT_EQUAL(ptrs[i]->a, i);
            }
        }
    }
    else {
        sparta::SpartaSharedPointer<MyType>::SpartaSharedPointerAllocator
            local_allocator(11000, 10000);
        std::vector<sparta::SpartaSharedPointer<MyType>> ptrs(num_ptrs);
        for(uint32_t j = 0; j < iterations; ++j)
        {
            for(uint32_t i = 0; i < num_ptrs; ++i) {
                ptrs[i] = sparta::allocate_sparta_shared_pointer<MyType>(local_allocator, i);
                EXPECT_EQUAL(ptrs[i]->a, i);
            }
        }
        // The number of pointers allocated will be +1 due to the
        // second iteration of the above for loop.  After the first
        // iteration, there will be exactly num_ptrs allocated, and
        // this is correct.  However on the very first allocation on
        // the _second_ iteration, a num_ptrs + 1 object will be
        // created _before_ the 0th object is returned back to the
        // pool due to the assignment "ptrs[i] = ".
        EXPECT_EQUAL(local_allocator.getNumAllocated(), num_ptrs + 1);
    }
}

int main()
{
    testBasicSpartaSharedPointer();
    testBasicAllocationSupport();
    testMoveSupport();
    for(uint32_t i = 0; i < 100; ++i) {
        testMemoryAllocation(i == 0, i == 0);
    }
    EXPECT_EQUAL(trivial_type_allocator.getNumFree(), trivial_type_allocator.getNumAllocated());

    // Old way
    auto start = std::chrono::system_clock::system_clock::now();
    testMemoryAllocationPerformance(true);
    auto end = std::chrono::system_clock::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Old time (seconds): " << dur / 1000.0 << std::endl;

    // New way
    start = std::chrono::system_clock::system_clock::now();
    testMemoryAllocationPerformance(false);
    end = std::chrono::system_clock::system_clock::now();
    dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "New time (seconds): " << dur /1000.0 << std::endl;

    REPORT_ERROR;
    return ERROR_CODE;
}
