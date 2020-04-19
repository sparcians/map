// <TupleHashCompute> -*- C++ -*-


/**
 * \file TupleHashCompute
 * \brief Contains the implementation of Hash Function
 *  which is used by a std::map where a std::tuple is
 *  the key. Since, the standard C++ does not provide
 *  the std::hash() for std::tuples, this file was
 *  necessary to implement.
 */
#pragma once

namespace hashtuple {
    template<typename TT>

    //! Base struct for types which have std::hash() defined.
    struct hash {
        size_t operator()(TT const& tt) const {
            return std::hash<TT>()(tt);
        }
    };

    namespace {
        template<typename T>
        inline void hash_combine(std::size_t& seed, T const& v) {
            seed ^= hashtuple::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        //! Template specialization where the first template parameter is
        //  a std::tuple and second implicit template parameter is defaulted
        //  to the size of parameter pack contained in that tuple.
        template<typename Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl {
            static void apply(size_t& seed, Tuple const& tuple) {

                //! Recursive Template unrolling.
                HashValueImpl<Tuple, Index -1>::apply(seed, tuple);
                hash_combine(seed, std::get<Index>(tuple));
            }
        };

        //! Template base case where all the types from the tuple
        //  has been popped.
        template<typename Tuple>
        struct HashValueImpl<Tuple, 0> {
            static void apply(size_t& seed, Tuple const& tuple) {
                hash_combine(seed, std::get<0>(tuple));
            }
        };
    }

    //! Template specialization where the template parameter
    //  is a std::tuple of parameter pack.
    template<typename... TT>
    struct hash<std::tuple<TT...>> {
        size_t operator()(std::tuple<TT...> const& tt) const {
            size_t seed = 0;

            //! Template parameter being passed is the full 
            //  std::tuple of parameter pack.
            HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
            return seed;
        }
    };
} // namespace hashtuple
