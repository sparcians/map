
#include <inttypes.h>
#include <iostream>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/memory/BlockingMemoryIF.hpp"
#include "sparta/memory/DMIBlockingMemoryIF.hpp"
#include "sparta/memory/BlockingMemoryIFNode.hpp"
#include "sparta/memory/TranslationIF.hpp"
#include "sparta/memory/TranslationIFNode.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Utils.hpp"

/*!
 * \file main.cpp
 * \brief Test for sparta MemoryObject and BlockingMemoryIF
 *
 * These are simple blocking memory interfaces and storage classes
 */

TEST_INIT;

#define MEM_SIZE 4096
#define BLOCK_SIZE 64

using sparta::memory::addr_t;

void testMemoryObjectRW();
void testMemoryObjectSparseness();
void testBlockingMemoryIFNode();
void testDebugMemoryIF();
void testMemoryObjectPerformance();
void testMemoryObjectSizes();
void testMemoryObjectFill();

int main()
{
    testMemoryObjectRW();
    testMemoryObjectSparseness();
    testBlockingMemoryIFNode();
    testDebugMemoryIF();
    testMemoryObjectPerformance();
    testMemoryObjectSizes();
    testMemoryObjectFill();

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}

//! Tests the MemoryObject directly for read/write accesses
void testMemoryObjectRW() {

    std::cout << "\nTesting MemoryObject read/write\nMem size: " << MEM_SIZE << ", Block size: " << BLOCK_SIZE << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4

    sparta::memory::MemoryObject mem(nullptr, BLOCK_SIZE, MEM_SIZE);
    std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;

    uint8_t dat[BLOCK_SIZE];
    uint8_t buf[BLOCK_SIZE];

    // General
    EXPECT_EQUAL(mem.getSize(), MEM_SIZE);
    EXPECT_EQUAL(mem.getBlockSize(), BLOCK_SIZE);
    EXPECT_EQUAL(mem.getNumBlocks(), mem.getSize() / mem.getBlockSize());
    EXPECT_EQUAL(mem.getFill(), 0xcc);

    // Simple reads
    EXPECT_NOTHROW(mem.read(0, 0, buf)); // Ok to read 0-size if caller wants. No reason to prohibit it
    EXPECT_NOTHROW(mem.read(0, BLOCK_SIZE/2, buf));
    EXPECT_NOTHROW(mem.read(0, BLOCK_SIZE-1, buf));
    EXPECT_NOTHROW(mem.read(0, BLOCK_SIZE, buf));
    EXPECT_EQUAL(buf[0], mem.getFill()); // expects default fill
    EXPECT_EQUAL(buf[BLOCK_SIZE-1], mem.getFill()); // expects default fill
    EXPECT_THROW(mem.read(0, BLOCK_SIZE+1, buf));
    EXPECT_THROW(mem.read(0, BLOCK_SIZE*2, buf)); // too large for block
    EXPECT_NOTHROW(mem.read(2, BLOCK_SIZE-2, buf));
    EXPECT_NOTHROW(mem.read(1, BLOCK_SIZE-1, buf));
    EXPECT_THROW(mem.read(1, BLOCK_SIZE, buf)); // spans block
    EXPECT_THROW(mem.read(1, BLOCK_SIZE+1, buf)); // spans block
    EXPECT_NOTHROW(mem.read(BLOCK_SIZE-2, 2, buf));
    EXPECT_NOTHROW(mem.read(BLOCK_SIZE-1, 1, buf));
    EXPECT_THROW(mem.read(BLOCK_SIZE-1, 2, buf)); // spans block
    EXPECT_THROW(mem.read(BLOCK_SIZE-1, 3, buf)); // spans block
    EXPECT_THROW(mem.read(MEM_SIZE, 4, buf)); // outside mem range
    EXPECT_THROW(mem.read(MEM_SIZE+(BLOCK_SIZE/2), 4, buf)); // way outside mem range
    EXPECT_THROW(mem.read(MEM_SIZE-2, 4, buf)); // partly outside mem range
    EXPECT_THROW(mem.read(MEM_SIZE-(BLOCK_SIZE/2), 1+BLOCK_SIZE/2, buf)); // partly outside mem range

    // Simple (valid) writes & validation
    dat[0] = 0xff;
    EXPECT_NOTHROW(mem.write(0, 1, dat));
    EXPECT_NOTHROW(mem.read(0, 1, buf));
    EXPECT_EQUAL(buf[0], 0xff); // just written


    // Simple (invalid) writes
    EXPECT_THROW(mem.write(MEM_SIZE, 4, buf)); // outside mem range
    EXPECT_THROW(mem.write(MEM_SIZE+(BLOCK_SIZE/2), 4, buf)); // way outside mem range
    EXPECT_THROW(mem.write(MEM_SIZE-2, 4, buf)); // partly outside mem range
    EXPECT_THROW(mem.write(MEM_SIZE-(BLOCK_SIZE/2), 1+BLOCK_SIZE/2, buf)); // partly outside mem range

    std::cout << "Done: " << std::endl << mem << std::endl << mem.getLineStates() << std::endl;
}

