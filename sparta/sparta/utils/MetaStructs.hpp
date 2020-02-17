// <MetaStructs.hpp> -*- C++ -*-


/**
 * \file MetaStructs.hpp
 * \brief Contains a collection implementation of various
 * compile-time metaprogramming and Type-Detection APIs useful
 * for Template Metaprogramming.
 */

#ifndef __META_STRUCTS_H__
#define __META_STRUCTS_H__

#include <vector>
#include <array>
#include <functional>
#include <queue>
#include <stack>
#include <list>
#include <deque>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <type_traits>

#include "sparta/utils/Enum.hpp"

namespace MetaStruct {
    // If compiler is C++11 compliant, then use explicit aliases.
    #if __cplusplus == 201103L

    /**
    * \brief This templated struct takes a parameter pack and
    * return a nested value to be true, if all the elements in
    * the pack are unsigned types.
    *
    * This is the generic template struct.
    */
    template<typename...>
    struct all_unsigned;

    /**
    * \brief This is the empty template specialization.
    */
    template<>
    struct all_unsigned<> : public std::true_type {};

    /**
    * \brief This is the template specialization with one or more
    * elements in the pack.
    */
    template<typename Head, typename... Tail>
    struct all_unsigned<Head, Tail...> {
        static constexpr bool value {std::is_unsigned<typename std::decay<Head>::type>::value and
                                     all_unsigned<Tail...>::value};
    };

    /**
    * \brief This templated struct takes a parameter pack and
    * return a nested value to be true, if all the elements in
    * the pack are signed types.
    *
    * This is the generic template struct.
    */
    template<typename...>
    struct all_signed;

    /**
    * \brief This is the empty template specialization.
    */
    template<>
    struct all_signed<> : public std::true_type {};

    /**
    * \brief This is the template specialization with one or more
    * elements in the pack.
    */
    template<typename Head, typename... Tail>
    struct all_signed<Head, Tail...> {
        static constexpr bool value {std::is_signed<typename std::decay<Head>::type>::value and
                                     all_signed<Tail...>::value};
    };

    /**
    * \brief This templated struct takes a parameter pack and
    * return a nested value to be true, if all the elements in
    * the pack are of the same sign.
    */
    template<typename... Args>
    struct all_same_sign {
         static constexpr bool value {all_signed<Args...>::value or all_unsigned<Args...>::value};
    };

    /**
    * \brief This templated struct takes a parameter pack and
    * return a nested value to be true, if all the elements in
    * the pack are integral types.
    *
    * This is the generic template struct.
    */
    template<typename...>
    struct all_are_integral;

    /**
    * \brief This is the empty template specialization.
    */
    template<>
    struct all_are_integral<> : public std::true_type {};

    /**
    * \brief This is the template specialization with one or more
    * elements in the pack.
    */
    template<typename Head, typename... Tail>
    struct all_are_integral<Head, Tail...> {
        static constexpr bool value {std::is_integral<typename std::decay<Head>::type>::value and
                                     all_are_integral<Tail...>::value};
    };

    /** \brief Alias Template for std::enable_if.
    */
    template<bool B, typename T = void>
    using enable_if_t = typename std::enable_if<B, T>::type;

    /** \brief Alias Template for std::decay
    */
    template<typename T>
    using decay_t = typename std::decay<T>::type;

    /** \brief Alias Template for std::underlying_type
    */
    template<typename T>
    using underlying_type_t = typename std::underlying_type<T>::type;

    // If compiler is C++14/17 compliant, then use standard aliases.
    #elif __cplusplus > 201103L
    /** \brief Alias Template for std::enable_if.
    */
    template<bool B, typename T = void>
    using enable_if_t = typename std::enable_if_t<B, T>;

    /** \brief Alias Template for std::decay
    */
    template<typename T>
    using decay_t = typename std::decay_t<T>;

    /** \brief Alias Template for std::underlying_type
    */
    template<typename T>
    using underlying_type_t = typename std::underlying_type_t<T>;
    #endif

    /**
    * \brief This templated struct takes a target type and another
    * parameter pack of types and returns a nested boolean value
    * of true, if the target type matches any one of the types in
    * the parameter pack.
    */
    template<typename...>
    struct matches_any;

    /**
    * \brief This templated struct takes a target type and another
    * parameter pack of types and returns a nested boolean value
    * of true, if the target type matches any one of the types in
    * the parameter pack.
    *
    * This is template specialization when there are no types left
    * in the parameter pack.
    */
    template<typename T>
    struct matches_any<T> : public std::true_type {};

    /**
    * \brief This templated struct takes a target type and another
    * parameter pack of types and returns a nested boolean value
    * of true, if the target type matches any one of the types in
    * the parameter pack.
    *
    * This is template specialization when there are one or more types
    * left in the parameter pack.
    */
    template<typename T, typename Head, typename... Tail>
    struct matches_any<T, Head, Tail...> {
        static constexpr bool value {std::is_same<T, Head>::value or matches_any<T, Tail...>::value};
    };

