
#ifndef __SPARTA_UTILS_POINTER_UTILS_H__
#define __SPARTA_UTILS_POINTER_UTILS_H__

#include <typeinfo>
#include <cxxabi.h>
#include <type_traits>
#include <memory>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
namespace utils {

#if !defined(__linux__)
// mac always has to use libc++
#define SHARED_PTR_NAME "std::__1::shared_ptr"
#else
// note that libc++ uses std::__1, so this will be the wrong name if you link against it
#define SHARED_PTR_NAME "std::shared_ptr"
#endif

/**
 * do a dynamic_pointer_cast and verify that it worked
 * \param right the shared_ptr that will be cast to the derived type
 */
template<class T, class U, bool Unsafe = false>
std::shared_ptr<T>
checked_dynamic_pointer_cast(const std::shared_ptr<U>& right)
{
    if (Unsafe) {
        return std::static_pointer_cast<T>(right);
    } else {
        std::shared_ptr<T> destination = std::dynamic_pointer_cast<T>(right);

        if (__builtin_expect(!destination, false)) {
            const auto td = typeid(right).name();
            int sts;
            const auto td2 = typeid(T).name();
            std::unique_ptr<char, void(*)(void*)> not_type {abi::__cxa_demangle(td2, nullptr, nullptr, &sts), std::free};
            std::unique_ptr<char, void(*)(void*)> is_type {abi::__cxa_demangle(td, nullptr, nullptr, &sts), std::free};

            // note that libc++ uses std::__1, so this will be the wrong name if you link against it
            sparta_assert2(destination != nullptr,
                           " dynamic_pointer_cast failed, this shared_ptr is of type " <<
                           is_type.get() << ", not of type " SHARED_PTR_NAME "<" <<
                           not_type.get() << ">");
        }
        return destination;
    }
}

/**
 * do a dynamic_pointer_cast and verify that it worked
 * \param right the pointer that will be cast to the derived type
 */
template<class T, class U, bool Unsafe = false>
T checked_dynamic_cast(const U right)
{
    if (Unsafe) {
        return static_cast<T>(right);
    } else {
        T destination = dynamic_cast<T>(right);

        if (__builtin_expect(!destination, false)) {
            const auto td = typeid(right).name();
            int sts;
            const auto td2 = typeid(T).name();
            std::unique_ptr<char, void(*)(void*)> not_type {abi::__cxa_demangle(td2, nullptr, nullptr, &sts), std::free};
            std::unique_ptr<char, void(*)(void*)> is_type {abi::__cxa_demangle(td, nullptr, nullptr, &sts), std::free};
            sparta_assert2(destination != nullptr,
                         " dynamic_cast failed, this pointer is of type " <<
                         is_type.get() << ", not of type " << not_type.get());
        }
        return destination;
    }
}

} // utils
} // sparta

#endif
