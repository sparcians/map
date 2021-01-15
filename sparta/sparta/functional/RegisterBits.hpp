// <RegisterBits> -*- C++ -*-
#pragma once

#include <cinttypes>
#include <vector>
#include <functional>
#include <string.h>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    class RegisterBits
    {
        template<typename SizeT, typename Op>
        static RegisterBits bitMerge_(const RegisterBits & in, const RegisterBits & rh_bits) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                Op()(*reinterpret_cast<const SizeT*>(in.data_),
                     *reinterpret_cast<const SizeT*>(rh_bits.data_));
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitNot_(const RegisterBits & in) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                std::bit_not<SizeT>()(*reinterpret_cast<const SizeT*>(in.data_));
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitShiftRight_(const RegisterBits & in, uint32_t amount) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                (*reinterpret_cast<const SizeT*>(in.data_)) >> amount;
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitShiftLeft_(const RegisterBits & in, uint32_t amount) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                (*reinterpret_cast<const SizeT*>(in.data_)) << amount;
            return final_value;
        }

    public:
        explicit RegisterBits(const uint32_t num_bytes) :
            local_storage_(num_bytes, 0),
            data_(local_storage_.data()),
            num_bytes_(num_bytes)
        {
        }

        template<class DataT>
        RegisterBits(const uint32_t num_bytes, const DataT & data) :
            local_storage_(num_bytes, 0),
            data_(local_storage_.data()),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= sizeof(DataT));
            set(data);
        }

        RegisterBits(uint8_t * data, const size_t num_bytes) :
            data_(data),
            num_bytes_(num_bytes)
        {}

        RegisterBits(const RegisterBits &)  = default;
        RegisterBits(      RegisterBits &&) = default;
        //RegisterBits & operator=(const RegisterBits&) = default;

        RegisterBits operator|(const RegisterBits & rh_bits) const
        {
            if(num_bytes_ == 8) {
                return bitMerge_<uint64_t, std::bit_or<uint64_t>>(*this, rh_bits);
            }
            else if(num_bytes_ == 4) {
                return bitMerge_<uint32_t, std::bit_or<uint32_t>>(*this, rh_bits);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                // 64-bit chunks
                for(uint32_t idx = 0; idx < num_bytes_; idx += 8)
                {
                    *reinterpret_cast<uint64_t*>(final_value.local_storage_.data() + idx) =
                        *reinterpret_cast<const uint64_t*>(data_ + idx) |
                        *reinterpret_cast<const uint64_t*>(rh_bits.data_ + idx);
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitMerge_<uint16_t, std::bit_or<uint16_t>>(*this, rh_bits);
            }
            else if(num_bytes_ == 1) {
                return bitMerge_<uint8_t, std::bit_or<uint8_t>>(*this, rh_bits);
            }

            // undefined
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator&(const RegisterBits & rh_bits) const
        {
            if(num_bytes_ == 8) {
                return bitMerge_<uint64_t, std::bit_and<uint64_t>>(*this, rh_bits);
            }
            else if(num_bytes_ == 4) {
                return bitMerge_<uint32_t, std::bit_and<uint32_t>>(*this, rh_bits);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                // 64-bit chunks
                for(uint32_t idx = 0; idx < num_bytes_; idx += 8)
                {
                    *reinterpret_cast<uint64_t*>(final_value.local_storage_.data() + idx) =
                        *reinterpret_cast<const uint64_t*>(data_ + idx) &
                        *reinterpret_cast<const uint64_t*>(rh_bits.data_ + idx);
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitMerge_<uint16_t, std::bit_and<uint16_t>>(*this, rh_bits);
            }
            else if(num_bytes_ == 1) {
                return bitMerge_<uint8_t, std::bit_and<uint8_t>>(*this, rh_bits);
            }

            // undefined
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator~() const
        {
            if(num_bytes_ == 8) {
                return bitNot_<uint64_t>(*this);
            }
            else if(num_bytes_ == 4) {
                return bitNot_<uint32_t>(*this);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                // 64-bit compares
                for(uint32_t idx = 0; idx < num_bytes_; idx += 8)
                {
                    *reinterpret_cast<uint64_t*>(final_value.local_storage_.data() + idx) =
                        ~*reinterpret_cast<const uint64_t*>(data_ + idx);
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitNot_<uint16_t>(*this);
            }
            else if(num_bytes_ == 1) {
                return bitNot_<uint8_t>(*this);
            }
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator>>(uint32_t shift) const
        {
            if(num_bytes_ == 8) {
                return bitShiftRight_<uint64_t>(*this, shift);
            }
            else if(num_bytes_ == 4) {
                return bitShiftRight_<uint32_t>(*this, shift);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                const uint64_t * src_data = reinterpret_cast<const uint64_t*>(data_);
                uint64_t *     final_data = reinterpret_cast<uint64_t*>(final_value.local_storage_.data());
                const uint32_t num_dbl_words = num_bytes_ / 8;

                // Determine the number of double words that will be shifted
                const uint32_t double_word_shift_count = shift / 64;

                //
                // Shift full double words first.
                //
                // Example:
                //
                // num_dbl_words = 4
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                //                                                    1111111111111111
                // double_word_shift_count = 3
                // dest[0] = src[3]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                //                                   1111111111111111 2222222222222222
                // double_word_shift_count = 2
                // dest[0] = src[2]
                // dest[1] = src[3]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                //                  1111111111111111 2222222222222222 3333333333333333
                // double_word_shift_count = 1
                // dest[0] = src[1]
                // dest[1] = src[2]
                // dest[2] = src[3]
                //
                uint32_t src_idx = double_word_shift_count;
                for(uint32_t dest_idx = 0; src_idx < num_dbl_words; ++dest_idx, ++src_idx) {
                    *(final_data + dest_idx) = *(src_data + src_idx);
                }

                const auto double_words_to_micro_shift = (num_dbl_words - double_word_shift_count);
                if(double_words_to_micro_shift > 0)
                {
                    const uint32_t remaining_bits_to_shift = shift % 64;
                    const uint64_t prev_dbl_word_bits_dropped_mask =
                        (uint64_t)(-(remaining_bits_to_shift != 0) &
                                   (-1 >> ((sizeof(uint64_t) * 8) - remaining_bits_to_shift)));
                    const uint32_t prev_dbl_word_bit_pos = 64 - remaining_bits_to_shift;
                    uint32_t idx =0;
                    while(true)
                    {
                        *(final_data + idx) = (*(src_data + idx) >> remaining_bits_to_shift);
                        if(++idx == double_words_to_micro_shift) {
                            break;
                        }
                        const uint64_t orig_dbl_word = *(src_data + idx);
                        const uint64_t bits_dropped =
                            (orig_dbl_word & prev_dbl_word_bits_dropped_mask) << prev_dbl_word_bit_pos;
                        *(final_data + (idx - 1)) |= bits_dropped;
                    }
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitShiftRight_<uint16_t>(*this, shift);
            }
            else if(num_bytes_ == 1) {
                return bitShiftRight_<uint8_t>(*this, shift);
            }

            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator<<(uint32_t shift) const
        {
            if(num_bytes_ == 8) {
                return bitShiftLeft_<uint64_t>(*this, shift);
            }
            else if(num_bytes_ == 4) {
                return bitShiftLeft_<uint32_t>(*this, shift);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                const uint64_t * src_data = reinterpret_cast<const uint64_t*>(data_);
                uint64_t * final_data = reinterpret_cast<uint64_t*>(final_value.local_storage_.data());
                const uint32_t num_dbl_words = num_bytes_ / 8;

                const uint32_t prev_dbl_word_bit_pos = 64 - shift;
                const uint64_t bits_dropped_mask = ((1ull << shift) - 1) << prev_dbl_word_bit_pos;

                // Shift each double word
                for(uint32_t idx = 0; idx < num_dbl_words; ++idx)
                {
                    *(final_data + idx) = (*(src_data + idx) << shift);

                    // Now, put in the bits dropped between each double word
                    if(idx > 0) {
                        const uint64_t orig_dbl_word = *(src_data + (idx - 1));
                        const uint64_t bits_dropped = (orig_dbl_word & bits_dropped_mask) >> prev_dbl_word_bit_pos;
                        *(final_data + idx) |= bits_dropped;
                    }
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitShiftLeft_<uint16_t>(*this, shift);
            }
            else if(num_bytes_ == 1) {
                return bitShiftLeft_<uint8_t>(*this, shift);
            }

            return RegisterBits(nullptr, 0);
        }

        void operator<<=(uint32_t shift)
        {
            (void)shift;
        }

        bool operator==(const RegisterBits & other) const {
            return (num_bytes_ == other.num_bytes_) &&
                (::memcmp(data_, other.data_, num_bytes_) == 0);
        }

        const uint8_t * operator[](const uint32_t idx) const {
            return data_ + idx;
        }

        template<class DataT>
        void set(const DataT & masked_bits) {
            sparta_assert(sizeof(DataT) <= num_bytes_);
            ::memcpy(data_, reinterpret_cast<const uint8_t*>(&masked_bits), sizeof(DataT));
        }

        const uint8_t *data() const {
            return data_;
        }

        template <typename T,
                  typename = typename std::enable_if<std::is_integral<T>::value>::type>
        T dataAs() const {
            return *reinterpret_cast<T*>(data_);
        }

        uint32_t getSize() const { return num_bytes_; }

    private:

        std::vector<uint8_t> local_storage_;
        uint8_t * data_ = nullptr;
        const uint32_t  num_bytes_;
    };
}
