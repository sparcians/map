// <SpartaKeyPairs.hpp> -*- C++ -*-


/**
 * \file SpartaKeyPairs.hpp
 *
 * \brief Contains classes used for SpartaKeyPair collection
 * concepts.
 */

#ifndef __KEY_PAIR_SPARTA_H__
#define __KEY_PAIR_SPARTA_H__

#include <memory>
#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include <iomanip>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <limits>
#include <unordered_map>
#include <functional>

#include "sparta/pairs/RegisterPairsMacro.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MetaTypeList.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "sparta/utils/DetectMemberUtils.hpp"

namespace sparta {

    /**
     * \class PairCache
     * \brief A pair cache is updated by KeyPairs. The pair cache has
     * a list of std::pair<string, string> where the two values in the pair
     * represent the most recent strigalized data from the keypairs at any given
     * time, a list of unsigned 16bit integers which hold the size in bytes of the values and
     * a list of strings which may contain the actual string representations of the intermediate
     * values or nothing, if the values don't have a string representation.
     *
     * The PairCache is accessed by the "Collectors" to collect the data. The
     * collectors never interface with KeyPairs directly since the paircache
     * always has the most relevant data. Also, during the collect phase, the "sizeof" vector is filled
     * with the sizeof(DataType) of every value. Additionally, the String List is populated with strings
     * or nothing depending on whether the value has an ultimate string representation or not.
     */
    class PairCache {
    public:
        template<class X> friend class PairCollecter;
        typedef std::pair<std::string, std::string> CachedPair;
        typedef std::pair<uint64_t, bool> ValidPair;

        /**
         * \brief Allow the key pairs to pass back to us their values if needed.
         */
        inline void updateNumericCache(const uint64_t val, const uint32_t id) {
            data_list_[id].first = val;
            data_list_[id].second = true;
        }

        //! Method which updates the sizeof of the value at index id.
        inline void updateSizeOfCache(const uint16_t val, const uint32_t id) {
            sizeof_list_[id] = val;
        }

        //! Method which updates the String Representation of the value at index id.
        inline void updateStringCache(const std::string & val, const uint32_t id) {
            string_value_list_[id] = val;
        }

        //! Method which updates the String Representation of the value at index id.
        inline void updateStringCache(std::string && val, const uint32_t id) {
            string_value_list_[id] = std::move(val);
        }

        //! Method which updates the Representation of the value at index id.
        inline void updateFormatCache(const uint16_t val, const uint32_t id) {
            formatter_list_[id] = val;
        }

        //! Method which updates the string format for display formatting.
        inline void updateArgosFormatGuide(const std::string & formatString) {
            string_format_ = formatString;
        }

        /**
        * \brief add the new key strings to have a position in the cache.
        */
        void addKey(const std::string & key, const uint32_t) {

            //! Fill up vector with placeholder values. ValidValue can also
            //  be used here.
            sizeof_list_.emplace_back(std::numeric_limits<uint16_t>::max());
            formatter_list_.emplace_back(std::numeric_limits<uint16_t>::max());
            name_strings_list_.emplace_back(key);
            string_value_list_.emplace_back("");
            data_list_.emplace_back(std::make_pair(std::numeric_limits<uint64_t>::max(), false));
        }

        /**
        * \brief A method to reserve all the containers to minimize costly reallocations.
        */
        void reserveThemAll(const size_t capacity) {
            sizeof_list_.reserve(capacity);
            name_strings_list_.reserve(capacity);
            data_list_.reserve(capacity);
            string_value_list_.reserve(capacity);
            formatter_list_.reserve(capacity);
        }

        //! Return the format guide to be used in Argos Viewer.
        inline const std::string & getArgosFormatGuide() const {
            return string_format_;
        }

        //! Method which return a constant reference to our private vector
        //  of pairs of names and values.
        inline const std::vector<ValidPair> & getDataVector() const {
            return data_list_;
        }

        //! Method which return a constant reference to our private vector
        //  of name strings.
        inline const std::vector<std::string> & getNameStrings() const {
            return name_strings_list_;
        }

        //! Method which return a constant reference to our private vector
        //  of sizeof integers.
        inline const std::vector<uint16_t> & getSizeOfVector() const {
            return sizeof_list_;
        }

        //! Method which return a constant reference to our private vector
        //  of value strings.
        inline const std::vector<std::string> & getStringVector() const {
            return string_value_list_;
        }

        //! Method which return a constant reference to our private vector
        //  of Representation.
        inline const std::vector<uint16_t> & getFormatVector() const {
            return formatter_list_;
        }

        //! Method which returns a string vector for PEvent generation.
        const std::vector<CachedPair> & getPEventLogVector() const {
            pevents_log_vector_.clear();
            for(std::size_t i = 0; i < name_strings_list_.size(); ++i) {
                if(!string_value_list_[i].empty()) {
                    pevents_log_vector_.emplace_back(std::make_pair(
                        name_strings_list_[i], string_value_list_[i]));
                }
                else if(data_list_[i].second) {
                    switch(formatter_list_[i]) {
                        case 1 :
                        {
                            std::stringstream ss;
                            ss << std::oct << data_list_[i].first;
                            pevents_log_vector_.emplace_back(std::make_pair(
                                name_strings_list_[i], ss.str()));
                            break;
                        }
                        case 2 :
                        {
                            std::stringstream ss;
                            ss << std::hex << data_list_[i].first;
                            pevents_log_vector_.emplace_back(std::make_pair(
                                name_strings_list_[i], ss.str()));
                            break;
                        }
                        default :
                            pevents_log_vector_.emplace_back(
                                std::make_pair(
                                    name_strings_list_[i],
                                    std::to_string(data_list_[i].first)));
                            break;
                    }
                }
            }
            return pevents_log_vector_;
        }

    private:

        /**
        * \brief We maintain a vector of string pairs<key, data>,
        * a vector of unsigned 16bit integers and a vector of strings to regenerate
        * the final output. After key pairs accept us, and then pass
        * back their data via visit, we update our string data, sizeof data
        * as well as the String Representation data, if needed, with the new data.
        * We are also keeping a vector of pure strings for PEvent logs and a list
        * custom YAML blocks which is needed to build the YAML file after collection
        * is complete in Collectable class.
        */
        std::string string_format_;
        std::vector<uint16_t> formatter_list_;
        std::vector<uint16_t> sizeof_list_;
        std::vector<std::string> name_strings_list_;
        std::vector<std::string> string_value_list_;
        std::vector<ValidPair> data_list_;
        mutable std::vector<CachedPair> pevents_log_vector_;
    };

    /**
    * \class PairCollector
    * \brief A PairCollector is the base class of any collector.
    * Simply override the abstract virtual generateCollectionString_()
    * to run your collection and output the data.
    *
    * \tparam PairDefinitionType is the specific PairDefinition
    * that the PairCollector is able to record.
    */
    template<typename PairDefinitionType>
    class PairCollector {
    public:
        typedef std::pair<std::string, std::string> CachedPair;
        typedef std::pair<uint64_t, bool> ValidPair;

