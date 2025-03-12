
#include "sparta/memory/CachedMemory.hpp"

#include <array>
#include <cinttypes>

#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/utils/SpartaTester.hpp"

constexpr sparta::memory::addr_t SYSTEM_BLOCK_SIZE = 0x1000;
constexpr sparta::memory::addr_t SYSTEM_TOTAL_SIZE = 0x8000000000000000;

//
// CachedMemoryTestSystem
//
//
//   core0 cached memory     core1 cached memory
//         ^    |                     |     ^
//         |     \     write/read    /      |
//   merge |      `-----------------'       | merge
//         |                |               |
//         |                V               |
//         `---- CoherentMemoryManager -----'
//                          |
//                          V
//                    system_memory
//
class CoherentMemoryManager : public sparta::memory::BlockingMemoryIF
{
public:
    CoherentMemoryManager(sparta::memory::BlockingMemoryIF * system_memory) :
        sparta::memory::BlockingMemoryIF ("coherent_memory_manager", SYSTEM_BLOCK_SIZE,
                                          sparta::memory::DebugMemoryIF::AccessWindow(0, SYSTEM_TOTAL_SIZE)),
        system_memory_(system_memory)
    {}

    void addCachedMemory(sparta::memory::CachedMemory<> * cm) {
        cached_memory_.emplace_back(cm);
    }

private:
    bool tryPeek_(sparta::memory::addr_t paddr, sparta::memory::addr_t size,
                  uint8_t *buf) const override final {
        return system_memory_->tryPeek(paddr, size, buf);
    }

    bool tryPoke_(sparta::memory::addr_t paddr, sparta::memory::addr_t size,
                  const uint8_t *buf) override final {
        return system_memory_->tryPoke(paddr, size, buf);
    }

    bool tryRead_(sparta::memory::addr_t paddr, sparta::memory::addr_t size, uint8_t *buf,
                  const void *in_supplement, void *out_supplement) override final {
        return system_memory_->tryRead(paddr, size, buf);
    }

    bool tryWrite_(sparta::memory::addr_t paddr, sparta::memory::addr_t size, const uint8_t *buf,
                   const void *in_supplement, void *out_supplement) override final
    {
        // Update the cached memory objects
        for(auto smem : cached_memory_)
        {
            // Skip the cached memory that sent the write.  The
            // in_supplement can be nullptr to update all cached
            // memory
            if(smem == in_supplement) { continue; }
            smem->mergeWrite(paddr, size, buf);
        }

        // Update system memory
        return system_memory_->tryWrite(paddr, size, buf);
    }


    sparta::memory::BlockingMemoryIF * system_memory_ = nullptr;
    std::vector<sparta::memory::CachedMemory<>*> cached_memory_;
};

union TestMemoryBlock
{
    TestMemoryBlock(uint64_t init_val = 0xdeadbeef) : data(init_val) {}
    uint64_t data;
    std::array<uint8_t, sizeof(data)> data_ptr;
};

const TestMemoryBlock deadbeef_data(0xDEADBEEF);
const TestMemoryBlock aaaaaaaa_data(0xA1A2A3A4890abcdeull);
const TestMemoryBlock bbbbbbbb_data(0xB1B2B3B4890abcdeull);
const TestMemoryBlock cccccccc_data(0xC1C2C3C4890abcdeull);
const TestMemoryBlock dddddddd_data(0xD1D2D3D4890abcdeull);
const TestMemoryBlock eeeeeeee_data(0xEEEEEEEEEEEEEEEEull);
const TestMemoryBlock ffffffff_data(0xFFFFFFFFFFFFFFFFull);

constexpr sparta::memory::addr_t paddr_0x1000 = 0x1000;
constexpr sparta::memory::addr_t paddr_0x2000 = 0x2000;

class CachedMemoryTestSystem
{
public:
    CachedMemoryTestSystem()
    {
        coherent_memory_manager.addCachedMemory(&cached_mem_core0);
        coherent_memory_manager.addCachedMemory(&cached_mem_core1);
    }

