// <MathUtils.hpp> -*- C++ -*-

#pragma once

#include <cmath>
#include <cinttypes>
#include <typeinfo>
#include <cassert>
#include <typeinfo>
#include <type_traits>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
namespace sparta {
    namespace utils {

        inline double log2 (double x) {
            // double y = std::log(x) / std::log(2.0);
            // return y;
            return std::log2(x);
        }

        template <class T>
        inline uint32_t log2_lsb(const T& x)
        {
            (void) x;
            //bool UNSUPPORTED_TYPE = false;
            throw SpartaException("Unsupported type for log2_lsb: ") << typeid(T).name();
        }

        template <>
        inline uint32_t log2_lsb<uint32_t>(const uint32_t &x)
        {
            // This uses a DeBrujin sequence to find the index
            // (0..31) of the least significant bit in iclass
            // Refer to Leiserson's bitscan algorithm:
            // http://chessprogramming.wikispaces.com/BitScan
            static const uint32_t index32[32] = {
                0,   1, 28,  2, 29, 14, 24, 3,
                30, 22, 20, 15, 25, 17,  4, 8,
                31, 27, 13, 23, 21, 19, 16, 7,
                26, 12, 18,  6, 11,  5, 10, 9
            };

            static const uint32_t debruijn32 = 0x077CB531u;

            return index32[((x & -x) * debruijn32) >> 27];
        }

        template <>
        inline uint32_t log2_lsb<uint64_t>(const uint64_t &x)
        {
            // This uses a DeBrujin sequence to find the index
            // (0..63) of the least significant bit in iclass
            // Refer to Leiserson's bitscan algorithm:
            // http://chessprogramming.wikispaces.com/BitScan
            static const uint32_t index64[64] = {
                63,  0, 58,  1, 59, 47, 53,  2,
                60, 39, 48, 27, 54, 33, 42,  3,
                61, 51, 37, 40, 49, 18, 28, 20,
                55, 30, 34, 11, 43, 14, 22,  4,
                62, 57, 46, 52, 38, 26, 32, 41,
                50, 36, 17, 19, 29, 10, 13, 21,
                56, 45, 25, 31, 35, 16,  9, 12,
                44, 24, 15,  8, 23,  7,  6,  5
            };

            static const uint64_t debruijn64 = 0x07EDD5E59A4E28C2ull;

            return index64[((x & -x) * debruijn64) >> 58];
        }

        template <class T>
        inline uint64_t floor_log2(T x)
        {
            // floor_log2(x) is the index of the most-significant 1 bit of x.
            // (This is the old iterative version)
            // NOTE: This function returns 0 for log2(0), but mathematically, it should be undefined
            // throw SpartaException("floor_log2(0) is undefined");
            uint64_t y = 0;
            while (x >>= 1) {
                y++;
            }
            return y;
        }

        template<>
        inline uint64_t floor_log2<double>(double x)
        {
            return std::floor(log2(x));
        }

        template <>
        inline uint64_t floor_log2<uint32_t>(uint32_t x)
        {
            if (x == 0) {
                // NOTE: This function returns 0 for log2(0) for compatibility with the old version,
                // but mathematically, it should be undefined
                // throw SpartaException("floor_log2(0) is undefined");
                return 0;
            }

            // This is a fast floor(log2(x)) based on DeBrujin's algorithm
            // (based on generally available and numerous sources)
            static const uint64_t lut[] = {
                0,  9,  1, 10, 13, 21,  2, 29,
                11, 14, 16, 18, 22, 25,  3, 30,
                8, 12, 20, 28, 15, 17, 24,  7,
                19, 27, 23,  6, 26,  5,  4, 31};

            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            return lut[(uint32_t)(x * 0x07C4ACDDul) >> 27];
        }

        template <>
        inline uint64_t floor_log2<uint64_t>(uint64_t x)
        {
            if (x == 0) {
                // NOTE: This function returns 0 for log2(0) for compatibility with the old version,
                // but mathematically, it should be undefined
                // throw SpartaException("floor_log2(0) is undefined");
                return 0;
            }

            // This is a fast floor(log2(x)) based on DeBrujin's algorithm
            // (based on generally available and numerous sources)
            static const uint64_t lut[] = {
                    63,  0, 58,  1, 59, 47, 53,  2,
                    60, 39, 48, 27, 54, 33, 42,  3,
                    61, 51, 37, 40, 49, 18, 28, 20,
                    55, 30, 34, 11, 43, 14, 22,  4,
                    62, 57, 46, 52, 38, 26, 32, 41,
                    50, 36, 17, 19, 29, 10, 13, 21,
                    56, 45, 25, 31, 35, 16,  9, 12,
                    44, 24, 15,  8, 23,  7,  6,  5};

            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            x |= x >> 32;
            return lut[((uint64_t)((x - (x >> 1)) * 0x07EDD5E59A4E28C2ull)) >> 58];
        }

        inline uint64_t ceil_log2 (uint64_t x)
        {
            // If x is a power of 2 then ceil_log2(x) is floor_log2(x).
            // Otherwise ceil_log2(x) is floor_log2(x) + 1.
            uint64_t y = floor_log2(x);
            if ((static_cast<uint64_t>(1) << y) != x) {
                y++;
            }
            return y;
        }

