// <RegisterPairsMacro.hpp> -*- C++ -*-


/**
 * \file RegisterPairsMacro.hpp
 *
 * \brief Contains macro for registering and invoking addPair()
 *  and flattenNestedPairs() calls for pevent and pipeline collection.
 * concepts.
 */

#ifndef __REGISTER_PAIRS_MACRO_H__
#define __REGISTER_PAIRS_MACRO_H__

// My reference for understanding preprocessor metaprogramming :
// 1. codecraft.co/2014/11/25/variadic-macros-tricks/
// 2. https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments

//! Expand FLATTEN macro to inject forwarding follwed by argument
#define SPARTA_FLATTEN(p_1) flattenNestedPairs(std::forward<Args>(args)..., p_1)

//! Macro to inject last argument and close parenthesis
#define _RESOLVED_1(p_1) p_1)

//! Macro to inject both arguments and close parenthesis
#define _RESOLVED_2(p_1, p_2) p_1, p_2)

//! Helper macro expanding to _RESOLVED_2 when two arguments passsed,
//  expanding to _RESOLVED_1 when one argument is passed
#define GET_ARGS(_1, _2, FCN_NAME, ...) FCN_NAME

//! Select one of the two overloads depending on number of remaining arguments
#define _ADDPAIR_RESOLVE(...) GET_ARGS(__VA_ARGS__, _RESOLVED_2, _RESOLVED_1)(__VA_ARGS__)

//! Helper macro to inject forwarding after first argument
#define _ADDPAIR_UTIL(p_1, ...) addPair(p_1, std::forward<Args>(args)..., _ADDPAIR_RESOLVE(__VA_ARGS__)

//! Expand ADDPAIR macro to inject forwarding after first argument
#define SPARTA_ADDPAIR(...) _ADDPAIR_UTIL(__VA_ARGS__)

// Each macro in this series of macros from 1 to 32
// handles a special case where the number of arguments
// passed are from 1 to 32
#define _REGISTER_PAIRS_1(x, ...)                                                                            \
    ptr->x;

#define _REGISTER_PAIRS_2(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_1(__VA_ARGS__)

#define _REGISTER_PAIRS_3(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_2(__VA_ARGS__)

#define _REGISTER_PAIRS_4(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_3(__VA_ARGS__)

#define _REGISTER_PAIRS_5(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_4(__VA_ARGS__)

#define _REGISTER_PAIRS_6(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_5(__VA_ARGS__)

#define _REGISTER_PAIRS_7(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_6(__VA_ARGS__)

#define _REGISTER_PAIRS_8(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_7(__VA_ARGS__)

#define _REGISTER_PAIRS_9(x, ...)                                                                            \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_8(__VA_ARGS__)

#define _REGISTER_PAIRS_10(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_9(__VA_ARGS__)

#define _REGISTER_PAIRS_11(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_10(__VA_ARGS__)

#define _REGISTER_PAIRS_12(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_11(__VA_ARGS__)

#define _REGISTER_PAIRS_13(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_12(__VA_ARGS__)

#define _REGISTER_PAIRS_14(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_13(__VA_ARGS__)

#define _REGISTER_PAIRS_15(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_14(__VA_ARGS__)

#define _REGISTER_PAIRS_16(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_15(__VA_ARGS__)

#define _REGISTER_PAIRS_17(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_16(__VA_ARGS__)

#define _REGISTER_PAIRS_18(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_17(__VA_ARGS__)

#define _REGISTER_PAIRS_19(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_18(__VA_ARGS__)

#define _REGISTER_PAIRS_20(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_19(__VA_ARGS__)

#define _REGISTER_PAIRS_21(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_20(__VA_ARGS__)

#define _REGISTER_PAIRS_22(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_21(__VA_ARGS__)

#define _REGISTER_PAIRS_23(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_22(__VA_ARGS__)

#define _REGISTER_PAIRS_24(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_23(__VA_ARGS__)

#define _REGISTER_PAIRS_25(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_24(__VA_ARGS__)

#define _REGISTER_PAIRS_26(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_25(__VA_ARGS__)

#define _REGISTER_PAIRS_27(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_26(__VA_ARGS__)