    sparta::RootTreeNode rtn{"root"};
    sparta::TreeNode     sys_tn {&rtn, "system", "system node"};
    sparta::memory::MemoryObject backend_memory{&sys_tn, SYSTEM_BLOCK_SIZE, SYSTEM_TOTAL_SIZE};
    sparta::memory::BlockingMemoryObjectIFNode downstream_memory{&sys_tn, "system_memory",
        sparta::TreeNode::GROUP_NAME_NONE,
        sparta::TreeNode::GROUP_IDX_NONE,
        "system memory", nullptr, backend_memory};

    CoherentMemoryManager coherent_memory_manager{&downstream_memory};
    const uint32_t outstanding_write_watermark = 4*1024;
    sparta::memory::CachedMemory<> cached_mem_core0{ "core0", 0, outstanding_write_watermark,
        SYSTEM_BLOCK_SIZE, SYSTEM_TOTAL_SIZE, &coherent_memory_manager};
    sparta::memory::CachedMemory<> cached_mem_core1{ "core1", 1, outstanding_write_watermark,
        SYSTEM_BLOCK_SIZE, SYSTEM_TOTAL_SIZE, &coherent_memory_manager};

    ~CachedMemoryTestSystem() { rtn.enterTeardown(); }
};

//
// Test requirements on just one core (core 0)
//
void test_requirements()
{
    CachedMemoryTestSystem test_system;

    ////////////////////////////////////////////////////////////////////////////////
    // Test poking goes to both cached and downstream
    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());

    TestMemoryBlock read_test_data(0x0);
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // Test peeking only from Cached Memory.
    //
    // Write a block of memory to downstream memory.  Should still see
    // the older memory written above when reading from cached
    test_system.downstream_memory.write(paddr_0x1000, sizeof(cccccccc_data.data), cccccccc_data.data_ptr.data());

    // Should be new data
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    // Should be the old data
    test_system.cached_mem_core0.peek(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // Test reading only from Cached Memory.
    //
    // This test uses the previous tests for verification.  At this
    // point, downstream_memory has the value 0xC1234567890abcdeull at
    // address 0x1000.  CachedMemory should have 0x00000000DEADBEEF at
    // address 0x1000.  A read from SM should only return
    // 0x00000000DEADBEEF.  A read from downstream_memory should still
    // return 0xC1234567890abcdeull
    read_test_data.data = 0x0;
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // Test writes will only go to Cached Memory
    //
    // This will write 0xA1234567890abcdeull to 0x2000 via cached memory
    // and 0xC1234567890abcdeull to 0x2000.  Only Cached memory should be
    // written

    // Initialize both memories @0x2000 to 0xC1234567890abcdeull
    test_system.cached_mem_core0.poke(paddr_0x2000, sizeof(cccccccc_data.data), cccccccc_data.data_ptr.data());

    // Write cached memory only
    test_system.cached_mem_core0.write(paddr_0x2000, sizeof(aaaaaaaa_data.data), aaaaaaaa_data.data_ptr.data()
                                       //,&in_supp
        );
    read_test_data.data = 0;
    test_system.cached_mem_core0.read(paddr_0x2000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, aaaaaaaa_data.data);
    test_system.downstream_memory.read(paddr_0x2000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // Test that CachedMemory tracks only outstanding, outstanding writes
    //
    // The previous test pushed an outstanding write into the Cached
    // Memory.  Get that value and test to see if the current value is
    // equal to the last.  Push another write uncomitted into Cached
    // Memory and test final value
    const auto mem_accesses = test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x2000);

    // Assert here instead of continuing the test, which depends on
    // this vector being non-zero in size
    sparta_assert(!mem_accesses.empty(), "Expected at least one outstanding write");
    EXPECT_EQUAL(mem_accesses.size(), 1);
    EXPECT_EQUAL(mem_accesses[0].getPAddr(), paddr_0x2000);
    EXPECT_EQUAL(mem_accesses[0].getSize(), sizeof(aaaaaaaa_data.data));
    EXPECT_EQUAL(::memcmp(mem_accesses[0].getStashDataPtr(),
                          aaaaaaaa_data.data_ptr.data(),
                          sizeof(aaaaaaaa_data.data)), 0);
    EXPECT_EQUAL(::memcmp(mem_accesses[0].getPrevDataPtr(),
                          cccccccc_data.data_ptr.data(),
                          sizeof(cccccccc_data.data)), 0);

    ////////////////////////////////////////////////////////////////////////////////
    // Test dropping of a outstanding write and that is restores
    //
    // First, test dropping of a write not performed on this
    // CachedMemory.  Second, drop write from the previous tests (at
    // 0x2000).  Memory should be restored to previous value.
    const sparta::memory::StoreData bad_write(0, paddr_0x1000, 8,
                                              ffffffff_data.data_ptr.data(),
                                              nullptr, nullptr);
    EXPECT_THROW  (test_system.cached_mem_core0.dropWrite(bad_write));
    EXPECT_NOTHROW(test_system.cached_mem_core0.dropWrite(mem_accesses[0]));
    EXPECT_TRUE(test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000).size() == 0);
    EXPECT_TRUE(test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x2000).size() == 0);

    read_test_data.data = 0;
    test_system.cached_mem_core0.read(paddr_0x2000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);
    test_system.downstream_memory.read(paddr_0x2000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // At this point, both CachedMemory and Downstream Memory should
    // be in sync for 0x2000 which was tested above.  Sync 0x1000 for
    // the next tests
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(cccccccc_data.data), cccccccc_data.data_ptr.data());
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    ////////////////////////////////////////////////////////////////////////////////
    // Test commits of a write and push to downstream memory
    //
    // In this test, two writes will made to the cached_mem_core0
    // followed by two commits.  Downstream memory should reflect the
    // values as they are pushed in order

    // Poke 0xdeadbeef in both cached and downstrea
    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());

    test_system.cached_mem_core0.write(paddr_0x1000, sizeof(aaaaaaaa_data.data), aaaaaaaa_data.data_ptr.data());
    test_system.cached_mem_core0.write(paddr_0x1000, sizeof(bbbbbbbb_data.data), bbbbbbbb_data.data_ptr.data());
    test_system.cached_mem_core0.write(paddr_0x1000, sizeof(cccccccc_data.data), cccccccc_data.data_ptr.data());
    const auto writes = test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000);
    sparta_assert(writes.size() == 3);
    EXPECT_EQUAL(writes[0].getPAddr(), paddr_0x1000);
    EXPECT_EQUAL(::memcmp(writes[0].getStashDataPtr()     , aaaaaaaa_data.data_ptr.data(), sizeof(aaaaaaaa_data.data)), 0);
    EXPECT_EQUAL(::memcmp(writes[0].getPrevDataPtr(), deadbeef_data.data_ptr.data(), sizeof(aaaaaaaa_data.data)), 0);
    EXPECT_EQUAL(writes[1].getPAddr(), paddr_0x1000);
    EXPECT_EQUAL(::memcmp(writes[1].getStashDataPtr()     , bbbbbbbb_data.data_ptr.data(), sizeof(bbbbbbbb_data.data)), 0);
    EXPECT_EQUAL(::memcmp(writes[1].getPrevDataPtr(), aaaaaaaa_data.data_ptr.data(), sizeof(aaaaaaaa_data.data)), 0);
    EXPECT_EQUAL(writes[2].getPAddr(), paddr_0x1000);
    EXPECT_EQUAL(::memcmp(writes[2].getStashDataPtr()     , cccccccc_data.data_ptr.data(), sizeof(cccccccc_data.data)), 0);
    EXPECT_EQUAL(::memcmp(writes[2].getPrevDataPtr(), bbbbbbbb_data.data_ptr.data(), sizeof(bbbbbbbb_data.data)), 0);

    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);

    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    EXPECT_NOTHROW(test_system.cached_mem_core0.commitWrite(writes[0]));
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, aaaaaaaa_data.data);
    EXPECT_TRUE(test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000).size() == 2);

    EXPECT_NOTHROW(test_system.cached_mem_core0.commitWrite(writes[1]));
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, bbbbbbbb_data.data);
    EXPECT_TRUE(test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000).size() == 1);

    EXPECT_NOTHROW(test_system.cached_mem_core0.commitWrite(writes[2]));
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, cccccccc_data.data);
    EXPECT_TRUE(test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000).size() == 0);

}