        /**
        * \brief Construct
        */
        PairCollector() : pair_definition_() {

            // Make sure our pair cache is properly set up.
            pair_definition_.finalizeKeys(&pair_cache_);
        }

        /**
         * \brief virtual destructor
         */
        virtual ~PairCollector() {}

        /**
         * \brief Invoke addPositionalPairArg on the pair definition. This a
         * allows added extra positional arguments through the collector itself
         * rather than defintion new a PairDefinition.
         * \see PairDefinition::addPositionalPairArg() for more details.
         * \note This should only be used
         * when individual events have special data that can be collected.
         */
        template<typename DataT>
        void addPositionalPairArg(const std::string & name,
            std::ios_base::fmtflags format = std::ios_base::dec) {
            pair_definition_.template addPositionalPairArg<DataT>(
                name, format, &pair_cache_);
        }

        /**
        * \brief provide public access to the pairs.
        * \note The const reference here is important!
        */

        //! Public method which is called by writeRecord function
        //  in Collectable class to get the Name Strings and the Values.
        inline const std::vector<ValidPair> & getDataVector() const {
            return pair_cache_.getDataVector();
        }

        //! Public method which is called by writeRecord function
        //  in Collectable class to get the Name Strings.
        inline const std::vector<std::string> & getNameStrings() const {
            return pair_cache_.getNameStrings();
        }

        //! Public method which is called by writeRecord function
        //  in Collectable class to get the Argos Format Guide.
        inline const std::string & getArgosFormatGuide() const {
            return pair_cache_.getArgosFormatGuide();
        }

        //! Public method which is called by writeRecord function
        //  in Collectable class to get the sizeof values.
        inline const std::vector<uint16_t> & getSizeOfVector() const {
            return pair_cache_.getSizeOfVector();
        }

        //! Public method which is called by writeRecord function
        //  in Collectable class  to get the actual String Representations
        //  of the values, if any.
        inline const std::vector<std::string> & getStringVector() const {
            return pair_cache_.getStringVector();
        }

        //! Public method which is called by writeRecord function
        //  in Collectable class to get the Representation values.
        inline const std::vector<uint16_t> & getFormatVector() const {
            return pair_cache_.getFormatVector();
        }

        //! Public method which is called by Collectable/PEvents to
        //  collect PEvent string generation.
        inline const std::vector<CachedPair> & getPEventLogVector() const {
            return pair_cache_.getPEventLogVector();
        }

        /**
        * \brief is this pair collector currently doing work
        * to run collection logic.
        */
        inline bool isCollecting() const {
            return collecting_;
        }

    protected:

        /**
         * \brief provide a method that visits all of the KeyPairs and
         * then, if the pair cache was changed during this process,
         * call the generateCollectionString_(). By doing this
         * the PairCollector has done it's part of recording the data,
         * it is now up to the Child class to override generateCollectionString_()
         * and record the data in the PairCache.
         */
        template<typename... Targs>
        void defaultCollect_(const Targs &... pos_args) {

            // Only do a collection if any of our keys were dirty, i.e our
            // updateData_ method was reached
            if(collect_(pos_args...)) {
                generateCollectionString_();
            }
        }

        /**
         * \brief Tell all of the pairs to collect themselves.
         * Returns true if any of the pairs were dirty during the collection
         */
        template<typename... Targs>
        bool collect_(const Targs &... args) {
            return pair_definition_.populatePairs(
                &pair_cache_, args...);
        }

        /**
         * \brief The method that you do your magic to capture the collection data.
         * \note This method is abstract
         */
        virtual void generateCollectionString_() = 0;

        /**
         * \brief Allow the child class to turn us on and off.
         * The child class is likely associated with it's own mechanism for controlling
         * on and off, so give it the ability to propagate that knowledge to this class.
         */
        inline void turnOn_() {
            collecting_ = true;
        }

        /**
         * \brief Allow the child class to turn us on and off.
         * The child class is likely associated with it's own mechanism for controlling
         * on and off, so give it the ability to propagate that knowledge to this class.
         */
        inline void turnOff_() {
            collecting_ = false;
        }

        // The user defined entity that defines which pairs are collected.
        PairDefinitionType pair_definition_;

        // A cache of string pairs of collected data. This data is always up to date when
        // generateCollectionString_ is called.
        PairCache pair_cache_;

        // We are not collecting by default
        bool collecting_ = false;
    };

    // Forward declare collectable entity since we need to friend it.
    template <class X>
    class PairDefinition;

    /**
     * \class Pair
     * \brief A pair is the ultimate base class of all key pair types.
     * At the least it holds onto the key, and has the ability to set
     * formatting as hexidecimal and a function for applying this format option
     * to a output stream.
     */
    class Pair {
    private:

        //! A typedef for defining formatters for custom attributes.
        typedef std::ios_base::fmtflags FormatFlags;

        //! A struct used for cacheing output format information about each key pair
        struct PrePostTags {
            PrePostTags() : pre_format(), post_format() {}

            char fill_char = ' ';
            int swidth = 0;
            std::string prefix = "";
            std::string postfix = "";
            FormatFlags pre_format;
            FormatFlags post_format;
        };

    public:
        Pair(const std::string & name, const uint32_t id) :
            name_(name), id_(id) {}

        virtual ~Pair() {}

        inline const std::string & getKey() const {
            return name_;
        }

        /**
         * \brief populate the formatting tags to make this key's value
         * display in proper format. Old way of doing stuff.
         */
        void setFormatter(const FormatFlags & formatter) {
            switch(formatter) {
                case std::ios::hex :
                setHex();
                break;
                case std::ios::oct :
                setOct();
                break;
                default:
                return;
            }
        }

        /**
         * \brief populate the formatting tags to make this key's value
         * display in proper format. New way of doing stuff.
         */
        inline void applyFormat(const FormatFlags & formatter) {
            switch(formatter) {
                case std::ios::hex :
                    f_switch_ = 2;
                    break;
                case std::ios::oct :
                    f_switch_ = 1;
                    break;
                default :
                    f_switch_ = 0;
                    return;
            }
        }

        /**
         * \brief populate the formatting tags to hex.
         */
        void setHex() {
            static const int hex_length = 8;
            format_tags_.pre_format = FormatFlags(std::ios::hex);
            format_tags_.prefix = "0x";
            format_tags_.swidth = hex_length;
            format_tags_.fill_char = '0';
        }

        /**
         * \brief populate the formatting tags to oct.
         */
        void setOct() {
            static const int oct_length = 8;
            format_tags_.pre_format = FormatFlags(std::ios::oct);
            format_tags_.prefix = "0";
            format_tags_.swidth = oct_length;
            format_tags_.fill_char = '0';
        }

    private:
        std::string name_;

    protected:
        /**
         * \brief Format the stringstream.
         */
        void formatStream_(std::stringstream & stream) {

            // Set up formatting
            stream.flags(format_tags_.pre_format);
            stream << format_tags_.prefix;
            stream << std::setfill(format_tags_.fill_char);
            stream << std::setw(format_tags_.swidth);
        }

        //! Formatting options for this key's value.
        PrePostTags format_tags_;
        uint32_t id_ = 0;
        uint16_t f_switch_ = 0;
    };

