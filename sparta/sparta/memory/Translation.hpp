
/**
 * \file   Translation.hpp
 *
 * \brief  File that defines Translation classes and typedefs for addressing
 */

#ifndef __TRANSLATION_H__
#define __TRANSLATION_H__

#include "sparta/memory/AddressTypes.hpp"

namespace sparta
{
    namespace memory
    {
        /**
         * \class Translation
         *
         * Class that will contain results of an address translation
         */
        class Translation
        {
        public:

            /*!
             * \brief Default constructor.
             *
             * Constructs an invalid Translation object
             */
            Translation() :
                valid_(false),
                vaddr_(0),
                raddr_(0)
            { }

            /*!
             * \brief Create a Translation object containing a pair of
             * (pre-translation, post-translation) addresses.
             * \param addr Input address
             * \param out Translated address (from \a addr)
             */
            Translation(addr_t addr, addr_t out) :
                valid_(true),
                vaddr_(addr),
                raddr_(out)
            {}

            /*!
             * \brief Assignment operator (builtin)
             */
            Translation& operator=(const Translation& rhp) = default;

            bool isValid() const { return valid_; }

            /*!
             * \brief Returns output address of this translation
             */
            addr_t getOutputAddress() const {
                return raddr_;
            }

            addr_t getRealAddress() const {
                return raddr_;
            }

            addr_t getVirtualAddress() const {
                return vaddr_;
            }

            // ... TBD, other attributes

        private:
            bool valid_;
            addr_t vaddr_;
            addr_t raddr_;
        };
    }

}

#endif