// Set up a scenario where there are four outstanding store words, but
// each store word is misaligned by 1 byte.  Then, update memory.
// Flush each store, starting with the newest and make sure memory is
// restored properly.
void test_lots_outstanding_misaligned_stores()
{
    CachedMemoryTestSystem test_system;
    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(ffffffff_data.data), ffffffff_data.data_ptr.data());

    uint32_t starting_offset = 3;  // Start at address 0x1003
    test_system.cached_mem_core0.write(paddr_0x1000 + starting_offset, 4, aaaaaaaa_data.data_ptr.data() + 4);
    starting_offset -= 1;
    test_system.cached_mem_core0.write(paddr_0x1000 + starting_offset, 4, bbbbbbbb_data.data_ptr.data() + 4);
    starting_offset -= 1;
    test_system.cached_mem_core0.write(paddr_0x1000 + starting_offset, 4, cccccccc_data.data_ptr.data() + 4);
    starting_offset -= 1;
    test_system.cached_mem_core0.write(paddr_0x1000 + starting_offset, 4, dddddddd_data.data_ptr.data() + 4);

    TestMemoryBlock read_test_data(0x0);
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    TestMemoryBlock expected_read_data(0xFFa1b1c1d1D2D3D4ull);
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);

    // Update memory via the coherent block with new memory.  This
    // should ONLY change the last byte
    test_system.coherent_memory_manager.write(paddr_0x1000, sizeof(eeeeeeee_data.data), eeeeeeee_data.data_ptr.data());
    expected_read_data.data = 0xEEa1b1c1d1D2D3D4ull;
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);

    // Now, the main part of the test.  Start dropping the writes and
    // see if cached memory restores back to all E's.  This call
    // should get the 4 writes that overlapped 0x1000+3:
    //
    // 0x1000 -> 0x1003
    // 0x1001 -> 0x1004
    // 0x1002 -> 0x1005
    // 0x1003 -> 0x1006
    //
    auto core0_writes = test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000 + 3);
    EXPECT_EQUAL(core0_writes.size(), 4);
    EXPECT_TRUE(core0_writes.back().getPAddr() == paddr_0x1000);  // Check to see if 0 is the newest write

    // This will drop dddddddd_data
    test_system.cached_mem_core0.dropWrite(core0_writes.back());
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    expected_read_data.data = 0xEEA1B1C1C2C3C4EE;
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);
    core0_writes.pop_back();

    // This will drop cccccccc_data
    EXPECT_TRUE(core0_writes.back().getPAddr() == paddr_0x1000 + 1);
    test_system.cached_mem_core0.dropWrite(core0_writes.back());
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    expected_read_data.data = 0xEEA1B1B2B3B4EEEE;
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);
    core0_writes.pop_back();

    // This will drop bbbbbbbb_data
    EXPECT_TRUE(core0_writes.back().getPAddr() == paddr_0x1000 + 2);
    test_system.cached_mem_core0.dropWrite(core0_writes.back());
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    expected_read_data.data = 0xEEA1A2A3A4EEEEEE;
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);
    core0_writes.pop_back();

    // This will drop aaaaaaaa_data
    EXPECT_TRUE(core0_writes.back().getPAddr() == paddr_0x1000 + 3);
    test_system.cached_mem_core0.dropWrite(core0_writes.back());
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    expected_read_data.data = 0xEEEEEEEEEEEEEEEE;
    EXPECT_EQUAL(read_test_data.data, expected_read_data.data);
    core0_writes.pop_back();

    EXPECT_TRUE(core0_writes.empty());
}

