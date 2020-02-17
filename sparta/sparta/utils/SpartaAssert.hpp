// <SpartaAssert> -*- C++ -*-


#ifndef __SPARTA_ASSERT_H__
#define __SPARTA_ASSERT_H__

#include <exception>
#include <string>
#include <iostream>
#include <sstream>
#include "sparta/utils/SpartaException.hpp"


/*!
 * \file SpartaAssert.hpp
 * \brief Set of macros used for assertions and performance enhancement
 */


/*!
 * \def SPARTA_EXPECT_FALSE
 * \brief A macro for hinting to the compiler a particular condition
 *        should be considered most likely false
 *
 * \code
 * if(SPARTA_EXPECT_FALSE(my_usually_false_condition)) {}
 * \endcode
 */
#define SPARTA_EXPECT_FALSE(x) __builtin_expect((x), false)

/*!
 * \def SPARTA_EXPECT_TRUE
 * \brief A macro for hinting to the compiler a particular condition
 *        should be considered most likely true
 *
 * \code
 * if(SPARTA_EXPECT_TRUE(my_usually_true_condition)) {}
 * \endcode
 */
#define SPARTA_EXPECT_TRUE(x) __builtin_expect((x), true)

#ifndef DO_NOT_DOCUMENT

#define ADD_FILE_INFORMATION(ex, file, line)                \
    ex << ": in file: '" << file << "', on line: " << std::dec << line;

#define SPARTA_THROW_EXCEPTION(reason, file, line)             \
    sparta::SpartaException ex(reason);                          \
    ADD_FILE_INFORMATION(ex, file, line)                     \
    throw ex;

#define SPARTA_ABORT(reason, file, line)                      \
    std::stringstream msg(reason);                          \
    ADD_FILE_INFORMATION(msg, file, line)                   \
    std::cerr << msg.str() << std::endl;                    \
    std::terminate();

#define sparta_assert1(e) \
    if(__builtin_expect(!(e), 0)) { SPARTA_THROW_EXCEPTION(#e, __FILE__, __LINE__) }

#define sparta_assert2(e, insertions)                                          \
    if(__builtin_expect(!(e), 0)) { sparta::SpartaException ex(std::string(#e) + ": " ); \
                                    ex << insertions;                             \
                                    ADD_FILE_INFORMATION(ex, __FILE__, __LINE__); \
                                    throw ex; }

#define sparta_throw(message) \
    { \
        std::stringstream msg; \
        msg << message; \
        sparta::SpartaException ex(std::string("abort: ") + msg.str()); \
        ADD_FILE_INFORMATION(ex, __FILE__, __LINE__); \
        throw ex; \
    }

#define sparta_abort1(e) \
    if(__builtin_expect(!(e), 0)) { SPARTA_ABORT(#e, __FILE__, __LINE__) }

#define sparta_abort2(e, insertions)                                          \
    if(__builtin_expect(!(e), 0)) { std::stringstream msg(std::string(#e) + ": " ); \
                                    msg << insertions;                             \
                                    ADD_FILE_INFORMATION(msg, __FILE__, __LINE__); \
                                    std::cerr << msg.str() << std::endl; \
                                    std::terminate(); }

#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)
#define sparta_assert_impl2(count, ...) sparta_assert##count(__VA_ARGS__)
#define sparta_assert_impl(count, ...)  sparta_assert_impl2(count, __VA_ARGS__)
#define sparta_abort_impl2(count, ...) sparta_abort##count(__VA_ARGS__)
#define sparta_abort_impl(count, ...)  sparta_abort_impl2(count, __VA_ARGS__)

// DO_NOT_DOCUMENT
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"
/*!
 * \def sparta_assert
 * \brief Simple variadic assertion that will throw a sparta_exception
 *        if the condition fails
 *
 * \throw SpartaException including file and line information if \a e evaluates to
 *        false
 * \note This assertion remains even if compiling with NDEBUG
 *
 * How to use:
 * \code
 *   sparta_assert(condition);
 *   sparta_assert(condition, "My nasty gram");
 *   sparta_assert(condition, "ostream supported message with a value: " << value);
 * \endcode
 *
 * Mental thought "Make sure that condition is true.  If not send message"
 */
#define sparta_assert(...) sparta_assert_impl(VA_NARGS(__VA_ARGS__), __VA_ARGS__)

#pragma GCC diagnostic pop

/*!
 * \def sparta_assert_errno
 * \brief Simple assert macro that throws a sparta_exception with a string
 *        representation of errno
 */
#define sparta_assert_errno(_cond) \
    sparta_assert(_cond, std::string(std::strerror(errno)))

/*!
 * \def sparta_abort
 * \brief Simple variatic assertion that will print a message to std::cerr
 *        and call std::terminate()
 *
 * \details Use instead of sparta_assert() whenever you need an assertion
 *          in a noexcept(true) function (commonly destructors).
 *
 * \note This assertion remains even if compiling with NDEBUG
 *
 * How to use:
 * \code
 *   sparta_abort(condition);
 *   sparta_abort(condition, "My nasty gram");
 *   sparta_abort(condition, "ostream supported message with a value: " << value);
 * \endcode
 *
 * Mental thought "Make sure that condition is true.  If not print message and abort"
 */
#define sparta_abort(...) sparta_abort_impl(VA_NARGS(__VA_ARGS__), __VA_ARGS__)

// __SPARTA_ASSERT_H__
#endif
