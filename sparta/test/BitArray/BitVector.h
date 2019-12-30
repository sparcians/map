//////////////////////////////////////////////////////////////////////
//               Copyright (C) 1999 Motorola, Inc.
//                     All Rights Reserved
//
//     THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MOTOROLA, INC.
//     The copyright notice above does not evidence any actual or
//     intended publication of such source code.
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
/*!
 * \file        BitVector.h
 * \brief       Class to abstract bit vectors and their operations
 *
 * \author      DB Murrell
 * \date        7/13/99
 *
 */
//////////////////////////////////////////////////////////////////////

#ifndef _T2_H_BITVECTOR
#define _T2_H_BITVECTOR

#include <inttypes.h>
#include <cassert>

namespace ttfw2 {

   template <class DataT>
   class SizeCalc
   {
   public:
      enum {
         BYTE_SIZEOF = sizeof(DataT),
         BIT_SIZEOF  = BYTE_SIZEOF * 8,
         MAX_BIT_NUM = BIT_SIZEOF - 1
      };
   };

}

namespace ttfw2
{

/*!
 * \brief  BitVector Class
 *
 * This class abstracts and encapsulates simple bit vector operations.
 * All operations are intended to be inlined to avoid any additional method
 * call overhead.
 */
class BitVector
{
   //! BITVECTOR constants
   //! One bit mask at leftmost end
   static const uint64_t  BITVECTOR_BIT_MASK = (uint64_t)(0x1)
      << SizeCalc<uint64_t>::MAX_BIT_NUM;

private:
   typedef      uint64_t  Bits;   //!< Bitvector base type
   typedef bool Bit;    //!< Individual bit type

public:

   //! Create a new bitvecotr with the indicated range set
   inline BitVector(uint32_t first_bit, uint32_t last_bit);

   //! Default class constructor
   inline BitVector(const Bits initial = 0ULL);

   //! Copy this bitvector
   inline BitVector(const BitVector &bv);

   //! Return length of this BitVector in bits
   inline uint32_t Length() const
   {
      return SizeCalc<Bits>::BIT_SIZEOF;
   }

   //! Cast value to base type
   inline operator Bits() const;

   //! Return value of a certain bit in the vector
   inline Bit operator[](const uint32_t index) const;

   //! Set a single bit within the vector
   inline void Set(const uint32_t index);

   //! Set all bits within the vector
   inline void SetAll();

   //! Clear a single bit within the vector
   inline void Clear(const uint32_t index);

   //! Clear all bits within the vector
   inline void ClearAll();

   //! Set the indiciated range of bits
   inline void SetRange (uint32_t first_bit, uint32_t last_bit);

   //! Assign this vector from a bit pattern
   inline BitVector& operator=(const Bits b);

   //! Assign this vector from another vector
   inline BitVector& operator=(const BitVector &rhs);

   //! Construct a new BitVector from this BITWISE-AND a bit pattern
   inline BitVector operator&(const Bits b) const;

   //! Compute BITWISE-AND of this vector and a bit pattern (destructive to this)
   inline BitVector& operator&=(const Bits b);

   //! Construct a new BitVector from this BITWISE-OR and a bit pattern
   inline BitVector operator|(const Bits b) const;

   //! Compute BITWISE-OR of this vector and a bit pattern (destructive to this)
   inline BitVector& operator|=(const Bits b);

   //! Construct a new BitVector from this BITWISE-XOR and a bit pattern
   inline BitVector operator^(const Bits b) const;

   //! Compute BITWISE-XOR of this vector and a bit pattern (destructive to this)
   inline BitVector& operator^=(const Bits b);

   //! Compute RightShift of the vector by given amount (destructive to this)
   inline BitVector& operator>>=(const uint32_t amt);

   //! Compute LeftShift of the vector by given amount (destructive to this)
   inline BitVector& operator<<=(const uint32_t amt);

   //! Construct a new BitVector from a BITWISE-NOT of this BitVector
   inline BitVector operator~() const;

   //! Equality relational operator
   inline bool operator==(const Bits b) const;

