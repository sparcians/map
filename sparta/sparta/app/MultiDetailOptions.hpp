// <MultiDetailOptions> -*- C++ -*-


/*!
 * \file MultiDetailOptions.hpp
 * \brief Wrapper for boost program_options option_description that allows
 * multiple levels of detail
 */

#ifndef __MULTI_DETAIL_OPTIONS_H__
#define __MULTI_DETAIL_OPTIONS_H__

#include <boost/program_options.hpp>

#include "sparta/sparta.hpp"

namespace po = boost::program_options;

namespace sparta {
    namespace app {

/*!
 * \brief Helper class for populating boost program options
 */
template <typename ArgT>
class named_value_type : public po::typed_value<ArgT>
{
    unsigned min_;
    unsigned max_;
    std::string my_name_;

public:

    /*!
     * \brief Type of base class
     */
    typedef po::typed_value<ArgT> base_t;

    /*!
     * \brief Constructor
     */
    named_value_type(std::string const& name, ArgT* val) :
        po::typed_value<ArgT>(val),
        min_(0),
        max_(1),
        my_name_(name)
    { }

    /*!
     * \brief Constructor with min and max extents
     */
    named_value_type(std::string const& name, ArgT* val, unsigned min, unsigned max) :
        po::typed_value<ArgT>(val),
        min_(min),
        max_(max),
        my_name_(name)
    { }

    virtual ~named_value_type() {}

    /*!
     * \brief boost semantic for getting name of this option
     */
    virtual std::string name() const override { return my_name_; }

    named_value_type* min(unsigned min)
    {
        min_ = min;
        return this;
    }

    named_value_type* max(unsigned max)
    {
        max_ = max;
        return this;
    }

    named_value_type* multitoken()
    {
        base_t::multitoken();
        return this;
    }

    /*!
     * \brief boost semantic for specifying min tokens
     */
    virtual unsigned min_tokens() const override { return min_; }

    /*!
     * \brief boost semantic for specifying max tokens
     */
    virtual unsigned max_tokens() const override { return max_; }

    /*!
     * \brief Override parser
     * \note Defined inline later because of dependency on named_value_parser
     */
    virtual void xparse(boost::any& value_store,
                        const std::vector<std::basic_string<char>>& new_tokens) const override;

    /*!
     * \brief Call xparse on base class. This is available to named_value_parser
     * for invoking the default parser when needed
     */
    void xparse_base_(boost::any& value_store,
                      const std::vector<std::basic_string<char>>& new_tokens) const {
        po::typed_value<ArgT>::xparse(value_store, new_tokens);
    }
};

/*!
 * \brief Parser helper for named_value_type
 */
template <typename ArgT>
class named_value_parser {
public:

    /*!
     * \brief Default implementation of parse_ - overridden in class template
     * specializations
     *
     * Invokes the corresponding named_value_type's base_class' parser
     */
    static void parse_(const named_value_type<ArgT>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        nvt.xparse_base_(value_store, new_tokens);
    }
};

template <class ArgT>
void named_value_type<ArgT>::xparse(boost::any& value_store,
                                     const std::vector<std::basic_string<char>>& new_tokens) const {
    // Invoked the appropriately typed named_value_parser
    //std::cout << "parsing " << my_name_ << " : " << new_tokens << std::endl;
    named_value_parser<ArgT>::parse_(*this, value_store, new_tokens);
}

/*!
 * \brief named_value_parser specialization for uint64_t
 */
template <>
class named_value_parser<uint64_t> {
public:
    static void parse_(const named_value_type<uint64_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        uint64_t val = utils::smartLexicalCast<uint64_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for int64_t
 */
template <>
class named_value_parser<int64_t> {
public:
    static void parse_(const named_value_type<int64_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        int64_t val = utils::smartLexicalCast<int64_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for uint32_t
 */
template <>
class named_value_parser<uint32_t> {
public:
    static void parse_(const named_value_type<uint32_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        uint32_t val = utils::smartLexicalCast<uint32_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for int32_t
 */
template <>
class named_value_parser<int32_t> {
public:
    static void parse_(const named_value_type<int32_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        int32_t val = utils::smartLexicalCast<int32_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for uint16_t
 */
template <>
class named_value_parser<uint16_t> {
public:
    static void parse_(const named_value_type<uint16_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        uint16_t val = utils::smartLexicalCast<uint16_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for int16_t
 */
template <>
class named_value_parser<int16_t> {
public:
    static void parse_(const named_value_type<int16_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        int16_t val = utils::smartLexicalCast<int16_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for uint8_t
 */
template <>
class named_value_parser<uint8_t> {
public:
    static void parse_(const named_value_type<uint8_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        uint8_t val = utils::smartLexicalCast<uint8_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};

/*!
 * \brief named_value_parser specialization for int8_t
 */
template <>
class named_value_parser<int8_t> {
public:
    static void parse_(const named_value_type<int8_t>& nvt,
                       boost::any& value_store,
                       const std::vector<std::basic_string<char>>& new_tokens) {
        (void) nvt;
        size_t end_pos;
        int8_t val = utils::smartLexicalCast<int8_t>(new_tokens.at(0), end_pos);
        //std::cout << "      got: " << val << std::endl;
        value_store = val;
    }
};


/*!
 * \brief Helper function for generating new named_value_type structs in the
 * boost style
 */
template <typename ArgT>
inline named_value_type<ArgT>* named_value(std::string const& name, ArgT* val=nullptr) {
    return new named_value_type<ArgT>(name, val);
}

template <typename ArgT>
inline named_value_type<ArgT>* named_value(std::string const& name, unsigned min, unsigned max, ArgT* val=nullptr) {
    return new named_value_type<ArgT>(name, val, min, max);
}


/*!
 * \brief Class for containing multiple levels of boost program options
 */
class MultiDetailOptions final
{
public:

    typedef uint32_t level_t;

    static const level_t VERBOSE = 0;
    static const level_t BRIEF = 1;

    /*!
     * \brief Helper class for chained calls to add_options
     */
    class OptAdder {
        MultiDetailOptions& opts_;

    public:
        OptAdder(MultiDetailOptions& opts) :
            opts_(opts)
        {;}

        /*!
         * \brief Acts like calling MultiDetailOptions::add_options on the
         * object that created this OptAdder.
         */
        template <typename ...Args>
        OptAdder& operator()(const char* name,
                             const char* verbose_desc,
                             const Args& ...args)
        {
            opts_.add_option_with_desc_level_(0, name, verbose_desc, args...);

            return *this;
        }

        /*!
         * \brief Acts like calling MultiDetailOptions::add_options on the
         * object that created this OptAdder.
         */
        template <typename ...Args>
        OptAdder operator()(const char* name,
                         const boost::program_options::value_semantic* s,
                         const char* verbose_desc,
                         const Args& ...args)
        {
            opts_.add_option_with_desc_level_(0, name, s, verbose_desc, args...);

            return *this;
        }
    };

    /*!
     * \brief Allow access to private adding methods
     */
    friend class OptAdded;

    /*!
     * \brief Not copy-constructable
     */
    MultiDetailOptions(const MultiDetailOptions&) = delete;

    /*!
     * \brief Not assignable
     */
    MultiDetailOptions& operator=(const MultiDetailOptions&) = delete;

    /*!
     * \brief Not default-constructable
     */
    MultiDetailOptions() = delete;

    /*!
     * \brief Construction with group nam
     * \post Ensures that the VERBOSE options entry and BRIEF entries exist
     */
    MultiDetailOptions(const std::string& name, uint32_t w=80, uint32_t hw=40) :
        opt_adder_(*this),
        name_(name)
    {
        descs_.emplace_back(new po::options_description(name_, w, hw));
        descs_.emplace_back(new po::options_description(name_, w, hw));

        sparta_assert(descs_.size() > VERBOSE);
        sparta_assert(descs_.size() > BRIEF);
    }

    /*!
     * \brief Gets the description object for a particular level if that level
     * exists. If that level does not exist, returns the highest level less than
     * \a level where options exist. Will never throw because VERBOSE is
     * guaranteed to exist.
     * Only VERBOSE and BRIEF levels are guaranteed to exist
     * \note When actually parsing using these options, use the VERBOSE level
     */
    const po::options_description& getOptionsLevelUpTo(level_t level) const {
        if(level < descs_.size()){
            return *descs_.at(level);
        }else{
            sparta_assert(descs_.size() > 0);
            return *descs_.at(descs_.size() - 1);
        }
    }

    /*!
     * \brief Gets the description object for a particular level.
     * Only VERBOSE and BRIEF levels are guaranteed to exist
     * \note When actually parsing using these options, use the VERBOSE level
     * \throws Exception if there is no options set at \a level
     */
    const po::options_description& getOptionsLevel(level_t level) const {
        return *descs_.at(level);
    }

    /*!
     * \brief Gets the description object for the VERBOSE level
     */
    const po::options_description& getVerboseOptions() const noexcept {
        return *descs_.at(VERBOSE);
    }

    /*!
     * \brief Returns the number of levels that have an options description
     * which can be retrieved through getLevel
     */
    size_t getNumLevels() const {
        return descs_.size();
    }

    /*!
     * \brief Add an option with NO value semantic and any number of
     * descriptions. See the other add_options signature for details
     */
    template <typename ...Args>
    OptAdder& add_options(const char* name,
                         const char* verbose_desc,
                         const Args& ...args)
    {
        add_option_with_desc_level_(0, name, verbose_desc, args...);

        return opt_adder_;
    }

    /*!
     * \brief Add an option with a value semantic and any number of descriptions
     * \tparam ...Args variadic argument container type. Automatically deduced
     * by call signature
     * \param name Name of the option to be interpreted by
     * boost::program_options::options_description (e.g. "help,h").
     * \param s Value semantic. Typically something generated by the helper
     * sparta::app::named_value or boost::program_options::value<T>().
     * \param verbose_desc Verbose description. All options have a required
     * verbose description so they show up in verbose help (which is practically
     * a man page) and can be appropriately parsed by boost
     * \param ...args Additional const char* description arguments (like
     * verbose_desc). Each additional argument is assigned to the next higher
     * options level for this option (\a name). If no additional descriptions
     * are given, this option will not have an entry when the options are
     * printed for that level. Generally, each additional level becomes more
     * brief.

     * Example:
     * \code
     * // MultiDetailOptions opts("foo");
     * opts.add_option("bar",
     *                 "verbose description of bar",
     *                 "brief bar desc")
     * \endcode
     */
    template <typename ...Args>
    OptAdder& add_options(const char* name,
                     const boost::program_options::value_semantic* s,
                     const char* verbose_desc,
                     const Args& ...args)
    {
        add_option_with_desc_level_(0, name, s, verbose_desc, args...);

        return opt_adder_;
    }

    /*!
     * \brief Empty add_options shell allowing the start of chained calls
     * (exists solely to mimic boost's syntax)
     *
     * Example:
     * \code
     * // MultiDetailOptions opts("foo");
     * opts.add_option()("bar",
     *                   "verbose description of bar",
     *                   "brief bar desc")
     * \endcode
     */
    OptAdder& add_options()
    {
        return opt_adder_;
    }

private:

    /*!
     * \brief Terminator for calls to add_option_with_desc_level_. Invoked when
     * ...args is empty
     */
    void add_option_with_desc_level_(size_t level,
                                     const char* name)
    {
        (void) level;
        (void) name;
    }

    /*!
     * \brief Private recursive helper for handling variable argument calls to
     * add_options. See the other signature for more details
     */
    template <typename ...Args>
    void add_option_with_desc_level_(size_t level,
                                     const char* name,
                                     const char* desc,
                                     const Args& ...args) {
        while(descs_.size() <= level){
            descs_.emplace_back(new po::options_description(name_));
        }

        descs_.at(level)->add_options()
            (name, desc);

        // Recursively invoke until terminator overload of this func is reached
        add_option_with_desc_level_(level+1, name, args...);
    }


    //// Terminator
    //void add_option_with_desc_level_(size_t level,
    //                                 const char* name,
    //                                 const boost::program_options::value_semantic* s)
    //{
    //    (void) level;
    //    (void) name;
    //    delete s; // Delete unused copy of value_semantic
    //}

    /*!
     * \brief Private recursive helper for handling variable argument calls to
     * add_options. Each description is added to the specified \a level and the
     * rest are passed recursively to be added at the next level. Once
     * \a ...args is empty, the non-templated terminator signature of this
     * function is called
     */
    template <typename ...Args>
    void add_option_with_desc_level_(size_t level,
                                     const char* name,
                                     const boost::program_options::value_semantic* s,
                                     const char* desc,
                                     const Args& ...args) {
        while(descs_.size() <= level){
            descs_.emplace_back(new po::options_description(name_));
        }

        descs_.at(level)->add_options()
            (name, s, desc);

        // Recursively invoke until terminator overload of this func is reached
        // Note that this invoked the OTHER signature with NO VALUE SEMANTIC.
        // Boost frees these on destruction, so assigning the same pointer to
        // multiple options will create a double-free segfault.
        // The implication is that parsing must be done at the verbose level and
        // higher levels will have no semantic information (e.g. arguments)
        add_option_with_desc_level_(level+1, name, args...);
    }

    /*!
     * \brief Option adder which is returned by add_options() to allow chained
     * calls
     */
    OptAdder opt_adder_;

    /*!
     * \brief Name of this set of options
     */
    const std::string name_;

    /*!
     * \brief Vector of levels of boost program options option_description sets
     */
    std::vector<std::unique_ptr<po::options_description>> descs_;

}; // class MultiDetailOptions

    } // namespace app
} // namespace sparta

#endif // #ifndef __MULTI_DETAIL_OPTIONS_H__