////////////////////////////////////////////////////////////////////////////////
// Test dual core

// Test dual core with one core committing and the other flushing
void test_two_cores_cacheable_with_commit_flush()
{
    // Set up memory
    CachedMemoryTestSystem test_system;
    TestMemoryBlock read_test_data(0x0);

    // Inialize memory in both core0, core1, and downstream memory to the same value
    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.cached_mem_core1.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.cached_mem_core1.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    // Write all A's to core0, B's to core1
    test_system.cached_mem_core0.write(paddr_0x1000, sizeof(aaaaaaaa_data.data), aaaaaaaa_data.data_ptr.data());
    test_system.cached_mem_core1.write(paddr_0x1000, sizeof(bbbbbbbb_data.data), bbbbbbbb_data.data_ptr.data());

    // Downstream memory should still be deadbeef
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    // Core0 should be all A's
    test_system.cached_mem_core0.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, aaaaaaaa_data.data);

    // Core1 should be all B's
    test_system.cached_mem_core1.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, bbbbbbbb_data.data);

    // Commit core0
    const auto core0_writes = test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000);
    EXPECT_NOTHROW(test_system.cached_mem_core0.commitWrite(core0_writes[0]));

    // Core1 should still be all B's
    test_system.cached_mem_core1.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, bbbbbbbb_data.data);

    // Flush core1
    const auto core1_writes = test_system.cached_mem_core1.getOutstandingWritesForAddr(paddr_0x1000);
    EXPECT_NOTHROW(test_system.cached_mem_core1.dropWrite  (core1_writes[0]));

    // Downstream memory should reflect the value of core0
    test_system.downstream_memory.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, aaaaaaaa_data.data);

    // Core1 should reflect the new value of memory from core0 (all
    // A's) since the write was dropped in Core0
    test_system.cached_mem_core1.read(paddr_0x1000, sizeof(read_test_data.data), read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, aaaaaaaa_data.data);
}

