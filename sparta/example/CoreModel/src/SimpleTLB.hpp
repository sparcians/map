
#pragma once

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MathUtils.hpp"
#include "cache/BasicCacheItem.hpp"
#include "cache/SimpleCache2.hpp"
#include "cache/ReplacementIF.hpp"

namespace core_example
{
    class SimpleTLBEntry : public sparta::cache::BasicCacheItem
    {
    public:
        SimpleTLBEntry() = delete;

        SimpleTLBEntry(uint32_t page_size) :
            page_size_(page_size),
            valid_(false)
        {

            sparta_assert(sparta::utils::is_power_of_2(page_size),
            "TLBEntry: Page size must be a power of 2. page_size=" << page_size);
        }

        // Copy constructor
        SimpleTLBEntry(const SimpleTLBEntry & rhs) :
            BasicCacheItem(rhs),
            page_size_(rhs.page_size_),
            valid_(rhs.valid_)
        {
        }

        // Copy assignment operator
        SimpleTLBEntry &operator=(const SimpleTLBEntry & rhs)
        {
            BasicCacheItem::operator=(rhs);
            page_size_ = rhs.page_size_;
            valid_ = rhs.valid_;

            return *this;
        }

        virtual ~SimpleTLBEntry() {}

        // Required by SimpleCache2
        void reset(uint64_t addr)
        {
            setValid(true);
            BasicCacheItem::setAddr(addr);
        }

        // Required by SimpleCache2
        void setValid(bool v) { valid_ = v; }

        // Required by BasicCacheSet
        bool isValid() const { return valid_; }

        // Required by SimpleCache2
        void setModified(bool m) { (void) m; }

        // Required by SimpleCache2
        bool read(uint64_t offset, uint32_t size, uint32_t *buf) const
        {
            (void) offset;
            (void) size;
            (void) buf;
            sparta_assert(false);
            return true;
        }

        // Required by SimpleCache2
        bool write(uint64_t offset, uint32_t size, uint32_t *buf) const
        {
            (void) offset;
            (void) size;
            (void) buf;
            sparta_assert(false);
            return true;
        }

    private:
        uint32_t page_size_;
        bool valid_;

    };  // class SimpleTLBEntry

    class SimpleTLB : public sparta::cache::SimpleCache2<SimpleTLBEntry>,
                      public sparta::Unit
    {
    public:
        static constexpr const char* name = "tlb";
        class TLBParameterSet : public sparta::ParameterSet
        {
        public:
            TLBParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {}
            PARAMETER(uint64_t, tlb_page_size, 4096, "Page size in bytes (power of 2)")
            PARAMETER(uint64_t, tlb_num_of_entries, 32, "L1 TLB # of entries (power of 2)")
            PARAMETER(uint64_t, tlb_associativity, 32, "L1 TLB associativity (power of 2)")
        };
        using Handle = std::shared_ptr<SimpleTLB>;

        SimpleTLB(sparta::TreeNode* node, const TLBParameterSet* p) :
            sparta::cache::SimpleCache2<SimpleTLBEntry> ( (p->tlb_page_size * p->tlb_num_of_entries) >> 10,
                                                        p->tlb_page_size,
                                                        p->tlb_page_size,
                                                        SimpleTLBEntry(p->tlb_page_size),
                                                        sparta::cache::TreePLRUReplacement(p->tlb_associativity)),
            sparta::Unit(node),
            hits(&unit_stat_set_, "tlb_hits", "number of TLB hits", sparta::Counter::COUNT_NORMAL)
        {}

        void touch(const SimpleTLBEntry& entry)
        {
            debug_logger_ << "TLB HIT";
            touchMRU(entry);
            hits++;
        }
    private:
        sparta::Counter hits;
    }; // class SimpleTLB

} // namespace core_example

