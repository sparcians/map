#pragma once

#include "cache/SimpleCache.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "cache/LRUReplacement.hpp"
#include "cache/LineData.hpp"
#include "cache/BlockingMemoryIF.hpp"


template< class CacheItemT >
class DL1 : public sparta::cache::SimpleCache<CacheItemT>
{
public:
    DL1( uint32_t cache_sz_kb,
         uint32_t item_sz,
         uint32_t stride,
         const CacheItemT  &default_line,
         const sparta::cache::ReplacementIF &rep) :
        sparta::cache::SimpleCache<CacheItemT>(cache_sz_kb, item_sz, stride, default_line, rep)
    {}

    void setL2(sparta::BlockingMemoryIF *l2) { l2_ = l2; }

protected:
    virtual void castout_(const CacheItemT &line)
    {
        ++ sparta::cache::SimpleCache<CacheItemT>::stat_num_castouts_;
        uint64_t addr      = line.getAddr();
        uint32_t sz        = line.getLineSize();
        const uint8_t *buf = line.getDataPtr();

        bool rc = l2_->write(addr, sz, buf);
        sparta_assert(rc);
    }

    virtual void reload_(uint64_t blk_addr, CacheItemT &line)
    {
        ++ sparta::cache::SimpleCache<CacheItemT>::stat_num_reloads_;
        line.setValid(true);
        line.setAddr(blk_addr);
        line.setModified(false);
        uint32_t sz   = line.getLineSize();
        uint8_t *buf  = line.getDataPtr();
        bool rc = l2_->read(blk_addr, sz, buf);
        sparta_assert(rc);
    }

    virtual void writeNextLevel_(uint64_t addr,
                                 uint32_t size,
                                 const uint8_t *buf)
    {
        static bool print_warning=true;

        if (print_warning) {
            std::cout << "Warning:  write merging is not modelled" << std::endl;
            print_warning = false;
        }

        ++ sparta::cache::SimpleCache<CacheItemT>::stat_num_write_next_level_;
        bool rc = l2_->write(addr, size, buf);
        sparta_assert(rc);
    }
private:
    sparta::BlockingMemoryIF *l2_ = nullptr;

}; // DL1
