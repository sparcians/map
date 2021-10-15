
#pragma once

#include <bitset>
#include <iostream>
#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{

    namespace cache
    {

        /* This class implements the tree PLRU algorithm using a binary tree.
         * The leaf of the tree represents a way in the replacement set.  The state
         * of the tree tells which ways are LRU or MRU.
         *
         * Since the binary tree is a complete binary, it is  implemented as an array,
         * with the top-most node at idx=1. idx0 is not used.
         *
         * The following is the lay-out of the array indices for a 4-way tree:
         *
         *                          [1]             <-- level0
         *                         /    \
         *                      [2]      [3]        <---level1
         *                      / \      / \
         *                     4   5    6   7       <--- cache lines
         *                   (w0) (w1)(w2) (w3)
         *
         * For a given node at idx
         *    - left child's index  = idx*2
         *    - right child's index = idx*2 + 1
         *
         * Given an element at idx, its way number is
         *    way = idx - number-of-ways
         * With this scheme, way0 is at left of the tree.  For example,
         * the way number for idx=6 is 6-4=2.
         *
         * */
        class TreePLRUReplacement : public ReplacementIF
        {
        public:
            TreePLRUReplacement(uint32_t num_ways) :
                ReplacementIF(num_ways) ,
                num_tree_levels_ ( utils::floor_log2(num_ways) )
            {
                // Increase MAX_NUM_WAYS as necessary
                sparta_assert(num_ways_ <= MAX_NUM_WAYS);
                sparta_assert(utils::is_power_of_2(num_ways));
                TreePLRUReplacement::reset();
            }


            void reset()
            {
                plru_bits_ = 0;
            }

            uint32_t getMRUWay() const
            {
                uint32_t idx = 1;
                for (uint32_t i=0; i<num_tree_levels_; ++i)
                {
                    // Since this is an LRU tree, and we are looking for the
                    // MRU way, we need flip the lru bit
                    idx = 2*idx + !plru_bits_[idx];
                }

                uint32_t way = idx - num_ways_;

                return way;
            }

            uint32_t getMRUWay(const std::vector<uint32_t>& way_order)
            {
                sparta_assert(false, "Not implemented");
            }

            uint32_t getLRUWay() const
            {
                uint32_t idx = 1;
                for (uint32_t i=0; i<num_tree_levels_; ++i)
                {
                    idx = 2*idx + plru_bits_[idx];
                }

                uint32_t way = idx - num_ways_;
                return way;

            }

            uint32_t getLRUWay(const std::vector<uint32_t>& way_order)
            {
                sparta_assert(false, "Not implemented");
            }

            void touchMRU(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                uint32_t idx     = way + num_ways_;
                for (uint32_t i=0; i<num_tree_levels_; ++i)
                {
                    bool mru_is_to_the_right = idx & 0x1;
                    idx = idx >> 1;

                    // We need to invert mru_is_to_the_right because the tree is lru
                    plru_bits_[idx] = !mru_is_to_the_right;
                }
            }

            void touchMRU(uint32_t way, const std::vector<uint32_t>& way_order)
            {
                sparta_assert(false, "Not implemented");
            }

            void touchLRU(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                uint32_t idx     = way + num_ways_;
                for (uint32_t i=0; i<num_tree_levels_; ++i)
                {
                    bool lru_is_to_the_right = idx & 0x1;
                    idx = idx >> 1;
                    plru_bits_[idx] = lru_is_to_the_right;
                }
            }

            void touchLRU(uint32_t way, const std::vector<uint32_t>& way_order)
            {
                sparta_assert(false, "Not implemented");
            }

            void lockWay(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                sparta_assert(0);
            }

            ReplacementIF *clone() const { return new TreePLRUReplacement(num_ways_); }

            std::string getDisplayString() const
            {
                // RNK0=MRU
                std::stringstream str;
                str << std::dec;
                for (int32_t rnk=num_ways_-1; rnk>=0; --rnk) {
                    uint32_t idx=1;
                    for (uint32_t i=0; i<num_tree_levels_; ++i)
                    {
                        bool lru_bit = (rnk >> (num_tree_levels_ - 1 -i)) & 0x1;
                        idx = 2*idx + (plru_bits_[idx] ^ lru_bit);
                    }
                    uint32_t way = idx - num_ways_;
                    str << " " << way;

                }

                return str.str();
            }

        private:
            static const uint32_t MAX_NUM_WAYS=128;
            const uint32_t num_tree_levels_;
            std::bitset<MAX_NUM_WAYS> plru_bits_=0;

        }; // class TreePLRUReplacment

    }; // namespace cache

}; // namespace sparta