    /**
     * \class ArbitraryPair
     * \brief An arbitrary Pair that purely accepts new data and updates the string cache.
     * The data for this pair by assumption is considered "changed data" if populateArgData is
     * called.
     * \tparam DataType the type of data that the pair uses.
     */
    template<class DataType>
    class ArbitraryPair : public Pair {
        using Pair::format_tags_;
        using Pair::id_;

    public:
        ArbitraryPair(const std::string & name, const uint32_t id) :
            Pair(name, id) {}

        bool populateArgData(PairCache * c, const DataType & dat) {

            // let the pair_cache use my data as a string. We were dirty by this point.
            // so we need to change the data in the pair pair_cache.
            std::stringstream s;
            Pair::formatStream_(s);

            // stringalize the actual data.
            s << dat;

            // Pass the data up to the key pair string cache.
            c->updateStringCache(s.str(), id_);
            return false;
        }
    };

    /**
     * \class BasePairFromEntity
     * \brief Just the base type of PairFromEntity so we can keep them in a list
     * without knowing the datatype the pair is collecting.
     * \tparam EntityType The type that is actually be collected by a
     * pair collector. So if we want to be able to collect an object
     * of type "A", then we have a PairDefinition<A>, and
     * EntityType="A"
     */
    template <class EntityType>
    class BasePairFromEntity : public Pair {

        // Collectable entity needs to be a friend of base pair
        friend PairDefinition<EntityType>;
    public:

        /**
         * \brief Construct a new pair.
         * \param name is the the key for the pair.
         */
        BasePairFromEntity(const std::string & name, const uint32_t id) :
            Pair(name, id) {}

        virtual bool populateFromEntity(PairCache *, const EntityType &)  = 0;
    };

    template <class DataT, class EntityType, class EntityBaseClass>
    class KeyPEventPairFromEntity : public BasePairFromEntity<EntityType> {
    private:

        typedef DataT (EntityBaseClass::*FuncType) () const;

        // We never want to cache a reference, we only cache a copy.
        typedef typename std::remove_reference<DataT>::type UnRefDataT;

        using Pair::id_;

    public:

        /**
         * \brief Construct a new KeyPairFromEntity
         * \param name the key
         * \param func a function pointer to where we can grab the
         * value for our pair.
         * \param i an identifier for this keypair.
         */
        KeyPEventPairFromEntity(const std::string & name, FuncType func, uint32_t i) :
            BasePairFromEntity<EntityType>(name, i), data_cpy_(), func_(func) {

            // no hackery here.
            sparta_assert(func_ != nullptr);
        }

        /**
         * \brief Pass data to this Key value pair, and allow the key value pair to update
         * the pair cache with the new data
         */
        virtual bool populateFromEntity(PairCache * c, const EntityType & owner) override final {

            // Get the new data, as a copy
            const UnRefDataT tmp = ((owner).*func_)();

            // Since we could potentially start out empty with
            // none primative data, with a non-default constructor
            // we are force to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new UnRefDataT(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }

            // let the pair_cache use my data as a string. We were dirty by this point.
            // so we need to change the data in the pair pair_cache.
            std::stringstream s;

            // Set up formatting
            Pair::formatStream_(s);

            // stringalize the actual data.
            s << tmp;

            // Pass the data up to the key pair string cache.
            c->updateStringCache(s.str(), id_);

            // cache the new data, so we can check if it is dirty next time.
            data_cpy_.reset(new UnRefDataT(tmp));
            return false;
        }

    protected:
        std::unique_ptr<UnRefDataT> data_cpy_;
        FuncType func_;
    };

    /**
    *  \class KeyPairFromEntity
    *  \brief A KeyPairFromEntity has a name, and can be visited by a pair pair_cache,
    *  when collected, this keypair can update it's data in the pair_cache if
    *  it was dirty.
    *
    *  \note KeyPairFromEntity requires that the unreferenced Datatype being collected has
    *  a copy constructor.
    *
    *  \tparam EntityType The owner object class type who may or may not own the key's value.
    *  \tparam Args... A parameter pack of Function Pointers right from the outermost class
    *  to the very innermost nested class.
    */
    template<typename...>
    class KeyPairFromEntity;

