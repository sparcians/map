/**
 * \file Interval.h
 * \author David Macke david.macke@samsung.com
 *
 * \copyright
 */

#ifndef __INTERVAL_H__
#define __INTERVAL_H__
#include <cassert>

namespace ISL {
    /*! \brief Interval is the generic template for the interval skip list.
    * The Interval class is templated so you can build an interval skip
    * list of whatever you want. When implementing Interval with any type
    * of basic int you declare it with that varaible. The generic getSide 
    * functions and contains functions provide all the required functionality.
    *
    * \tparam <Dat_t> { The Interval is templated on Dat_t for convenience.
    *   Having the class templated allows the user to specify an Interval
    *   of any type of data they want. For the IWAPI, the Interval template 
    *   uses an uint64_t. If anything other than a number is used for the Interval
    *   then the user must define the comparisson operators. }
    */
    template<class Dat_t>
    class Interval {
    private:
        Dat_t left_;             // Left Boundary
        Dat_t right_;            // Right Boundary
    public:
        typedef Dat_t IntervalDataT;
    
        Interval(const Dat_t &lval, const Dat_t &rval) : left_(lval),right_(rval){ 
            assert ( lval <= rval ); 
        }

        //! \brief return the private member left and right boundaries
        Dat_t getLeft() const noexcept{ return(left_); }
        Dat_t getRight() const noexcept{ return(right_); }
    
        //! \brief check if value is within interval
        bool contains(const Dat_t &V)const noexcept{ 
            return((V >= left_) && (V < right_)); 
        } 
        //! \brief Check to see if the left and right value provided is
        //  contained within the intervals left and right values.
        bool containsInterval(const Dat_t &l, const Dat_t &r) const{ 
            return( ( left_ <= l) && (right_ >= r) ); 
        }
        ~Interval() {}
    };
}
// __INTERVAL_H__
#endif
