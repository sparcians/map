// <KeyValue> -*- C++ -*-

#pragma once

#include <inttypes.h>
#include <vector>
#include <map>

#include <boost/mpl/for_each.hpp>
#include <boost/tokenizer.hpp>
//#include <boost/algorithm/string.hpp>
#include <boost/variant.hpp>
#include <boost/foreach.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/insert_range.hpp>
#include <boost/static_assert.hpp>

#include "sparta/utils/SpartaException.hpp"


namespace sparta
{

    /* \brief Macro to make the variant type for the different types that a
     * KeyValue can be.  In addition, a type name string list is created
     * which is parsed and turned into a global type/name map for pretty
     * printing.  This is better than saying, your type 'y' doesn't match
     * type 'i'.
     */
    #define MAKE_VARIANT_OVER(...)                                      \
        typedef boost::mpl::vector<__VA_ARGS__> VectorofTypes;          \
        typedef boost::make_variant_over<VectorofTypes>::type ValueVar; \
        static const std::string type_names = #__VA_ARGS__;


    /*! The variant list that the KeyValue class will support.  To add a
     * new type, just add it to the end of this list.
     *
     * All elements in this list must:
     * \li Support default construction
     * \li Support assignment operator (=)
     * \li Support comparison operator (==) as a const qualified method. For
     * vector, this implies that the value_type supports operator== as a const
     * method as well.
     *
     * \warning If adding a new type that is not a copyable POD type, a
     * std::string, or a vector of POD types (or std::strings), code
     * will likely break. Never add [const]char* to this list; it creates
     */
    MAKE_VARIANT_OVER(bool,
                      int32_t,
                      uint32_t,
                      int64_t,
                      uint64_t,
                      double,
                      std::string,
                      std::vector<bool>,
                      std::vector<int32_t>,
                      std::vector<uint32_t>,
                      std::vector<int64_t>,
                      std::vector<uint64_t>,
                      std::vector<double>,
                      std::vector<std::string>);

    //! \brief The type-to-type name map.  Not really necessary, but useful.
    typedef std::map<std::string, std::string> TypeToTypeNameMap;


    /*!
     * \class KeyValue
     * \brief This class hold a key/value pair
     *
     *
     */
    class KeyValue
    {
    public:

        /*!
         * \brief Map of internal typeid names to C++ names.
         *
         * Used for looking up human-readable C++ names based on variant or
         * template types.
         *
         * \note This is used by Parameters for a type list
         */
        static TypeToTypeNameMap GBL_type_name_map;

        /*!
         * \brief Construct a Key/Value pair
         * \param key The key
         * \param val The ValueVar (can be anything of any of the above supported types)
         *
         * Example usage:
         * \code
         * KeyValue val1("unsigned integer", 10u);
         * KeyValue val2("float", 10.1);
         * KeyValue val3("string", "hello");
         * KeyValue val4("signed ", -10);
         * KeyValue val5("unsigned long", 10ull);
         * \endcode
         */
        template <class U>
        KeyValue(const std::string & key,
                 U val,
                 const std::string & desc="") :
            key_(key), value_(val), desc_(desc)
        {

            // Why am I allowed to do this?.
            // Variant constructor is to agressive. Holds type as bool.
            //ValueVar x((const char*)"12345");
            //std::cout << x.type().name() << std::endl;
            //std::cout << boost::get<const char*>(x) << std::endl;

            // While this is not valid:
            //ValueVar xv(std::pair<int,int>());
            //std::cout << boost::get<std::pair<int,int> >(xv).size() << std::endl;
        }

        /*!
         * \brief Get the key
         * \return The key
         */
        std::string getKey() const {
            return key_;
        }

        /*!
         * \brief Set the description
         * \param desc The description of this key/value pair
         */
        void setDesc(const std::string& desc) {
            desc_ = desc;
        }

        /*!
         * \brief Get the Description
         * \return The Description
         */
        std::string getDesc() const {
            return key_;
        }

        /*!
         * \brief Get the value of this key/value
         * \return The value of this key/value pair
         * \throw boost::bad_get if the type requested does not match the type of the key
         *
         * A painful way to get to the value of the key/value.
         * Example usage:
         * \code
         * KeyValue val1("unsigned integer", 10u);
         * unsigned int val = val1.getValue<unsigned int>();
         * \endcode
         */
        template<class T>
        const T getValue() const {
            try {
                return boost::get<T>(value_);
            }
            catch(const boost::bad_get & e) {
                std::string whatitis = "unknown";
                std::string whatyouwanted = "unknown";
                TypeToTypeNameMap::const_iterator it = GBL_type_name_map.find(value_.type().name());
                if(it != GBL_type_name_map.end()) {
                    whatitis = (*it).second;
                }
                it = GBL_type_name_map.find(typeid(T).name());
                if(it != GBL_type_name_map.end()) {
                    whatyouwanted = (*it).second;
                }
                std::cerr << "ERROR: Attempt to get a '" << whatyouwanted
                          << "' (" << typeid(T).name() << ") type on KeyValue '"
                          << key_ << "' that's a '" << whatitis << "'" << std::endl;
                throw;
            }
        }

        /*!
         * \brief Assign a new value to this object
         * \return
         * \throw SpartaException if value specified cannot be assigned to the
         * internal variant.
         * \note This method is NOT restricted to the current held value type.
         * Other types can be specified
         */
        template<class T>
        void operator=(const T& rhp) {
            value_ = rhp;
        }

