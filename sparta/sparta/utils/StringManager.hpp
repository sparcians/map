// <StringManager> -*- C++ -*-

#ifndef __STRING_MANAGER_H__
#define __STRING_MANAGER_H__

#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <iomanip>

#include "sparta/utils/StaticInit.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    class StringManager;

    /*!
     * \brief Manages string internment for SPARTA.
     * This allows strings to be compared by pointer once interned.
     */
    class StringManager
    {

        /*!
         * \brief Deleter for std::unique_ptr<std::string> that can optionally
         * not delete them based on instantiation options.
         * \note uses delete x to deelte string pointers
         */
        class Deleter {

            /*!
             * \brief Should this instance free the pointer
             */
            const bool should_free_;

        public:

            /*!
             * \brief Default constructor disabled
             */
            Deleter() = delete;

            /*!
             * \brief Constructor
             * \param should_free Should the call operator in this intance
             * delete its argument
             */
            Deleter(bool should_free) : should_free_(should_free) {;}

            /*!
             * \brief Default copy constructor
             */
            Deleter(const Deleter&) = default;

            /*!
             * \brief No copy assignment operator
             */
            Deleter& operator=(Deleter&) = delete;

            /*!
             * \brief Deleter function.
             * \post Deletes sp if this instance was constructed with
             * should_free = true. Has no effect otherwise
             */
            void operator()(std::string* sp) const {
                if(should_free_){
                    delete sp;
                }
            }
        };


        /*!
         * \brief Deletes the strings in unique_ptrs for which it is the deleter
         */
        Deleter do_delete_;

        /*!
         * \brief Does not delete the strings in unique_ptrs for which it is the
         * deleter
         * \note Used for interning static strings where the string is allocated
         * prior to interning
         */
        Deleter dont_delete_;

    public:

        /*!
         * \brief Allow initialization of statics through this helper
         */
        friend class SpartaStaticInitializer;

        /*!
         * \brief Mapping of strings to their addresses
         */
        using StringUniquePtr = std::unique_ptr<std::string, Deleter>;
        typedef std::map<std::string, StringUniquePtr> StringMap;

        /*!
         * \brief Mapping of ID to string for getting the string associated
         * with this ID. Allows lookup with a string from anywhere to see if any
         * equivalent string are in this manager.
         */
        StringMap string_map_;
        uint32_t max_string_len_; //!< Maximum string length held by the StringManager

        /*!
         * \brief Has this singleton been constructed yet. Yes if equal to
         * IS_CONSTRUCTED_CONST
         */
        const uint64_t is_constructed;

        /*!
         * \brief Value of is_constructed after construction
         */
        static const uint64_t IS_CONSTRUCTED_CONST = 0x0123456789abcdef;

        /*!
         * \brief Constructor
         */
        StringManager() :
            do_delete_(true),
            dont_delete_(false),
            max_string_len_(0),
            is_constructed(IS_CONSTRUCTED_CONST),
            EMPTY(internString(""))
        { }

        /*!
         * \brief Stores a string in shared space within this manager
         * unless it is already stored.
         * \return pointer to interned string equivalent to the
         * given string string. Never returns nullptr;
         * \param s string to intern. Can be anything
         * \post s will be interned if not already interned.
         */
        std::string* internString(const std::string& s) {
            std::string* result = findString(s);
            if(result){
                return result;
            }

            // Unfortunately cannot point to key as it is not guaranteed
            // stable. Duplicating the string string is minor waste.
            max_string_len_ = std::max<decltype(max_string_len_)>(max_string_len_, s.size());
            StringUniquePtr ns(new std::string(s), do_delete_);
            string_map_.emplace(s, std::move(ns));
            return string_map_.find(s)->second.get();
        }

        /*!
         * \brief Find a string in this manager.
         * \return Pointer to the interned string if present. If not already
         * interned, returns nullptr
         */
        std::string* findString(const std::string& s) const {
            auto itr = string_map_.find(s);
            if(itr != string_map_.end()){
                std::string* result = itr->second.get();
                sparta_assert(result != nullptr);
                return result;
            }
            return nullptr;
        }

        /*!
         * \brief Checks to see if a string is present without adding it.
         * \return true if string is interned and false if not.
         */
        bool hasString(const std::string& s) const {
            return nullptr != findString(s);
        }

        /*!
         * \brief Determines if a string is interned in this string manager by
         * its pointer.
         * \return true if string is interned with this pointer
         * \note this is a slow method since it iterates the entire internal map
         */
        bool isInterned(const std::string* s) const {
            for(auto & p : string_map_){
                if(p.second.get() == s){
                    return true;
                }
            }

            return false;
        }

        /*!
         * \brief Write all strings interned in the string manager to an
         * ostream.
         */
        void dumpStrings(std::ostream& o, bool pretty=false) const {
            (void) pretty;

            std::ios::fmtflags f = o.flags();

            for(auto& pair : string_map_){
                o << "\"" << pair.first << "\" "
                  << std::setw((max_string_len_+4)-pair.first.size())
                  << " @ " << std::setw(10) << pair.second.get() << "\n";
            }

            // restore ostream flags
            o.flags(f);
        }

        /*!
         * \brief Gets the number of strings in this StringManager
         */
        uint32_t getNumStrings() const {
            return string_map_.size();
        }

        //! \brief Holds interned empty strings
        const std::string* const EMPTY;

        /*!
         * \brief Returns the StringManager singleton
         * \note This method is valid until static destruction
         */
        static StringManager& getStringManager() {
            sparta_assert(_GBL_string_manager && _GBL_string_manager->is_constructed == IS_CONSTRUCTED_CONST,
                              "Attempted to access StringManager singleton before it was "
                              "statically constructed.")
            return *_GBL_string_manager;
        }

    private:

        /*!
         * \brief Singleton pointer. This pointer has controlled construction
         * and destruction performed by the StringManagerStaticInitializer
         * class
         */
        static sparta::StringManager* _GBL_string_manager;
    };

} // namespace sparta

// __STRING_MANAGER_H__
#endif
