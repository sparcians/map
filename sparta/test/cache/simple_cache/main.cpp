

#include "DL1.hpp"
#include "cache/SimpleCache2.hpp"

void same_line_read_write_test();
void same_set_read_write_test();

static const uint32_t LINE_SIZE=64;
static const uint32_t LINE_OFFSET_MASK = LINE_SIZE-1;

// Test 1
DL1<sparta::cache::LineData>
dl1_(32,  // 32KB in size
     LINE_SIZE,  // line size
     LINE_SIZE,  // stride
     sparta::cache::LineData(LINE_SIZE), // line_size
     sparta::cache::LRUReplacement(8) );   // num_ways

sparta::cache::SimpleCache<sparta::cache::LineData>
l2_(512, // 512KB
    LINE_SIZE,  // line size
    LINE_SIZE,  // stride
    sparta::cache::LineData(LINE_SIZE),  // line size
    sparta::cache::TreePLRUReplacement(16) );  // num_ways


// Test 2
sparta::cache::SimpleCache2<sparta::cache::LineData>
btb_a_(4, // 4kb
    LINE_SIZE, // line size
    LINE_SIZE, // stride
    sparta::cache::LineData(LINE_SIZE),  // default line copied for initialization
    sparta::cache::LRUReplacement(4) );  // num_ways

sparta::cache::SimpleCache2<sparta::cache::LineData>
btb_b_(4096, // 4kb
    LINE_SIZE, // line size
    LINE_SIZE, // stride
    sparta::cache::LineData(LINE_SIZE),  // default line copied for initialization
    sparta::cache::TreePLRUReplacement(4), // num_ways
    false);  // cache_size_unit_is_kb

// Test 1
void read_test(uint32_t block_size_kb)
{
    dl1_.resetStats();
    l2_.resetStats();
    std::cout << std::endl;
    std::cout  << "READ TEST:  block_size=" << block_size_kb << "KB" << std::endl;

    uint8_t  data[4] = {1,1,1,1};
    uint32_t block_size = block_size_kb*1024;
    for (uint32_t i=0; i<1000; ++i)
    {
        uint64_t addr = 0;
        while (addr < block_size)
        {
            dl1_.read(addr, sizeof(data), data);
            addr += LINE_SIZE;
        }
    }

    std::cout << "L1 Stats:" << std::endl;
    std::cout << dl1_.getStatDisplayString() << std::endl;
    std::cout << "L2 Stats:" << std::endl;
    std::cout << l2_.getStatDisplayString() << std::endl;
}

void read_getline_test(uint32_t block_size_kb)
{

    // if you do:
    //    read_test(N);
    //    read_getline_test(N);
    // the read_getline_test stats should look the same as read_test stats with
    // the following changes:
    //   * num_reads will be reported as 0 because you are bypassing SimpleCache::read()
    //   * num_read_misses will be reported as num_getline_misses. Again
    //     because you are bypassing SimpleCache::read() and the counting in
    //     SimpleCache can't distinguish between read & write. users of SimpleCache::getLine()
    //     need to have their own counters to break down the getline_misses

    dl1_.resetStats();
    l2_.resetStats();
    std::cout << std::endl;
    std::cout  << "READ GETLINE TEST:  block_size=" << block_size_kb << "KB" << std::endl;

    uint8_t  data[4] = {1,1,1,1};
    uint32_t block_size = block_size_kb*1024;
    for (uint32_t i=0; i<1000; ++i)
    {
        uint64_t addr = 0;
        while (addr < block_size)
        {
            auto line = dl1_.getLine(addr);
            uint32_t offset = addr & LINE_OFFSET_MASK;
            line->read(offset, sizeof(data), data);
            addr += LINE_SIZE;
        }
    }

    std::cout << "L1 Stats:" << std::endl;
    std::cout << dl1_.getStatDisplayString() << std::endl;
    std::cout << "L2 Stats:" << std::endl;
    std::cout << l2_.getStatDisplayString() << std::endl;
}


void write_test(uint32_t block_size_kb)
{
    dl1_.resetStats();
    l2_.resetStats();

    std::cout << std::endl;
    std::cout  << "WRITE TEST:  block_size=" << block_size_kb << "KB" << std::endl;

    uint8_t  data[4] = {1,1,1,1};
    uint32_t block_size = block_size_kb*1024;
    for (uint32_t i=0; i<1000; ++i)
    {
        uint64_t addr = 0;
        while (addr < block_size)
        {
            dl1_.write(addr, sizeof(data), data);
            addr += LINE_SIZE;
        }
    }

    std::cout << "L1 Stats:" << std::endl;
    std::cout << dl1_.getStatDisplayString() << std::endl;
    std::cout << "L2 Stats:" << std::endl;
    std::cout << l2_.getStatDisplayString() << std::endl;
}

void write_getline_test(uint32_t block_size_kb)
{
    dl1_.resetStats();
    l2_.resetStats();

    std::cout << std::endl;
    std::cout  << "WRITE GETLINE TEST:  block_size=" << block_size_kb << "KB" << std::endl;

    uint8_t  data[4] = {1,1,1,1};
    uint32_t block_size = block_size_kb*1024;
    for (uint32_t i=0; i<1000; ++i)
    {
        uint64_t addr = 0;
        while (addr < block_size)
        {
            auto line = dl1_.getLine(addr);
            uint32_t offset = addr & LINE_OFFSET_MASK;
            line->write(offset, sizeof(data), data);
            addr += LINE_SIZE;
        }
    }

    std::cout << "L1 Stats:" << std::endl;
    std::cout << dl1_.getStatDisplayString() << std::endl;
    std::cout << "L2 Stats:" << std::endl;
    std::cout << l2_.getStatDisplayString() << std::endl;
}

// Test 2
void check_equivalency(const uint64_t& addr) {
    sparta_assert(btb_a_.getNumWays() == btb_b_.getNumWays());
    sparta_assert(btb_a_.getNumSets() == btb_b_.getNumSets());
    auto dec_a_ = btb_a_.getAddrDecoder();
    auto dec_b_ = btb_b_.getAddrDecoder();
    sparta_assert(dec_a_ && dec_b_);
    sparta_assert(dec_a_->calcIdx(addr) == dec_b_->calcIdx(addr));
    sparta_assert(dec_a_->calcTag(addr) == dec_b_->calcTag(addr));
    sparta_assert(dec_a_->calcBlockAddr(addr) == dec_b_->calcBlockAddr(addr));
    sparta_assert(dec_a_->calcBlockOffset(addr) == dec_b_->calcBlockOffset(addr));
}

int main()
{

    // 1. Test 1
    dl1_.setWriteAllocateMode(true);
    l2_.setWriteAllocateMode(true);
    dl1_.setL2(&l2_);

#if 0
    read_test(16); // block size
    read_test(32);
    read_test(64);
    read_test(1024);

    read_getline_test(16); // block size
    read_getline_test(32);
    read_getline_test(64);
    read_getline_test(1024);

    write_test(16); // block size
    write_test(32);
    write_test(64);
    write_test(1024);

    write_getline_test(16); // block size
    write_getline_test(32);
    write_getline_test(64);
    write_getline_test(1024);
#endif

    same_line_read_write_test();
    same_set_read_write_test();

    // 2. Test 2
    check_equivalency(0x0);
    check_equivalency(0xFFFFFFFFFFFFFFFF);
    check_equivalency(0x00F1F2F3F4F5F6F7);

    return 0;
}
