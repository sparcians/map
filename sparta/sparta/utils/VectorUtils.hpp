// <VectorUtils.hpp> -*- C++ -*-

#pragma once

#include <memory>

#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta {
namespace utils {

template<typename T> struct is_vector : public std::false_type {};
template<typename T, typename A>
    struct is_vector<std::vector<T, A>> : public std::true_type {};

/*!
 * \brief Utility for element-wise potentially-deep-copying a vector.
 * Invoked by methods at the end of this file
 */
template <template<class...> class C, typename T>
class VectorCopier
{
public:
    typedef C<T> container_t;

    const std::vector<container_t>& input_;

    VectorCopier(const std::vector<container_t>& input)
      : input_(input)
    {;}

    void deepCopy(std::vector<container_t>& output) const
    {
        for (const auto& el : input_){
            output.push_back(el); // No copy of T because this unspecialized class does not know how
        }
    }

    void extractRawCopy(std::vector<T*>& output) const
    {
        for (const container_t& el : input_){
            output.push_back(el.get()); // Extract raw pointer
        }
    }
};

/*!
 * \brief Specialization for unique_ptr
 */
template <typename VT>
class VectorCopier<std::unique_ptr, VT>
{
public:

    typedef std::unique_ptr<VT> container_t;
    const std::vector<container_t>& input_;

    VectorCopier(const std::vector<container_t>& input)
      : input_(input)
    {;}

    void deepCopy(std::vector<container_t>& output) const
    {
        for(const container_t& el : input_){
            if(el.get() != nullptr){
                output.emplace_back(new VT(*el.get())); // Copy T
            }else{
                output.emplace_back(nullptr);
            }
        }
    }

    void extractRawCopy(std::vector<VT*>& output) const
    {
        for (const container_t& el : input_){
            output.push_back(el.get()); // Extract raw pointer
        }
    }
};

/*!
 * \brief Specialization for shared_ptr
 */
template <typename VT>
class VectorCopier<std::shared_ptr, VT>
{
public:

    typedef std::shared_ptr<VT> container_t;
    const std::vector<container_t>& input_;

    VectorCopier(const std::vector<container_t>& input)
      : input_(input)
    {;}

    void deepCopy(std::vector<container_t>& output) const
    {
        for(const container_t& el : input_){
            if(el.get() != nullptr){
                output.emplace_back(new VT(*el.get())); // Copy T
            }else{
                output.emplace_back(nullptr);
            }
        }
    }

    void extractRawCopy(std::vector<VT*>& output) const
    {
        for (const container_t& el : input_){
            output.push_back(el.get()); // Extract raw pointer
        }
    }
};

/*!
 * \brief Copy a vector based on its contained type. If this contains unique
 * pointers or shared pointers, clone the object
 *
 * This is not useful as often as you might think.
 */
template <template<class...> class C, typename T>
void copyVectorDeep(const std::vector<C<T>>& input,
                    std::vector<C<T>>& output)
{
    VectorCopier<C,T>(input).deepCopy(output);
}

/*!
 * \brief Copy a vector based on its contained type. If this contains unique
 * pointers or shared pointers, clone the object
 *
 * This is not useful as often as you might think.
 */
template <template<class...> class C, typename T>
void copyVectorExtractRawPointers(const std::vector<C<T>>& input,
                                  std::vector<T*>& output)
{
    VectorCopier<C,T>(input).extractRawCopy(output);
}

} // namepace utils
} // namespace sparta