#define _REGISTER_PAIRS_28(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_27(__VA_ARGS__)

#define _REGISTER_PAIRS_29(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_28(__VA_ARGS__)

#define _REGISTER_PAIRS_30(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_29(__VA_ARGS__)

#define _REGISTER_PAIRS_31(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_30(__VA_ARGS__)

#define _REGISTER_PAIRS_32(x, ...)                                                                           \
    ptr->x;                                                                                                  \
    _REGISTER_PAIRS_31(__VA_ARGS__)

//! Macro to static cast the Derived Pair-Definition class into
//  Parent Pair-Definiton class
#define SPARTA_INVOKE_PAIRS(type) nestedPairCallback(static_cast<sparta::PairDefinition<type>*>(this));

//! Macro to generate 32 numbers in reverse order.
//  This is a helper macro used to count the number
//  of arguments in __VA_ARGS__
#define COUNT_RSEQ_N()                                                                                       \
    31, 30, 29, 28, 27, 26, 25,                                                                              \
    24, 23, 22, 21, 20, 19, 18, 17,                                                                          \
    16, 15, 14, 13, 12, 11, 10, 9,                                                                           \
    8, 7, 6, 5, 4, 3, 2, 1, 0

//! Macro which always returns the 32th indexed macro
#define COUNT_ARG_N(                                                                                         \
    _1, _2, _3, _4, _5, _6, _7, _8,                                                                          \
    _9, _10, _11, _12, _13, _14, _15, _16,                                                                   \
    _17, _18, _19, _20, _21, _22, _23, _24,                                                                  \
    _25, _26, _27, _28, _29, _30, _31, N, ...) N

#define COUNT_NARG_(...) COUNT_ARG_N(__VA_ARGS__)

//! Helper macro to count the number of arguments in __VA_ARGS__
#define COUNT_NARG(...) COUNT_NARG_(__VA_ARGS__,COUNT_RSEQ_N())

#define INVOKE_PAIRS(type) nestedPairCallback(static_cast<sparta::PairDefinition<type>*>(this));

//! Macro to generate 32 numbers in reverse order.
//  This is a helper macro used to count the number
//  of arguments in __VA_ARGS__
#define COUNT_RSEQ_N()                                                                                       \
    31, 30, 29, 28, 27, 26, 25,                                                                              \
    24, 23, 22, 21, 20, 19, 18, 17,                                                                          \
    16, 15, 14, 13, 12, 11, 10, 9,                                                                           \
    8, 7, 6, 5, 4, 3, 2, 1, 0

//! Macro which always returns the 32th indexed macro
#define COUNT_ARG_N(                                                                                         \
    _1, _2, _3, _4, _5, _6, _7, _8,                                                                          \
    _9, _10, _11, _12, _13, _14, _15, _16,                                                                   \
    _17, _18, _19, _20, _21, _22, _23, _24,                                                                  \
    _25, _26, _27, _28, _29, _30, _31, N, ...) N

#define COUNT_NARG_(...) COUNT_ARG_N(__VA_ARGS__)

//! Helper macro to count the number of arguments in __VA_ARGS__
#define COUNT_NARG(...) COUNT_NARG_(__VA_ARGS__,COUNT_RSEQ_N())

//! Macro to inject the template signature of the function
#define _FUNCTION_START                                                                                      \
    template<typename T, typename... Args>                                                                   \
    static void nestedPairCallback(PairDefinition<T> * const ptr, Args &&... args){

#define _FUNCTION_STOP }

#define _REGISTER_PAIRS_IMPL(count, ...) _REGISTER_PAIRS_##count(__VA_ARGS__)

#define _REGISTER_PAIRS_UTIL(...) _REGISTER_PAIRS_IMPL(__VA_ARGS__)

//! This macro constructs the entire function call and injects it in the source code
#define SPARTA_REGISTER_PAIRS(...)                                                                             \
    _FUNCTION_START                                                                                          \
    _REGISTER_PAIRS_UTIL(COUNT_NARG(__VA_ARGS__), __VA_ARGS__)                                               \
    _FUNCTION_STOP

#endif