        /*!
         * \brief Cast operator for the key/value
         * \return The value of this key/value pair, if the types match
         * \throw boost::bad_get if the type requested does not match the type of the key
         *
         * A fast way to get to the value of the key/value.
         * Example usage:
         * \code
         * KeyValue val1("unsigned integer", 10u);
         * unsigned int val = val1;
         * \endcode
         */
        template<class T>
        operator T() const {
            return getValue<T>();
        }

        /*!
         * \brief Determines if there is a known compiler-independent typename for type \a T
         * \tparam T Type for which a name will be looked up
         * \return true if a name is found for type \a T
         */
        template <typename T>
        static bool hasTypeNameFor() {
            TypeToTypeNameMap::const_iterator it = GBL_type_name_map.find(typeid(T).name());
            if(it != GBL_type_name_map.end()) {
                return true;
            }
            return false;
        }

        /*!
         * \brief Determines if there is a known compiler-independent typename for type \a T
         * \tparam T Type for which a name will be looked up
         * \return Name associated with type \a T.
         * \throw SpartaException if no name was found associated with type \a T
         */
        template <typename T>
        static const std::string& lookupTypeName() {
            TypeToTypeNameMap::const_iterator it = GBL_type_name_map.find(typeid(T).name());
            if(it != GBL_type_name_map.end()) {
                return (*it).second;
            }
            SpartaException e("Could not get compiler-independent type name for \"");
            e << typeid(T).name() << "\". Valid type keys are:";
            for(TypeToTypeNameMap::value_type& p : GBL_type_name_map){
                e << " " << p.first << ":" << p.second << " ";
            }
            throw e;
        }

        //! Visitor class for looking up type name
        class TypeNameVisitor : public boost::static_visitor<const std::string&>
        {
        public:
            template <typename T>
            const std::string& operator()(const T&) const { return lookupTypeName<T>(); }
        };

        /*!
         * \brief Gets the compiler-independent readable type string of the value
         * currently held.
         * \return compiler-independent type name.
         * \throw SpartaException if held variant type is not a known type. This
         * indicates a serious error.
         * \note This is not necessarily what would be returned from typeid() or
         * Variant::type().
         */
        const std::string& getTypeName() const {
            return boost::apply_visitor(TypeNameVisitor(), value_);
        }

    protected:

        /*!
         * \brief Gets a reference to the value currently held by this object
         * if the correct template type is specified.
         * \tparam T type to get current value as. This nmust be the type
         * described by getTypeName or an exception will likely be thrown
         * \warning Getting the reference to the held value is dangerous. This
         * reference is invalidated if the held type (type of value_) changes.
         * \warning discard this reference as quickly as possible.
         * \return reference to the value currenty held in this variant.
         * \throw boost::bad_get if T does not match the current held type.
         */
        template<class T>
        T& getValueRef_() {
            try {
                return boost::get<T&>(value_);
            }
            catch(const boost::bad_get & e) {
                throw throwBadGet_<T>(e);
            }
        }

        /*!
         * \brief Gets a const reference to the value currently held by this
         * object if the correct template type is specified.
         * \overload getValueRef_
         */
        template<class T>
        const T& getValueRef_() const {
            try {
                return boost::get<const T&>(value_);
            }
            catch(const boost::bad_get & e) {
                throw throwBadGet_<T>(e);
            }
        }

    private:

        /*!
         * \brief Print a message the type-mismatch in boost::get that presumably
         * caused it to fail.
         * \tparam Type of value that caused the 'get' to fail
         * \param e Exception generated by boost::get.
         * \note Returns a bad get but never reaches the return statement. GCC
         * did not understand that a function (getValueRef_) which called this
         * method did not actually need any return type since this method always
         * throws.
         * \throw The boost_bad get exception passed in; always.
         */
        template<class T>
        const boost::bad_get& throwBadGet_(const boost::bad_get& e) const {
            std::string whatitis = "unknown";
            std::string whatyouwanted = "unknown";
            TypeToTypeNameMap::const_iterator it = GBL_type_name_map.find(value_.type().name());
            if(it != GBL_type_name_map.end()) {
                whatitis = (*it).second;
            }
            it = GBL_type_name_map.find(typeid(T).name());
            if(it != GBL_type_name_map.end()) {
                whatyouwanted = (*it).second;
            }
            std::cerr << "ERROR: Attempt to get a '" << whatyouwanted
                      << "' (" << typeid(T).name() << ") type on KeyValue '"
                      << key_ << "' that's a '" << whatitis << "'" << std::endl;
            throw e;
        }

        const std::string key_;
        ValueVar value_;
        std::string desc_;
    }; // KeyValue

} // namespace sparta


//! \brief Required in simulator source to define some globals.
#define SPARTA_KVPAIR_BODY \
    namespace sparta                                                      \
    {                                                                   \
        struct NameMapper                                               \
        {                                                               \
            template<class type>                                        \
            void operator()(type) {                                     \
                assert(current_type_ != tok_.end());                    \
                KeyValue::GBL_type_name_map[typeid(type).name()] = boost::trim_left_copy(*current_type_); \
                ++current_type_;                                        \
        }                                                               \
                                                                        \
        NameMapper() : tok_(type_names, boost::char_separator<char>(",")) \
        {                                                               \
            current_type_ = tok_.begin();                               \
            boost::mpl::for_each<ValueVar::types>(*this);                  \
        }                                                               \
                                                                        \
    private:                                                            \
        boost::tokenizer<boost::char_separator<char> >::iterator current_type_; \
        boost::tokenizer<boost::char_separator<char> > tok_;            \
    };                                                                  \
                                                                        \
    TypeToTypeNameMap KeyValue::GBL_type_name_map;                      \
    static NameMapper mapper;                                           \
    }
