// <RegisterBits> -*- C++ -*-
#pragma once

#include <cinttypes>
#include <vector>
#include <functional>

namespace sparta
{
    class RegisterBits
    {
        template<typename SizeT, typename Op>
        RegisterBits bitMerge_(const RegisterBits & rh_bits) const {
            RegisterBits final_value(num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                Op()(*reinterpret_cast<const SizeT*>(data_),
                     *reinterpret_cast<const SizeT*>(rh_bits.data_));
            return final_value;
        }

        template<typename SizeT>
        RegisterBits bitNot_() const {
            RegisterBits final_value(num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                std::bit_not()(*reinterpret_cast<const SizeT*>(data_));
            return final_value;
        }

        template<typename SizeT>
        RegisterBits bitShiftRight_(uint32_t amount) const {
            RegisterBits final_value(num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                (*reinterpret_cast<const SizeT*>(data_)) >> amount;
            return final_value;
        }

        template<typename SizeT>
        RegisterBits bitShiftLeft_(uint32_t amount) const {
            RegisterBits final_value(num_bytes_);
            *reinterpret_cast<SizeT*>(final_value.local_storage_.data()) =
                (*reinterpret_cast<const SizeT*>(data_)) << amount;
            return final_value;
        }

    public:
        explicit RegisterBits(const uint32_t num_bytes) :
            local_storage_(num_bytes, 0),
            data_(local_storage_.data()),
            num_bytes_(num_bytes)
        {
        }

        RegisterBits(const uint8_t * data, const size_t num_bytes) :
            data_(data),
            num_bytes_(num_bytes)
        {}

        RegisterBits(const RegisterBits &)  = default;
        RegisterBits(      RegisterBits &&) = default;
        //RegisterBits & operator=(const RegisterBits&) = default;

        RegisterBits operator|(const RegisterBits & rh_bits) const
        {
            if(num_bytes_ == 8) {
                return bitMerge_<uint64_t, std::bit_or<uint64_t>>(rh_bits);
            }
            else if(num_bytes_ == 4) {
                return bitMerge_<uint32_t, std::bit_or<uint32_t>>(rh_bits);
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
                return bitMerge_<uint16_t, std::bit_or<uint16_t>>(rh_bits);
            }
            else if(num_bytes_ == 1) {
                return bitMerge_<uint8_t, std::bit_or<uint8_t>>(rh_bits);
            }

            // undefined
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator&(const RegisterBits & rh_bits) const
        {
            if(num_bytes_ == 8) {
                return bitMerge_<uint64_t, std::bit_and<uint64_t>>(rh_bits);
            }
            else if(num_bytes_ == 4) {
                return bitMerge_<uint32_t, std::bit_and<uint32_t>>(rh_bits);
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
                return bitMerge_<uint16_t, std::bit_and<uint16_t>>(rh_bits);
            }
            else if(num_bytes_ == 1) {
                return bitMerge_<uint8_t, std::bit_and<uint8_t>>(rh_bits);
            }

            // undefined
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator~() const
        {
            if(num_bytes_ == 8) {
                return bitNot_<uint64_t>();
            }
            else if(num_bytes_ == 4) {
                return bitNot_<uint32_t>();
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
                return bitNot_<uint16_t>();
            }
            else if(num_bytes_ == 1) {
                return bitNot_<uint8_t>();
            }
            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator>>(uint32_t shift) const
        {
            if(num_bytes_ == 8) {
                return bitShiftRight_<uint64_t>(shift);
            }
            else if(num_bytes_ == 4) {
                return bitShiftRight_<uint32_t>(shift);
            }
            else if(num_bytes_ > 8)
            {
                RegisterBits final_value(num_bytes_);
                const uint64_t * src_data = reinterpret_cast<const uint64_t*>(data_);
                uint64_t * final_data = reinterpret_cast<uint64_t*>(final_value.local_storage_.data());
                const uint32_t num_dbl_words = num_bytes_ / 8;

                const uint64_t bits_dropped_mask = (1ull << shift) - 1;
                const uint32_t prev_dbl_word_bit_pos = 64 - shift;

                // Shift each double word
                for(uint32_t idx = 0; idx < num_dbl_words; ++idx)
                {
                    *(final_data + idx) = (*(src_data + idx) >> shift);

                    // Now, put in the bits dropped between each double word
                    if(idx > 0) {
                        const uint64_t orig_dbl_word = *(src_data + idx);
                        const uint64_t bits_dropped = (orig_dbl_word & bits_dropped_mask) << prev_dbl_word_bit_pos;
                        *(final_data + (idx - 1)) |= bits_dropped;
                    }
                }
                return final_value;
            }
            else if(num_bytes_ == 2) {
                return bitShiftRight_<uint16_t>(shift);
            }
            else if(num_bytes_ == 1) {
                return bitShiftRight_<uint8_t>(shift);
            }

            return RegisterBits(nullptr, 0);
        }

        RegisterBits operator<<(uint32_t shift) const
        {
            if(num_bytes_ == 8) {
                return bitShiftLeft_<uint64_t>(shift);
            }
            else if(num_bytes_ == 4) {
                return bitShiftLeft_<uint32_t>(shift);
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
                return bitShiftLeft_<uint16_t>(shift);
            }
            else if(num_bytes_ == 1) {
                return bitShiftLeft_<uint8_t>(shift);
            }

            return RegisterBits(nullptr, 0);
        }

        bool operator==(const RegisterBits & other) const {
            return (num_bytes_ == other.num_bytes_) &&
                (::memcmp(data_, other.data_, num_bytes_) == 0);
        }

        const uint8_t * operator[](const uint32_t idx) const {
            return data_ + idx;
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
        const uint8_t * data_ = nullptr;
        const uint32_t  num_bytes_;
    };
}
