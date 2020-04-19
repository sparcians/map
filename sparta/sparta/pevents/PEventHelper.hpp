#pragma once

#include <tuple>
#include <array>
#include <iostream>
#include <utility>
#include "sparta/log/MessageSource.hpp"
#include <type_traits>
#include <boost/format.hpp>
namespace sparta{
namespace pevents{

    //!A typedef for defining formatters for custom attributes.
    typedef std::ios_base::fmtflags FormatFlags;

//we'll want a way to force that certain keys are not used else where.
namespace PEventProtection
{
    enum ProtectAttrs{
        EV,
        CYCLE
    };
    extern std::array<std::string, 2> PEventProtectedAttrs;


    /**
     * \brief A helper method used when asserting that quotes do not
     * already exist in String type pevent attributes, since we will
     * be appending the quotes automatically.
     */
    template<typename T>
     bool CheckNoQuotes(const T&, const std::string&, const std::string&)
    {
        return true;
    }


    /**
     * \brief a helper method for appending quotes only when called by a
     * string type attribute.
     */
    template<typename T>
     void AppendQuote(std::stringstream&)
    {/*do nothing*/}


    template<>
    inline bool CheckNoQuotes(const std::string& obj, const std::string& pre, const std::string& post)
    {
        if(SPARTA_EXPECT_FALSE(std::strcmp(&obj.front(), "\"") == 0))
        {
            return false;
        }
        if(SPARTA_EXPECT_FALSE(std::strcmp(&obj.back(), "\"") == 0))
        {
            return false;
        }
        if(SPARTA_EXPECT_FALSE(std::strcmp(&pre.front(), "\"") == 0))
        {
            return false;
        }
        if(SPARTA_EXPECT_FALSE(std::strcmp(&post.back(), "\"") == 0))
        {
            return false;
        }
        //There were no problems with quotes.
        return true;
    }

    template<>
    inline void AppendQuote<const std::string&>(std::stringstream& ss)
    {
        ss << "\"";
    }


}



template<typename ...CustomAttrs>
class PEvent
{

    typedef PEvent<CustomAttrs...> ThisType;
    //!Some attributes that are required by each pevent.
    struct RequiredAttrs{
        RequiredAttrs(const std::string& name) :
            event_name(name)
        {}
        uint64_t cycle_time = 0; //!Where are we getting time, is the PEvent passed a clock, or are we asking the scheduler singleton
        uint64_t unique_id = 0;
        std::string event_name = "";
    };

    //!A struct used for cacheing output format information about each custom paramater.
    struct PrePostTags{
        PrePostTags() :
            pre_format(),
            post_format()
        {}
        std::string prefix = "";
        std::string postfix = "";
        FormatFlags pre_format;
        FormatFlags post_format;
        int swidth = 0;
        char fill_char = ' ';

    };

    //!We need to map the attributes to some text names
    //!Is there some need to ensure that custom attribute names follow a convention?
    std::array<std::string, sizeof...(CustomAttrs)> custom_attrs_names_;

    //!We need to capture some formatting information for each attribute
    std::array<PrePostTags, sizeof...(CustomAttrs)> custom_attrs_formats_;
    //!We need to cache attributes.
    std::tuple<CustomAttrs...> custom_attrs_;



    //!A struct with some required attributes.
    RequiredAttrs required_attrs_;

    //!We will need a logger to use
    log::MessageSource& logger_;
    //!The clock tied to this PEvent, yes the log is going to manage time also...
    const Clock* clk_;


    void writeRequiredData_(std::stringstream& s)
    {
        //Write event name
        std::stringstream name_str;
        name_str << "\"" << required_attrs_.event_name << "\"";
        s.flags(FormatFlags(std::ios::left));
        s << std::get<PEventProtection::EV>(PEventProtection::PEventProtectedAttrs) << "=" << std::setw(12) << name_str.str() << " ";
    }

    void appendCycle_(std::stringstream& s)
    {
        //Write cycle
        s << std::get<PEventProtection::CYCLE>(PEventProtection::PEventProtectedAttrs) << "=" << clk_->currentCycle();
    }

