// <MetaTypeList.hpp> -*- C++ -*-


/**
 * \file MetaTypeList.hpp
 * \brief Contains the implementation of Metatype_lists,
 *  a compile-time list for storing different type structures
 *  which can be accessed by using nested type aliases inside.
 *  Metatype_lists are extremely useful in Template MetaProgramming.
 */

#ifndef __META_TYPE_LIST_H__
#define __META_TYPE_LIST_H__

#include <type_traits>
#include <memory> // For unique_ptr

namespace MetaTypeList {

    /**
     * \brief The variadic class template type_list
     *  has no implementation and is empty by itself.
     */
    template<typename...>
    class type_list {};

    /**
     * \brief The Variadic Class Template create takes a
     *  variable number of different types and creates
     *  a well-formed type_list consisting of those types.
     */
    template<typename...>
    class create;

    /**
     * \brief Specialization where no type_list exist.
     */
    template<typename Head, typename... Tail>
    class create<type_list<>, Head, Tail...> :
        public create<type_list<Head>, Tail...> {};

    /**
     * \brief Specialization where type_list exists in
     *  the intermediate derivations.
     */
    template<typename Head, typename... Tail, typename... Elements>
    class create<type_list<Elements...>, Head, Tail...> :
        public create<type_list<Elements..., Head>, Tail...> {};

    /**
     * \brief Base case where we have consumed
     *  all the paramters in the pack and created
     *  the type_list. We just typedef it.
     */
    template<typename... Elements>
    class create<type_list<Elements...>> {
    public:
        using type = type_list<Elements...>;
    };

    /**
     * \brief create Alias Template.
     */
    template<typename... Elements>
    using create_t = typename create<type_list<Elements...>>::type;

    /**
     * \brief The class template frontT takes type_list
     *  as template param. and typedefs the type of the
     *  first template param., templatized on type_list,
     *  in its own nested type alias.
     */
    template<typename>
    class frontT;

    /**
     * \brief frontT Template Specialization.
     */
    template<typename Head, typename... Tail>
    class frontT<type_list<Head, Tail...>> {
    public:
        using type = Head;
    };

    /**
     * \brief frontT Alias Template.
     */
    template<typename List>
    using front = typename frontT<List>::type;

    /**
     * \brief The class template pop_frontT takes type_list
     *  as template param., extracts out the first type in
     *  type_list template param, and typedefs the rest of the
     *  parameter pack in its own nested type alias.
     */
    template<typename>
    class pop_frontT;

    /**
     * \brief pop_frontT Template Specialization.
     */
    template<typename Head, typename... Tail>
    class pop_frontT<type_list<Head, Tail...>> {
    public:
        using type = type_list<Tail...>;
    };

    /**
     * \brief pop_frontT Alias Template.
     */
    template<typename List>
    using pop_front = typename pop_frontT<List>::type;

    /**
     * \brief The class template push_frontT takes an
     *  existing type_list and a new type, inserts that
     *  new type at front of the type_list and instantiate
     *  a new type_list in this process and typedefs it
     *  inside itself.
     */
    template<typename, typename>
    class push_frontT;

    /**
     * \brief push_frontT Template Specialization.
     */
    template<typename... Elements, typename NewElement>
    class push_frontT<type_list<Elements...>, NewElement> {
    public:
        using type = type_list<NewElement, Elements...>;
    };

    /**
     * \brief push_frontT Alias Template.
     */
    template<typename List, typename NewElement>
    using push_front = typename push_frontT<List, NewElement>::type;

    /**
     * \brief The class template push_backT takes an
     *  existing type_list and a new type, inserts that
     *  new type at end of the type_list and instantiate
     *  a new type_list in this process and typedefs it
     *  inside itself.
     */
    template<typename, typename>
    class push_backT;

    /**
     * \brief push_backT Template Specialization.
     */
    template<typename... Elements, typename NewElement>
    class push_backT<type_list<Elements...>, NewElement> {
    public:
        using type = type_list<Elements..., NewElement>;
    };

    /**
     * \brief push_backT Alias Template.
     */
    template<typename List, typename NewElement>
    using push_back = typename push_backT<List, NewElement>::type;

    /**
     * \brief This class template gives us the type of Nth
     *  element in a type_list by taking advantage of the nested
     *  alias template in frontT<T>.
     */
    template<typename List, unsigned N>
    class nth_elementT : public nth_elementT<pop_front<List>, N-1> {};

    /**
     * \brief nth_elementT Template Specialization.
     */
    template<typename List>
    class nth_elementT<List, 0> : public frontT<List> {};

    /**
     * \brief nth_elementT Alias Template.
     */
    template<typename List, unsigned N>
    using nth_element = typename nth_elementT<List, N>::type;

    /**
     * \brief Class Template is_empty Generic case.
     */
    template<typename>
    class is_empty : public std::false_type {};

    /**
     * \brief Class Template is_empty Specialization.
     */
    template<>
    class is_empty<type_list<>> : public std::true_type {};

    /**
     * \brief Class Template get_pack_size return the
     *  current number of types in the type_list.
     */
    template<typename...>
    class get_pack_size;

    /**
     * \brief Unroll Templates till Base case
     *  specialization is reached and accumulate.
     */
    template<typename Head, typename... Tail>
    class get_pack_size<type_list<Head, Tail...>> {
    public:
        static constexpr std::size_t value =
            1 + get_pack_size<type_list<Tail...>>::value;
    };

    /**
     * \brief Base case specialization.
     */
    template<>
    class get_pack_size<type_list<>> {
    public:
        static constexpr std::size_t value = 0;
    };

    /**
     * \brief Variadic Class Template which takes a
     *  type and matches it if that is a type_list.
     *  Generic case results to false.
     */
    template<typename...>
    struct is_meta_typelist : public std::false_type {};

    /**
     * \brief Template Specialization when the type indeed
     *  is a type_list.
     */
    template<typename... Args>
    struct is_meta_typelist<MetaTypeList::type_list<Args...>> : public std::true_type {};

    /**
     * \brief Class Template to return the index
     *  of a certain type in a certain typelist.
     */
    template<std::size_t, typename...>
    class get_index;

    /**
     * \brief Class Template to return the index
     *  of a certain type in a certain typelist.
     *  Case when queried type matches front type.
     */
    template<std::size_t S, typename T, typename Head, typename... Tail>
    class get_index<S, T, type_list<Head, Tail...>,
                    typename std::enable_if<std::is_same<T, Head>::value>::type> {
    public:
        static constexpr std::size_t value = (S - sizeof...(Tail)) - 1;
    };

    /**
     * \brief Class Template to return the index
     *  of a certain type in a certain typelist.
     *  Case when queried type does not match front type.
     */
    template<std::size_t S, typename T, typename Head, typename... Tail>
    class get_index<S, T, type_list<Head, Tail...>,
                    typename std::enable_if<!std::is_same<T, Head>::value>::type> :
        public get_index<S, T, type_list<Tail...>> {};

} // namespace MetaTypeList

#endif //__META_TYPE_LIST_H__
