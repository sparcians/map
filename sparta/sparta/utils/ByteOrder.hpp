// <ByteOrder> -*- C++ -*-

/*!
 * \file ByteOrder.hpp
 * \brief Byte order types and byte-swapping routines.
 *
 * Contains byte_swap methods for a number of types.
 */

#ifndef __BYTE_ORDER_H__
#define __BYTE_ORDER_H__

#include <iostream>

#include <typeinfo>
#include <boost/mpl/assert.hpp>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

//! Targeted for a Little Endian architecture.
//! If this were to change, ArchData store/load routines would need to be
//! updated to detect or force a byte-order. read/write routines would also need
//! to change.
#if __BYTE_ORDER != __LITTLE_ENDIAN
#error Byte order of host must be little endian for ArchData to run properly
#endif

#define HOST_INT_SIZE sizeof(uint32_t)

namespace sparta
{
    /*!
     * \brief Byte order enum for read/write methods
     *
     * Example 32 Byte register layout which can be accessed as smaller types by
     * index. Layout in memory is unaware of byte-order. Read/Write methods
     * dictate the storage.
     *
     * The following shows how the value index offset and access sizes map to some data in memory.
     * \verbatim
     * MEMORY   addr: 0x 0     2     4     6     8     a     c     e     10    12    14    16    18    1a    1c    1e    20
     *                   [- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 32 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- )
     *                   [- -- -- -- -- -- -- -- 16 -- -- -- -- -- -- -- )                       ,                       ,
     *                   [- -- -- -- 8- -- -- -- )                                                                       ,
     *                   [- -- 4- -- )           ,                       ,                       ,                       ,
     *                   [- 2- )                                                                                         ,
     *                   [- )        .           ,           .           ,           .           ,           .           ,
     *          val: 0x  00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f ,
     *
     * Little (LE)       ,                                               ,                                               ,
     *                   LSB                                             ,                                            MSB,
     *  type      idx    ,                                               ,                                               ,
     *  uint64_t  0      [- -- -- -- -- -- -- -]                         ,                                               , => 0x0706050403020100
     *  uint64_t  3      ,                                               ,                       [- -- -- -- -- -- -- -] , => 0x1f1e1d1c1b1a1918
     *  uint32_t  0      [- -- -- -]                                     ,                                               , => 0x03020100
     *  uint32_t  1      ,           [- -- -- -]                         ,                                               , => 0x07060504
     *  uint8_t   1      ,  []                                           ,                                               , => 0x01
     *
     * Big (BE)          ,                                               ,                                               ,
     *                   MSB                                             ,                                            LSB,
     *  uint64_t  0      [- -- -- -- -- -- -- -]                         ,                                               , => 0x0001020304050607
     *  uint64_t  3      ,                                               ,                       [- -- -- -- -- -- -- -] m => 0x18191a1b1c1d1e1f
     *  uint8_t   1      ,  []                                           ,                                               , => 0x01
     *
     * Bitfield (LE)     ,                                               ,                                               ,
     *  07-00            []                                              ,                                               , => 0x00
     *  15-08            ,  []                                           ,                                               , => 0x01
     *  23-16            ,     []                                        ,                                               , => 0x02
     *  31-24            ,        []                                     ,                                               , => 0x03
     *
     * \endverbatim
     */
    enum ByteOrder
    {
        LE = 0, //! Little endian
        BE = 1  //! Big endian
    };


    //! Swaps the order of bytes for various types
    template <typename T>
    inline typename std::enable_if<std::is_integral<T>::value, T>::type
    byte_swap(T val)
    {
        throw SpartaException("Do not know how to byteswap type '") << typeid(T).name()
              << "' for value " << val;
    }

    //! \overload byte_swap(T val)
    template <>
    inline uint8_t byte_swap(uint8_t val) {
        return val;
    }

    //! \overload byte_swap(T val)
    template <>
    inline int8_t byte_swap(int8_t val) {
        return val;
    }

    //! \overload byte_swap(T val)
    template <>
    inline uint16_t byte_swap(uint16_t val) {
        return (val << 8 | val >> 8);
    }

    //! \overload byte_swap(T val)
    template <>
    inline int16_t byte_swap(int16_t val) {
        return (val << 8 | val >> 8);
    }

    //! \overload byte_swap(T val)
    template <>
    inline uint32_t byte_swap(uint32_t val) {
        return __builtin_bswap32(val);
    }

    //! \overload byte_swap(T val)
    template <>
    inline int32_t byte_swap(int32_t val) {
        return __builtin_bswap32(val);
    }

    //! \overload byte_swap(T val)
    template <>
    inline uint64_t byte_swap(uint64_t val) {
        return __builtin_bswap64(val);
    }

    //! \overload byte_swap(T val)
    template <>
    inline int64_t byte_swap(int64_t val) {
        return __builtin_bswap64(val);
    }


    //! Takes a value of type T from native byte order to the designed byte order
    //! Specialization for Big-Endian
    template <typename T, enum ByteOrder BO>
    inline
    typename std::enable_if<std::is_same<boost::mpl::int_<BO>, boost::mpl::int_<BE> >::value , T>::type
    reorder(const T& t) {
        return byte_swap<T>(t); // byte-reordering required
    }

    //! Takes a value of type T from native byte order to the designed byte order
    //! Specialization for Little-Endian
    template <typename T, enum ByteOrder BO>
    inline
    typename std::enable_if<std::is_same<boost::mpl::int_<BO>, boost::mpl::int_<LE> >::value , T>::type
    reorder(const T& t) {
        return t; // No reorder needed on LE host.
    }

} // namespace sparta

// __BYTE_ORDER_H__
#endif
