// <FrontArray.hpp> -*- C++ -*-

/**
 * \file FrontArray.hpp
 * \brief Type of Array with special allocation policies to
 *        support writing to the front most valid entry in the
 *        Array
 */

#ifndef __FRONT_ARRAY_H__
#define __FRONT_ARRAY_H__

#include "sparta/resources/Array.hpp"

namespace sparta{

    /**
     * \class FrontArray
     * \brief a type of Array with special allocation policies to
     *        support writing to the front most valid entry in the
     *        Array, as well as reading the nth valid entry in the
     *        array.
     *
     * \tparam DataT the data type to store in the array.
     * \tpaam ArrayT the type of array, Aged vs Normal
     * \tparam CollectionT the type of pipeline collection to run
     *                     on the Array.
     */
    template <class DataT, ArrayType ArrayT>
    class FrontArray : public Array<DataT, ArrayT>
    {
        //Typedef for the base type we inherit from
        typedef Array<DataT, ArrayT> BaseArray;

    public:
        /**
         * \brief Construct the array.
         * \param ps the port set to use during construction.
         * \param name the name of the array.
         * \param num_entries the size of the array.
         * \param clk a pointer to a clock used by the Array.
         */
        FrontArray(const std::string& name, uint32_t num_entries, const Clock* clk,
                   StatisticSet* statset = nullptr) :
            BaseArray(name, num_entries, clk, statset)
        {
        }

        ///read the nth valid object from the front of the array.
        const DataT& readValid(const uint32_t nth=0) const
        {
            sparta_assert(nth < BaseArray::capacity(), "Cannot read at index larger than the size of the array");
            sparta_assert(nth < BaseArray::numValid(), "Asked for an idx that is not valid");
            // We need to iterate over the underlaying vector, array_.
            // Then count how many valid entries we have passed until
            // we are at the nth'th valid entree.
            uint32_t n = 0;
            uint32_t i = 0;
            while(n <= nth)
            {
                if(BaseArray::isValid(n))
                {
                    ++n;
                }
                ++i;
            }
            // if i is not > 0, then we are going to fail that return
            // line with i-1
            sparta_assert(i > 0);
            return BaseArray::read(i - 1);
        }

        /**
         * \brief a public method that can be used to write data
         * to the first invalidate entry in the array. '
         * \param dat the data to be written to the front of the array.
         * \note this method is only accessable on AllocationP == FrontAccessable
         * type Arrays.
         */
        uint32_t writeFront(const DataT& dat)
        {
            return writeFrontImpl_(dat);
        }

        /**
         * \brief a public method that can be used to write data
         * to the first invalidate entry in the array. '
         * \param dat the data to be written to the front of the array.
         * \note this method is only accessable on AllocationP == FrontAccessable
         * type Arrays.
         */
        uint32_t writeFront(DataT&& dat)
        {
            return writeFrontImpl_(std::move(dat));
        }

        /**
         * \brief a public method that can be used to write data
         * to the last invalidate entry in the array. '
         * \param dat the data to be written to the front of the array.
         * \note this method is only accessable on AllocationP == FrontAccessable
         * type Arrays.
         */
        uint32_t writeBack(const DataT& dat)
        {
            return writeBackImpl_(dat);
        }

        /**
         * \brief a public method that can be used to write data
         * to the last invalidate entry in the array. '
         * \param dat the data to be written to the front of the array.
         * \note this method is only accessable on AllocationP == FrontAccessable
         * type Arrays.
         */
        uint32_t writeBack(DataT&& dat)
        {
            return writeBackImpl_(std::move(dat));
        }

        private:
        template<typename U>
        uint32_t writeFrontImpl_(U&& dat)
        {
            uint32_t i;
            for(i = 0; i < BaseArray::capacity(); ++i)
            {
                if(!BaseArray::isValid(i))
                {
                    break; //we found an entry that is invalid. This is the front!
                }
            }
            sparta_assert(i < BaseArray::capacity(), "Cannot write to the front of the Array. There are no free entries.");
            BaseArray::write(i, std::forward<U>(dat));
            return i;
        }

        template<typename U>
        uint32_t writeBackImpl_(U&& dat)
        {
            int32_t i;
            for(i = BaseArray::capacity()-1; i>=0;--i)
            {
                if(!BaseArray::isValid(i))
                {
                    break; //we found an entry that is invalid. This is the front!
                }
            }
            sparta_assert(i >= 0, "Cannot write to the back of the array. There are no free entries.");
            BaseArray::write(i, std::forward<U>(dat));
            return i;
        }
    };
}

#endif //__FRONT_ARRAY_H__
