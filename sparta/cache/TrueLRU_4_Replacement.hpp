
#ifndef _TRUE_LRU_4_REPLACEMENT_H_
#define _TRUE_LRU_4_REPLACEMENT_H_

#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta 
{

    namespace cache
    {

        // This class implements an effecient 4-way true LRU using a 6-bit encoding
        //
        // The 6-bit encoding:
        //  1. b0:  W0>W1  (way0 is more recently used)
        //  2. b1:  W0>W2
        //  3. b2:  W0>W3
        //  4. b3:  W1>W2
        //  5. b4:  W1>W3
        //  6. b5:  W2>W3
        // Encoding: 0x3F (b'111111) means W0>W1>W2>W3>W4>W5
        //           ...
        //           0x00 (b'000000) means W0<W1<W2<W3<W4<W5
        //           
        // In this implementation, we define b0 as least significant
        // 
        // Of the 64 possible encodings, only 24 are valid
        //
        class TrueLRU_4_Replacement : ReplacementIF
        {
        public:
            TrueLRU_4_Replacement() : ReplacementIF(NUM_WAYS)
            {
                setup_transition_tbl_();
                TrueLRU_4_Replacement::reset();
            }

            void reset()
            {
                cur_encoding_ = 0;
            }
            
            uint32_t getMRUWay() const {
                static const uint32_t MRUIDX=0;
                return transition_tbl_[cur_encoding_].way_order[MRUIDX];
            }
    
            uint32_t getLRUWay() const {
                static const uint32_t LRUIDX=3;
                return transition_tbl_[cur_encoding_].way_order[LRUIDX];
            }
    
            void touchMRU(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                cur_encoding_ = transition_tbl_[cur_encoding_].next_mru_encoding[way];
                sparta_assert(cur_encoding_ !=  INVALID_ENCODING);

            }
    
            void touchLRU(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                cur_encoding_ = transition_tbl_[cur_encoding_].next_lru_encoding[way];
                sparta_assert(cur_encoding_ !=  INVALID_ENCODING);
            }
    
            void lockWay(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                sparta_assert(0);
            }

            ReplacementIF *clone() const { return new TrueLRU_4_Replacement(); }
        private:
            static const uint32_t INVALID_ENCODING = 0xFFFFFFFF;
            static const uint32_t NUM_WAYS = 4;
            struct TrueLRU4Entry {
                uint32_t encoding;                    /* 6-bit encoding */
                uint32_t way_order[NUM_WAYS];         /* Order of the ways for the encoding, idx==0 is MRU way
                                                       * For example, way_order[0]==2 means way2 is MRU */
                uint32_t next_mru_encoding[NUM_WAYS]; /* Next encoding if a way is touchMRU.
                                                       * If way1 is touched, next encoding is next_mru_encoding[1] */
                uint32_t next_lru_encoding[NUM_WAYS]; /* Next encoding if a way is touchMRU.
                                                       * If way1 is touched LRU, next encoding is next_lru_encoding[1] */
            };

            void setup_transition_tbl_()
            {
                // Table used for state transition
                // The data in this table was generated using another program
                // Note:  way-order={0,1,2,3} means W0 is MRU & W3 is LRU
                // Note:  in the 6-bit encoding, b0 is least significant
                TrueLRU4Entry tbl[24]= {
                    // cur   way-order          touchMRU transition       touchLRU transition
                    // enc   MRU      LRU     w0     w1    w2    w3      w0    w1    w2    w3
                    { 0x3F,  {0, 1, 2, 3},   {0x3F, 0x3E, 0x35, 0x0B},  {0x38, 0x27, 0x1F, 0x3F} }, 
                    { 0x1F,  {0, 1, 3, 2},   {0x1F, 0x1E, 0x35, 0x0B},  {0x18, 0x07, 0x1F, 0x3F} }, 
                    { 0x37,  {0, 2, 1, 3},   {0x37, 0x3E, 0x35, 0x03},  {0x30, 0x27, 0x1F, 0x37} }, 
                    { 0x0F,  {0, 3, 1, 2},   {0x0F, 0x1E, 0x25, 0x0B},  {0x08, 0x07, 0x0F, 0x3F} }, 
                    { 0x27,  {0, 2, 3, 1},   {0x27, 0x3E, 0x25, 0x03},  {0x20, 0x27, 0x0F, 0x37} }, 
                    { 0x07,  {0, 3, 2, 1},   {0x07, 0x1E, 0x25, 0x03},  {0x00, 0x07, 0x0F, 0x37} }, 
                    { 0x3E,  {1, 0, 2, 3},   {0x3F, 0x3E, 0x34, 0x0A},  {0x38, 0x27, 0x1E, 0x3E} }, 
                    { 0x1E,  {1, 0, 3, 2},   {0x1F, 0x1E, 0x34, 0x0A},  {0x18, 0x07, 0x1E, 0x3E} }, 
                    { 0x35,  {2, 0, 1, 3},   {0x37, 0x3C, 0x35, 0x01},  {0x30, 0x25, 0x1F, 0x35} }, 
                    { 0x0B,  {3, 0, 1, 2},   {0x0F, 0x1A, 0x21, 0x0B},  {0x08, 0x03, 0x0B, 0x3F} }, 
                    { 0x25,  {2, 0, 3, 1},   {0x27, 0x3C, 0x25, 0x01},  {0x20, 0x25, 0x0F, 0x35} }, 
                    { 0x03,  {3, 0, 2, 1},   {0x07, 0x1A, 0x21, 0x03},  {0x00, 0x03, 0x0B, 0x37} }, 
                    { 0x3C,  {1, 2, 0, 3},   {0x3F, 0x3C, 0x34, 0x08},  {0x38, 0x25, 0x1E, 0x3C} }, 
                    { 0x1A,  {1, 3, 0, 2},   {0x1F, 0x1A, 0x30, 0x0A},  {0x18, 0x03, 0x1A, 0x3E} }, 
                    { 0x34,  {2, 1, 0, 3},   {0x37, 0x3C, 0x34, 0x00},  {0x30, 0x25, 0x1E, 0x34} }, 
                    { 0x0A,  {3, 1, 0, 2},   {0x0F, 0x1A, 0x20, 0x0A},  {0x08, 0x03, 0x0A, 0x3E} }, 
                    { 0x21,  {2, 3, 0, 1},   {0x27, 0x38, 0x21, 0x01},  {0x20, 0x21, 0x0B, 0x35} }, 
                    { 0x01,  {3, 2, 0, 1},   {0x07, 0x18, 0x21, 0x01},  {0x00, 0x01, 0x0B, 0x35} }, 
                    { 0x38,  {1, 2, 3, 0},   {0x3F, 0x38, 0x30, 0x08},  {0x38, 0x21, 0x1A, 0x3C} }, 
                    { 0x18,  {1, 3, 2, 0},   {0x1F, 0x18, 0x30, 0x08},  {0x18, 0x01, 0x1A, 0x3C} }, 
                    { 0x30,  {2, 1, 3, 0},   {0x37, 0x38, 0x30, 0x00},  {0x30, 0x21, 0x1A, 0x34} }, 
                    { 0x08,  {3, 1, 2, 0},   {0x0F, 0x18, 0x20, 0x08},  {0x08, 0x01, 0x0A, 0x3C} }, 
                    { 0x20,  {2, 3, 1, 0},   {0x27, 0x38, 0x20, 0x00},  {0x20, 0x21, 0x0A, 0x34} }, 
                    { 0x00,  {3, 2, 1, 0},   {0x07, 0x18, 0x20, 0x00},  {0x00, 0x01, 0x0A, 0x34} }
                };

                for (uint32_t i=0; i<64; ++i) {
                    transition_tbl_[i] = { INVALID_ENCODING,
                                           {0, 0, 0, 0 },    // way-order
                                           {INVALID_ENCODING, INVALID_ENCODING, INVALID_ENCODING, INVALID_ENCODING},     // MRU transition
                                           {INVALID_ENCODING, INVALID_ENCODING, INVALID_ENCODING, INVALID_ENCODING} };   // LRU transition
                }

                for (uint32_t i=0; i<24; ++i) {
                    uint32_t enc = tbl[i].encoding;
                    sparta_assert(enc !=  INVALID_ENCODING);
                    transition_tbl_[ enc ] = tbl[i];
                }
            }
    
            TrueLRU4Entry transition_tbl_[64];
            uint32_t cur_encoding_;
    
    
        }; // class TrueLRU_4_Replacment

    }; // namespace cache

}; // namespace sparta
#endif  // _TRUE_LRU_4_REPLACEMENT_H_
