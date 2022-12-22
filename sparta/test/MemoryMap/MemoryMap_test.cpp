
#include <inttypes.h>
#include <iostream>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/memory/SimpleMemoryMapNode.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/Utils.hpp"


/*!
 * \file main.cpp
 * \brief Test for sparta MemoryMap and MemoryMapNode
 *
 * These are simple blocking memory interfaces and storage classes
 */

TEST_INIT

#define MEM_SIZE 4096
#define BLOCK_SIZE 64

using sparta::memory::addr_t;

void testMemoryMap();

int main()
{
    testMemoryMap();

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
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

public:
    uint32_t writes;

    MemPostWriteObserver() :
        expect_addr_(0xdefec8ed),
        expect_size_(0xdefec8ed),
        expect_prior_(nullptr),
        expect_tried_(nullptr),
        expect_final_(nullptr),
        expect_in_supplement_(nullptr),
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
     * \param supplement Expected supplementary pointer in future notifications
     */
    void expect(addr_t addr,
                addr_t size,
                const uint8_t* prior,
                const uint8_t* tried,
                const uint8_t* final,
                const void* in_supplement) {
        expect_addr_ = addr;
        expect_size_ = size;
        expect_prior_ = prior;
        expect_tried_ = tried;
        expect_final_ = final;
        expect_in_supplement_ = in_supplement;
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

public:
    uint32_t reads;

    MemReadObserver() :
        expect_addr_(0xdefec8ed),
        expect_size_(0xdefec8ed),
        expect_data_(nullptr),
        expect_in_supplement_(nullptr),
        reads(0)
    { }

    /*!
     * \brief Sets expected data during next callback.
     * Causes callback to indicate test error if callback data does not match
     * \param addr Expected address of next accesses
     * \param size Expected size of next accesses
     * \param data Expected data read by next accesses (Must contain at least
     * \a size bytes). Caller maintains ownership
     * \param supplement, Expected supplementary pointer in notificaiton for future accesses
     */
    void expect(addr_t addr,
                addr_t size,
                const uint8_t* data,
                const void* in_supplement) {
        expect_addr_ = addr;
        expect_size_ = size;
        expect_data_ = data;
        expect_in_supplement_ = in_supplement;
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

//! Test the SimpleMemoryMapNode
void testMemoryMap() {

    std::cout << "\nTesting SimpleMemoryMapNode\n" << std::endl << std::endl;
    sparta_assert(BLOCK_SIZE >= 4); // Test requires block size >= 4
    sparta_assert(MEM_SIZE >= 0x40); // Test requires mem size >= 0x40

    sparta::RootTreeNode root;

    // Memory Setup
    sparta::memory::MemoryObject m1(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::BlockingMemoryObjectIFNode mif1(&root, "m1", "memory object 1", nullptr, m1);
    sparta::memory::MemoryObject m2(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::BlockingMemoryObjectIFNode mif2(&root, "m2", "memory object 2", nullptr, m2);
    sparta::memory::MemoryObject m3(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::BlockingMemoryObjectIFNode mif3(&root, "m3", "memory object 3", nullptr, m3);
    sparta::memory::MemoryObject m4(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::BlockingMemoryObjectIFNode mif4(&root, "m4", "memory object 4", nullptr, m4);
    sparta::memory::MemoryObject m5(nullptr, BLOCK_SIZE, MEM_SIZE);
    sparta::memory::BlockingMemoryObjectIFNode mif5(&root, "m5", "memory object 5", nullptr, m5);

    // Print current memory set by the ostream insertion operator
    std::cout << "MemoryObject: " << std::endl << m1 << std::endl;
    std::cout << "MemoryObject: " << std::endl << m2 << std::endl;
    std::cout << "MemoryObject: " << std::endl << m3 << std::endl;
    std::cout << "MemoryObject: " << std::endl << m4 << std::endl;
    std::cout << "MemoryObject: " << std::endl << m5 << std::endl;

    // Print current memory set by the ostream insertion operator
    std::cout << "Tree:\n" << root.renderSubtree(-1, true) << std::endl;

    // Add a map object

    sparta::memory::SimpleMemoryMapNode mmap(&root, "map", "Test mapping object", BLOCK_SIZE, 0x1000);
    EXPECT_EQUAL(mmap.getBlockSize(), BLOCK_SIZE);

    root.enterConfiguring();
    root.enterFinalized();

    std::cout << "\nbefore\n";
    mmap.dumpTree(std::cout);
    EXPECT_NOTHROW(mmap.addMapping(0x100, 0x200, &mif1, 0x0)); // Maps [0x100, 0x200) to m1: [0x0, 0x100)
    std::cout << "\nafter mif1\n";
    mmap.dumpTree(std::cout);
    EXPECT_NOTHROW(mmap.addMapping(0x500, 0x700, &mif2, 0x0)); // Maps [0x500, 0x700) to m2: [0x0, 0x200)
    std::cout << "\nafter mif2\n";
    mmap.dumpTree(std::cout);
    EXPECT_THROW(mmap.addMapping(0x600, 0x640, &mif3, 0x0)); // ERROR: COLLIDES WITH mif2 MAPPING
    std::cout << "\nafter mif3\n";
    mmap.dumpTree(std::cout);
    EXPECT_NOTHROW(mmap.addMapping(0x300, 0x400, &mif4, 0x0)); // Maps [0x300, 0x400) to m4: [0x0, 0x100)
    std::cout << "\nafter mif4\n";
    mmap.dumpTree(std::cout);
    EXPECT_THROW(mmap.addMapping(0x401, 0x435, &mif4, 0x0)); // ERROR: Input range NOT BLOCK ALIGNED
    std::cout << "\nafter mif4(2)\n";
    mmap.dumpTree(std::cout);
    EXPECT_THROW(mmap.addMapping(0x800, 0x800, &mif4, 0x0)); // ERROR: Input range is size 0
    std::cout << "\nafter mif4(2)\n";
    mmap.dumpTree(std::cout);
    EXPECT_NOTHROW(mmap.addMapping(0x400, 0x500, &mif5, 0x0)); // Maps [0x400, 0x500) to m5: [0x0, 0x100) (shares edges with m2 & m4)
    std::cout << "\nafter mif5\n";
    mmap.dumpTree(std::cout);
    EXPECT_NOTHROW(mmap.addMapping(0x0, 0x40, &mif5, 0x40)); // Maps [0x0, 0x40) to m5: [0x40, 0x80) (needed to ensure placing a node at 0 is ok)
    std::cout << "\nafter mif5(2)\n";
    mmap.dumpTree(std::cout);
    EXPECT_THROW(mmap.addMapping(0x840, 0x880, &mif5, 0x33)); // ERROR: dest_off arg is not a block_size multiple
    std::cout << "\nafter mif5(3)\n";
    mmap.dumpTree(std::cout);

    EXPECT_EQUAL(mmap.getNumMappings(), 5);

    // Dump
    std::cout << "\nMappings: " << std::endl;
    mmap.dumpMappings(std::cout);

    // Explicity find a mapping at each endpoint.
    EXPECT_EQUAL(mmap.findInterface(0x00), &mif5);
    EXPECT_EQUAL(mmap.findInterface(0x3f), &mif5);
    EXPECT_EQUAL(mmap.findInterface(0x40), nullptr);
    EXPECT_EQUAL(mmap.findInterface(0xff), nullptr);
    EXPECT_EQUAL(mmap.findInterface(0x100), &mif1);
    EXPECT_EQUAL(mmap.findInterface(0x1ff), &mif1);
    EXPECT_EQUAL(mmap.findInterface(0x200), nullptr);
    EXPECT_EQUAL(mmap.findInterface(0x2ff), nullptr);
    EXPECT_EQUAL(mmap.findInterface(0x300), &mif4);
    EXPECT_EQUAL(mmap.findInterface(0x3ff), &mif4);
    EXPECT_EQUAL(mmap.findInterface(0x400), &mif5);
    EXPECT_EQUAL(mmap.findInterface(0x4ff), &mif5);
    EXPECT_EQUAL(mmap.findInterface(0x500), &mif2);
    EXPECT_EQUAL(mmap.findInterface(0x6ff), &mif2);

    EXPECT_NOTHROW(mmap.verifyHasMapping(0x100,0x100)); // Second arg is size
    EXPECT_NOTHROW(mmap.verifyHasMapping(0x0,1));
    EXPECT_NOTHROW(mmap.verifyHasMapping(0x500,1));
    EXPECT_THROW(mmap.verifyHasMapping(0x4f0,0x20)); // Spans 0x500, which separates mif5 and mif2
    EXPECT_NOTHROW(mmap.verifyHasMapping(0x4f0,0x10));

    uint8_t dat[BLOCK_SIZE];
    uint8_t buf[BLOCK_SIZE];

    // Do some writes and reads

    dat[0] = 0xff;
    EXPECT_NOTHROW(mmap.write(0x0, 1, dat));
    EXPECT_THROW(mmap.write(0x40, 1, dat));
    buf[0] = 0;
    buf[1] = 0x12;
    EXPECT_NOTHROW(mmap.read(0x0, 1, buf));
    EXPECT_THROW(mmap.read(0x40, 1, buf));
    EXPECT_EQUAL(buf[0], dat[0]);


    // Notifications

    MemPostWriteObserver mwos[6];
    MemReadObserver mros[6];

    // Expect notifications on the map itself

    mwos[0].registerFor(&mmap);
    mros[0].registerFor(&mmap);

    // Expect notifications on destinations

    mwos[1].registerFor(&mif1);
    mros[1].registerFor(&mif1);

    mwos[2].registerFor(&mif2);
    mros[2].registerFor(&mif2);

    mwos[3].registerFor(&mif3);
    mros[3].registerFor(&mif3);

    mwos[4].registerFor(&mif4);
    mros[4].registerFor(&mif4);

    mwos[5].registerFor(&mif5);
    mros[5].registerFor(&mif5);


    const void* suppl = nullptr;

    std::cout << "\nWriting 0xdeadbeef to 0x0" << std::endl;
    memset(buf, 0xcc, 4);
    buf[0] = 0xff;
    uint32_t val = 0xdeadbeef;
    memcpy(dat, &val, 4);
    mwos[0].expect(0x0, 4, buf, dat, dat, suppl);
    mwos[5].expect(0x40, 4, buf, dat, dat, suppl); // 0x0 maps to m5: 0x40
    EXPECT_EQUAL(mmap.mapAddress(0x0).first, &mif5);
    EXPECT_EQUAL(mmap.mapAddress(0x0).second, 0x40);
    EXPECT_NOTHROW(mmap.write(0x0, 4, dat, suppl));

    std::cout << "\nWriting 0xdefec8ed to 0x1fc" << std::endl;
    memset(buf, 0xcc, 4);
    val = 0xdefec8ed;
    memcpy(dat, &val, 4);
    mwos[0].expect(0x1fc, 4, buf, dat, dat, suppl);
    mwos[1].expect(0xfc, 4, buf, dat, dat, suppl); // 0x1fc maps to m1: 0xfc
    EXPECT_EQUAL(mmap.mapAddress(0x1fc).first, &mif1);
    EXPECT_EQUAL(mmap.mapAddress(0x1fc).second, 0xfc);
    EXPECT_NOTHROW(mmap.write(0x1fc, 4, dat, suppl));

    std::cout << "\nWriting 0xc0ffeeee to 0x501" << std::endl;
    memset(buf, 0xcc, 4);
    val = 0xc0ffeeee;
    memcpy(dat, &val, 4);
    mwos[0].expect(0x501, 4, buf, dat, dat, suppl);
    mwos[2].expect(0x1, 4, buf, dat, dat, suppl); // 0x501 maps to m2: 1
    EXPECT_EQUAL(mmap.mapAddress(0x501).first, &mif2);
    EXPECT_EQUAL(mmap.mapAddress(0x501).second, 0x1);
    EXPECT_NOTHROW(mmap.write(0x501, 4, dat, suppl));

    std::cout << "\nWriting 0xf1abf00d to 0x3fc" << std::endl;
    memset(buf, 0xcc, 4);
    val = 0xf1abf00d;
    memcpy(dat, &val, 4);
    mwos[0].expect(0x3fc, 4, buf, dat, dat, suppl);
    mwos[4].expect(0xfc, 4, buf, dat, dat, suppl); // 0x3fc maps to m4: 0xfc
    EXPECT_EQUAL(mmap.mapAddress(0x3fc).first, &mif4);
    EXPECT_EQUAL(mmap.mapAddress(0x3fc).second, 0xfc);
    EXPECT_NOTHROW(mmap.write(0x3fc, 4, dat, suppl));

    std::cout << "\nWriting 0xc0011eaf to 0x400" << std::endl;
    memset(buf, 0xcc, 4);
    val = 0xc0011eaf;
    memcpy(dat, &val, 4);
    mwos[0].expect(0x400, 4, buf, dat, dat, suppl);
    mwos[5].expect(0x0, 4, buf, dat, dat, suppl); // 0x400 maps to m5: 0x0
    EXPECT_EQUAL(mmap.mapAddress(0x400).first, &mif5);
    EXPECT_EQUAL(mmap.mapAddress(0x400).second, 0x0);
    EXPECT_NOTHROW(mmap.write(0x400, 4, dat, suppl));

    EXPECT_EQUAL(mmap.mapAddress(0x800).first, nullptr);
    EXPECT_EQUAL(mmap.mapAddress(0x840).first, nullptr);
    EXPECT_THROW(mmap.write(0x800, 4, dat, suppl)); // ERROR: Maps to NOTHING
    EXPECT_THROW(mmap.write(0x840, 4, dat, suppl)); // ERROR: Maps to NOTHING

    // Peek & Poke

    std::cout << "\nPoking 0x12 to 0x0" << std::endl;
    dat[0] = 0x12;
    EXPECT_NOTHROW(mmap.poke(0x0, 1, dat));
    EXPECT_NOTHROW(mmap.peek(0x0, 1, buf));
    EXPECT_EQUAL(buf[0], dat[0]);

    std::cout << "\nPoking 0xdeadbeef to 0x100" << std::endl;
    memset(buf, 0xcc, 4);
    val = 0xdeadbeef;
    memcpy(dat, &val, 4);
    EXPECT_NOTHROW(mmap.poke(0x100, 4, dat));
    EXPECT_NOTHROW(mmap.peek(0x100, 4, buf));
    for(uint32_t i = 0; i < 4; ++i){
        EXPECT_EQUAL(buf[i], dat[i]);
    }

    std::cout << "\nPoking 0x45 to [0x1c0,0x1c0+BLOCK_SIZE)" << std::endl;
    memset(dat, 0x45, BLOCK_SIZE);
    EXPECT_NOTHROW(mmap.poke(0x1c0, BLOCK_SIZE, dat));
    EXPECT_NOTHROW(mmap.peek(0x1c0, BLOCK_SIZE, buf));
    for(uint32_t i = 0; i < BLOCK_SIZE; ++i){
        EXPECT_EQUAL(buf[i], dat[i]);
    }

    // Test read/write notification counts
    // Note that peeks & pokes do not generate notifications

    EXPECT_EQUAL(mwos[0].writes, 5);
    EXPECT_EQUAL(mwos[1].writes, 1);
    EXPECT_EQUAL(mwos[2].writes, 1);
    EXPECT_EQUAL(mwos[3].writes, 0);
    EXPECT_EQUAL(mwos[4].writes, 1);
    EXPECT_EQUAL(mwos[5].writes, 2);


    // Get some DMI pointers
    // Current mapping up to this point
    // map: [    0,  0x40) -> "memory object 5" +0x40
    // map: [0x100, 0x200) -> "memory object 1" +0x0
    // map: [0x300, 0x400) -> "memory object 4" +0x0
    // map: [0x400, 0x500) -> "memory object 5" +0x0
    // map: [0x500, 0x700) -> "memory object 2" +0x0
    EXPECT_NOTEQUAL(mmap.getDMI(  0x0, BLOCK_SIZE), nullptr);
    EXPECT_NOTEQUAL(mmap.getDMI(0x100, BLOCK_SIZE), nullptr);
    EXPECT_NOTEQUAL(mmap.getDMI(0x300, BLOCK_SIZE), nullptr);
    EXPECT_NOTEQUAL(mmap.getDMI(0x400, BLOCK_SIZE), nullptr);
    EXPECT_NOTEQUAL(mmap.getDMI(0x500, BLOCK_SIZE), nullptr);

    // Try getting a DMI to 4K of memory.  This will be illegal as the
    // object at that location can only allow access to BLOCK_SIZE of
    // data at a time.
    EXPECT_EQUAL(mmap.getDMI(0x100, MEM_SIZE), nullptr);

    // Block m5 is mapped in 2 places: one at 0x0 -> 0x40 for offest
    // of 0x40 and at 0x400.  We should be able to get DMIs to both
    // locations via two different calls.  In theory the DMIs should be the same
    sparta::memory::DMIBlockingMemoryIF * m5bmi_offset = mmap.getDMI(0x0, BLOCK_SIZE);
    sparta::memory::DMIBlockingMemoryIF * m5bmi = mmap.getDMI(0x440, BLOCK_SIZE);
    EXPECT_TRUE(m5bmi_offset->getRawDataPtr() == m5bmi->getRawDataPtr());

    std::cout << "Tree:\n" << root.renderSubtree(-1, true) << std::endl;

    std::cout << "Done: " << std::endl
              << m1 << std::endl << m1.getLineStates() << std::endl
              << m2 << std::endl << m2.getLineStates() << std::endl
              << m3 << std::endl << m3.getLineStates() << std::endl
              << m4 << std::endl << m4.getLineStates() << std::endl
              << m5 << std::endl << m5.getLineStates() << std::endl
              << std::endl;

    root.enterTeardown();
}
