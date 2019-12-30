/* Param.h
 *
 ***************************************************************************/

#ifndef __SPARTA_UTILS_PARAM_H__
#define __SPARTA_UTILS_PARAM_H__


#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include "inttypes.h"
#include "sparta/utils/StringUtils.hpp"

namespace sparta {
namespace utils {

    //--------------------------------------------------
    // Typedefs
    //
    typedef std::map<std::string, std::string> ParamDataType;


    //===========================================================================
    // class Param
    //
    // Example usage:
    //   params.addPair("foo", "  17  ");
    //   params.addPair("bar", "  29.3  ");
    //   params.addPair("baz", "  howdy, sailor!  ");
    //   params.addPair("baf1", "  true  ");
    //   params.addPair("baf2", "  false  ");
    //   params.addPair("baf3", "  1  ");
    //   params.addPair("baf4", "  0  ");
    //   params.addPair("baf5", "  barf  ");
    //
    //   uint32_t    fooval  = params.getValue_uint32_t("foo"); // 17
    //   double      barval  = params.getValue_double  ("bar"); // 29.3
    //   std::string bazval  = params.getValue_string  ("baz"); // "  howdy, sailor!  "
    //   bool        baf1val = params.getValue_bool    ("baf1");    // true
    //   bool        baf2val = params.getValue_bool    ("baf2");    // false
    //   bool        baf3val = params.getValue_bool    ("baf3");    // true
    //   bool        baf4val = params.getValue_bool    ("baf4");    // false
    //   bool        baf5val = params.getValue_bool    ("baf5");    // false
    //
    class Param
    {
    public:
        //--------------------------------------------------
        // method Param::addPair()
        //
        inline void addPair(const std::string &key, const std::string &value)
        {
            data_[key] = value;
        }   // method Param::addPair()


        //--------------------------------------------------
        // method Param::paramExists()
        // - return whether or not the param exists
        //
        inline bool paramExists(const std::string &key) const
        {
            return (data_.count(key) > 0);
        }   // method Param::paramExists()


        //--------------------------------------------------
        // method Param::size()
        // - return number of params
        //
        inline size_t size(void) const
        {
            return (data_.size());
        }   // method Param::size()


        //--------------------------------------------------
        // method Param::begin()
        // - return iterator to first param
        //
        inline ParamDataType::const_iterator begin(void) const
        {
            return (data_.begin());
        }   // method Param::begin()


        //--------------------------------------------------
        // method Param::begin()
        // - return iterator to first param
        //
        inline ParamDataType::iterator begin(void)
        {
            return (data_.begin());
        }   // method Param::begin()


        //--------------------------------------------------
        // method Param::end()
        // - return iterator to one past last param
        //
        inline ParamDataType::const_iterator end(void) const
        {
            return (data_.end());
        }   // method Param::end()


        //--------------------------------------------------
        // method Param::end()
        // - return iterator to one past last param
        //
        inline ParamDataType::iterator end(void)
        {
            return (data_.end());
        }   // method Param::end()


        //--------------------------------------------------
        // method Param::getValue_uint32_t()
        //
        inline uint32_t getValue_uint32_t(const std::string &key)
        {
            if (data_.count(key) != 1) {
                std::cout << "*** ERROR:  param \"" << key << "\" hasn't been defined" << std::endl;
                exit (-1);
            }

            std::stringstream value_stream(sparta::utils::strip_whitespace(data_[key]));

            uint32_t return_value;
            value_stream >> return_value;

            return return_value;
        }   // method Param::getValue_uint32_t()


        //--------------------------------------------------
        // method Param::getValue_double()
        //
        inline double getValue_double(const std::string &key)
        {
            if (data_.count(key) != 1) {
                std::cout << "*** ERROR:  param \"" << key << "\" hasn't been defined" << std::endl;
                exit (-1);
            }

            std::stringstream value_stream(sparta::utils::strip_whitespace(data_[key]));

            double return_value;
            value_stream >> return_value;

            return return_value;
        }   // method Param::getValue_double()


        //--------------------------------------------------
        // method Param::getValue_string()
        //
        inline std::string getValue_string(const std::string &key)
        {
            if (data_.count(key) != 1) {
                std::cout << "*** ERROR:  param \"" << key << "\" hasn't been defined" << std::endl;
                exit (-1);
            }

            return data_[key];
        }   // method Param::getValue_string()


        //--------------------------------------------------
        // method Param::getValue_bool()
        //
        inline bool getValue_bool(const std::string &key)
        {
            if (data_.count(key) != 1) {
                std::cout << "*** ERROR:  param \"" << key << "\" hasn't been defined" << std::endl;
                exit (-1);
            }

            //-------------------------
            // Handle "true" and "false" boolean values specially
            //
            std::string value = sparta::utils::strip_whitespace(data_[key]);

            if (value == "true") {
                return true;
            } else if (value == "false") {
                return false;
            }

            std::stringstream value_stream(sparta::utils::strip_whitespace(data_[key]));

            bool return_value;
            value_stream >> return_value;

            return return_value;
        }   // method Param::getValue_bool()

    private:
        ParamDataType data_;
    };  // class Param


}   // namespace utils
}   // namespace sparta

//===========================================================================
// operator<<() for Param
//
inline std::ostream & operator<< (std::ostream & os, const sparta::utils::Param &params)
{
    bool first_record = true;
    for (sparta::utils::ParamDataType::const_iterator it = params.begin(); it != params.end(); it++) {
        if (!first_record) {
            os << ", ";
        }

        os << it->first << "=" << it->second;
        first_record = false;
    }

    return os;
}   // operator<<() for BPConfig


#endif  // __PARAM_H__