        inline uint64_t pow2 (uint64_t x) {
            uint64_t y = static_cast<uint64_t>(1) << x;
            return y;
        }

        inline bool is_power_of_2 (uint64_t x) {
            bool y = x && !(x & (x - 1));
            return y;
        }

        inline uint64_t next_power_of_2(uint64_t v)
        {
            if(v < 2) {
                return 1ull;
            }
            return 1ull << ((sizeof(uint64_t) * 8) - __builtin_clzll(v - 1ull));
        }

        inline uint64_t ones (uint64_t x) {
            uint64_t y = (static_cast<uint64_t>(1) << x) - 1;
            return y;
        }

        // Be careful of the types, here. The default
        // version of abs() expects to return the same type
        // as the given argument x. This is ok for float, double, eg.
        // but not for unsigned integer types, since the x < 0 test
        // will always be false.
        template <class T>
        inline T abs(T x)
        {
            return (x < 0 ? -x : x);
        }

        template <>
        inline uint8_t abs<uint8_t>(uint8_t x) {
            uint8_t sign_mask = int8_t(x) >> 7;
            return (x + sign_mask) ^ sign_mask;
        }

        template <>
        inline uint16_t abs<uint16_t>(uint16_t x) {
            uint16_t sign_mask = int16_t(x) >> 15;
            return (x + sign_mask) ^ sign_mask;
        }

        template <>
        inline uint32_t abs<uint32_t>(uint32_t x) {
            uint32_t sign_mask = int32_t(x) >> 31;
            return (x + sign_mask) ^ sign_mask;
        }

        template <>
        inline uint64_t abs<uint64_t>(uint64_t x) {
            uint64_t sign_mask = int64_t(x) >> 63;
            return (x + sign_mask) ^ sign_mask;
        }

        template <class T>
        inline T gcd(T u, T v)
        {
            (void) u;
            (void) v;
            static_assert("This is an unsupported type");
        }

        // Adapted from WIKI article on binary GCD algorithm
        template <>
        inline uint32_t gcd<uint32_t>(uint32_t u, uint32_t v)
        {
            // GCD(0,x) == GCD(x,0) == x
            if (u == 0 || v == 0)
                return u | v;

            // Let shift := lg K, where K is the greatest power of 2
            // dividing both u and v.
            uint32_t shift = log2_lsb(u | v);

            u >>= shift;
            u >>= log2_lsb(u);

            // From here on, u is always odd.
            v >>= shift;
            do {
                v >>= log2_lsb(v);

                // Now u and v are both odd. Swap if necessary so u <= v,
                // then set v = v - u (which is even). For bignums, the
                // swapping is just pointer movement, and the subtraction
                // can be done in-place.
                if (u > v) {
                    uint32_t t = u;
                    u = v;
                    v = t;
                }
                v = v - u;
            } while (v != 0);

            return u << shift;
        }

        // Adapted from WIKI article on binary GCD algorithm
        template <>
        inline uint64_t gcd<uint64_t>(uint64_t u, uint64_t v)
        {
            // GCD(0,x) == GCD(x,0) == x
            if (u == 0 || v == 0)
                return u | v;

            // Let shift := lg K, where K is the greatest power of 2
            // dividing both u and v.
            uint32_t shift = log2_lsb(u | v);

            u >>= shift;
            u >>= log2_lsb(u);

            // From here on, u is always odd.
            v >>= shift;
            do {
                v >>= log2_lsb(v);

                // Now u and v are both odd. Swap if necessary so u <= v,
                // then set v = v - u (which is even). For bignums, the
                // swapping is just pointer movement, and the subtraction
                // can be done in-place.
                if (u > v) {
                    uint64_t t = u;
                    u = v;
                    v = t;
                }
                v = v - u;
            } while (v != 0);

            return u << shift;
        }

        template <class T>
        inline T lcm(const T& u, const T& v)
        {
            (void) u;
            (void) v;
            //bool UNSUPPORTED_TYPE = false;
            throw SpartaException("Unsupported type for lcm: ") << typeid(T).name();
        }

        template <>
        inline uint32_t lcm<uint32_t>(const uint32_t &u, const uint32_t &v)
        {
            if (u == 1) {
                return v;
            } else if (v == 1) {
                return u;
            } else {
                // Do it this way to avoid potential overflows from large numbers
                return  u / gcd(u, v) * v;
            }
        }

        template <>
        inline uint64_t lcm<uint64_t>(const uint64_t &u, const uint64_t &v)
        {
            if (u == 1) {
                return v;
            } else if (v == 1) {
                return u;
            } else {
                // Do it this way to avoid potential overflows from large numbers
                return  u / gcd(u, v) * v;
            }
        }

        template <class T>
        inline T
        safe_power(T n, T e)
        {
            static_assert(std::is_integral<T>::value, "sparta::safe_power only supports integer data types");

            if (e == 0)
                return 1;

            T result = n;
            for (T x = 1; x < e; x++)
            {
                T old_result = result;
                result *= n;
                if (old_result > result)
                {
                    throw SpartaException("power() overflowed!");
                }
            }
            return result;
        }

    } // utils
} // sparta