    //!Some crazyness to unroll our tuples/array at compile time.
    template <class Tuple, class Array, class Formats, std::size_t N>
    struct TupleUnroller {
        static void unroller_(const Tuple& t, const Array& a, const Formats& f, std::stringstream& s){
            TupleUnroller<Tuple, Array, Formats, N-1>::unroller_(t, a, f, s);
            s << std::get<N-1>(a) << "=";
            //Assert that the first and last character are not quotes on a string, since we are going to insert
            //our own quotes on strings.
            sparta_assert(PEventProtection::CheckNoQuotes(std::get<N-1>(t), std::get<N-1>(f).prefix, std::get<N-1>(f).postfix), "Quotes are appended to string types for PEvent logging automatically and should not be done by the modeller in the prefix, postfix, or the string itself.");
            //Automatically insert quotes to the beginning of the string types.
            PEventProtection::AppendQuote<decltype(std::get<N-1>(t))>(s);
            s.flags(std::get<N-1>(f).pre_format);
            s << std::get<N-1>(f).prefix;
            s << std::setfill(std::get<N-1>(f).fill_char);
            s << std::setw(std::get<N-1>(f).swidth);
            /*!     Writing the data  */
            s << std::get<N-1>(t);
            s << std::get<N-1>(f).postfix;
            //Automatically insert quotes to the end of the string types.
            PEventProtection::AppendQuote<decltype(std::get<N-1>(t))>(s);
            s << " ";
            s.flags(std::get<N-1>(f).post_format);
        }
    };
    template <class Tuple, class Array, class Formats>
    struct TupleUnroller<Tuple, Array, Formats, 1> {
        static void unroller_(const Tuple& t, const Array& a, const Formats& f, std::stringstream& s){
            s << std::get<0>(a) << "=";
            s.flags(std::get<0>(f).pre_format);
            s << std::get<0>(f).prefix;
            //Assert that the first and last character are not quotes on a string, since we are going to insert
            //our own quotes on strings, also do the assertions on pre and post fixes.
            sparta_assert(PEventProtection::CheckNoQuotes(std::get<0>(t), std::get<0>(f).prefix, std::get<0>(f).postfix), "Quotes are appended to string types for PEvent logging automatically and should not be done by the modeller in the prefix, postfix, or the string itself.");
            //Automatically insert quotes to the beginning of the string types.
            PEventProtection::AppendQuote<decltype(std::get<0>(t))>(s);
            s << std::get<0>(f).postfix;
            s << std::setfill(std::get<0>(f).fill_char);
            s << std::setw(std::get<0>(f).swidth);
            /*!     Writing the data  */
            s << std::get<0>(t);
            s.flags(std::get<0>(f).post_format);
            //Automatically insert quotes to the end of the string types.
            PEventProtection::AppendQuote<decltype(std::get<0>(t))>(s);
            s << " ";
        }
    };
public:

    //!Would set the event_type, as well as initial values for custom attrs
    //!Set the custom attribute names.
    //!We can probably create our own logger? But maybe they want to use the
    ///same logger for all these events?
    template<typename ...ArgNames>
    PEvent(const std::string& name, log::MessageSource& logger, const Clock* clk, ArgNames&&... names) :
        custom_attrs_names_{{names...}}, //some crazyness with array initializer lists, and stuff.
        required_attrs_(name),
        logger_(logger),
        clk_(clk)
    {
        //run time assert that checks to makes sure none of that custom attributes
        //are using a restricted name, such as names for required attributes.
        for(auto n : custom_attrs_names_)
        {
            sparta_assert(n != PEventProtection::PEventProtectedAttrs[0], "custom attribute name cannot be one of the protected attribute names");
            sparta_assert(n != PEventProtection::PEventProtectedAttrs[1], "custom attribute name cannot be one of the protected attribute names");
        }
    }


    void setName(const std::string& name)
    {
        required_attrs_.event_name = name;
    }
    /**
     * \brief add formatting options to each custom attribute.
     * \param n the position of the custom attribute in the parameters, with 0 being the
     * first custom parameter.
     * \param pre_flag the number format such as std::hex
     * \param post_flag the number format after leaving this, most always std::dec i assume.
     * \param pre a string to be appended to the front of the data, such as "0x"
     * \param post the string to be appended to the end of the data, such as ","
     *
     * \example
     * setFormatFlags(1, pevents::FormatFlags(std::ios::hex),
     *                   pevents::FormatFlags(std::ios::dec),
     *                   "0x", "");
     * would output as
     * EV="SOMETYPE", ... other attributes..., custom_param2=0x40
     */
    void setFormatFlags(const uint32_t n, const FormatFlags& pre_flag,
                        const FormatFlags& post_flag,
                        const std::string& pre, const std::string& post)
    {
        sparta_assert(n < sizeof...(CustomAttrs), "Cannot set the format flags for an attribute where n >= the number of custom attributes.");

        custom_attrs_formats_[n].prefix = pre;
        custom_attrs_formats_[n].postfix = post;
        custom_attrs_formats_[n].pre_format = pre_flag;
        custom_attrs_formats_[n].post_format = post_flag;
    }
    void setFormatFlags(const uint32_t n, const std::string& pre, const std::string& post)
    {
        setFormatFlags(n, FormatFlags(), FormatFlags(), pre, post);
    }

