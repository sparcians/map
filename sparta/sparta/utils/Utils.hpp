// <Utils> -*- C++ -*-

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <math.h>
#include <cxxabi.h>
#include <vector>
#include <algorithm>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaExpBackoff.hpp"

//! \return Empty string if s is nullptr, otherwise returns s
#define NULL_TO_EMPTY_STR(s) (((s)==nullptr)?"":s)

/*!
 * \brief Represents the internal buffer size for demangling C++ symbols via
 * sparta::demangle
 */
#define DEMANGLE_BUF_LENGTH 4096


// Custom Literals

/*!
 * \brief Custom literal for uint64
 */
constexpr inline uint64_t operator "" _u64(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for uint32
 */
constexpr inline uint32_t operator "" _u32(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for uint16
 */
constexpr inline uint16_t operator "" _u16(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for uint8
 */
constexpr inline uint8_t operator "" _u8(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for int64
 */
constexpr inline int64_t operator "" _64(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for int32
 */
constexpr inline int32_t operator "" _32(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for int16
 */
constexpr inline int16_t operator "" _16(unsigned long long i) {
    return i;
}

/*!
 * \brief Custom literal for int8
 */
constexpr inline int8_t operator "" _8(unsigned long long i) {
    return i;
}

namespace sparta
{

/*!
 * \brief Function to invert a maps or an unordered_map, or any type of
 * class that has key/value semantics.  The variadic template
 * argument is necessary 'cause map/unordered_map don't have just
 * two template parameters
 */
template<typename K, typename V, typename... Args, template<typename...> class MapType>
MapType<V, K> flipMap(const MapType<K, V, Args...>& map){
    auto inverted_map {MapType<V, K>{}};
    for(const auto& [key, value] : map){
        inverted_map.insert({value, key});
    }
    return inverted_map;
}

/*!
 * \brief Template type helper that removes a pointer, adds a const,
 * and then re-adds the pointer. This is useful to turn "T" [T=U*] into
 * "U const *" in places where simply using "const T" results in "U* const"
 * \tparam T Type of object to use. Must be a pointer, typically with only
 * one level (i.e. "U*", not "U**").
 * \tparam ConstT The const object pointer type derived from T. Do not set.
 * Use the default value instead
 *
 * Example:
 * \code
 * typedef ptr_to_const_obj_ptr<T>::type ConstType;
 * \endcode
 * or
 * \code
 * template <typename T,
 *           typename ConstT=typename ptr_to_const_obj_ptr<T>::type>
 * struct S {
 *    ConstT getConstT() const { ... }
 * };
 * \endcode
 */
template <typename T,
          typename ConstT=typename std::add_pointer<
              typename std::add_const<
                  typename std::remove_pointer<T>::type
                  >::type
                  >::type>
struct ptr_to_const_obj_ptr {
    typedef ConstT type;
};

/*!
 * \brief Determines if input is 0 or a power of 2
 * \return true if number is 0 or a power of 2
 * \note Counting set bits is likely the fastest way to do this, though it
 * probably doesn't make a difference if this is just used for checking
 * register sizes during initialization.
 * \warn Do not use this method in performance-critical code until it is
 * faster.
 */
inline bool isPowerOf2(uint64_t x) {
    return ((x==0) || !(x & (x-1)));
}

/*!
 * \brief Computes a maks and the lsb-position given a block size. The mask
 * generated can be AND-ed with any number to get a value rounded down to
 * the nearest multiple of \a size.
 * \tparam T Type of value to compute mask for. Expected to be uint[XX]_t.
 * \param size Size for which the mask and lsbshould be computed.
 * \param lsb_pos Bit position of lsb in mask. A value can be shifted right
 * by this amount to effectively get a "block index" which is the \a mask /
 * \a size.
 * \return Mask computed as described
 *
 * If size = 0, lsb_pos is number of bits in \a T and mask returned is 0.
 */
template <typename T>
inline T computeMask(T size, T& lsb_pos) {
    T mask;
    if(size >= 1){
        double tmp = log2(size);
        if(tmp != floor(tmp)){
            throw SpartaException("For computeMask, size must be a power of 2, is ")
                << size;
        }
        lsb_pos = (T)tmp;
        mask = ~((1 << lsb_pos) - 1);

        sparta_assert((1ul << lsb_pos) == size);
    }else{
        lsb_pos = sizeof(T) * 8;
        mask = (T)0;
    }
    return mask;
}

/*!
 * \brief Convenience wrapper around computeMask to get the shift value.
 * \see computeMask
 *
 * This exists so that the shift can be computed in a constructor
 * initialization list.
 */
template <typename T>
inline T computeMaskShift(T size) {
    T shift;
    computeMask(size, shift);
    return shift;
}


/*!
 * \brief Demangles a C++ symbol
 * \param Name Symbol name to demangle
 * \return Demangled name if successful. If failed, returns the input
 * name. Note that demangled names may match input name.
 * \note Demangling is limited by DEMANGLE_BUF_LENGTH. results may be
 * truncated or fail for very symbols. Change this value to support longer
 * symbol names.
 */
inline std::string demangle(const std::string& name) noexcept {
    char buf[DEMANGLE_BUF_LENGTH];
    size_t buf_size = DEMANGLE_BUF_LENGTH;
    int status;
    char* out = __cxxabiv1::__cxa_demangle(name.c_str(), buf, &buf_size, &status);
    if(nullptr == out){
        return name;
    }
    return std::string(out);
}

/*!
 * \brief Ensures that a pointer is not null.
 * \param p Pointer to check
 * \return p unless exception is thrown
 * \throw SpartaException if pointer is null.
 *
 * Useful for checking for null in constructor initializer lists which refer
 * to members through object pointers.
 */
template <typename T>
inline T* notNull(T* p) {
    if(p == nullptr){
        throw SpartaException("notNull: pointer was null: " + demangle(typeid(T).name()));
    }
    return p;
}

/*!
 * \brief Templated for determining if ValueType is std::vector for use in
 * metaprogramming constructs.
 * This is intended to be be consumed by enable_if
 */
template <typename T> struct is_vector {
    /*!
     * \brief 'value' field used by enable_if construct.
     *
     * Is false if T is not a vector. true if T is a vector.
     */
    static const bool value = false;
};

//! \overload sparta::is_vector
template <typename T> struct is_vector<std::vector<T> > {
    static const bool value = true;
};

/*!
 * \brief Replaces within a string 's' all instances of some string
 * 'from' with 'to'
 * \param s String to perform replacement on
 * \param from String to replace with <to>
 * \param to String to replace all instances of <from> with.
 * \return Number of replacements
 * \note This can resize the string <s> if from and to are different
 * lengths.
 *
 * Replaces based on found instances of <from> starting from position 0.
 * After replacing, the loop advances to the end of the replaced text
 * and continues there. Characters inserted from <to> will never be
 * matched as part of a <from>
 */
inline uint32_t replaceSubstring(std::string& s, const std::string& from, const std::string& to){
    uint32_t num_replacements = 0;
    size_t pos = 0;
    while((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.length();
        ++num_replacements;
    }
    return num_replacements;
}

/*!
 * \brief Copies from 1 string to another with replacement of a single
 * character with a string.
 *
 * Result should be moved if possible
 */
inline std::string copyWithReplace(const std::string& s, char from, const std::string& to){
    std::string result;
    result.reserve(s.size()*2); // Take an initial guess so we don't have to reallocate

    const char* p = s.c_str();
    while(1){
        if(*p == from){
            result += to;
        }else if(*p == '\0'){
            break;
        }else{
            result += *p;
        }
        ++p;
    }

    return result;
}

/**
 * \brief Take all of the C++-isms out of a string (like parens,
 *        angle brackets, colons, etc) and replace them with an '_'
 * \param s The string to clean up
 * \return The cleaned up string
 */
inline std::string convertCPPStringToPython(const std::string& s)
{
    std::string result = s;
    const char * cpp_stuff = "<>:";
    const char replacement = '_';

    std::string::size_type pos;
    while(pos = result.find_first_of(cpp_stuff), pos != std::string::npos) {
        result[pos] = replacement;
    }

    // Strip parens
    while(pos = result.find_first_of("()"), pos != std::string::npos) {
        std::string newstr = result.substr(0, pos);
        newstr += result.substr(pos + 1);
        result = newstr;
    }


    return result;
}

/*!
 * \brief Insert a number of some character into an ostream
 */
inline void writeNChars(std::ostream& out, uint32_t num, char chr=' ') {
    for(uint32_t i=0; i<num; ++i){
        out << chr;
    }
}

/*!
 * \brief Makes a copy of an input string s in lower-case
 */
inline std::string toLower(const std::string& s){
    std::string out(s.size(), ' ');
    std::transform(s.begin(), s.end(), out.begin(), (int(*)(int))std::tolower);
    return out;
}

/*!
 * \brief Boolean with a default capable of being changed to the opposite value
 * only. it can never set to the default even if already at the default value.
 * \tparam Default default value of this bool. The bool can never be change to
 * this value, only to the opposite
 */
template <bool Default>
class OneWayBool
{
    bool value_ = Default; //!< \brief What is the value

public:

    /*!
     * \brief Default construction. Defaults to template argument 'Default'
     */
    OneWayBool() = default;

    /*
     * \brief Initialize with a value. This value must be the opposite of value
     */
    OneWayBool(bool value)
      : value_(value)
    {
        sparta_assert(value != Default,
                    "OneWayBool<" << Default << "> can only be explicitly initalized to " << !Default
                    << ". Otherwise, it must be default-constructed which will provide a value of "
                    << Default);
    }

    /*!
     * \brief Get current value as a bool
     */
    operator bool () const { return value_; }

    /*!
     * \brief Compare value with a bool
     */
    bool operator==(bool b) const { return value_ == b; }

    /*!
     * \brief Compare value with another OneWayBool
     */
    template <bool ArgDefault>
    bool operator==(const OneWayBool<ArgDefault>& b) const { return value_ == b.value_; }

    /*!
     * \brief Assign value from another OneWayBool
     * \param[in] b Value to assign; must currently be equal to !Default.
     * \pre Can only be aclled once
     */
    template <bool ArgDefault>
    bool operator=(const OneWayBool<ArgDefault>& b) {
        sparta_assert(Default != b.value_,
                    "OneWayBool<" << Default << "> can never be set to " << Default
                    << " except at initialization");
        value_ = b.value;
        return value_;
    }

    /*!
     * \brief Assign value from a bool
     * \param[in] b Value to assign; must be equal to !Default
     * \pre Can only be called once
     */
    bool operator=(bool b) {
        sparta_assert(Default != b,
                    "OneWayBool<" << Default << "> can never be set to " << Default
                    << " except at initialization");
        value_ = b;
        return value_;
    }

    /*!
     * \brief set this object arbitrarily
     * \deprecated This should not be used. It is a temporary fix for
     * AssignOnceObject's unassign_DEPRECATED function which has its own
     * dependency.
     */
    void set_DEPRECATED(bool b) {
        value_ = b;
    }
};

/*!
 * \brief Object which can only have it's value set once. Throws exception
 * if being set more than once.
 * \tparam Template parameter
 */
template <typename T>
class AssignOnceObject
{
    OneWayBool<false> set_; //!< Has this been set yet

    bool defaulted_ = false; //!< Did this object have a default value

    T value_; //!< What is the current value

    const char * name_ = nullptr; //!< Name of this object for useful error messags

public:

    /*!
     * \brief Copy Constructor
     */
    AssignOnceObject(const AssignOnceObject& rhp)
      : set_(rhp.set_),
        defaulted_(rhp.defaulted_),
        value_(rhp.value_),
        name_(rhp.name_)
    {;}

    /*!
     * \brief Move-Construction disabled until needed
     */
    AssignOnceObject(AssignOnceObject&& rhp)
      : set_(rhp.set_),
        defaulted_(rhp.defaulted_),
        value_(rhp.value_),
        name_(rhp.name_)
    {
        // No need to invalidate rhp
    }

    /*!
     * \brief Construct with a default and name for use during errors. This
     * constructor starts the object in unassigned state, but provides a default
     * value.
     * \param[in] def_value Default value
     * \param[in] name Name of the object to print out in errors
     */
    AssignOnceObject(const T& def_value,
                     const char* name)
       : defaulted_(true),
         value_(def_value),
         name_(name)
    {;}

    /*!
     * \brief Construct with no name
     */
    AssignOnceObject()
    {;}

    /*!
     * \brief Cast to contained type T
     */
    operator const T& () const {
        return get();
    }

    /*!
     * \brief Get contained value of type T
     */
    const T& get() const {
        sparta_assert(set_ || defaulted_, getPrintableQuotedName_() << " must be set before reading");
        return value_;
    }

    /*!
     * \brief Compare against another AssignOnceObject of the same contained type T
     */
    bool operator==(const AssignOnceObject<T>& b) const {
        sparta_assert(set_ || defaulted_, getPrintableQuotedName_() << " must be set before comparing");
        return value_ == b.get();
    }

    /*!
     * \brief Assign using another AssignOnceObject of the same contained type T
     */
    const T& operator=(const AssignOnceObject<T>& b) {
        sparta_assert(set_ == false,
                    getPrintableQuotedName_() << " has already been assigned once. It cannot be re-assigned");
        sparta_assert(b.set_ || b.defaulted_,
                    getPrintableQuotedName_() << " cannot be assigned with another AssignOnceObject which is not defaulted or set");

        set_ = true; // arg 'b' must be defaulted or set
        value_ = b.value_;
        return value_;
    }

    /*!
     * \brief Assign using a given value of type T
     */
    const T& operator=(const T& v) {
        sparta_assert(set_ == false,
                    getPrintableQuotedName_() << " has already been assigned once. It cannot be re-assigned");

        set_ = true;
        value_ = v;
        return value_;
    }

    /*
     * \brief Has this value been assigned
     */
    bool assigned() const {
        return set_;
    }

private:

    template <typename X>
    friend inline std::ostream& operator<<(std::ostream& o, const AssignOnceObject<X>& v);

    /*!
     * \brief Get a name for this object enclosed in quotes for printing during errors.
     * \note Only needed during errors and debug - so this can do slow operations like
     * create a string
     */
    std::string getPrintableQuotedName_() const {
        if(name_ != nullptr){
            return std::string("\"") + name_ + "\"";
        }else{
            return std::string("\"AssignOnceObject<") + typeid(T).name() + ">";
        }
    }
};

template <typename T>
inline std::ostream& operator<<(std::ostream& o, const AssignOnceObject<T>& v) {
    if(v.set_ || v.defaulted_){
        o << v.value_;
    }else{
        o << "<uninitialized>";
    }
    return o;
}

/*!
 * \brief Bounded integer type with range-checking
 * \tparam T type to hold
 * \tparam min_bound Minimum bound (inclusive)
 * \tparam max_bound Maximum bound (inclusive)
 */
template <typename T, T min_bound=std::numeric_limits<T>::min(), T max_bound=std::numeric_limits<T>::max()>
class bounded_int
{
    T val_;
public:

    bounded_int(const bounded_int&) = default;

    explicit bounded_int(bounded_int&&) = default;

    template <typename ArgT=T>
    bounded_int(ArgT, typename std::enable_if<std::is_integral<ArgT>::value==false
                                                  && std::is_convertible<ArgT,T>::value==false>::type* = 0)
     : val_(0)
    {
        static_assert(std::is_integral<ArgT>::value,
                      "Cannot store a non-integral value in a bounded integer type");
    }

    template <typename ArgT=T>
    bounded_int(ArgT val, typename std::enable_if<std::is_integral<ArgT>::value==false
                                                  && std::is_convertible<ArgT,T>::value==true>::type* = 0)
     : val_(static_cast<T>(val))
    {
        rangeCheck_(static_cast<T>(val));
    }

    // Different sign
    template <typename ArgT=T>
    bounded_int(ArgT val, typename std::enable_if<std::is_integral<ArgT>::value
                                                  && std::is_signed<ArgT>::value!=std::is_signed<T>::value>::type* = 0)
     : val_(val)
    {
        rangeCheck_(val);
    }

    // Same sign, smaller (or same-sized) input value data type
    template <typename ArgT=T>
    bounded_int(ArgT val, typename std::enable_if<std::is_integral<ArgT>::value
                                                  && (sizeof(ArgT) <= sizeof(T))
                                                  && std::is_signed<ArgT>::value==std::is_signed<T>::value>::type* = 0)
     : val_(val)
    {
        // Types have same signed-ness and argument is smaller than container.
        // Container type T bounds guaranteed unexceeded

        // Check bounds if there are user defined bounds

        rangeCheck_(val);
    }

    // Same sign, larger input value data type
    template <typename ArgT=T>
    bounded_int(ArgT val, typename std::enable_if<std::is_integral<ArgT>::value
                                                  && (sizeof(ArgT) > sizeof(T))
                                                  && std::is_signed<ArgT>::value==std::is_signed<T>::value>::type* = 0)
     : val_(val)
    {
        // Types have same signed-ness but argument is larger
        rangeCheck_(val);
    }

    template <typename ArgT>
    void rangeCheck_(ArgT val){
        lowerRangeCheck_(val);
        upperRangeCheck_(val);
    }

    template <typename ArgT=T>
    typename std::enable_if<std::numeric_limits<ArgT>::min()!=min_bound, void>::type
    lowerRangeCheck_(ArgT val) const {
        sparta_assert(val >= min_bound,
            "Tried to store a " << typeid(ArgT).name() << " into a bounded " << typeid(T).name() << ". Not safe to "
            "cast  " << val << " into a type with a minimum of " << min_bound);
    }

    template <typename ArgT=T>
    typename std::enable_if<std::numeric_limits<ArgT>::min()==min_bound, void>::type
    lowerRangeCheck_(ArgT val) const {
        (void) val; // Lower bound of val is same as this bounded_int
    }

    template <typename ArgT=T>
    typename std::enable_if<std::numeric_limits<ArgT>::max()!=max_bound, void>::type
    upperRangeCheck_(ArgT val) const {
        sparta_assert(val <= max_bound,
            "Tried to store a " << typeid(ArgT).name() << " into a bounded " << typeid(T).name() << ". Not safe to "
            "cast " << val << " into a type with a maximum of " << max_bound);
    }

    template <typename ArgT=T>
    typename std::enable_if<std::numeric_limits<ArgT>::max()==max_bound, void>::type
    upperRangeCheck_(ArgT val) const {
        (void) val; // Upper bound of val is same as this bounded_int
    }

    /*!
     * \todo Add static methods to convert a value using additional bounds.
     * A new bounded value can be constructed to achieve this as well.
     */
    //template <T local_min_bound, T local_max_bound, typename U=T>
    //static T bound(U value) {
    //    bt = bounded_type<T, local_min_bound, local_max_bound>(value);
    //    return bt.val_;
    //}

    /*!
     * \brief Cast to the held type
     * \note May be made explicit. Methods for comparison and truncation would be made available.
     */
    operator T() const { return val_; }

    /*!
     * \brief Deleted comaprison with bounded in (until implemented fully)
     */
    bool operator==(const bounded_int&) = delete;

    /*!
     * \brief Deleted comaprison with bounded in (until implemented fully)
     */
    bool operator=(const bounded_int&) = delete;

};

}; // namespace sparta

/*!
 * \brief Macro to generate a helper templated struct to check if template
 * argument has attribute with the given name.
 * \param name Name of attribute to check for in templated struct generated
 *
 * Example
 * \code
 * GENERATE_HAS_ATTR(foo)
 * //...
 * struct TestA { int foo; };
 * struct TestB { };
 * //...
 * sparta_assert(has_attr_foo<TestA>::value == true);
 * sparta_assert(has_attr_foo<TestB>::value == false);
 * \endcode
 */
#define GENERATE_HAS_ATTR(name)                                         \
    template <typename T>                                               \
    struct has_attr_##name {                                            \
        typedef char one[1];                                            \
        typedef char two[2];                                            \
                                                                        \
        template <typename C> static one& test( decltype(&C::name) ) ;  \
        template <typename C> static two& test(...);                    \
                                                                        \
        enum { value = sizeof(test<T>(nullptr)) == sizeof(one) };       \
    };

// __UTILS_H__
#endif
