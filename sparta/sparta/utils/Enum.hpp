// <Enum> -*- C++ -*-


#ifndef _H_SPARTA_ENUM
#define _H_SPARTA_ENUM

#include <inttypes.h>
#include <string>
#include <memory>
#include <vector>
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
namespace utils
{

    template<class EnumType>
    class Enum
    {
        friend class Value;
        static const std::unique_ptr<std::string[]>          names_;


    public:
        typedef EnumType value_type;

        //! Method which populates an argument vector with string-names of
        //  enum constants of the class enum template argument.
        static void populateNames(std::vector<std::string>& names){
            for(std::size_t i = 0; i < static_cast<uint32_t>(value_type::__LAST); ++i){
                names.emplace_back(names_[i]);
            }
        }

        class UnknownNameException : public SpartaException
        {
        public:
            UnknownNameException() :
               SpartaException()
            {}

            UnknownNameException(const std::string& reason) :
               SpartaException(reason)
            {}
        };

    public:
        class Value
        {
        public:
            Value(const EnumType& val = EnumType::__FIRST):
                val_(val)
            {}

            Value(const Value& other):
                val_(other.val_)
            {}

            explicit operator EnumType() const
            {
                return val_;
            }

            operator uint32_t() const
            {
                return static_cast<uint32_t>(val_);
            }

            operator std::string() const
            {
                return Enum<EnumType>::names_[static_cast<uint32_t>(val_)];
            }

            Value& operator=(const Value& other)
            {
                val_ = other.val_;
                return *this;
            }

            bool operator==(const Value& other) const
            {
                return (val_ == other.val_);
            }

            bool operator!=(const Value& other) const
            {
                return (val_ != other.val_);
            }

            const EnumType & getEnum() const {
                return val_;
            }

        private:
            EnumType        val_;
        };

    public:
        class iterator
        {
        public:
            iterator(const EnumType& val):
                val_(val)
            {}

            iterator(const iterator& other):
                val_(other.val_)
            {}

            void operator++()
            {
                val_ = static_cast<EnumType>(static_cast<uint32_t>(val_) + 1);
            }

            Value operator*() const
            {
                return Value(val_);
            }

            bool operator==(const iterator& other) const
            {
                return (val_ == other.val_);
            }

            bool operator!=(const iterator& other) const
            {
                return (val_ != other.val_);
            }

        private:
            EnumType          val_;
        };

    public:

        Enum() {}

        template<class...Args>
        Enum(const Args&...args)
        {
            static_assert(std::is_enum<EnumType>::value,
                          "ERROR: EnumType is not an enum");
            setName_(args...);
        }

        Enum(const Enum& other)
        {
            (void) other;
        }

        const Value operator()(const EnumType& val) const
        {
            return Value(val);
        }

        const Value operator()(const uint32_t i) const
        {
            return Value(static_cast<EnumType>(i));
        }

        const Value operator()(const std::string& s) const
        {
            // This is OK for small enums when this method is not used in
            // critical path code. Use a real string matching algorithm
            // if needed eventually
            for (uint32_t i = 0; i < static_cast<uint32_t>(EnumType::__LAST); ++i) {
                if (names_[i] == s) {
                    return this->operator()(i);
                }
            }
            //throw UnknownEnumException("Undeclared Enum value: ") << s;
            throw UnknownNameException();
            return Value(EnumType::__LAST);
        }

        iterator begin() const
        {
            return iterator(EnumType::__FIRST);
        }

        iterator end() const
        {
            return iterator(EnumType::__LAST);
        }

        size_t size() const {
            return static_cast<uint32_t>(EnumType::__LAST);
        }

    private:
        void setName_()
        {
            names_[static_cast<uint32_t>(EnumType::__LAST)] = "<<LAST>>";
        }

        template<class...Args>
        void setName_(const EnumType& id, const std::string& name, const Args&...args)
        {
            names_[static_cast<uint32_t>(id)] = name;
            setName_(args...);
        }
    };

} // utils
} // sparta
#endif //_H_SPARTA_ENUM