    /**
    * \brief This templated struct takes a type and gives
    *  back a nested typedef of a pointer to that type.
    */
    template<typename T>
    struct add_pointer { using type = T *; };

    template<typename T>
    struct add_pointer<T *> { using type = T; };

    template<typename T>
    struct add_pointer<const T *> { using type = T; };

    template<typename T>
    struct add_pointer<T * const> { using type = T; };

    template<typename T>
    struct add_pointer<const T * const> { using type = T; };

    /** \brief Alias Template for add_pointer.
    */
    template<typename T>
    using add_pointer_t = typename add_pointer<T>::type;

    /**
    * \brief This templated struct lets us know about
    *  whether the datatype is actually an ordinary object or
    *  pointer to that object. This is specialized for
    *  a couple different signatures.
    */
    template<typename>
    struct is_any_pointer : public std::false_type {};

    template<typename T>
    struct is_any_pointer<T *> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<T * const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<const T *> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<const T * const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::shared_ptr<T>> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::shared_ptr<T> const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::shared_ptr<T> &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::shared_ptr<T> const &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::unique_ptr<T>> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::unique_ptr<T> const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::unique_ptr<T> &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::unique_ptr<T> const &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::weak_ptr<T>> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::weak_ptr<T> const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::weak_ptr<T> &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<std::weak_ptr<T> const &> : public std::true_type {};

    /*!
    * \brief Template type helper that removes any pointer.
    * A modeler may call certain APIs with shared pointers to the
    * actual Collectable classes, or templatize Collectables with
    * pointers to collectable objects.
    * To make our API have a single interface and still work when passed
    * pointers, we will remove the pointer and then do all the decision
    * making work, by default.
    * It is harmless if the modeler passes a non pointer type as
    * removing a pointer from something which is not a pointer
    * results in itself.
    */
    template<typename T>
    struct remove_any_pointer { using type = T; };

    template<typename T>
    struct remove_any_pointer<T *> { using type = T; };

    template<typename T>
    struct remove_any_pointer<T * const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<const T *> { using type = T; };

    template<typename T>
    struct remove_any_pointer<const T * const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::shared_ptr<T>> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::shared_ptr<T> const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::shared_ptr<T> &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::shared_ptr<T> const &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::unique_ptr<T>> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::unique_ptr<T> const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::unique_ptr<T> &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::unique_ptr<T> const &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::weak_ptr<T>> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::weak_ptr<T> const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::weak_ptr<T> &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<std::weak_ptr<T> const &> { using type = T; };

    /** \brief Alias Template for remove_pointer.
    */
    template<typename T>
    using remove_any_pointer_t = typename remove_any_pointer<T>::type;

    /**
    * \brief This templated struct takes a type and tells
    *  us whether that type is a STL container.
    */
    template<typename>
    struct is_stl_container : std::false_type {};

    template<typename T, std::size_t N>
    struct is_stl_container<std::array<T, N>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::vector<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::deque<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::list<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::forward_list<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::set<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::multiset<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::map<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::multimap<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::unordered_set<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::unordered_multiset<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::unordered_map<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::unordered_multimap<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::stack<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::queue<Args...>> : public std::true_type {};

    template<typename... Args>
    struct is_stl_container<std::priority_queue<Args...>> : public std::true_type {};

    template <typename T>
    struct is_stl {
        static constexpr bool value =
            is_stl_container<typename std::decay<T>::type>::value;
    };

    /**
    * \brief This Variadic templated struct contains a nested value
    *  which stores the length of any parameter pack it gets templatized on.
    */
    template<typename... Args>
    struct parameter_pack_length {
        static constexpr std::size_t value = sizeof...(Args);
    };

    template<typename...>
    struct peek_last_type;

    template<typename Head, typename... Tail>
    struct peek_last_type<Head, Tail...> : public peek_last_type<Tail...> {};

    template<typename Tail>
    struct peek_last_type<Tail> {
        using type = Tail;
    };

    template<typename... Args>
    using peek_last_type_t = typename peek_last_type<Args...>::type;
    /**
    * \brief This Variadic templated struct helps us know about the
    *  type of the very last or tail item in a parameter pack.
    *  It works by peeling of one parameter at a time from the pack
    *  and when it hits the last item, it specializes the struct by
    *  typedefing the template parameter T in its namespace.
    */
    template<std::size_t, typename...>
    struct last_index_type {};

    /**
    * \brief Base case when we have just the last item of the
    *  parameter pack.
    */
    template<typename T>
    struct last_index_type<0, T> { using type = T; };

    /**
    * \brief Recursive case when we recursively peel off items
    *  from the front of the pack until we hit the last item.
    */
    template<std::size_t N, typename T, typename... Args>
    struct last_index_type<N, T, Args...> : public last_index_type<N - 1, Args...> {};

    /** \brief Alias Template for last_index_type.
    */
    template<std::size_t N, typename... Args>
    using last_index_type_t = typename last_index_type<N, Args...>::type;

