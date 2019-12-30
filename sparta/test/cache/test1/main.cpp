#include <iostream>
#include "cache/Cache.hpp"
#include "cache/BasicCacheSet.hpp"
#include "cache/LineData.hpp"
#include "cache/RandomReplacement.hpp"

using namespace sparta;

typedef cache::LineData       MyLine;
typedef cache::Cache<MyLine>  MyCache;
const uint32_t cache_sz_kb = 32;
const uint32_t line_sz     = 64;
const uint32_t stride      = 64;
const uint32_t num_ways    = 8;
MyCache cache_(cache_sz_kb,
           line_sz,
           stride,
           cache::LineData(64),
           cache::RandomReplacement(num_ways)  );
const cache::DefaultAddrDecoder *addr_decoder_;

void write32(uint64_t addr, uint32_t val);
void read32(uint64_t addr, uint32_t &val);
void ReadLineFromMemory(uint64_t addr, MyLine &line);
void manuallySearchForTag(uint64_t addr);
void snoopToShared(uint64_t addr);

int main()
{

    addr_decoder_ = dynamic_cast<const cache::DefaultAddrDecoder *>(cache_.getAddrDecoder());

    uint64_t addr=0x210000;
    uint32_t val=0;

    write32(addr,        0x11111110);
    write32(addr+0x1000, 0x11111111);
    write32(addr+0x2000, 0x11111112);
    write32(addr+0x3000, 0x11111113);
    write32(addr+0x4000, 0x11111114);
    write32(addr+0x5000, 0x11111115);
    write32(addr+0x6000, 0x11111116);
    write32(addr+0x7000, 0x11111117);
    write32(addr+0x8000, 0x11111118);
    write32(addr+0x9000, 0x11111119);
    write32(addr+0xA000, 0x111111a0);
    write32(addr+0xA004, 0x111111a4);
    write32(addr+0xA008, 0x111111a8);

    read32(addr, val);
    std::cout << "val=0x" << std::hex << val << std::endl;

    read32(addr+0xA000, val);
    std::cout << "val=0x" << std::hex << val << std::endl;

    read32(addr+0xA004, val);
    std::cout << "val=0x" << std::hex << val << std::endl;

    read32(addr+0xA008, val);
    std::cout << "val=0x" << std::hex << val << std::endl;


    manuallySearchForTag(addr);


    read32(addr+0xA008, val);
    std::cout << "val=0x" << std::hex << val << std::endl;
    std::cout << "TEST PASSED\n";
    return 0;

}

// read with allocate
void read32(uint64_t addr, uint32_t &val)
{
    MyLine *line = cache_.getItem(addr);
    if ( (line == nullptr) || !line->isValid() ){
    MyLine &victim_line = cache_.getLRUItem(addr);
    if (victim_line.isValid() &&
        victim_line.isModified() )
    {
        // cast-out
        std::cout << "- castout: addr=0x" << std::hex << victim_line.getAddr()
              << " way=" << std::dec << victim_line.getWay()
              << std::endl;
    }

    ReadLineFromMemory(addr, victim_line);

    // Update item with new address
    // XXX: should the cache library handle this?  If so, how?
    victim_line.setAddr(addr);
    victim_line.setValid(true);

    line = &victim_line;
    }

    uint32_t offset = addr_decoder_->calcBlockOffset(addr);
    val =  line->read<uint32_t, BE>(offset);

    // update replacement
    uint32_t line_way = line->getWay();
    cache::ReplacementIF *rep = cache_.getReplacementIF(addr);
    rep->touchMRU(line_way);

}

void write32(uint64_t addr, uint32_t val)
{
    MyLine *line = cache_.getItem(addr);
    if ( line == nullptr ) {
    MyLine &victim_line = cache_.getLRUItem(addr);
    if (victim_line.isValid() &&
        victim_line.isModified() )
    {
        // cast-out
        uint64_t castout_addr = victim_line.getAddr();
        std::cout << "- castout: addr=0x" << std::hex << castout_addr
              << " way=" << std::dec << victim_line.getWay()
              << std::endl;
    }

    ReadLineFromMemory(addr, victim_line);

    // User's responsible for updating line with new addr and tag
    victim_line.setValid(true);
    victim_line.setAddr(addr);

    line = &victim_line;
    }

    uint32_t offset = addr_decoder_->calcBlockOffset(addr);
    line->write<uint32_t, BE>(offset, val);
    line->setModified(true);

    // update replacement
    uint32_t line_way = line->getWay();
    cache::ReplacementIF *rep = cache_.getReplacementIF(addr);
    rep->touchMRU(line_way);


}


void ReadLineFromMemory(uint64_t addr, MyLine &line)
{
    // write the addr to the first word, the rest are 0's

    uint32_t line_sz   = line.getLineSize();
    uint32_t num_words = line_sz/4;

    for (uint32_t i=0; i<num_words; ++i) {
    line.write<uint32_t, BE>(i*4, 0);
    }
    line.write<uint32_t, BE>(0, addr);
}

void snoopToShared(uint64_t addr)
{
    MyLine *line = cache_.getItem(addr);
    if ( (line == 0) || !line->isValid() )
    return;

    // XXX:  code will break if coherency type is not MESICoherency
    line->setShared(true);

}


void manuallySearchForTag(uint64_t addr)
{
    const cache::Cache<MyLine> &const_cache        = cache_;
    const cache::BasicCacheSet<MyLine> &set             = const_cache.peekCacheSet(addr);
    //cache::CacheSet<MyLine>::const_iterator it     = set.begin();
    //cache::CacheSet<MyLine>::const_iterator it_end = set.end();
    //for (; it !=it_end; ++it) {
    for (auto line : set) {
    //const MyLine &line= *it;
    if (line.isValid()) {
        std::cout << "tag=0x" << std::hex << line.getTag() << std::endl;
    }
    }

}