//! Tests the MemoryObject for sparseness behavior
void testMemoryObjectSparseness() {
    std::cout << "\nTesting MemoryObject Sparseness\nMem size: " << MEM_SIZE << ", Block size: " << BLOCK_SIZE << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4

    sparta::memory::MemoryObject mem(nullptr, BLOCK_SIZE, MEM_SIZE);
    std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;

    //uint8_t buf[BLOCK_SIZE];

    // General
    EXPECT_EQUAL(mem.getSize(), MEM_SIZE);
    EXPECT_EQUAL(mem.getBlockSize(), BLOCK_SIZE);
    EXPECT_EQUAL(mem.getNumBlocks(), mem.getSize() / mem.getBlockSize());
    EXPECT_EQUAL(mem.getFill(), 0xcc);

    // get allocate block count (0)
    // read unallocated block
    // get allocate block count (still 0)
    // test for allocated block at read (not allocated)
    // write to a block
    // get allocated block count (now 1)

    std::cout << "Done: " << std::endl << mem << std::endl << mem.getLineStates() << std::endl;
}

//! Tests the memory object fill sizes
void testMemoryObjectFill() {
    const uint32_t block_size = 512;
    const uint32_t mem_size = block_size*16;
    sparta::memory::MemoryObject m1B(nullptr, block_size, mem_size, 0xef, 1);
    sparta::memory::MemoryObject m2B(nullptr, block_size, mem_size, 0xcdef, 2);
    sparta::memory::MemoryObject m4B(nullptr, block_size, mem_size, 0x89abcdef, 4);
    sparta::memory::MemoryObject m8B(nullptr, block_size, mem_size, 0x0123456789abcdef, 8);

    uint32_t buf;
    static_assert(block_size > 8*10, "Large enough block size required for this test");

    // Read unwritten blocks
    // Assumes this is an LE machine...
    EXPECT_EQUAL((m1B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xefefefef);
    EXPECT_EQUAL((m2B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xcdefcdef);
    EXPECT_EQUAL((m2B.read(2*6+1, 2, reinterpret_cast<uint8_t*>(&buf)), buf&0xffff), 0xefcd);
    EXPECT_EQUAL((m4B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdef);
    EXPECT_EQUAL((m8B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdef);
    EXPECT_EQUAL((m8B.read(8*6+1, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x6789abcd);
    EXPECT_EQUAL((m8B.read(8*7+1, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x6789abcd);
    EXPECT_EQUAL((m8B.read(8*8+4, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x01234567);
    EXPECT_EQUAL((m8B.read(8*8+5, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xef012345);
    EXPECT_EQUAL((m8B.read(4,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x01234567);
    EXPECT_EQUAL((m8B.read(8,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdef);
    uint64_t buf64;
    EXPECT_EQUAL((m8B.read(8,     8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0x0123456789abcdef);
    EXPECT_EQUAL((m8B.read(9,     8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0xef0123456789abcd);
    EXPECT_EQUAL((m8B.read(12,    8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0x89abcdef01234567);

    // Test reading from written blocks now
    uint8_t write_buf = 0xaa;
    EXPECT_EQUAL(m1B.tryGetLine(0), nullptr);
    m1B.write(0, 1, &write_buf);
    EXPECT_NOTEQUAL(m1B.tryGetLine(0), nullptr);
    EXPECT_EQUAL(m2B.tryGetLine(0), nullptr);
    m2B.write(0, 1, &write_buf);
    EXPECT_NOTEQUAL(m2B.tryGetLine(0), nullptr);
    EXPECT_EQUAL(m4B.tryGetLine(0), nullptr);
    m4B.write(0, 1, &write_buf);
    EXPECT_NOTEQUAL(m4B.tryGetLine(0), nullptr);
    EXPECT_EQUAL(m8B.tryGetLine(0), nullptr);
    m8B.write(0, 1, &write_buf);
    EXPECT_NOTEQUAL(m8B.tryGetLine(0), nullptr);
    EXPECT_EQUAL((m1B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xefefefaa);
    EXPECT_EQUAL((m2B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xcdefcdaa);
    EXPECT_EQUAL((m2B.read(2*6+1, 2, reinterpret_cast<uint8_t*>(&buf)), buf&0xffff), 0xefcd);
    EXPECT_EQUAL((m4B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdaa);
    EXPECT_EQUAL((m8B.read(0,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdaa);
    EXPECT_EQUAL((m8B.read(8*6+1, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x6789abcd);
    EXPECT_EQUAL((m8B.read(8*7+1, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x6789abcd);
    EXPECT_EQUAL((m8B.read(8*8+4, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x01234567);
    EXPECT_EQUAL((m8B.read(8*8+5, 4, reinterpret_cast<uint8_t*>(&buf)), buf),        0xef012345);
    EXPECT_EQUAL((m8B.read(4,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x01234567);
    EXPECT_EQUAL((m8B.read(8,     4, reinterpret_cast<uint8_t*>(&buf)), buf),        0x89abcdef);
    EXPECT_EQUAL((m8B.read(8,     8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0x0123456789abcdef);
    EXPECT_EQUAL((m8B.read(9,     8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0xef0123456789abcd);
    EXPECT_EQUAL((m8B.read(12,    8, reinterpret_cast<uint8_t*>(&buf64)), buf64), 0x89abcdef01234567);

    EXPECT_THROW(sparta::memory::MemoryObject bad(nullptr, block_size, mem_size, 0x1ff, 1);); // initial value is too large
    EXPECT_THROW(sparta::memory::MemoryObject bad(nullptr, block_size, mem_size, 0x1ffff, 2);); // initial value is too large
    EXPECT_THROW(sparta::memory::MemoryObject bad(nullptr, block_size, mem_size, 0x1ffffffff, 4);); // initial value is too large
}

class MemPostWriteObserver
{
    // Expected
    addr_t expect_addr_;
    addr_t expect_size_;
    const uint8_t* expect_prior_;
    const uint8_t* expect_tried_;
    const uint8_t* expect_final_;
    const void * expect_in_supplement_;
    void * expect_out_supplement_;

public:
    uint32_t writes;

    MemPostWriteObserver() :
        expect_addr_(0xdefec8ed),
        expect_size_(0xdefec8ed),
        expect_prior_(nullptr),
        expect_tried_(nullptr),
        expect_final_(nullptr),
        expect_in_supplement_(nullptr),
        expect_out_supplement_(nullptr),
        writes(0)
    { }

    /*!
     * \brief Sets expected data during next callback.
     * Causes callback to indicate test error if callback data does not match
     * \param addr Expected address of next write accesses
     * \param size Expected size of next write accesses
     * \param prior Expected data contained immediately before next write
     * accesses (Must contain at least \a size bytes). Caller maintains
     * ownership. Set to nullptr to ignore
     * \param tried Expected data that a write access attempted to write during
     * the next write accesses (Must contain at least \a size bytes). Caller
     * maintains ownership. Set to nullptr to ignore
     * \param final Expected data that was actually wirtten to memory during the
     * next write accesses (Must contain at least \a size bytes). Caller
     * maintains ownership
     * \param in_supplement Expected in_supplement pointer in future notifications
     * \param out_supplement Expected out_supplement pointer for getting data back
     */
    void expect(addr_t addr,
                addr_t size,
                const uint8_t* prior,
                const uint8_t* tried,
                const uint8_t* final,
                const void * in_supplement,
                void * out_supplement) {
        expect_addr_ = addr;
        expect_size_ = size;
        expect_prior_ = prior;
        expect_tried_ = tried;
        expect_final_ = final;
        expect_in_supplement_ = in_supplement;
        expect_out_supplement_ = out_supplement;
    }

    void registerFor(sparta::memory::BlockingMemoryIFNode* m) {
        m->getPostWriteNotificationSource().REGISTER_FOR_THIS(callback);
    }

    void deregisterFor(sparta::memory::BlockingMemoryIFNode* m) {
        m->getPostWriteNotificationSource().DEREGISTER_FOR_THIS(callback);
    }

    void callback(const sparta::memory::BlockingMemoryIFNode::PostWriteAccess& data){
        (void) data;
        writes++;

        EXPECT_EQUAL(data.addr, expect_addr_);
        EXPECT_EQUAL(data.size, expect_size_);
        if(expect_prior_){
            for(uint32_t i=0; i < std::min(data.size, expect_size_); ++i){
                EXPECT_EQUAL(data.prior[i], expect_prior_[i]);
            }
        }
        if(expect_tried_){
            for(uint32_t i=0; i < std::min(data.size, expect_size_); ++i){
                EXPECT_EQUAL(data.tried[i], expect_tried_[i]);
            }
        }
        if(expect_final_){
            uint8_t buf[2048];
            assert(data.size <= sizeof(buf)); // Cannot read larger than this
            data.mem->peek(data.addr, data.size, buf);
            for(uint32_t i=0; i < std::min(data.size, expect_size_); ++i){
                EXPECT_EQUAL(buf[i], expect_final_[i]);
            }
        }
        EXPECT_EQUAL(data.in_supplement, expect_in_supplement_);
    }
};

class MemReadObserver
{
    // Expected
    addr_t expect_addr_;
    addr_t expect_size_;
    const uint8_t* expect_data_;
    const void * expect_in_supplement_;
    void * expect_out_supplement_;

public:
    uint32_t reads;

    MemReadObserver() :
        expect_addr_(0xdefec8ed),
        expect_size_(0xdefec8ed),
        expect_data_(nullptr),
        expect_in_supplement_(nullptr),
        expect_out_supplement_(nullptr),
        reads(0)
    { }

    /*!
     * \brief Sets expected data during next callback.
     * Causes callback to indicate test error if callback data does not match
     * \param addr Expected address of next accesses
     * \param size Expected size of next accesses
     * \param data Expected data read by next accesses (Must contain at least
     * \a size bytes). Caller maintains ownership
     * \param supplement Expected supplementary pointer in notification for future accesses
     */
    void expect(addr_t addr,
                addr_t size,
                const uint8_t* data,
                const void * in_supplement,
                void * out_supplement) {
        expect_addr_ = addr;
        expect_size_ = size;
        expect_data_ = data;
        expect_in_supplement_ = in_supplement;
        expect_out_supplement_ = out_supplement;
    }

    void registerFor(sparta::memory::BlockingMemoryIFNode* m) {
        m->getReadNotificationSource().REGISTER_FOR_THIS(callback);
    }

    void deregisterFor(sparta::memory::BlockingMemoryIFNode* m) {
        m->getReadNotificationSource().DEREGISTER_FOR_THIS(callback);
    }

    void callback(const sparta::memory::BlockingMemoryIFNode::ReadAccess& data){
        reads++;

        EXPECT_EQUAL(data.addr, expect_addr_);
        EXPECT_EQUAL(data.size, expect_size_);
        if(expect_data_){
            for(uint32_t i=0; i < std::min(data.size, expect_size_); ++i){
                EXPECT_EQUAL(data.data[i], expect_data_[i]);
            }
        }
        EXPECT_EQUAL(data.in_supplement, expect_in_supplement_);
    }
};

//! Test the BlockingMemoryIFNode (though BlockingMemoryObjectIFNode)
void testBlockingMemoryIFNode() {

    std::cout << "\nTesting BlockingMemoryIFNode\nMem size: " << MEM_SIZE << ", Block size: " << BLOCK_SIZE << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4

    sparta::RootTreeNode root;

    // Memory Setup
    sparta::memory::MemoryObject mem(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::TranslationIF trans("virtual", "physical");
    sparta::memory::BlockingMemoryObjectIFNode membif(&root, "mem1", "Blocking memory object", &trans, mem);

    // Print current memory set by the ostream insertion operator
    std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;

    // Print current memory set by the ostream insertion operator
    std::cout << "BlockingMemoryIFNode: " << std::endl << *(static_cast<sparta::TreeNode*>(&membif)) << std::endl << std::endl;
    std::cout << "BlockingMemoryIF: " << std::endl << *(static_cast<sparta::memory::BlockingMemoryIF*>(&membif)) << std::endl << std::endl;

    std::cout << "Tree:\n" << root.renderSubtree(-1, true) << std::endl;

    root.enterConfiguring();
    root.enterFinalized();

    // Notifications

    MemPostWriteObserver mwo;
    MemReadObserver mro;

    mwo.registerFor(&membif);
    mro.registerFor(&membif);

    uint8_t dat[BLOCK_SIZE];
    uint8_t buf[BLOCK_SIZE];
    dat[0] = 0xff;

    // Read/Write through BlockingMemoryIFNode

    const uint8_t exp_prior[] = {0xcc};
    const uint8_t exp_tried[] = {0xff};
    const uint8_t exp_final[] = {0xff};
    const uint8_t exp_in_suppl[] = {0x00};
    const uint8_t exp_out_suppl = 0;
    mwo.expect(0, 1, exp_prior, exp_tried, exp_final,
              (const void*) exp_in_suppl, (void*) exp_out_suppl);
    EXPECT_NOTHROW(membif.write(0, 1, dat, (const void*) exp_in_suppl));

    const uint8_t exp_read[] = {0xff};
    mro.expect(0, 1, exp_read, (void*) 101, nullptr);
    EXPECT_NOTHROW(membif.read(0, 1, buf, (void*) 101));
    EXPECT_EQUAL(buf[0], 0xff);

    std::unique_ptr<uint8_t[]> exp_prior_large(new uint8_t[BLOCK_SIZE]);
    std::unique_ptr<uint8_t[]> exp_tried_large(new uint8_t[BLOCK_SIZE]);
    std::unique_ptr<uint8_t[]> exp_final_large(new uint8_t[BLOCK_SIZE]);
    for(uint32_t i=0; i<BLOCK_SIZE; ++i){
        exp_prior_large[i] = 0xCC;
        exp_tried_large[i] = i%256;
        exp_final_large[i] = exp_tried_large[i];
    }

    mwo.expect(BLOCK_SIZE, BLOCK_SIZE, exp_prior_large.get(), exp_tried_large.get(), exp_final_large.get(), (void*) 102, nullptr);
    EXPECT_NOTHROW(membif.write(BLOCK_SIZE, BLOCK_SIZE, exp_tried_large.get(), (void*) 102));

    mro.expect(BLOCK_SIZE, BLOCK_SIZE, exp_final_large.get(), (void*) 103, nullptr);
    EXPECT_NOTHROW(membif.read(BLOCK_SIZE, BLOCK_SIZE, buf, (void*) 103));


    // Read/Write directly through mem (no notifications!)
    EXPECT_NOTHROW(mem.write(1, 1, dat));
    EXPECT_NOTHROW(mem.read(1, 1, buf));

    // Peek/Poke through BlockingMemoryIFNode (no notifications!)
    EXPECT_NOTHROW(membif.poke(1, 1, dat));
    EXPECT_NOTHROW(membif.peek(1, 1, buf));

    mwo.deregisterFor(&membif);
    mro.deregisterFor(&membif);

    EXPECT_EQUAL(mwo.writes, 2);
    EXPECT_EQUAL(mro.reads, 2);


    std::vector<sparta::TreeNode::NotificationInfo> info;
    info.clear();
    EXPECT_EQUAL(membif.getPossibleNotifications(info), 0);
    EXPECT_EQUAL(membif.getReadNotificationSource().getPossibleNotifications(info), 1);
    EXPECT_EQUAL(membif.getReadNotificationSource().getPossibleNotifications(info), 1);
    info.clear();
    EXPECT_EQUAL(membif.getPossibleSubtreeNotifications(info), 2);
    EXPECT_EQUAL(info.size(), 2);

    std::vector<sparta::TreeNode*> srcs;
    srcs.clear();
    EXPECT_EQUAL(membif.locateNotificationSources<sparta::memory::BlockingMemoryIFNode::PostWriteAccess>(srcs), 1);
    EXPECT_EQUAL(srcs.size(), 1);
    srcs.clear();
    EXPECT_EQUAL(membif.locateNotificationSources<sparta::memory::BlockingMemoryIFNode::ReadAccess>(srcs), 1);
    EXPECT_EQUAL(srcs.size(), 1);

    std::cout << "Tree:\n" << root.renderSubtree(-1, true) << std::endl;

    std::cout << "Done: " << std::endl << mem << std::endl << mem.getLineStates() << std::endl;

    root.enterTeardown();
}

//! Tests the DebugMemoryIF
void testDebugMemoryIF() {

    std::cout << "\nTesting DebugMemoryIF\nMem size: " << MEM_SIZE << ", Block size: " << BLOCK_SIZE << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4

    sparta::RootTreeNode root;

    // Memory Setup
    sparta::memory::MemoryObject mem(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::TranslationIFNode trans(&root, "trans1", "Translation Interface","virtual", "physical");
    sparta::memory::BlockingMemoryObjectIFNode membif(&root, "mem1", "Blocking memory object", &trans, mem);
    sparta::memory::DebugMemoryIF* dbgmem = &membif; // upcast

    // Print current memory set by the ostream insertion operator
    std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;
    std::cout << "DebugMemoryIF: " << std::endl << dbgmem << std::endl << std::endl;

    root.enterConfiguring();
    root.enterFinalized();


    // Peek & Poke through DebugMemoryIF

    const uint64_t BUF_SIZE = 0x100000; // 1.48576 MB
    // Block-spanning pokes
    assert(BLOCK_SIZE * 2 <= BUF_SIZE); // Required for the following tests
    assert(MEM_SIZE > BLOCK_SIZE * 4); // Required for the following tests

    uint8_t dat[BUF_SIZE];
    uint8_t buf[BUF_SIZE];

    // Normal-sized, safe pokes
    dat[0] = 0xaa;
    EXPECT_NOTHROW(dbgmem->poke(0, 1, dat));
    dat[0] = 0xff;
    EXPECT_NOTHROW(dbgmem->poke(1, 1, dat));
    dat[0] = 0xdd; dat[1] = 0xee;
    EXPECT_NOTHROW(dbgmem->poke(2, 2, dat));
    memset(dat, 0x12, BLOCK_SIZE);
    EXPECT_NOTHROW(dbgmem->poke(BLOCK_SIZE, BLOCK_SIZE, dat));
    dat[0] = 0x34; dat[1] = 0x56;
    EXPECT_NOTHROW(dbgmem->poke(MEM_SIZE - 2, 2, dat));

    // Verify pokes
    EXPECT_NOTHROW(dbgmem->peek(0, 1, buf));
    EXPECT_EQUAL(buf[0], 0xaa);
    EXPECT_NOTHROW(dbgmem->peek(1, 1, buf));
    EXPECT_EQUAL(buf[0], 0xff);
    EXPECT_NOTHROW(dbgmem->peek(2, 2, buf));
    EXPECT_EQUAL(buf[0], 0xdd);
    EXPECT_EQUAL(buf[1], 0xee);
    EXPECT_NOTHROW(dbgmem->peek(BLOCK_SIZE, BLOCK_SIZE, buf));
    for(addr_t i=0; i<BLOCK_SIZE; ++i){
        EXPECT_EQUAL(buf[i], 0x12);
    }
    EXPECT_NOTHROW(dbgmem->peek(MEM_SIZE - 2, 2, buf));
    EXPECT_EQUAL(buf[0], 0x34);
    EXPECT_EQUAL(buf[1], 0x56);

    memset(dat, 0xa0, BLOCK_SIZE * 2);
    EXPECT_NOTHROW(dbgmem->poke(0, BLOCK_SIZE * 2, dat));
    memset(dat, 0xb1, 2);
    EXPECT_NOTHROW(dbgmem->poke(BLOCK_SIZE-1, 2, dat));

    // Verify block-spanning pokes
    EXPECT_NOTHROW(dbgmem->peek(0, BLOCK_SIZE * 2, buf));
    for(addr_t i=0; i<BLOCK_SIZE * 2; ++i){
        if(i < BLOCK_SIZE-1 || i > BLOCK_SIZE){
            EXPECT_EQUAL(buf[i], 0xa0);
        }else{
            EXPECT_EQUAL(buf[i], 0xb1);
        }
    }

    // Giant peeks/pokes
    EXPECT_NOTHROW(dbgmem->poke(0, std::min<uint64_t>(MEM_SIZE, BUF_SIZE), dat));
    EXPECT_NOTHROW(dbgmem->peek(0, std::min<uint64_t>(MEM_SIZE, BUF_SIZE), dat));
    EXPECT_NOTHROW(dbgmem->poke(1, std::min<uint64_t>(MEM_SIZE-1, BUF_SIZE), dat));
    EXPECT_NOTHROW(dbgmem->peek(1, std::min<uint64_t>(MEM_SIZE-1, BUF_SIZE), dat));

    // Illegal peeks/pokes
    EXPECT_THROW(dbgmem->poke(MEM_SIZE - BLOCK_SIZE, BLOCK_SIZE + 1, dat));
    EXPECT_THROW(dbgmem->poke(MEM_SIZE, 1, dat));
    EXPECT_THROW(dbgmem->peek(MEM_SIZE, 1, dat));
    EXPECT_THROW(dbgmem->poke(MEM_SIZE - 1, 2, dat));
    EXPECT_THROW(dbgmem->peek(MEM_SIZE - 1, 2, dat));


    std::cout << "Tree:\n" << root.renderSubtree(-1, true) << std::endl;

    std::cout << "Done: " << std::endl << mem << std::endl << mem.getLineStates() << std::endl;

    root.enterTeardown();
}

//! \brief Prints out performance number of accesses per second and latest performance
void reportPerformance(const std::string& acc_type,
                       uint64_t num_accesses,
                       const boost::timer::cpu_timer& t) {

    auto dflgs = std::cout.flags();
    auto dfill = std::cout.fill();
    auto dprec = std::cout.precision();
    auto dwid = std::cout.width();

    std::cout << std::setw(36) << acc_type << ": "
              << std::right << std::fixed << std::setw(10)
              << std::setprecision(5) << num_accesses/(t.elapsed().user/1000000000.0)/1000000.0
              << " Macc/s for " << std::right << std::setw(10) << num_accesses
              << " accesses, " << std::setfill(' ') << std::right << std::fixed << std::setw(16) << std::setprecision(14)
              << (t.elapsed().user/1000000000.0) / num_accesses << "s each" << std::endl;

    std::cout.flags(dflgs);
    std::cout.fill(dfill);
    std::cout.precision(dprec);
    std::cout .width(dwid);
}

/*!
 * \brief Does a test of memory object read/write performance so that tests with
 * different sparseness impementations can be compared
 */
void testMemoryObjectPerformance() {

    const uint64_t mem_size = 274877906944; // 256 GB
    const uint64_t block_size = 64; // 64B
    const uint64_t num_accesses = 50000; // 50k - Run this many accesses per test. This should be large enough that block_size*num_accesses > l2cache
    const uint64_t report_step = 50000; // 50k - Report performance this often
    const uint64_t num_alloced_accesses = 10000000; // 10M - For read and writes and already-allocated blocks
    const uint64_t report_alloced_step = 10000000; // 10M

    std::cout << "\nTesting MemoryObject Performance Mem size: " << mem_size << ", Block size: " << block_size << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4

    sparta::memory::MemoryObject mem(nullptr, block_size, mem_size);
    std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;

    uint8_t dat[block_size];
    uint8_t buf[block_size];

    // General
    EXPECT_EQUAL(mem.getSize(), mem_size);
    EXPECT_EQUAL(mem.getBlockSize(), block_size);
    EXPECT_EQUAL(mem.getNumBlocks(), mem.getSize() / mem.getBlockSize());
    EXPECT_EQUAL(mem.getFill(), 0xcc);


    // Access performance test
    boost::timer::cpu_timer t;
    const uint64_t num_blocks = mem_size / block_size;
    const uint64_t num_reports = num_accesses / report_step;
    const uint64_t num_reports_alloced = num_alloced_accesses / report_alloced_step;
    const uint64_t num_alloced_blocks = num_accesses;

    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.read(block_size*(rand()%num_blocks), block_size, dat);
        }
        t.stop();
        reportPerformance("Random Read NONE Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports; ++i){
        for(uint64_t j=0; j<report_step; ++j){
            mem.write(block_size*(rand()%num_blocks), block_size, buf);
        }
        reportPerformance("Random Write Allocating", (i+1)*report_step, t);
        t.resume();
    }
    t.stop();

    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.read(block_size*(rand()%num_blocks), block_size, dat);
        }
        t.stop();
        reportPerformance("Random Read Some Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and realize the first <num_accesses> blocks so they are readable in the next test
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_accesses; ++i){
        mem.write(block_size*i, block_size, buf);
    }
    t.stop();
    reportPerformance("Linear Write (ALLOCATING)", num_accesses, t);

    // Walk through and read every realized block
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.read(block_size*(j%num_alloced_blocks), block_size, dat);
        }
        t.stop();
        reportPerformance("Linear Read ALL Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and read every realized block with only 1B to eliminate memcpy overhead
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.read(block_size*(j%num_alloced_blocks), 1, dat);
        }
        t.stop();
        reportPerformance("Linear Read (1B) ALL Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and re-write every realized block
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.write(block_size*(j%num_alloced_blocks), block_size, buf);
        }
        t.stop();
        reportPerformance("Linear Write ALL Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and write every realized block with only 1B to eliminate memcpy overhead
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem.write(block_size*(j%num_alloced_blocks), 1, buf);
        }
        t.stop();
        reportPerformance("Linear Write (1B) ALL Allocated", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and write every realized block with only 1B to eliminate memcpy overhead
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem._lookupAndValidate(block_size*(j%num_alloced_blocks), 1, buf);
        }
        t.stop();
        reportPerformance("_lookupAndValidate (no write)", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Walk through and test canAccess many times
    std::cout << std::endl;
    t.start();
    for(uint64_t i=0; i<num_reports_alloced; ++i){
        for(uint64_t j=0; j<report_alloced_step; ++j){
            mem._canAccess(block_size*(j%num_alloced_blocks), 1, buf);
        }
        t.stop();
        reportPerformance("_canAccess", (i+1)*report_alloced_step, t);
        t.resume();
    }
    t.stop();

    // Write all over the place for fun
    std::cout << std::endl;
    t.start();
    const uint64_t acc_mult = 12;
    for(uint64_t i=0; i<num_accesses*acc_mult; ++i){
        mem.write(block_size*(rand()%num_blocks), block_size, buf);
    }
    reportPerformance("More random Write Allocating", num_accesses*acc_mult, t);
    t.stop();

    std::cout << "Done: " << std::endl << mem << std::endl;// << mem.getLineStates() << std::endl;

    auto& lmap = mem.getLineMap();
    std::cout << "MemoryObject nodes: " << lmap.getNumNodes()
              << ", tiers: " << lmap.getNumTiers()
              << ", est mem(MB): " << std::setprecision(4) << lmap.getEstimatedMemory()/1000000.0 << std::endl;
}


/*!
 * \brief Does a test of memory object sizes to make sure there are no errors
 */
void testMemoryObjectSizes() {

    const uint64_t MIN_MEM_SIZE = 2;
    const uint64_t MAX_MEM_SIZE = 2199023255552; // 2 TB

    const uint64_t MIN_BLOCK_SIZE = 2;
    const uint64_t MAX_BLOCK_SIZE = 4096;

    for(uint64_t mem_size = MIN_MEM_SIZE; mem_size <= MAX_MEM_SIZE; mem_size*=2){
        for(uint64_t block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size*=2){
            if(mem_size >= block_size){
                sparta::memory::MemoryObject mem(nullptr, block_size, mem_size);

                uint8_t b = 0xbb;
                mem.write(0, 1, &b);
                mem.write(mem_size-1, 1, &b);

                mem.read(0, 1, &b);
                mem.read(mem_size-1, 1, &b);

                std::cout << "MemoryObject: " << std::endl << mem << std::endl << std::endl;
            }
        }
    }
}
