
/**
 * \file   TranslationInterface.hpp
 *
 * \brief  File that defines translation interface
 */

#ifndef __TRANSLATION_INTERFACE_H__
#define __TRANSLATION_INTERFACE_H__

#include "sparta/memory/AddressTypes.hpp"
#include "sparta/memory/Translation.hpp"
#include "sparta/memory/MemoryExceptions.hpp"

namespace sparta
{
    namespace memory
    {
        /**
         * \brief Blocking translation interface with 1:1 translation unless
         * subclassed
         *
         * Class that will perform 1-directional address translations from some
         * arbitrary input address space to some output.
         *
         * This is noncopyable and nonassignable
         *
         * This class is NOT pure-virtual and has a default implementation
         * which will return the input address when a translation is performed.
         */
        class TranslationIF
        {
        public:

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            TranslationIF& operator=(const TranslationIF&) = delete;
            TranslationIF(const TranslationIF&) = delete;

            /*!
             * \brief Constructor
             * \param input_type Name of input memory address type
             * \param output_type Name of output memory address type
             */
            TranslationIF(const std::string& input_type,
                          const std::string& output_type) :
                input_type_(input_type),
                output_type_(output_type)
            { }

            /*!
             * \brief Default Constructor
             * \param input_type Name of input memory address type
             * \param output_type Name of output memory address type
             */
            TranslationIF() :
                TranslationIF("virtual", "physical")
            { }

            /*!
             * \brief Virtual Destructor
             */
            virtual ~TranslationIF() {}

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Debug Memory Access
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief gets the name of the input address type (e.g. virtual)
             */
            const std::string& getInputType() const { return input_type_; }

            /*!
             * \brief gets the name of the output address type (e.g. physical)
             */
            const std::string& getOutputType() const { return output_type_; }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Translation
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Perform a translation from input-type to output-type.
             * \param addr Address to translate from input-type to output-type
             * \param trans Optional translation attributes. Can be null
             * [default] to ignore this parameter
             * \return true if the tranlation succeeded, false if it failed
             * \throw MemoryTranslationError
             */
            bool tryTranslate(const addr_t& addr, Translation* trans=nullptr) const {
                Translation local_trans;
                Translation& t(trans!=nullptr ? (*trans) : local_trans);
                if(!tryTranslate_(addr, t)){
                    // Determine failure cause.
                    // Check if failure is caused by out-of-range or block-spanning
                    // If not, why???
                    throw MemoryTranslationError(addr, "Cannot translate");
                }
                return true;
            }

            /*!
             * \brief Perform a translation from input-type to output-type.
             * \param addr Address to translate from input-type to output-type
             * \param trans Optional translation attributes. Can be null
             * [default] to ignore this parameter
             * \return address result of the translation (if succeeded)
             * \throw MemoryTranslationError
             */
            addr_t translate(const addr_t& addr, Translation* trans=nullptr) const {
                Translation local_trans;
                Translation& t(trans!=nullptr ? (*trans) : local_trans);

                if(!tryTranslate_(addr, t)){
                    //! \todo Determine failure cause.
                    // Check if failure is caused by out-of-range or block-spanning
                    // If not, why???
                    throw MemoryTranslationError(addr, "Cannot translate");
                }
                return t.getOutputAddress();
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

        private:

            /*!
             * \brief Translator implementation
             *
             * Subclass must implement this to support translation with
             * attributes. Without overriding, interface succeeds for all
             * translations and the output address equals the input address.
             */
            virtual bool tryTranslate_(const addr_t& addr, Translation& trans) const noexcept {
                trans = Translation(addr, addr);
                return true;
            }

            const std::string input_type_; //!< Name of input address-type
            const std::string output_type_; //!< Name of output (translated) address-type

        };
    } // namespace memory
} // namespace sparta

#endif // __TRANSLATION_INTERFACE_H__