   //! Inequality relational operator
   inline bool operator!=(const Bits b) const;

private:
   Bits                 value_;         //!< Data storage for bit vector

};

/*!
 * \param       initial Initial value
 *
 * Assign initial bit vector value (0 by default)
 */
inline BitVector::BitVector(const Bits initial) :
   value_(initial)
{
}

/*!
 * \param       b       bit pattern "to the right" of this in the "&"
 *          expression
 * \return      Newly constructed result BitVector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector BitVector::operator&(const Bits b) const
{
   return BitVector(value_ & b);
}

/*!
 * \return      Newly constructed result BitVector
 *
 * Returns a new BitVector which is the direct bitwise-not of this
 * BitVector
 */
inline BitVector BitVector::operator~() const
{
   return BitVector(~value_);
}

/*!
 * \return      internal value
 *
 * Cast operator overload function.
 */
inline BitVector::operator BitVector::Bits() const
{
   return value_;
}

/*!
 * \param       amt             amount to shift left by
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector& BitVector::operator<<=(const uint32_t amt)
{
   // GCC on Solaris doesn't like to shift if the number of bits you
   // are shifting is greater than the size of the item. So, we
   // special case that here.
   if (amt >= SizeCalc<Bits>::BIT_SIZEOF)
   {
      value_ = 0;
   }
   else
   {
      value_ <<= amt;
   }
   return *this;
}

/*!
 * \param       b       Bit pattern to copy
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector & BitVector::operator=(const Bits b)
{
   value_ = b;
   return *this;
}

/*!
 *  \param rhs  The vector being assigned
 *  \return this vector
 */
inline BitVector & BitVector::operator=(const BitVector &rhs)
{
   value_ = rhs.value_;
   return *this;
}

/*!
 * Set all of the bits in the vector.
 */
inline void BitVector::SetAll()
{
#ifndef WIN32
   value_ = (Bits)(-1ll);
#else
   value_ = (Bits)(-1);
#endif
}

/*!
 * \param  first_bit  The first bit to set
 * \param  last_bit   The last bit to set
 */
inline void BitVector::SetRange (uint32_t first_bit, uint32_t last_bit)
{
   assert (first_bit <= last_bit);
   assert (last_bit < SizeCalc<BitVector>::BIT_SIZEOF);

   // Create a mask of the new valid bits which need to be set in our
   // valids_ vector
   BitVector upper_mask;
   BitVector lower_mask;
   upper_mask.SetAll();
   lower_mask.SetAll();

   upper_mask <<= first_bit;
   lower_mask <<= (last_bit + 1);
   lower_mask = ~lower_mask;
   value_ = (upper_mask & lower_mask);
   assert (value_ != 0);
}


/*!
 * \param       first_bit  The first bit to be set in the vector
 * \param   last_bit   The last bit to be set in the vector
 *
 * Set a range of bits in the bit vector
 */
inline BitVector::BitVector(uint32_t first_bit, uint32_t last_bit)
{
   SetRange (first_bit, last_bit);
}

/*!
 * \param bv  The vector we are copying from
 */
inline BitVector::BitVector(const BitVector &bv) :
   value_(bv.value_)
{
}

/*!
 * \param       index   Index of desired bit (0 is MSB)
 * \return      Bit value as a boolean
 *
 * Operator overloading function for [].  Return value of a
 * certain bit in the vector.
 */
inline BitVector::Bit BitVector::operator[](const uint32_t index) const
{
   assert(index < SizeCalc<Bits>::BIT_SIZEOF);
   return (value_ & (BITVECTOR_BIT_MASK >> index));
}

/*!
 * \param       index   Index of desired bit (0 is MSB)
 *
 * Set a single bit within the vector.
 */
inline void BitVector::Set(const uint32_t index)
{
   assert(index < SizeCalc<Bits>::BIT_SIZEOF);
   value_ |= (BITVECTOR_BIT_MASK >> index);
}

/*!
 * \param       index   Index of desired bit (0 is MSB)
 *
 * Clear a single bit within the vector.
 */
inline void BitVector::Clear(const uint32_t index)
{
   assert(index < SizeCalc<Bits>::BIT_SIZEOF);
   value_ &= ~(BITVECTOR_BIT_MASK >> index);
}

/*!
 * \brief       Clear all bits within the vector
 *
 * Clear all bits within the vector
 */
inline void BitVector::ClearAll()
{
   value_ = 0;
}

/*!
 * \param       b       Right-Hand-Side bit pattern
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector& BitVector::operator&=(const Bits b)
{
   value_ &= b;
   return *this;
}

/*!
 * \param       b       Vector "to the right" of this in the "|" expression
 * \return      Newly constructed result BitVector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector BitVector::operator|(const Bits b) const
{
   return BitVector(value_ | b);
}

/*!
 * \param       b       Right-Hand-Side bit pattern
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector& BitVector::operator|=(const Bits b)
{
   value_ |= b;
   return *this;
}

/*!
 * \param       b       Vector "to the right" of this in the "^" expression
 * \return      Newly constructed result BitVector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector BitVector::operator^(const Bits b) const
{
   return BitVector(value_ ^ b);
}

/*!
 * \param       b       Right-Hand-Side bit pattern
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector& BitVector::operator^=(const Bits b)
{
   value_ ^= b;
   return *this;
}

/*!
 * \param       amt             amount to shift right by
 * \return      This vector
 *
 * Note: RHS operands which are BitVectors will cause the
 * compiler to invoke the Bits() typecast above. This should
 * not cause a performance hit if these methods are inlined.
 */
inline BitVector& BitVector::operator>>=(const uint32_t amt)
{
   // GCC on Solaris doesn't like to shift if the number of bits you
   // are shifting is greater than the size of the item. So, we
   // special case that here.
   if (amt >= SizeCalc<Bits>::BIT_SIZEOF)
   {
      value_ = 0;
   }

   else
   {
      value_ >>= amt;
   }
   return *this;
}

/*!
 * \param       b       Expression value to test
 * \return      true if value_ is equal to given expression
 *
 * Determine if the set of Bits b is equivalent to this BitVector.
 */
inline bool BitVector::operator==(const Bits b) const
{
   return (value_ == b);
}

/*!
 * \param       b       Expression value to test
 * \return      true if value_ is NOT equal to given expression
 *
 * Determine if the set of Bits b is not equivalent to this BitVector.
 */
inline bool BitVector::operator!=(const Bits b) const
{
   return (value_ != b);
}

}  // namespace ttfw2

// _T2_H_BITVECTOR
#endif
