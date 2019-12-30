

#include <iostream>
#include "cache/SimpleCAMCache.hpp"

class MyTag
{
public:
    uint64_t hi_addr=0;
    uint64_t lo_addr=0;
    bool operator==(const MyTag &rhs) {
        return (hi_addr==rhs.hi_addr)&&(lo_addr==rhs.lo_addr);
    }
};

#include "cache/BasicCacheItem.hpp"
class MyItem : public sparta::cache::BasicCacheItem
{
public:
    MyItem(std::string pl, uint64_t hi, uint64_t lo) : payload(pl)
    {
        setTag(hi, lo);
    }
    ~MyItem() {}

    void setTag(uint64_t hi, uint64_t lo)
    {
        tag.hi_addr = hi;
        tag.lo_addr = lo;
    }
    MyTag getTag() const { return tag; }
    void setValid(bool v) { valid = v; }
    bool isValid() const  { return valid; }
    std::string payload;
    MyTag tag;
    bool  valid = false;
}; // class MyItem


int main()
{
    static const uint32_t NUM_WAYS=8;
    MyItem default_line("BAD LINE", 0x1111ULL, 0x0000);
    sparta::cache::SimpleCAMCache< MyItem, MyTag > arb_entries( default_line,
                                                              sparta::cache::TrueLRUReplacement(NUM_WAYS) );

    for (uint32_t i=0; i<NUM_WAYS; ++i) {
        MyItem &line = arb_entries.getLRULine();

        std::stringstream str;
        str << "LINE #" << i;
        line.payload = str.str();
        line.setValid( true );     // Valid and tag management are user's responsibility
        line.setTag(0x2222, i);
        arb_entries.touchMRU( line );
    }


    for (uint32_t i=0; i<NUM_WAYS; ++i) {
        MyItem &line = arb_entries.getLRULine();
        std::cout << " Line=" << line.payload << std::endl;
        arb_entries.touchMRU( line );
    }

    MyTag tag1;
    tag1.hi_addr =0x2222;
    tag1.lo_addr =0x1;
    const MyItem *line = arb_entries.peekLine( tag1 );
    sparta_assert(line != nullptr, "Expected a valid line");
    sparta_assert(line->payload=="LINE #1",  "Expected LINE #1");
    std::cout << " Found line=" << line->payload << std::endl;


    return 0;
}
