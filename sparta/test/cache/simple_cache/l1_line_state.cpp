
#include "DL1.hpp"
extern DL1<sparta::cache::LineData> dl1_;

void same_line_read_write_test()
{

    static const uint64_t addr=0x7000;
    uint8_t  rdata[4] = {1,1,1,1};
    uint8_t  wdata[4] = {7,7,7,7};
    const sparta::cache::LineData* line = nullptr;

    std::cout << std::endl;
    std::cout  << "SAME LINE READ/WRITE TEST" << std::endl;

    // read-read
    if ( dl1_.isHit(addr) )
        dl1_.invalidateLine(addr);
    dl1_.read(addr, sizeof(rdata), rdata);
    dl1_.read(addr, sizeof(rdata), rdata);
    line = dl1_.peekLine(addr);
    sparta_assert( !line->isModified(),
                       "SimpleCacheTest:  expected line to be unmodified");
    std::cout << "    Read-read:  PASSED" << std::endl;



    // read-write
    if ( dl1_.isHit(addr) )
        dl1_.invalidateLine(addr);
    dl1_.read(addr, sizeof(rdata), rdata);
    dl1_.write(addr, sizeof(wdata), wdata);
    line = dl1_.peekLine(addr);
    sparta_assert( line->isModified(),
                       "SimpleCacheTest:  expected line to be modified");
    std::cout << "   Read-write:  PASSED" << std::endl;


    // write-read
    if ( dl1_.isHit(addr) )
        dl1_.invalidateLine(addr);
    dl1_.write(addr, sizeof(wdata), wdata);
    dl1_.read(addr, sizeof(rdata), rdata);
    line = dl1_.peekLine(addr);
    sparta_assert( line->isModified(),
                       "SimpleCacheTest:  expected line to be modified");
    std::cout << "   Write-read:  PASSED" << std::endl;


    // write-write
   if ( dl1_.isHit(addr) )
        dl1_.invalidateLine(addr);
    dl1_.write(addr, sizeof(wdata), wdata);
    dl1_.read(addr, sizeof(rdata), rdata);
    line = dl1_.peekLine(addr);
    sparta_assert( line->isModified(),
                       "SimpleCacheTest:  expected line to be modified");
    std::cout << "  Write-write:  PASSED" << std::endl;

}


void same_set_read_write_test()
{
    static const uint64_t addr=0x7000;
    uint64_t addr2 = addr;
    uint8_t  rdata[4] = {1,1,1,1};
    uint8_t  wdata[4] = {7,7,7,7};
    const sparta::cache::LineData* line = nullptr;

    std::cout << std::endl;
    std::cout  << "SAME SET  READ/WRITE TEST" << std::endl;


    // read-read
    dl1_.invalidateAll();
    dl1_.read(addr - 4*1024, sizeof(rdata), rdata);
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        dl1_.read(addr2, sizeof(rdata), rdata);  // read miss will allocate a line
        addr2 += 4*1024;
    }
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        line = dl1_.peekLine(addr2);
        sparta_assert( (line != nullptr),
                           "SimpleCacheTest:  expected line to be !nullptr, addr=0x" << std::hex << addr2);
        sparta_assert(  !line->isModified(),
                           "SimpleCacheTest:  expected line to be modified, addr=0x" << std::hex << addr2);
        addr2 += 4*1024;
    }
    std::cout << "    Read-read:  PASSED" << std::endl;


    // read-write
    dl1_.invalidateAll();
    dl1_.read(addr - 4*1024, sizeof(rdata), rdata);
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        dl1_.write(addr2, sizeof(wdata), wdata);
        addr2 += 4*1024;
    }
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        line = dl1_.peekLine(addr2);
        sparta_assert( (line != nullptr),
                           "SimpleCacheTest:  expected line to be !nullptr, addr=0x" << std::hex << addr2);
        sparta_assert(  line->isModified(),
                           "SimpleCacheTest:  expected line to be modified, addr=0x" << std::hex << addr2);
        addr2 += 4*1024;
    }
    std::cout << "   Read-write:  PASSED" << std::endl;

    // write-read
    dl1_.invalidateAll();
    dl1_.write(addr - 4*1024, sizeof(wdata), wdata);
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        dl1_.read(addr2, sizeof(rdata), rdata);  // read miss will allocate a line
        addr2 += 4*1024;
    }
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        line = dl1_.peekLine(addr2);
        sparta_assert( (line != nullptr),
                           "SimpleCacheTest:  expected line to be !nullptr, addr=0x" << std::hex << addr2);
        sparta_assert(  !line->isModified(),
                           "SimpleCacheTest:  expected line to be modified, addr=0x" << std::hex << addr2);
        addr2 += 4*1024;
    }
    std::cout << "   write-read:  PASSED" << std::endl;

    // write-write
    dl1_.invalidateAll();
    dl1_.write(addr - 4*1024, sizeof(wdata), wdata);
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        dl1_.write(addr2, sizeof(wdata), wdata);
        addr2 += 4*1024;
    }
    addr2 = addr;
    for (uint32_t i=0; i<8; ++i) {
        line = dl1_.peekLine(addr2);
        sparta_assert( (line != nullptr),
                           "SimpleCacheTest:  expected line to be !nullptr, addr=0x" << std::hex << addr2);
        sparta_assert(  line->isModified(),
                           "SimpleCacheTest:  expected line to be modified, addr=0x" << std::hex << addr2);
        addr2 += 4*1024;
    }
    std::cout << "  write-write:  PASSED" << std::endl;





}