    template<typename EntityType, typename... Args>
    class KeyPairFromEntity<EntityType, Args...> :
        public BasePairFromEntity<EntityType> {
    private:
        using Pair::f_switch_;
        using Pair::id_;

        //! When dealing with Virtual Abstract Base Pointers, we need to know
        //  the underlying Derived type that pointer is pointing to. This is
        //  because all the Method Pointers that were collected are members of
        //  Derived classes. DerivedTypeVerification_ class is a technique of
        //  figuring out which Derived type, a Virtual Abstract Base pointer is
        //  actually pointing to. It uses the TypeList contained in the Base class
        //  along with TypeList Unrolling to figure that out and then give control
        //  back to KeyPairFromEntity method, populateFromEntityUtility_.
        template<typename, typename, typename, typename...>
        class DerivedTypeVerification_;

        //! Use the technique of deriving from One-Less Type Class to elegantly
        //  unroll the TypeList and do the required processing.
        template<typename E, typename T, typename H, typename... Tail, typename... Ts>
        class DerivedTypeVerification_<E, T, MetaTypeList::type_list<H, Tail...>, Ts...> :
            public DerivedTypeVerification_<E, T, MetaTypeList::type_list<Tail...>, Ts...> {
        public:

            DerivedTypeVerification_(
                const E & outer, PairCache *& c, const T & owner, Ts &&... ts,
                MetaStruct::enable_if_t<MetaStruct::is_any_pointer<MetaStruct::decay_t<T>>::value>* = 0) :
                DerivedTypeVerification_<E, T, MetaTypeList::type_list<Tail...>, Ts...>(
                    outer, c, owner, std::forward<Ts>(ts)...) {
                const auto dtype = dynamic_cast<MetaStruct::add_pointer_t<H>>(owner.get());
                if(dtype) {
                    outer->populateFromEntityUtility_(c, dtype, std::forward<Ts>(ts)...);
                }
            }
        };

        //! Base case when we are done unrolling the TypeList.
        template<typename E, typename T, typename... Ts>
        class DerivedTypeVerification_<E, T, MetaTypeList::type_list<>, Ts...> {
        public:
            DerivedTypeVerification_(const E &, PairCache *&, const T &, Ts &&...) {}
        };

        /**
         * \brief Converts a tuple of Function Pointers into Variadic Template and passes it on.
         * \param c is a PairCache reference.
         * \param owner is the object for which we are querying this value.
         * \param sequence_generator is a compile-time generator of a sequence of integers.
         */
        template<std::size_t... S>
        bool unpackTupleAsIndices_(
            PairCache *& c, const EntityType & owner, MetaStruct::sequence_generator<S...>) {
            return populateFromEntityUtility_(c, owner, std::get<S>(parameterPack_)...);
        }

        //! During object-method invocation, if we encounter an Abstract type, we
        //  need to handle it specially. Here T is an abstract base type.
        //  \code{.cpp}
        //  Generic Base pointer b;
        //  Member Function:
        //      const std::shared_ptr<Base> & getBP() const { return b; }
        //  addPair() callset -> (X::b, X::getBP, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename... Ts>
        MetaStruct::enable_if_t<

            //! Abstract Type is encountered.
            std::is_abstract<MetaStruct::decay_t<MetaStruct::remove_any_pointer_t<T>>>::value and
            MetaStruct::is_any_pointer<T>::value ,bool>
        populateFromEntityUtility_(PairCache *& c, const T & object, Ts &&... ts) {

            //! This abstract type must have a TypeList.
            static_assert(MetaTypeList::is_meta_typelist<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type does not contain a well-formed MetaTypeList.");

            //! The TypeList cannot be empty.
            static_assert(MetaTypeList::get_pack_size<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type cannot contain an empty MetaTypeList.");

            //! Start Unrolling that TypeList to verify Derived Type.
            auto && verify_derived_type = DerivedTypeVerification_<
                decltype(this), T,
                    typename MetaStruct::decay_t<
                        MetaStruct::remove_any_pointer_t<T>>::derived_type_list,
                            Ts &&...>(
                this, c, object, std::forward<Ts>(ts)...);

            (void)verify_derived_type;
            return false;
        }

        //! During object-method invocation, if we encounter an Abstract type, we
        //  need to handle it specially. Here T is an abstract base type but it is
        //  not a pointer. Overload for multiple method pointers in list.
        //  \code{.cpp}
        //  Generic Base pointer b;
        //  Member Function:
        //      const std::shared_ptr<Base> & getBP() const { return b; }
        //  addPair() callset -> (X::*b, X::getBP, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<
            //! Abstract Type is encountered.
            std::is_abstract<MetaStruct::decay_t<MetaStruct::remove_any_pointer_t<T>>>::value and
            !MetaStruct::is_any_pointer<T>::value ,bool>
        populateFromEntityUtility_(PairCache *& c, const T & object, R (S :: * func)() const, Ts &&... ts) {

            //! This abstract type must have a TypeList.
            static_assert(MetaTypeList::is_meta_typelist<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type does not contain a well-formed MetaTypeList.");

            //! The TypeList cannot be empty.
            static_assert(MetaTypeList::get_pack_size<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type cannot contain an empty MetaTypeList.");

            return populateFromEntityUtility_(c, (object.*func)(), std::forward<Ts>(ts)...);
        }

        //! During object-method invocation, if we encounter an Abstract type, we
        //  need to handle it specially. Here T is an abstract base type but it is
        //  not a pointer. Overload for single method pointer in list.
        //  \code{.cpp}
        //  Generic Base pointer b;
        //  Member Function:
        //      const std::shared_ptr<Base> & getBP() const { return b; }
        //  addPair() callset -> (X::*b, X::getBP)
        //  \endcode
        template<typename T, typename R, typename S>
        MetaStruct::enable_if_t<
            //! Abstract Type is encountered.
            std::is_abstract<MetaStruct::decay_t<MetaStruct::remove_any_pointer_t<T>>>::value and
            !MetaStruct::is_any_pointer<T>::value ,bool>
        populateFromEntityUtility_(PairCache *& c, const T & object, R (S :: * func)() const) {

            //! This abstract type must have a TypeList.
            static_assert(MetaTypeList::is_meta_typelist<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type does not contain a well-formed MetaTypeList.");

            //! The TypeList cannot be empty.
            static_assert(MetaTypeList::get_pack_size<
                typename MetaStruct::decay_t<
                    MetaStruct::remove_any_pointer_t<T>>::derived_type_list>::value,
                        "Abstract Base Type cannot contain an empty MetaTypeList.");

            // Get the new data, as a copy.
            const ValueType & tmp = ((object).*func)();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not a pointer.
        // Functional object is Method Pointer.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object matches enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      const int getValue() const { return value; }
        //  addPair() callset -> (X::b, X::getValue, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, R (S :: * func)() const, Ts &&... ts) {
            return populateFromEntityUtility_(c, (object.*func)(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not a pointer.
        // Functional object is Method Pointer.
        // Functional object does return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object matches enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      const std::function getFunc() const { return func; }
        //  addPair() callset -> (X::b, X::getFunc, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value &&
        /**
         * \brief Generates intermediate objects of the same type as the next
         *  Function Pointer and does an object.method invocation to get the next
         *  object.
         * \param c is a PairCache reference.
         * \param object is the intermediate object which will be used to
         *  invoke the next Function Pointer.
         * \param first is the current head Function Pointer in our parameter pack.
         */

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R ()> const S :: * func, Ts &&... ts) {
            return populateFromEntityUtility_(c, (object.*func)(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not a pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object does not matter as functional takes no parameters.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getX = int getValue() { return value; }
        //  addPair() callset -> (X::b, getX, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T &, std::function<R ()> const S, Ts &&... ts) {
            return populateFromEntityUtility_(c, S(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not a pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the not last function in the list.
        // Type of object match parameter type of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getX = int getValue(Object b) { return value; }
        //  addPair() callset -> (X::b, getX, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename H, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R (const H &)> const S, Ts &&... ts) {
            return populateFromEntityUtility_(c, S(object), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not a pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the not last function in the list.
        // Type of object does not match parameter type of functional.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getX = int getValue(Object c) { return value; }
        //  addPair() callset -> (X::b, getX, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename H, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            !std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R (const H &)> const, Ts...) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is Method Pointer.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object match enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      int getValue() { return value; }
        //  addPair() callset -> (X::&b, x::getX, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, R (S :: * func)() const, Ts &&... ts) {
            return populateFromEntityUtility_(c, (*object.*func)(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is Method Pointer.
        // Functional object does return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object match enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      std::function getV = int getValue() { return value; }
        //  addPair() callset -> (X::&b, x::getV, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R ()> const S :: * func, Ts &&... ts) {
            return populateFromEntityUtility_(c, (*object.*func)(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the not last function in the list.
        // Type of object is irrelevant because functional takes no parameters.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue() { return value; }
        //  addPair() callset -> (X::&b, x::getV, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T &, std::function<R ()> const S, Ts &&... ts) {
            return populateFromEntityUtility_(c, S(), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the not last function in the list.
        // Type of object match parameter type of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue(Object b) { return value; }
        //  addPair() callset -> (X::&b, x::getV, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename H, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R (const H &)> const S, Ts &&... ts) {
            return populateFromEntityUtility_(c, S(*object), std::forward<Ts>(ts)...);
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the not last function in the list.
        // Type of object does not match parameter type of functional.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue(Object c) { return value; }
        //  addPair() callset -> (X::&b, x::getV, getFoo, getBar, ...)
        //  \endcode
        template<typename T, typename R, typename H, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            !std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R (const H &)> const, Ts &&...) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not pointer.
        // Functional object is Method pointer.
        // Functional object does not returns std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Type of object matches enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      int getValue(Object b) { return value; }
        //  addPair() callset -> (X::b, x::getValue)
        //  \endcode
        //! If type is not abstract and not Terminal Type, but there is
        //  an object-method match, we don't discard the method pointer and
        //  use the current one. Only for special cases when there is just
        //  one Method Pointer left in the parameter pack.
        template<typename T, typename R, typename S>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, R (S :: * func)() const) {

            // Get the new data, as a copy.
            const ValueType & tmp = ((object).*func)();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not pointer.
        // Functional object is Method pointer.
        // Functional object returns std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Type of object matches enclosing class of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      std::function getValue() { return value; }
        //  addPair() callset -> (X::b, x::getValue)
        //  \endcode
        template<typename T, typename R, typename S>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R ()> const S :: * func) {

            // Get the new data, as a copy.
            const ValueType & tmp = ((object).*func)();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Do not care about object since functional takes no parameter
        // and is also the last functional in the list.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getValue() { return value; }
        //  addPair() callset -> (X::b, getValue)
        //  \endcode
        template<typename T, typename R>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T &, std::function<R ()> const S) {

            // Get the new data, as a copy.
            const ValueType & tmp = S();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the last function in the list.
        // Type of object does match parameter type of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getValue(Object b) { return value; }
        //  addPair() callset -> (X::b, getValue)
        //  \endcode
        template<typename T, typename R, typename H>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R (const H &)> const S) {

            // Get the new data, as a copy.
            const ValueType & tmp = S(object);

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is not pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the last function in the list.
        // Type of object does not match parameter type of functional.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getValue(Object c) { return value; }
        //  addPair() callset -> (X::b, getValue)
        //  \endcode
        template<typename T, typename R, typename H>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            !std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            !MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R (const H &)> const) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is Method Pointer.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Type of object matches enclosing class of method pointer.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      int getValue() { return value; }
        //  addPair() callset -> (X::&b, X::getValue)
        //  \endcode
        /**
         * \brief Generates the very last object pointer of the innermost nested class type.
         *  This is of the same type as the return value of the target function.
         *  We do the last object.method invocation to get the final value to store in DB.
         * \param c is a PairCache reference.
         * \param object is the last shared pointer object which will be used to invoke the
         *  last Function Pointer.
         * \param last is the target Function Pointer in our parameter pack.
         */

        //! If type is not abstract and not Terminal Type, but there is
        //  an object-method match, we don't discard the method pointer and
        //  use the current one. Only for special cases when there is just
        //  one Method Pointer left in the parameter pack.
        template<typename T, typename R, typename S>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, R (S :: * func)() const) {

            // Get the new data, as a copy.
            const ValueType & tmp = (*object.*func)();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is Method Pointer.
        // Functional object returns std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Type of object matches enclosing class of method pointer.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      std::function getValue() { return value; }
        //  addPair() callset -> (X::&b, X::getValue)
        //  \endcode
        template<typename T, typename R, typename S>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value &&

            //! Method Pointer Type and Object Type match.
            std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R ()> const S :: * func) {

            // Get the new data, as a copy.
            const ValueType & tmp = (*object.*func)();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes no parameter.
        // Functional is the last function in the list.
        // Type of object does not matter because functional
        // no parameters and is the last in list.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue() { return value; }
        //  addPair() callset -> (X::&b, getValue)
        //  \endcode
        template<typename T, typename R>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T &, std::function<R ()> const S) {

            // Get the new data, as a copy.
            const ValueType & tmp = S();

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the last function in the list.
        // Type of object matches parameter type of functional.
        // Can proceed.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue(Object b) { return value; }
        //  addPair() callset -> (X::&b, getValue)
        //  \endcode
        template<typename T, typename R, typename H>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *& c, const T & object, std::function<R (const H &)> const S) {

            // Get the new data, as a copy.
            const ValueType & tmp = S(*object);

            // Since we could potentially start out empty with
            // none primitive data with a non-default constructor
            // we are forced to wait till we get the first copy.
            if(SPARTA_EXPECT_FALSE(data_cpy_.get() == nullptr)) {
                data_cpy_.reset(new ValueType(tmp));
            }
            else if(*data_cpy_ == tmp) {
                return true;
            }
            updateValueInCache_(c, id_, tmp);
            data_cpy_.reset(new ValueType(tmp));
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Type of object is pointer.
        // Functional object is std::function.
        // Functional object does not return std::function.
        // Functional object takes one parameter.
        // Functional is the last function in the list.
        // Type of object does not match parameter type of functional.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      std::function getV = int getValue(Object c) { return value; }
        //  addPair() callset -> (X::&b, getValue)
        //  \endcode
        template<typename T, typename R, typename H, typename... Ps>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            !std::is_same<T, MetaStruct::decay_t<H>>::value &&

            //! Check if object is a pointer or not.
            MetaStruct::is_any_pointer<T>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R (const H &)> const) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Pointer or non-pointer type of object is irrelevant.
        // Functional object is method pointer.
        // Functional object does not return std::function.
        // Functional object takes no parameters.
        // Functional is also not the last function in the list.
        // Type of object does not match enclosing class type of method pointer.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Member Function:
        //      int getValue() { return value; }
        //  addPair() callset -> (X::&b, Y::getValue, Z::getFoo, X::getBar)
        //  \endcode
        template<typename T, typename R, typename S, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value &&

            //! Method Pointer Type and Object Type mismatch.
            !std::is_same<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<S>>,
                MetaStruct::remove_any_pointer_t<T>>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, R (S :: *)() const, Ts &&...) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Pointer or non-pointer type of object is irrelevant.
        // Functional object is std::function,
        // Functional object does not return std::function.
        // Functional object takes no parameters.
        // Functional is also not the last function in the list.
        // Functional does not depend on object but other functions might.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      int getValue() { return value; }
        //  addPair() callset -> (X::&b, getValue, Z::getFoo, X::getBar)
        //  \endcode
        template<typename T, typename R, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R ()> const, Ts &&...) {
            return false;
        }

        // Type of object is not abstract.
        // Type of object is not Terminal(POD).
        // Pointer or non-pointer type of object is irrelevant.
        // Functional object is std::function,
        // Functional object does not return std::function.
        // Functional object takes multiple parameters.
        // Cannot invoke callable with just object.
        // Functional is also not the last function in the list.
        // Cannot proceed and return false.
        //  \code{.cpp}
        //  Object b;
        //  Free Function:
        //      int getValue(a, b, c, d) { return value; }
        //  addPair() callset -> (X::&b, getValue, Z::getFoo, X::getBar)
        //  \endcode
        template<typename T, typename R, typename... Ps, typename... Ts>
        MetaStruct::enable_if_t<

            //! Must be non-abstract type.
            !std::is_abstract<MetaStruct::decay_t<
                MetaStruct::remove_any_pointer_t<T>>>::value, bool>

        populateFromEntityUtility_(
            PairCache *&, const T &, std::function<R (Ps...)> const, Ts &&...) {
            return false;
        }

        //! Process if type is integral.
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_integral<MetaStruct::decay_t<T>>::value &&
            std::is_pod<MetaStruct::decay_t<T>>::value &&
            !MetaStruct::is_bool<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T tmp) {
            c->updateSizeOfCache(sizeof(T), id);
            c->updateNumericCache(tmp, id);
            c->updateFormatCache(f_switch_, id);
        }

        //! Process if type is C++ enum and has ostream overload.
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_enum<MetaStruct::decay_t<T>>::value and
            sparta::utils::has_ostream_operator<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            std::stringstream ss;
            ss << tmp;
            c->updateNumericCache(static_cast<uint64_t>(tmp), id);
            c->updateSizeOfCache(sizeof(MetaStruct::underlying_type_t<T>), id);
            c->updateStringCache(ss.str(), id);
        }

        //! Process if type is C++ enum and has no ostream overload.
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_enum<MetaStruct::decay_t<T>>::value and
            !sparta::utils::has_ostream_operator<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            c->updateNumericCache(static_cast<uint64_t>(tmp), id);
            c->updateSizeOfCache(sizeof(MetaStruct::underlying_type_t<T>), id);
        }

        //! Process if type is sparta::utils::enum.
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_convertible<MetaStruct::decay_t<T>, std::string>::value and
            std::is_convertible<MetaStruct::decay_t<T>, uint32_t>::value and
            !MetaStruct::is_string<MetaStruct::decay_t<T>>::value and
            !MetaStruct::is_char_pointer<MetaStruct::decay_t<T>>::value, void>
        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            std::string ss(tmp);
            c->updateNumericCache(static_cast<uint64_t>(tmp), id);
            c->updateSizeOfCache(sizeof(uint32_t), id);
            c->updateStringCache(std::move(ss), id);
        }

        //! Process if type is bool.
        template<typename T>
        MetaStruct::enable_if_t<
            MetaStruct::is_bool<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T tmp) {
            c->updateSizeOfCache(sizeof(bool), id);
            if(tmp) {
                std::string val("true");
                c->updateStringCache(std::move(val), id);
            }
            else {
                std::string val("false");
                c->updateStringCache(std::move(val), id);
            }
            c->updateNumericCache(tmp, id);
        }

        //! Process if type is char *.
        template<typename T>
        MetaStruct::enable_if_t<
            MetaStruct::is_char_pointer<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            sparta_assert(tmp);
            std::string str(tmp);
            c->updateStringCache(std::move(str), id);
        }

        //! Process if type is string.
        template<typename T>
        MetaStruct::enable_if_t<
            MetaStruct::is_string<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            c->updateStringCache(tmp, id);
        }

        //! Process if type is pair.
        template<typename T>
        MetaStruct::enable_if_t<
            MetaStruct::is_pair<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            static bool flag = false;
            if(!flag){
                updateValueInCache_(c, id, tmp.first);
            }
            else {
                updateValueInCache_(c, id, tmp.second);
            }
            flag = !flag;
        }

        //! Process if type is floating/double.
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_floating_point<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T tmp) {
            c->updateStringCache(std::to_string(tmp), id);
            c->updateFormatCache(f_switch_, id);
        }

        //! Process if type is any STL.
        template<typename T>
        MetaStruct::enable_if_t<
            MetaStruct::is_stl<MetaStruct::decay_t<T>>::value, void>

        updateValueInCache_(
            PairCache *& c, const uint32_t id, const T & tmp) {
            std::stringstream ss;
            ss << tmp;
            c->updateStringCache(ss.str(), id);
        }

    protected:
        //! Aliasing only the return type of the
        //  last Method Pointer in the parameter pack.
        using ValueType =
            MetaStruct::decay_t<
                MetaStruct::return_type_t<
                    MetaStruct::last_index_type_t<
                        MetaStruct::parameter_pack_length<
                            Args...>::value - 1, Args...>>>;

        // A tuple which holds on to the variadic template argument of method pointers.
        std::tuple<Args...> parameterPack_;

        // Have a unique pointer to the ultimate terminal type of this KeyPairEntity.
        std::unique_ptr<ValueType> data_cpy_;

    public:
        KeyPairFromEntity(uint32_t i, const std::string & name, Args &&... args) :
            BasePairFromEntity<EntityType>(name, i),
            parameterPack_(std::forward<Args>(args)...),
            data_cpy_(nullptr) {
            static_assert(MetaStruct::parameter_pack_length<Args...>::value,
                "There must be at least one Method Pointer which is passed from addPair API");
            }

        /**
        * \brief Pass data to this Key Value pair and allow the key value pair
        *  to update the pair cache with the new data
        */
        virtual bool populateFromEntity(PairCache * c, const EntityType & owner)
        override final {
            return unpackTupleAsIndices_(c, owner, MetaStruct::generate_sequence_t<sizeof...(Args)>());
        }
    };

    /**
     * \class NoEntity
     * \brief An empty dummy class. When used as the TypeCollected for a pair definition,
     * the pair definition will not try to collect from an entity.
     */
    class NoEntity {
    private:

        //! This class can never actually be created.
        NoEntity() {}
    };

    template <typename EntityType>
    class PairDefinition {
    private:

        /**
         * \brief the templatized types of pairs that this PairDefinition has.
         */
        typedef BasePairFromEntity<EntityType> BoundPairType;

        /**
         * \brief typedef for a list of pairs.
         */
        typedef std::vector<BoundPairType *> BoundPairList;
        typedef std::vector<Pair *> ArbitraryPairList;
        typedef std::vector<std::unique_ptr<Pair>> PairList;

        /**
        * \brief This special character will act as placeholder
        * for printing values when formatting the string to be
        * displayed in Argos Viewer.
        */
        unsigned char specialDelimiter = '#';
        std::string argosFormatPair;

        template<int ArgPos>
        void populatePositionalPairArg_(PairCache *) {

            // we are done unrolling the positional args.
            sparta_assert(ArgPos == arbitrary_pairs_.size(),
            "Too many or two few positional arguments were supplied to the collect method of your pair collector.");
        }

        /**
         * \brief recurively unroll the arguments and call the AribtraryPair that owns this
         * argument to populate the datacache with the stringaliziation of the value.
         */
        template<int ArgPos, typename T, typename... Targs>
        void populatePositionalPairArg_(PairCache * cache,
            const T & t, const Targs &... args) {
            reinterpret_cast<ArbitraryPair<T> *>(
                arbitrary_pairs_[ArgPos])->populateArgData(cache, t);
            populatePositionalPairArg_<ArgPos+1, Targs...>(cache, args...);
        }

        /**
         * \brief If this PairDefinition is trying to collect data from some Entity, we need to allow this.
         * but if we are not, we cannot try to do this since our first argument will
         * not be the argument of an entity.
         * This is the case when EntityType==NoEntity.
         */
        template <typename Ret, typename EntityT, typename T>
        inline MetaStruct::enable_if_t<!std::is_same<EntityT, NoEntity>::value, Ret>
        populateFromEntityHelper_(PairCache * cache, BoundPairType * pair, const T & owner) {
            bool val = reinterpret_cast<BoundPairType *>(pair)->populateFromEntity(cache, owner);
            return val;
        }

        template <typename Ret, typename EntityT, typename T>
        inline MetaStruct::enable_if_t<std::is_same<EntityT, NoEntity>::value, Ret>
        populateFromEntityHelper_(PairCache *, BoundPairType *, const T &) {
            return false;
        }

        /**
         * \brief unroll the positional arguments that were supplied and pass these values up to the
         * appropriate ArbitraryPair. This method is specially enabled to ignore the first argument
         * unless EntityType!=NoEntity. Since if we are collected from an Entity directly,
         * we need to pass that entity reference to the appropriate BoundPair's to extract the
         * data out of the entity.
         */
        template <typename Ret, typename EntityT, typename T, typename... Targs>
        inline MetaStruct::enable_if_t<!std::is_same<EntityT, NoEntity>::value, Ret>
        populatePositionalArguments_(PairCache * cache,
            const T &, const Targs &... pos_args) {
            constexpr size_t nargs = sizeof...(Targs);
            sparta_assert(nargs == arbitrary_pairs_.size(),
                "Attempting to give " << std::dec << nargs <<
                " arguments to a PairDefinition which accepts "
                << std::dec << arbitrary_pairs_.size() <<
                " positional arguments");

            populatePositionalPairArg_<0, Targs...>(cache, pos_args...);
        }

        template <typename Ret, typename EntityT, typename T, typename... Targs>
        inline MetaStruct::enable_if_t<std::is_same<EntityT, NoEntity>::value, Ret>
        populatePositionalArguments_(PairCache * cache,
            const T & arg0, const Targs &... pos_args) {
            constexpr size_t nargs = sizeof...(Targs);
            sparta_assert(nargs + 1 == arbitrary_pairs_.size(),
                "Attempting to give " << std::dec << nargs <<
                " arguments to a PairDefinition which accepts "
                << std::dec << arbitrary_pairs_.size()
                << " positional arguments and an owner");

            populatePositionalPairArg_<0, T, Targs...>(cache, arg0, pos_args...);
        }

        //! Template Unrolling to extract the last arg. from Parameter Pack.
        template<typename Head, typename... Tail>
        std::ios_base::fmtflags extractFormat_(const Head &, Tail &&... tail) {
            return extractFormat_<Tail...>(std::forward<Tail>(tail)...);
        }

        //! Template Unrolling Base case.
        template<typename Last>
        std::ios_base::fmtflags extractFormat_(const Last & format) {
            return format;
        }

        //! Use compile-time index generator to build the Key-Pair.
        template<std::size_t... S, typename... Args>
        void dispatchMethodPointers_(std::ios_base::fmtflags format,
                                     uint32_t size, const std::string & name,
                                     const std::tuple<Args...> & mp_tuple,
                                     MetaStruct::sequence_generator<S...>) {
            buildKeyPair_(format, size, name, std::move(std::get<S>(mp_tuple))...);
        }

        //! Build the KeyPairEntity and propagate format information upto Pair class.
        template<typename... Args>
        void buildKeyPair_(std::ios_base::fmtflags format,
                           uint32_t size, const std::string & name,
                           Args &&... args) {
            auto * new_pair = new KeyPairFromEntity<EntityType, Args...>(
                size, name, std::forward<Args>(args)...);
            pairs_.emplace_back(new_pair);
            bound_pairs_.emplace_back(new_pair);
            new_pair->applyFormat(format);
        }

        //! Type is not Virtual Abstract, do the normal way
        //  of NestedPair Collection.
        template<typename T, typename... Args>
        MetaStruct::enable_if_t<!std::is_abstract<T>::value, void>
        processIfAbstractBaseType_(Args &&... args) {
            T::type::nestedPairCallback(this, std::forward<Args>(args)...);
        }

        //! Type is Virtual Abstract.
        template<typename T, typename... Args>
        MetaStruct::enable_if_t<std::is_abstract<T>::value, void>
        processIfAbstractBaseType_(Args &&... args) {

            using meta_type = typename T::derived_type_list;

            //! Virtual Abstract Base Type must have TypeList.
            static_assert(MetaTypeList::is_meta_typelist<meta_type>::value,
            "Abstract Base class does not contain a well-formed nested type alias of MetaTypeList.");

            //! TypeList cannot be empty.
            static_assert(MetaTypeList::get_pack_size<meta_type>::value,
            "Type List cannot be empty.");

            //! Unroll the TypeList to optimistically collect NestedPairs
            //  from every Derived Type possible. Later on, we have technique
            //  to figure out which Method Pointers to disable based on the
            //  Run-Time Type Information of the Base Pointer.
            auto && unrolled_list = UnrollTypeList_<decltype(this),
                                                    meta_type,
                                                    Args &&...>(this,
                                                                std::forward<Args>(args)...);

            (void)unrolled_list;
        }

        //! Since we cannot use a for or while loop to walk
        //  a templated TypeList, we need to use the technique
        //  of Template Unrolling to extract each Derived type
        //  and then walk in that class and collect the NestedPairs.
        template<typename, typename, typename...>
        class UnrollTypeList_;

        //! Use the technique of deriving from One-Less Type Class to elegantly
        //  unroll the TypeList and do the required processing.
        template<typename T, typename Head, typename... Tail, typename... Args>
        class UnrollTypeList_<T, MetaTypeList::type_list<Head, Tail...>, Args...> :
            public UnrollTypeList_<T, MetaTypeList::type_list<Tail...>, Args...> {
        public:
            explicit UnrollTypeList_(const T & ptr, Args &&... args) :
                UnrollTypeList_<T, MetaTypeList::type_list<Tail...>, Args...>(
                    ptr, std::forward<Args>(args)...) {
                Head::type::nestedPairCallback(ptr, std::forward<Args>(args)...);
            }
        };

        //! Base specialization when TypeList is empty.
        template<typename T, typename... Args>
        class UnrollTypeList_<T, MetaTypeList::type_list<>, Args...> {
        public:
            explicit UnrollTypeList_(const T &, Args &&...) {}
        };

    public:

        PairDefinition() = default;

        inline void setArgosFormatGuide(const std::string & guideString) {
            argosFormatPair += guideString;
        }

        inline std::string makeToken(const std::string & name) {
            return std::string(1, specialDelimiter)
                + name + std::string(1, specialDelimiter);
        }

        /**
         * \brief Add a position pair argument to the pair definition.
         * \details This method is used to add position arguments to the PairDefinition.
         * This method will allow that the collect() method of some collector may be extra arguments
         * in order that they were added using this method. So you cal allow the following.
         * \verbatim
         * some_collector_.collect(entity_(optional), arg_val0, arg_val1,...);
         * \endverbatim
         * Note that entity_ is an optional reference to some entity from which data is being extracted
         * directly from due to addPair(). If This PairDefinition::EntityType == NoEntity the entity_
         * will be interpretted as arg_val0.
         * \tparam DataT the datatype of the argument that will be collected.
         * \param name the key value for this pair.
         * \param hex whether or not to try to format this value as hexidecimal.
         * \param pair_cache a pointer to a PairCache that is being used. If this is not nullptr,
         * then the pair cache will be updated immidietly, otherwise the pair cache will be updated
         * upon finalizeKeys(). Therefor pair_cache != nullptr should only be used after finalize has
         * been done.
         */
        template<class DataT>
        void addPositionalPairArg(const std::string & name,
                                  std::ios_base::fmtflags format = std::ios_base::dec,
                                  PairCache * pair_cache=nullptr) {
            ArbitraryPair<DataT> * new_pair =
                new ArbitraryPair<DataT>(name, pairs_.size());
            pairs_.emplace_back(new_pair);
            arbitrary_pairs_.emplace_back(new_pair);

            // We allow this function to be called to add the positional argument directly to a pair cache.
            // if pair_cache is null, the pair will be added during finalizeKeys().
            if(pair_cache != nullptr) {
                sparta_assert(finalized_);
                pair_cache->addKey(pairs_[pairs_.size() - 1]->getKey(), pairs_.size() - 1);
            }
            else {
                sparta_assert(!finalized_);
            }
            new_pair->setFormatter(format);
        }

        /**
        * \brief Add a new pair of values to the PairDefinition.
        * This should be called during construction.
        * \param name the key
        * \param Args A variadic template containing all the
        * function pointers right from the outermost to the innermost
        * nested pair classes.
        * Specialization when no formatter is specified.
        */
        template<typename... Args>
        MetaStruct::enable_if_t<!std::is_same<MetaStruct::peek_last_type_t<Args...>,
                                const std::ios_base::fmtflags &>::value, void>
        addPair(const std::string & name, Args &&... args) {
            sparta_assert(!name.empty());
            static_assert(sizeof...(Args),
                          "There must be atleast one Method Pointer in addPair() call.");
            auto * new_pair = new KeyPairFromEntity<EntityType, Args...>(
                pairs_.size(), name, std::forward<Args>(args)...);
            pairs_.emplace_back(new_pair);
            bound_pairs_.emplace_back(new_pair);
            new_pair->applyFormat(std::ios::dec);
        }

        /**
        * \brief Add a new pair of values to the PairDefinition.
        * This should be called during construction.
        * \param name the key
        * \param args A variadic template containing all the
        * function pointers right from the outermost to the innermost
        * nested pair classes.
        * Specialization when a formatter is specified.
        */
        template<typename... Args>
        MetaStruct::enable_if_t<std::is_same<MetaStruct::peek_last_type_t<Args...>,
                                const std::ios_base::fmtflags&>::value, void>
        addPair(const std::string & name, Args &&... args) {
            sparta_assert(!name.empty());
            static_assert(sizeof...(Args),
                          "There must be atleast one Method Pointer in addPair() call.");

            //! Store the Parameter Pack in a std::tuple.
            auto mp_tuple = std::make_tuple(std::forward<Args>(args)...);

            //! Extract the last parameter from the Parameter Pack.
            auto format = extractFormat_(std::forward<Args>(args)...);

            //! Separate the formatter from the method pointers.
            dispatchMethodPointers_(format, pairs_.size(), name, mp_tuple,
                                    MetaStruct::generate_sequence_t<sizeof...(Args) - 1>());
        }

        /**
        * \brief In case of nested pairs, this figures out where to go
        * and grab the nested pair structures from.
        * \param Args A variadic template containing all the
        * function pointers right from the outermost to the innermost
        * nested pair classes
        */
        template<typename... Args>
        void flattenNestedPairs(Args &&... args) {
            using EnclosingClassType =
                MetaStruct::remove_any_pointer_t<
                    MetaStruct::decay_t<
                        MetaStruct::return_type_t<
                            MetaStruct::last_index_type_t<
                                MetaStruct::parameter_pack_length<
                                    Args...>::value - 1, Args...>>>>;

            //! Check if the Type we want to collect NestedPairs
            //  from is Virtual Abstract or not.
            processIfAbstractBaseType_<EnclosingClassType>(std::forward<Args>(args)...);
        }

        //! Old Non-Nested way of Pevent add call.
        template <typename DataType, typename BaseClassEntityType>
        void addPEventsPair(const std::string & name,
                            DataType (BaseClassEntityType::*func)()const,
                            std::ios_base::fmtflags format = std::ios_base::dec) {
            static_assert(std::is_base_of<BaseClassEntityType, EntityType>::value,
                          "Class from which func will be called must be base class of EntityType");

            auto * new_pair = new KeyPEventPairFromEntity<
                DataType, EntityType, BaseClassEntityType>(
                    name, func, pairs_.size());
            pairs_.emplace_back(new_pair);
            bound_pairs_.emplace_back(new_pair);

            // mark the pair to be formatted pair if necessary
            new_pair->setFormatter(format);
        }

        /**
        * \brief Set key strings in the PairCache of string key value pairs
        * Also, setting the sizeof values in the sizeof vector to be NULL
        * and the string values in the String Representation vector to be ""
        * by making use of Overloaded function addKey with one parameter.
        */
        void finalizeKeys(PairCache * pair_cache) {
            pair_cache->reserveThemAll(pairs_.size());
            for(uint32_t i = 0; i < pairs_.size(); ++i) {
                pair_cache->addKey(pairs_[i]->getKey(), i);
            }
            finalized_ = true;
        }

        /**
         * \return How many pairs are we collecting from the entity
         */
        inline uint32_t size() const {
            return pairs_.size();
        }

        /**
         * \brief Accept a pair_cache to collect us.
         * \return returns true if any of our key pairs were dirty.
         */
        template<typename T, typename... Targs>
        bool populatePairs(PairCache * pair_cache, const T & owner,
            const Targs &... pos_args) {

            // let the pair_cache visit each pair
            bool was_clean = true;
            for(auto & pair : bound_pairs_) {
                was_clean = was_clean &
                    populateFromEntityHelper_<bool, EntityType, T>(pair_cache, pair, owner);
            }
            if(arbitrary_pairs_.size() > 0) {

                // positional collected arguments always invoke a change.
                was_clean = false;
                populatePositionalArguments_<void, EntityType, T, Targs...>(
                    pair_cache, owner, pos_args...);
            }
            if(!argosFormatPair.empty()) {
                size_t count = std::count(argosFormatPair.begin(),
                    argosFormatPair.end(), specialDelimiter);
                sparta_assert(!bound_pairs_.empty());
                sparta_assert(count == 2 * (bound_pairs_.size() - 1));
                pair_cache->updateArgosFormatGuide(argosFormatPair);
            }
            return !was_clean;
        }

    protected:
        PairList pairs_;
        BoundPairList bound_pairs_;
        ArbitraryPairList arbitrary_pairs_;
        bool finalized_;
    };

    /**
     * \brief Depricated alias for PairDefintion<>
     */
    template<typename EntityT>
    using CollectableEntity = PairDefinition<EntityT>;

    /**
     * \class PositionOnlyPairDef
     * \brief Define a dummy pair defintion to be used when the user only wants dynamicly
     * appended positional arguments
     */
    class PositionOnlyPairDef : public PairDefinition<NoEntity> {
    public:
        typedef sparta::NoEntity TypeCollected;

        PositionOnlyPairDef() : PairDefinition<TypeCollected>() {}
    };
} //namespace sparta

#endif //__KEY_PAIR_SPARTA_H__
