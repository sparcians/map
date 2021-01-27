// <RegisterBits> -*- C++ -*-
#pragma once

#include <cinttypes>
#include <array>
#include <functional>
#include <limits>
#include <string.h>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    class RegisterBits
    {
        template<typename SizeT, typename Op>
        static RegisterBits bitMerge_(const RegisterBits & in, const RegisterBits & rh_bits) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_data_) =
                Op()(*reinterpret_cast<const SizeT*>(in.remote_data_),
                     *reinterpret_cast<const SizeT*>(rh_bits.remote_data_));
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitNot_(const RegisterBits & in) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_data_) =
                std::bit_not<SizeT>()(*reinterpret_cast<const SizeT*>(in.remote_data_));
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitShiftRight_(const RegisterBits & in, uint32_t amount) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_data_) =
                (*reinterpret_cast<const SizeT*>(in.remote_data_)) >> amount;
            return final_value;
        }

        template<typename SizeT>
        static RegisterBits bitShiftLeft_(const RegisterBits & in, uint32_t amount) {
            RegisterBits final_value(in.num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_data_) =
                (*reinterpret_cast<const SizeT*>(in.remote_data_)) << amount;
            return final_value;
        }

        void convert_() {
            if(nullptr == local_data_) {
                local_data_ = local_storage_.data();
                ::memcpy(local_data_, remote_data_, num_bytes_);
                remote_data_ = local_data_;
            }
        }

    public:
        explicit RegisterBits(const uint64_t num_bytes) :
            local_data_(local_storage_.data()),
            remote_data_(local_data_),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        template<class DataT>
        RegisterBits(const uint64_t num_bytes, const DataT & data) :
            local_storage_(),
            local_data_(local_storage_.data()),
            remote_data_(local_data_),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
            sparta_assert(sizeof(DataT) <= num_bytes);
            set(data);
        }

        RegisterBits(uint8_t * data, const size_t num_bytes) :
            local_data_(data),
            remote_data_(local_data_),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        RegisterBits(const uint8_t * data, const size_t num_bytes) :
            remote_data_(data),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        RegisterBits(std::nullptr_t) {}

        RegisterBits(const RegisterBits & orig) :
            local_storage_(orig.local_storage_),
            local_data_(orig.local_data_ == nullptr ? nullptr : local_storage_.data()),
            remote_data_(orig.local_data_ == orig.remote_data_ ? local_data_ : orig.remote_data_)
        {}

        RegisterBits(RegisterBits && orig) :
            local_storage_(std::move(orig.local_storage_)),
            num_bytes_(orig.num_bytes_)
        {
            local_data_  = (orig.local_data_ == nullptr ? nullptr : local_storage_.data());
            remote_data_ = (orig.local_data_ == orig.remote_data_ ? local_data_ : orig.remote_data_);
            orig.local_data_ = nullptr;
            orig.remote_data_ = nullptr;
        }

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
                        *reinterpret_cast<const uint64_t*>(remote_data_ + idx) |
                        *reinterpret_cast<const uint64_t*>(rh_bits.remote_data_ + idx);
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
            return RegisterBits(nullptr);
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
                        *reinterpret_cast<const uint64_t*>(remote_data_ + idx) &
                        *reinterpret_cast<const uint64_t*>(rh_bits.remote_data_ + idx);
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
            return RegisterBits(nullptr);
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
                        ~*reinterpret_cast<const uint64_t*>(remote_data_ + idx);
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitNot_<uint16_t>(*this);
            }
            else if(num_bytes_ == 1) {
                return bitNot_<uint8_t>(*this);
            }
            return RegisterBits(nullptr);
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
                const uint64_t * src_data = reinterpret_cast<const uint64_t*>(remote_data_);
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

                // Micro-shift:
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                //                  0011111111111111 1122222222222222 2233333333333333
                //
                const auto double_words_to_micro_shift = (num_dbl_words - double_word_shift_count);
                if(double_words_to_micro_shift > 0)
                {
                    const uint32_t remaining_bits_to_shift = shift % 64;
                    if(remaining_bits_to_shift)
                    {
                        const uint64_t prev_dbl_word_bits_dropped_mask =
                            (uint64_t)(-(remaining_bits_to_shift != 0) &
                                       (-1 >> ((sizeof(uint64_t) * 8) - remaining_bits_to_shift)));
                        const uint32_t prev_dbl_word_bit_pos = 64 - remaining_bits_to_shift;
                        uint32_t idx =0;
                        while(true)
                        {
                            *(final_data + idx) >>= remaining_bits_to_shift;
                            if(++idx == double_words_to_micro_shift) {
                                break;
                            }
                            const uint64_t bits_pulled_in =
                                (*(final_data + idx) & prev_dbl_word_bits_dropped_mask) << prev_dbl_word_bit_pos;
                            *(final_data + (idx - 1)) |= bits_pulled_in;
                        }
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

            return RegisterBits(nullptr);
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
                const uint64_t * src_data = reinterpret_cast<const uint64_t*>(remote_data_);
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
                // 4444444444444444
                // double_word_shift_count = 3
                // dest[3] = src[0]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 3333333333333333 4444444444444444
                // double_word_shift_count = 2
                // dest[2] = src[0]
                // dest[3] = src[1]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 2222222222222222 3333333333333333 4444444444444444
                // double_word_shift_count = 1
                // dest[1] = src[0]
                // dest[2] = src[1]
                // dest[3] = src[2]
                //
                uint32_t dest_idx = double_word_shift_count;
                for(uint32_t src_idx = 0; dest_idx < num_dbl_words; ++dest_idx, ++src_idx) {
                    *(final_data + dest_idx) = *(src_data + src_idx);
                }

                // Micro-shift:
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 2222222222222233 3333333333333344 4444444444444400
                //
                const uint32_t remaining_bits_to_shift = shift % 64;
                if(remaining_bits_to_shift)
                {
                    const int32_t double_words_to_micro_shift = (num_dbl_words - double_word_shift_count);
                    if(double_words_to_micro_shift > 0)
                    {
                        const uint64_t prev_dbl_word_bit_pos = 64 - remaining_bits_to_shift;
                        const uint64_t prev_dbl_word_bits_dropped_mask =
                            (uint64_t)(-(prev_dbl_word_bit_pos != 0) &
                                       (std::numeric_limits<uint64_t>::max() << prev_dbl_word_bit_pos));
                        int32_t idx = num_dbl_words - 1; // start at the top
                        while(true)
                        {
                            *(final_data + idx) <<= remaining_bits_to_shift;
                            --idx;
                            if(idx < 0) { break; }
                            const uint64_t bits_dropped_from_next_double_word =
                                (*(final_data + idx) & prev_dbl_word_bits_dropped_mask) >> prev_dbl_word_bit_pos;
                            *(final_data + (idx + 1)) |= bits_dropped_from_next_double_word;
                        }
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

            return RegisterBits(nullptr);
        }

        void operator|=(const RegisterBits & rh_bits)
        {
            convert_();
            if(num_bytes_ == 8) {
                *reinterpret_cast<uint64_t*>(local_data_) |=
                    *reinterpret_cast<const uint64_t*>(rh_bits.remote_data_);
            }
            else if(num_bytes_ == 4) {
                *reinterpret_cast<uint32_t*>(local_data_) |=
                    *reinterpret_cast<const uint32_t*>(rh_bits.remote_data_);
            }
            else if(num_bytes_ > 8)
            {
                // 64-bit chunks
                for(uint32_t idx = 0; idx < num_bytes_; idx += 8)
                {
                    *reinterpret_cast<uint64_t*>(local_data_ + idx) |=
                        *reinterpret_cast<const uint64_t*>(rh_bits.remote_data_ + idx);
                }
            }
            else if(num_bytes_ == 2) {
                *reinterpret_cast<uint16_t*>(local_data_) |=
                    *reinterpret_cast<const uint16_t*>(rh_bits.remote_data_);
            }
            else if(num_bytes_ == 1) {
                *reinterpret_cast<uint8_t*>(local_data_) |=
                    *reinterpret_cast<const uint8_t*>(rh_bits.remote_data_);
            }

        }

        void operator<<=(uint32_t shift)
        {
            convert_();
            if(num_bytes_ == 8) {
                *reinterpret_cast<uint64_t*>(local_data_) =
                    (*reinterpret_cast<const uint64_t*>(remote_data_)) << shift;
            }
            else if(num_bytes_ == 4) {
                *reinterpret_cast<uint32_t*>(local_data_) =
                    (*reinterpret_cast<const uint32_t*>(remote_data_)) << shift;
            }
            else if(num_bytes_ > 8)
            {
                uint64_t * src_data   = reinterpret_cast<uint64_t*>(local_data_);
                uint64_t * final_data = reinterpret_cast<uint64_t*>(local_data_);
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
                // 4444444444444444
                // double_word_shift_count = 3
                // dest[3] = src[0]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 3333333333333333 4444444444444444
                // double_word_shift_count = 2
                // dest[2] = src[0]
                // dest[3] = src[1]
                //
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 2222222222222222 3333333333333333 4444444444444444
                // double_word_shift_count = 1
                // dest[1] = src[0]
                // dest[2] = src[1]
                // dest[3] = src[2]
                //
                if(double_word_shift_count > 0)
                {
                    uint32_t dest_idx = double_word_shift_count;
                    for(uint32_t src_idx = 0; dest_idx < num_dbl_words; ++dest_idx, ++src_idx) {
                        *(final_data + dest_idx) = *(src_data + src_idx);
                        *(src_data + src_idx) = 0;
                    }
                }

                // Micro-shift:
                // 1111111111111111 2222222222222222 3333333333333333 4444444444444444
                // 2222222222222233 3333333333333344 4444444444444400
                //
                const uint32_t remaining_bits_to_shift = shift % 64;
                if(remaining_bits_to_shift)
                {
                    const int32_t double_words_to_micro_shift = (num_dbl_words - double_word_shift_count);
                    if(double_words_to_micro_shift > 0)
                    {
                        const uint64_t prev_dbl_word_bit_pos = 64 - remaining_bits_to_shift;
                        const uint64_t prev_dbl_word_bits_dropped_mask =
                            (uint64_t)(-(prev_dbl_word_bit_pos != 0) &
                                       (std::numeric_limits<uint64_t>::max() << prev_dbl_word_bit_pos));
                        int32_t idx = num_dbl_words - 1; // start at the top
                        while(true)
                        {
                            *(final_data + idx) <<= remaining_bits_to_shift;
                            --idx;
                            if(idx < 0) { break; }
                            const uint64_t bits_dropped_from_next_double_word =
                                (*(final_data + idx) & prev_dbl_word_bits_dropped_mask) >> prev_dbl_word_bit_pos;
                            *(final_data + (idx + 1)) |= bits_dropped_from_next_double_word;
                        }
                    }
                }
            }
            else if(num_bytes_ == 2) {
                *reinterpret_cast<uint16_t*>(local_data_) =
                    (*reinterpret_cast<const uint16_t*>(remote_data_)) << shift;
            }
            else if(num_bytes_ == 1) {
                *reinterpret_cast<uint8_t*>(local_data_) =
                    (*reinterpret_cast<const uint8_t*>(remote_data_)) << shift;
            }
        }

        template<class DataT>
        std::enable_if_t<std::is_integral_v<DataT>, bool>
        operator==(const DataT dat) const {
            // sparta_assert(sizeof(DataT) <= num_bytes_);
            return *(reinterpret_cast<const DataT*>(remote_data_)) == dat;
        }

        bool operator==(const RegisterBits & other) const {
            return (num_bytes_ == other.num_bytes_) &&
                (::memcmp(remote_data_, other.remote_data_, num_bytes_) == 0);
        }

        const uint8_t * operator[](const uint32_t idx) const {
            return remote_data_ + idx;
        }

        template<class DataT>
        void set(const DataT & masked_bits) {
            // sparta_assert(sizeof(DataT) <= num_bytes_);
            convert_();
            ::memcpy(local_data_, reinterpret_cast<const uint8_t*>(&masked_bits), sizeof(DataT));
        }

        void fill(const uint8_t fill_data) {
            convert_();
            local_storage_.fill(fill_data);
        }

        const uint8_t *data() const {
            return remote_data_;
        }

        uint8_t *data() {
            convert_();
            return local_data_;
        }

        template <typename T,
                  typename = typename std::enable_if<std::is_integral<T>::value>::type>
        T dataAs() const {
            T ret_data = 0;
            ::memcpy(&ret_data, remote_data_, std::min(sizeof(T), num_bytes_));
            return ret_data;
        }

        uint32_t getSize() const { return num_bytes_; }

        // Returns true if all zero
        bool none() const {
            const auto mem_data_plus_one = remote_data_ + 1;
            return (::memcmp(remote_data_, mem_data_plus_one, num_bytes_ - 1) == 0);
        }

        // Returns true if anything non-zero
        bool any() const {
            return !none();
        }

    private:

        std::array<uint8_t, 64> local_storage_ = {0};
        uint8_t       * local_data_  = nullptr;
        const uint8_t * remote_data_ = nullptr;
        const uint64_t  num_bytes_ = 0;
    };
}
