/*
 */
#pragma once

#include <vector>
#include <iomanip>
#include <type_traits>
#include <algorithm>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
    namespace utils {

        template <class InputIterator,
                  class OutputIterator,
                  class T,
                  class UnaryOperation>
        static OutputIterator slide(InputIterator first,
                                    InputIterator last,
                                    OutputIterator result,
                                    T init,
                                    UnaryOperation operation)
        {
            if (first == last) {
                return result;
            }

            for (; first + 1 != last; ++first, ++result) {
                *result = operation(*first, *(first + 1));
            }
            *result = operation(*first, init);

            return result;
        }

        /*!
         * \class BitArray
         * \brief Class for fast bit manipulation
         *
         * Type to represent a bit array whose size is not known at compile time. If
         * the size is known at compile time use std::bitset.
         */
        class BitArray
        {
        private:
            using data_type = uint8_t;

        public:
            /**
             * Construct a BitArray from a data buffer
             *
             * \param data Pointer to the data buffer
             * \param data_size Size of the data buffer in bytes
             * \param array_size The size of the BitArray in bytes
             *
             * If the array_size parameter is not specified or zero, the size of the
             * array will be the same as the size of the data buffer.
             */
            BitArray(const void *data, size_t data_size, size_t array_size = 0)
            {
                if (!array_size) {
                    array_size = data_size;
                }

                initializeStorage_(roundUpTo_(array_size, sizeof(data_type)));
                size_t offset = 0;
                do {
                    std::memcpy(data_ + offset, data, std::min(data_size, array_size - offset));
                    offset += std::min(data_size, array_size - offset);
                } while (offset < array_size);
            }

            /**
             * Construct a BitArray from an integer type
             *
             * \param value The integer
             * \param array_size The size of the BitArray in bytes
             *
             * If the array_size parameter is not specified, the size of the array will
             * be the same as the size of the integer type.
             */
            template <typename T,
                      typename = typename std::enable_if<std::is_integral<T>::value>::type>
            explicit BitArray(const T &value, size_t array_size = sizeof(T))
                : BitArray(reinterpret_cast<const void *>(&value), sizeof(T), array_size)
            {
            }

            BitArray(const BitArray &other)
            {
                initializeStorage_(other.data_size_);
                std::copy(other.data_, other.data_ + data_size_, data_);
            }

            BitArray &operator=(const BitArray &other) = default;

            BitArray(BitArray &&) = default;

            BitArray &operator=(BitArray &&) = default;

            const void *getValue() const
            {
                return data_;
            }

            template <typename T,
                      typename = typename std::enable_if<std::is_integral<T>::value>::type>
            T getValue() const
            {
                T tmp = 0;
                std::memcpy(&tmp, data_, std::min(sizeof(T), data_size_));
                return tmp;
            }

            size_t getSize() const
            {
                return data_size_ * sizeof(data_type);
            }

            /**
             * Fill the array by repeating value
             */
            template <typename T,
                      typename = typename std::enable_if<std::is_integral<T>::value>::type>
            void fill(const T &value)
            {
                for (size_t i = 0; i < getSize(); ++i) {
                    const auto shift = 8 * (i % sizeof(T));
                    data_[i] = (value >> shift) & 0xff;
                }
            }

            bool operator==(const BitArray &other) const
            {
                return (data_size_ == other.data_size_) &&
                    std::equal(data_, data_ + data_size_, other.data_);
            }

            bool operator!=(const BitArray &other) const
            {
                return !(*this == other);
            }

            BitArray operator<<(size_t amount) const
            {
                BitArray res(*this);
                shiftLeft_(res, amount);
                return res;
            }

            BitArray &operator<<=(size_t amount)
            {
                shiftLeft_(*this, amount);
                return *this;
            }

            BitArray operator>>(size_t amount) const
            {
                BitArray res(*this);
                shiftRight_(res, amount);
                return res;
            }

            BitArray &operator>>=(size_t amount)
            {
                shiftRight_(*this, amount);
                return *this;
            }

            /**
             * The size of the resulting BitArray will equal that of the left hand size
             * array. If the right hand side array is the larger of the two, its upper
             * bits will be truncated and will not be included in the result. If the
             * left hand side is the larger, the result will contain its upper bits
             * unchanged.
             */
            BitArray operator&(const BitArray &other) const
            {
                BitArray res(*this);
                and_(res, other);
                return res;
            }

            BitArray &operator&=(const BitArray &other)
            {
                and_(*this, other);
                return *this;
            }

            /**
             * The size of the resulting BitArray will equal that of the left hand size
             * array. If the right hand side array is the larger of the two, its upper
             * bits will be truncated and will not be included in the result. If the
             * left hand side is the larger, the result will contain its upper bits
             * unchanged.
             */
            BitArray operator|(const BitArray &other) const
            {
                BitArray res(*this);
                or_(res, other);
                return res;
            }

            BitArray &operator|=(const BitArray &other)
            {
                or_(*this, other);
                return *this;
            }

            BitArray operator~() const
            {
                BitArray res(*this);
                negate_(res);
                return res;
            }

        private:
            void initializeStorage_(size_t data_size)
            {
                /* This method should only be called once at construction */
                sparta_assert(data_size_ == 0);
                sparta_assert(data_ == nullptr);

                if (data_size > SMALL_OPTIMIZATION_SIZE) {
                    data_ls_.reserve(data_size);
                    data_ = &data_ls_[0];
                } else {
                    data_ = data_ss_;
                }

                /* Update size after allocating data to maintain the invariant that
                 * data_size_ matches the allocated memory size in case new throws */
                data_size_ = data_size;
            }

            static size_t roundUpTo_(size_t value, size_t target)
            {
                return (value + target - 1) / target;
            }

            static void shiftLeft_(BitArray &array, size_t amount)
            {
                const auto bits_per_word = 8 * sizeof(data_type);

                shiftLeftWords_(array, amount / bits_per_word);
                shiftLeftBits_(array, amount % bits_per_word);
            }

            static void shiftLeftWords_(BitArray &array, size_t amount)
            {
                const auto first = array.data_;
                const auto last = array.data_ + array.data_size_;

                std::rotate(first, last - amount, last);
                std::fill(first, first + amount, 0);
            }

            static void shiftLeftBits_(BitArray &array, size_t bits)
            {
                const auto rbegin =
                    std::reverse_iterator<data_type *>(array.data_ + array.data_size_);
                const auto rend = std::reverse_iterator<data_type *>(array.data_);

                slide(rbegin, rend, rbegin, data_type(0),
                      [bits](data_type low, data_type high)
                      {
                          return low << bits | high >> (8 * sizeof(data_type) - bits);
                      });
            }

            static void shiftRight_(BitArray &array, size_t amount)
            {
                const auto bits_per_word = 8 * sizeof(data_type);

                shiftRightWords_(array, amount / bits_per_word);
                shiftRightBits_(array, amount % bits_per_word);
            }

            static void shiftRightWords_(BitArray &array, size_t amount)
            {
                const auto first = array.data_;
                const auto last = array.data_ + array.data_size_;

                std::rotate(first, first + amount, last);
                std::fill(last - amount, last, 0);
            }

            static void shiftRightBits_(BitArray &array, size_t bits)
            {
                const auto begin = array.data_;
                const auto end = array.data_ + array.data_size_;

                slide(begin, end, begin, data_type(0),
                      [bits](data_type low, data_type high)
                      {
                          return low >> bits | high << (8 * sizeof(data_type) - bits);
                      });
            }

            static void and_(BitArray &array, const BitArray &other)
            {
                const auto min_size =
                    std::min(array.getSize(), other.getSize());

                for (size_t i = 0; i < min_size; i++) {
                    array.data_[i] &= other.data_[i];
                }
            }

            static void or_(BitArray &array, const BitArray &other)
            {
                const auto min_size =
                    std::min(array.getSize(), other.getSize());

                for (size_t i = 0; i < min_size; i++) {
                    array.data_[i] |= other.data_[i];
                }
            }

            static void negate_(BitArray &array)
            {
                for (size_t i = 0; i < array.data_size_; i++) {
                    array.data_[i] = ~array.data_[i];
                }
            }

            static const size_t SMALL_OPTIMIZATION_SIZE = 16;

            /** Used to store large arrays */
            std::vector<data_type> data_ls_;

            /** Used to store small arrays */
            data_type data_ss_[SMALL_OPTIMIZATION_SIZE / sizeof(data_type)] = {0};

            /** Points to either data_ls_ or data_ss_ depending on the size of the array */
            data_type *data_ = nullptr;

            /** Size of the array in bytes */
            size_t data_size_ = 0;
        };

    } /* namespace utils */
} /* namespace sparta */
