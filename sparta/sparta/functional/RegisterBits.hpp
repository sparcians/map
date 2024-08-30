// <RegisterBits> -*- C++ -*-
#pragma once

#include <cinttypes>
#include <array>
#include <functional>
#include <limits>
#include <string.h>
#include <algorithm>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    /**
     * \class RegisterBits
     *
     * This class is used in conjuntion with sparta::RegisterBase to
     * quickly write masked registers of sizes between 1 and 512
     * bytes.  This class replaces the use of BitArray.
     *
     * The class works by assuming register data is handed to it via a
     * char array.  The class will "view" into this data until it's
     * requested to modify it.  When it's modified, the data will be
     * copied to local storage within this class.  The user of the
     * class is responsible for writing the data back to the original
     * storage.
     */
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

        // Copy the remote register data locally.
        void convert_() {
            if(nullptr == local_data_) {
                local_data_ = local_storage_.data();
                ::memset(local_data_, 0, local_storage_.size());
                ::memcpy(local_data_, remote_data_, num_bytes_);
                remote_data_ = local_data_;
            }
        }

    public:
        /**
         * \brief Create an empty class with the given number of bytes
         * \param num_bytes The number of bytes to allocate
         */
        explicit RegisterBits(const uint64_t num_bytes) :
            local_storage_(),
            local_data_(local_storage_.data()),
            remote_data_(local_data_),
            num_bytes_(num_bytes)
        {
            ::memset(local_data_, 0, local_storage_.size());
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        /**
         * \brief Create a class with the given number of bytes and initialize it to the given data
         * \param num_bytes The number of bytes to allocate
         * \param data The data to write to the lower portion of memory
         *
         * The data is copied
         */
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

        /**
         * \brief Create a class pointing into the given data, of the given size
         * \param data_ptr The data to view
         * \param num_bytes The number of bytes available to view
         *
         * No data is copied
         */
        RegisterBits(uint8_t * data_ptr, const size_t num_bytes) :
            local_data_(data_ptr),
            remote_data_(local_data_),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        /**
         * \brief Create a class pointing into the given data constantly, of the given size
         * \param data_ptr The data to view
         * \param num_bytes The number of bytes available to view
         *
         * No data is copied
         */
        RegisterBits(const uint8_t * data, const size_t num_bytes) :
            remote_data_(data),
            num_bytes_(num_bytes)
        {
            sparta_assert(num_bytes <= local_storage_.size(),
                          "RegisterBits size is locked to 64 bytes. num_bytes requested: " << num_bytes);
        }

        /**
         * \brief Create a nullptr version of the data.  This would be an invalid class
         */
        RegisterBits(std::nullptr_t) {}

        /**
         * \brief Make a copy
         * \param orig The original to copy
         *
         * If the original is pointing to its own memory, that will be copied
         */
        RegisterBits(const RegisterBits & orig) :
            local_storage_(orig.local_storage_),
            local_data_(orig.local_data_ == nullptr ? nullptr : local_storage_.data()),
            remote_data_(orig.local_data_ == orig.remote_data_ ? local_data_ : orig.remote_data_)
        {}

        /**
         * \brief Move
         * \param orig The original to move
         *
         * If the original is pointing to its own memory, that data
         * will be moved.  The original is nullified.
         */
        RegisterBits(RegisterBits && orig) :
            local_storage_(std::move(orig.local_storage_)),
            num_bytes_(orig.num_bytes_)
        {
            local_data_  = (orig.local_data_ == nullptr ? nullptr : local_storage_.data());
            remote_data_ = (orig.local_data_ == orig.remote_data_ ? local_data_ : orig.remote_data_);
            orig.local_data_ = nullptr;
            orig.remote_data_ = nullptr;
        }

        /**
         * \brief "or" together two classes
         * \param rh_bits The other to "or" in
         * \return A new RegisterBits (to be moved) of this instance or'ed with the other
         */
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

        /**
         * \brief "and" together two classes
         * \param rh_bits The other to "and" in
         * \return A new RegisterBits (to be moved) of this instance and'ed with the other
         */
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

        /**
         * \brief "not" this class
         * \return A new RegisterBits (to be moved) of this instance not-ted
         */
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

        /**
         * \brief Shift this instance right and return a copy
         * \param shift The shift count
         * \return A new RegisterBits (to be moved) of this instance shifted right
         */
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

        /**
         * \brief Shift this instance left and return a copy
         * \param shift The shift count
         * \return A new RegisterBits (to be moved) of this instance shifted left
         */
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

        /**
         * \brief Shift this instance left
         * \param shift The shift count
         */
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

        /**
         * \brief Compare the register bits to the given data
         * \param dat The data to compare
         * \return True if equivalent
         */
        template<class DataT>
        std::enable_if_t<std::is_integral_v<DataT>, bool>
        operator==(const DataT dat) const {
            // sparta_assert(sizeof(DataT) <= num_bytes_);
            return *(reinterpret_cast<const DataT*>(remote_data_)) == dat;
        }

        /**
         * \brief Compare the register bits to another
         * \param other The other RegisterBits instance to compare
         * \return True if equivalent
         */
        bool operator==(const RegisterBits & other) const {
            return (num_bytes_ == other.num_bytes_) &&
                (::memcmp(remote_data_, other.remote_data_, num_bytes_) == 0);
        }

        /**
         * \brief Set the given masked_bits in this RegisterBits instance
         * \param masked_bits The bit value to set
         *
         * If this RegisterBits class was pointing to a remote data
         * view, the data will first be copied then the masked_bits written
         */
        template<class DataT>
        void set(const DataT & masked_bits) {
            // sparta_assert(sizeof(DataT) <= num_bytes_);
            convert_();
            ::memcpy(local_data_, reinterpret_cast<const uint8_t*>(&masked_bits), sizeof(DataT));
        }

        /**
         * \brief Fill the RegisterBits with the given fill_data
         * \param fill_data
         *
         * If this RegisterBits class was pointing to a remote data
         * view, the data will first be copied then the fill data written
         */
        void fill(const uint8_t fill_data) {
            convert_();
            local_storage_.fill(fill_data);
        }

        /**
         * \brief Get the data offset at the given index
         * \param idx The index offset
         * \return A data pointer at the given index
         */
        const uint8_t * operator[](const uint32_t idx) const {
            return remote_data_ + idx;
        }

        /**
         * \brief Get the internal data pointer
         * \return The data pointer
         */
        const uint8_t *data() const {
            return remote_data_;
        }

        /**
         * \brief Get the internal data pointer
         * \return The data pointer
         */
        uint8_t *data() {
            convert_();
            return local_data_;
        }

        /**
         * \brief Get the internal data as the given dta type
         * \return The data
         */
        template <typename T,
                  typename = typename std::enable_if<std::is_integral<T>::value>::type>
        T dataAs() const {
            T ret_data = 0;
            ::memcpy(&ret_data, remote_data_, std::min(sizeof(T), (size_t)num_bytes_));
            return ret_data;
        }

        /**
         * \brief Get the number of bytes
         * \return The number of bytes
         */
        uint32_t getSize() const { return num_bytes_; }

        /**
         * \brief Returns true if no bits are set
         * \return true if no bits are set
         */
        bool none() const {
            sparta_assert(num_bytes_ > 0);
            const auto mem_data_plus_one = remote_data_ + 1;
            return (::memcmp(remote_data_, mem_data_plus_one, num_bytes_ - 1) == 0);
        }

        /**
         * \brief Returns true if any bit is set
         * \return true if any bit is set
         */
        bool any() const {
            return !none();
        }

    private:

        std::array<uint8_t, 64> local_storage_; //!< Local storage
        uint8_t               * local_data_  = nullptr; //!< Points to null if using remote data
        const uint8_t         * remote_data_ = nullptr; //!< Remove data; points to local_data_ if no remote
        const uint64_t          num_bytes_ = 0; //!< Number of bytse
    };
}
