// <LRUReplacement.hpp> -*- C++ -*-

//!
//! \file LRUReplacement.hpp
//! \brief Provides a simple true LRU implementation
//!

#pragma once

#include <vector>
#include <list>

#include "ReplacementIF.hpp"

namespace sparta::cache
{
    // This class models true LRU with an std::list of way indices. The head of the list is LRU, and
    // the tail is MRU. An std::vector of iterators to the list is used to provide constant-time
    // index -> iterator lookup. A way is placed at LRU or MRU by moving its respective iterator to
    // the beginning or end of the list.
    class LRUReplacement : public ReplacementIF
    {
      public:
        explicit LRUReplacement(const uint32_t num_ways) : ReplacementIF(num_ways)
        {
            for (uint32_t i = 0; i < num_ways; ++i)
            {
                way_map_.emplace_back(lru_stack_.emplace(lru_stack_.end(), i));
            }
        }

        ReplacementIF* clone() const override
        {
            return new LRUReplacement(num_ways_);
        }

        void touchLRU(uint32_t way) override
        {
            auto lru_it = way_map_[way];
            lru_stack_.splice(lru_stack_.begin(), lru_stack_, lru_it);
        }

        void touchLRU(uint32_t way, const std::vector<uint32_t> & way_order) override
        {
            sparta_assert(false, "Not implemented");
        }

        void touchMRU(uint32_t way) override
        {
            auto lru_it = way_map_[way];
            lru_stack_.splice(lru_stack_.end(), lru_stack_, lru_it);
        }

        void touchMRU(uint32_t way, const std::vector<uint32_t> & way_order) override
        {
            sparta_assert(false, "Not implemented");
        }

        uint32_t getLRUWay() const override { return lru_stack_.front(); }

        uint32_t getLRUWay(const std::vector<uint32_t> & way_order) override
        {
            sparta_assert(false, "Not implemented");
        }

        uint32_t getMRUWay() const override { return lru_stack_.back(); }

        uint32_t getMRUWay(const std::vector<uint32_t> & way_order) override
        {
            sparta_assert(false, "Not implemented");
        }

        void lockWay(uint32_t way) override
        {
            sparta_assert(way < num_ways_);
            sparta_assert(false, "Not implemented");
        }

      private:
        std::list<uint32_t> lru_stack_;
        std::vector<std::list<uint32_t>::const_iterator> way_map_;
    };
} // namespace shinro