    /**
     * \brief set the setw size, and the alignment format.
     *
     * \example
     * //to set a number as hex that displays 8 leading zero's
     * setFormatLength(0, 8, FormatFlags(std::ios::left), '0');
     */
    void setFormatLength(const uint32_t n, const int length, const FormatFlags& align, const char fill)
    {
        sparta_assert(n < sizeof...(CustomAttrs), "Cannot set the format flags for an attribute where n >= the number of custom attributes.");
        custom_attrs_formats_.at(n).swidth = length;
        custom_attrs_formats_.at(n).pre_format = custom_attrs_formats_.at(n).pre_format | align;
        custom_attrs_formats_.at(n).fill_char = fill;
    }

    /**
     * \brief set many attributes as strings,
     * \usage
     * setAsString({1,3,5}); //where 1, 3, and 5 are attributes to be set as a string.
     */
    void setAsStrings(const std::initializer_list<uint32_t> list)
    {
        for(uint32_t i : list){
            setFormatFlags(i, "\"", "\"");
        }
    }

    /**
     * \brief a helper to set a custom attribute to be outputted in hex format.
     * \usage
     * setAsHex({1, 2, 3}); //will set custom attributes 1, 2, 3 as hex displayed outputs
     */
    void setAsHex(const std::initializer_list<uint32_t>& list)
    {
        static const int hex_length = 8;
        for(uint32_t i : list){
            setFormatFlags(i, pevents::FormatFlags(std::ios::hex), pevents::FormatFlags(std::ios::dec), "0x", "");
            setFormatLength(i, hex_length, pevents::FormatFlags(std::ios::right), '0');
        }
    }




    //!We probably need a way to set the attributes dynamically?
    //!Or would we rather set them every fireEvent?
    //!Or would the attributes not change?
    //!Is something like this sufficient?
    void setAttrs(CustomAttrs... attrs) { custom_attrs_ = std::make_tuple<CustomAttrs...>(std::forward<CustomAttrs>(attrs)...); }

    //!Set individual attributes?
    //!Would be cool if we could so some weirdness to set attribute by name... maybe some macro magic.
    template<typename AttrType, uint32_t N>
    void setAttr(const AttrType& attr) { std::get<N>(custom_attrs_) = attr; }

    //!Maybe it would be worth allowing some ability to have this PEvent instance query objects for attribute data...
    ///I don't know where the PEvent data is actually coming from?

    //!I guess we will need some way to alert the PEvent that it has occured?
    //!This is where the event would be dumped?
    void fireEvent()
    {
        if(logger_.observed())
        {
            std::stringstream s;
            writeRequiredData_(s);
            TupleUnroller<decltype(custom_attrs_), decltype(custom_attrs_names_), decltype(custom_attrs_formats_), sizeof...(CustomAttrs)>::unroller_(custom_attrs_, custom_attrs_names_, custom_attrs_formats_, s);
            appendCycle_(s);
            s << ";";
            logger_ << s.str();
        }
        //if(Pevent logging).... ? We need this if?
        //1)build a string with the required attrs.

        //2)append custom attrs to the string

        //3)dump the string, this may call a singleton head or something which passes information to the sparta logger?
        /// do what ever else the Pevent needs to do such as notifying observers?

        //We are dumping in a string format for sure?

        //Would I pass this to a logger now?
        //std::cout << //required_attrs_.cycle_time << " id: " << required_attrs_.unique_id << " EventType: "  << " ";
        //std::cout << std::get<PEventProtection::EV>(PEventProtection::PEventProtectedAttrs) << "="<< required_attrs_.event_name << " ";
        //This is gonna burn compile time, but oh well.
        //TupleUnroller<decltype(custom_attrs_), decltype(custom_attrs_names_), sizeof...(CustomAttrs)>::unroller_(custom_attrs_, custom_attrs_names_);
        //std::cout << std::endl;
    }

    //!Maybe overload fireEvent to allow setting the attributes when firing.
    void fireEvent(CustomAttrs... attrs)
    {
        setAttrs(attrs...);
        fireEvent();
    }

    bool observed()
    {
        return logger_.observed();
    }

};


}//namespace pevents
}//namespace sparta