// Test dual core with two cores committing, but overlapping in addresses
//
// 0x1000 0xdeadbeef
// 0x1004 0xdeadbeef
//
// 0x1002 0xbeefdead
//
// Core0 writes to 0x1000, 4 bytes, 0xA1234567
// Core1 writes to 0x1002, 4 bytes, 0xB1234567
//
// Core0 memory 8 bytes @ 0x1000: 0xdeadbeefA1234567
// Core1 memory 8 bytes @ 0x1000: 0xdeadB1234567beef
// Downstream Memory            : 0xdeadbeefdeadbeef
//
// Core 0 writes memory first, then core 1.  Final value should be
// core 1's mixed with core 0's
//
//
void test_two_cores_cacheable_overlap_two_commits()
{
    CachedMemoryTestSystem test_system;
    TestMemoryBlock read_test_data(0x0);

    const sparta::memory::addr_t paddr_0x1002 = 0x1002;
    const sparta::memory::addr_t paddr_0x1004 = 0x1004;

    test_system.cached_mem_core0.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.cached_mem_core0.poke(paddr_0x1004, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.cached_mem_core1.poke(paddr_0x1000, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.cached_mem_core1.poke(paddr_0x1004, sizeof(deadbeef_data.data), deadbeef_data.data_ptr.data());
    test_system.downstream_memory.read(paddr_0x1000, 4, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);
    test_system.downstream_memory.read(paddr_0x1004, 4, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, deadbeef_data.data);

    const TestMemoryBlock beefdead_data(0xBEEFDEAD);
    test_system.cached_mem_core0.read(paddr_0x1002, 4, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, beefdead_data.data);

    // Core0 writes to 0x1000, 4 bytes, 0xA1234567
    test_system.cached_mem_core0.write(paddr_0x1000, 4, aaaaaaaa_data.data_ptr.data()+4);

    // Core1 writes to 0x1002, 4 bytes, 0xB1234567
    test_system.cached_mem_core1.write(paddr_0x1002, 4, bbbbbbbb_data.data_ptr.data()+4);

    // Core0 memory 8 bytes @ 0x1000: 0xdeadbeefA1A2A3A4
    // Core1 memory 8 bytes @ 0x1000: 0xdeadB1B2B3B4beef
    // Downstream Memory            : 0xdeadbeefdeadbeef
    const TestMemoryBlock core0_data  (0xdeadbeefA1A2A3A4);
    const TestMemoryBlock core1_data  (0xdeadB1B2B3B4beef);
    const TestMemoryBlock ds_mem_data (0xdeadbeefdeadbeef);
    const TestMemoryBlock final_memory(0xdeadB1B2B3B4A3A4);

    test_system.cached_mem_core0 .read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, core0_data.data);
    test_system.cached_mem_core1 .read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, core1_data.data);
    test_system.downstream_memory.read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, ds_mem_data.data);

    // Now commit core0
    //
    // Before:
    //
    // Core0 memory 8 bytes @ 0x1000: 0xdeadbeefA1A2A3A4
    // Core1 memory 8 bytes @ 0x1000: 0xdeadB1B2B3B4beef
    // Downstream Memory            : 0xdeadbeefdeadbeef
    //
    // After Core0 commit:
    //
    // Core0 memory 8 bytes @ 0x1000: 0xdeadbeefA1A2A3A4
    // Core1 memory 8 bytes @ 0x1000: 0xdeadB1234567A3A4
    // Downstream Memory            : 0xdeadbeefA1A2A3A3
    //
    //
    const auto core0_writes = test_system.cached_mem_core0.getOutstandingWritesForAddr(paddr_0x1000);
    sparta_assert(core0_writes.size() == 1);
    test_system.cached_mem_core0.commitWrite(core0_writes[0]);

    // Downstream memory should reflect the same data as Core 0
    test_system.downstream_memory.read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, core0_data.data);

    // Core 1 should be merged with Core0 Data that it didn't touch
    const TestMemoryBlock core1_data_merged_with_core0  (0xdeadB1B2B3B4A3A4);
    test_system.cached_mem_core1.read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, core1_data_merged_with_core0.data);

    // Now, commit core 1 memory
    // Core0 memory 8 bytes @ 0x1000: 0xdeadbeefA1A2A3A4
    // Core1 memory 8 bytes @ 0x1000: 0xdeadB1B2B3B4A3A4
    // Downstream Memory            : 0xdeadB1B2B3B4A3A4
    const auto core1_writes = test_system.cached_mem_core1.getOutstandingWritesForAddr(paddr_0x1002);
    sparta_assert(core1_writes.size() == 1);
    test_system.cached_mem_core1.commitWrite(core1_writes[0]);

    // Everyone should have the same value of memory
    test_system.cached_mem_core0 .read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, final_memory.data);
    test_system.cached_mem_core1 .read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, final_memory.data);
    test_system.downstream_memory.read(paddr_0x1000, 8, read_test_data.data_ptr.data());
    EXPECT_EQUAL(read_test_data.data, final_memory.data);
}

int main()
{
    // Quick test on the TestMemoryBlock
    const TestMemoryBlock deadbeef_data_test(0xCCCCCCCCdeadbeef);
    EXPECT_EQUAL(deadbeef_data_test.data_ptr[0], 0xEF);
    EXPECT_EQUAL(deadbeef_data_test.data_ptr[1], 0xBE);
    EXPECT_EQUAL(deadbeef_data_test.data_ptr[2], 0xAD);
    EXPECT_EQUAL(deadbeef_data_test.data_ptr[3], 0xDE);
    EXPECT_EQUAL(*reinterpret_cast<const uint64_t*>(deadbeef_data_test.data_ptr.data()),
                 0xCCCCCCCCdeadbeef);

    // Test basic requirements
    test_requirements();

    // Test single core
    test_lots_outstanding_misaligned_stores();

    // Test dual core
    test_two_cores_cacheable_with_commit_flush();
    test_two_cores_cacheable_overlap_two_commits();

    REPORT_ERROR;
    return ERROR_CODE;
}