    /**
    * \brief This Variadic templated struct basically works much like
    *  std::integer_sequence. It represents a compile-time sequence of
    *  integers. This is used as a parameter to the Collection function template
    *  and helps in type deduction, unpacking and transforming our tuple of
    *  random parameters back into a variadic template.
    *
    *  Given a tuple, this Indices Sequence Generator takes that tuple and
    *  transforms it back to a variadic template argument.
    */
    template<std::size_t...>
    struct sequence_generator {};

    /**
    * \brief This is the generic template.
    */
    template<int N, std::size_t... S>
    struct generate_sequence : generate_sequence<N - 1, N - 1, S...> {};

    /**
    * \brief This is the specialization which kicks in when the first
    *  template parameter is 0.
    */
    template<std::size_t... S>
    struct generate_sequence<0, S...> { using type = sequence_generator<S...>; };

    /** \brief Alias Template for generate_sequence.
    */
    template<std::size_t... Args>
    using generate_sequence_t = typename generate_sequence<Args...>::type;

    /**
    * \brief This templated struct lets us know about
    *  the return type from any random function pointer. This is
    *  specialized for a couple different signatures.
    */
    template<typename T>
    struct return_type { using type = T; };

    template<typename R, typename... Ts>
    struct return_type<std::function<R (Ts...)>> { using type = R; };

    template<typename R, typename... Ts>
    struct return_type<std::function<R (Ts...)> const> { using type = R; };

    template<typename R, typename T, typename... Ts>
    struct return_type<std::function<R (Ts...)> T:: *> { using type = R; };

    template<typename R, typename T, typename... Ts>
    struct return_type<std::function<R (Ts...)> const T:: *> { using type = R; };

    template<typename R, typename T, typename... Ts>
    struct return_type<std::function<R (Ts...)> T:: * const &> { using type = R; };

    template<typename R, typename T, typename... Ts>
    struct return_type<std::function<R (Ts...)> const T:: * const> { using type = R; };

    template<typename R, typename... Ts>
    struct return_type<R (*)(Ts...)> { using type = R; };

    template<typename R, typename... Ts>
    struct return_type<R& (*)(Ts...)> { using type = R; };

    template<typename R, typename T>
    struct return_type<R (T:: *)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<R & (T:: *)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> (T:: *)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> & (T:: *)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<R (T:: * const)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<R & (T:: * const)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> (T:: * const)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> & (T:: * const)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<R (T:: * const &)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<R & (T:: * const &)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> (T:: * const &)() const> { using type = R; };

    template<typename R, typename T>
    struct return_type<std::shared_ptr<R> & (T:: * const &)() const> { using type = R; };

    /** \brief Alias Template for return_type.
    */
    template<typename T>
    using return_type_t = typename return_type<T>::type;

    /**
    * \brief Templated struct for detecting Boolean Type.
    */
    template<typename>
    struct is_bool : public std::false_type {};

    template<>
    struct is_bool<bool> : public std::true_type {};

    template<>
    struct is_bool<bool &> : public std::true_type {};

    template<>
    struct is_bool<bool const &> : public std::true_type {};

    template<>
    struct is_bool<bool const> : public std::true_type {};

    /**
    * \brief This templated struct lets us know about
    * whether the datatype is actually an std::pair object.
    * This is specialized for a couple different signatures.
    * The collection procedure for a pair object is broken down
    * into first collecting the first member and then, the second.
    */
    template<typename>
    struct is_pair : public std::false_type {};

    template<typename T, typename U>
    struct is_pair<std::pair<T, U>> : public std::true_type{} ;

    template<typename T, typename U>
    struct is_pair<std::pair<T, U> &> : public std::true_type {};

    template<typename T, typename U>
    struct is_pair<std::pair<T, U> const &> : public std::true_type {};

    template<typename T, typename U>
    struct is_pair<std::pair<T, U> const> : public std::true_type {};

    /**
    * \brief Templated struct for detecting String Type.
    */
    template<typename>
    struct is_string : public std::false_type {};

    template<>
    struct is_string<std::string> : public std::true_type {};

    template<>
    struct is_string<std::string &> : public std::true_type {};

    template<>
    struct is_string<std::string const &> : public std::true_type {};

    template<>
    struct is_string<std::string const> : public std::true_type {};

    /**
    * \brief Templated struct for detecting char pointer type.
    */
    template<typename>
    struct is_char_pointer : public std::false_type{};

    template<>
    struct is_char_pointer<char*> : public std::true_type{};

    template<>
    struct is_char_pointer<const char*> : public std::true_type{};

    template<>
    struct is_char_pointer<char* const> : public std::true_type{};

    template<>
    struct is_char_pointer<const char* const> : public std::true_type{};

    /**
    * \brief Detect whether template parameter is sparta::utils::Enum type.
    *  Case when it is not a sparta::utils::Enum type.
    */
    template<typename T>
    struct is_sparta_enum : public std::false_type {};

    /**
    * \brief Detect whether template parameter is sparta::utils::Enum type.
    *  Case when it is a sparta::utils::Enum type.
    */
    template<typename T>
    struct is_sparta_enum<sparta::utils::Enum<T>> : public std::true_type {};
} // namespace MetaStruct
#endif // __META_STRUCTS_H__
