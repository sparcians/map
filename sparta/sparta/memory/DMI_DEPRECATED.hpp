
/**
 * \file DMI_DEPRECATED.hpp
 * \brief Define sparta::DMI_DEPRECATED
 */

#ifndef __SPARTA_DMI_DEPRECATED_H__
#define __SPARTA_DMI_DEPRECATED_H__

//#define SAFE_DMI 1

#include <cstdio>
#include "sparta/memory/BlockingMemoryIF.hpp"
#include "sparta/memory/AddressTypes.hpp"

namespace sparta {
    namespace memory {
        /*!
         * \class DMI_DEPRECATED
         * \brief Defines a DMI which can be used as a fast memory
         * interface that writes directly to a raw pointer.
         *
         * This class implements BlockingMemoryIF
         */
        class DMI_DEPRECATED : public BlockingMemoryIF
        {
            //! Copy constructors are not allowed.
            DMI_DEPRECATED& operator=(const DMI_DEPRECATED&) = delete;
            DMI_DEPRECATED (const DMI_DEPRECATED&) = delete;
        public:

            /*! \brief Just construct the DMI
             * \param start_memory the pointer to the raw memory location.
             * \param size the length this raw memory spans.
             */
            DMI_DEPRECATED(addr_t addr, const addr_t size) :
                BlockingMemoryIF("DMI", size, DebugMemoryIF::AccessWindow(addr, addr + size)),
                addr_(addr),
                size_(size)
            {}

            virtual ~DMI_DEPRECATED() = default;

            /*!
             * \brief must set the dmi pointer at least once. The dmi is invalid until this has been executed.
             */
            void set(void* start_memory)
            {
                data_ = (uint8_t*)start_memory;
            }

            /*!
             * \brief determine if an address is valid for this DMI.
             */
            bool inRange(addr_t addr, const addr_t size = 0) const
            {
                return (addr >= addr_) && (addr + size < (addr_ + size_));
            }

            /*!
             * \brief return the size of dmi from the start addr.
             */
            addr_t getSize() const
            {
                return size_;
            }

            /*!
             * \brief Return the start address of the DMIs range.
             */
            addr_t getAddr() const
            {
                return addr_;
            }

        private:
            /*!
             * \brief Find the offset to read/write from
             * \param addr the post translated address we want to read/write from.
             * \return the direct pointer for addr
             */
            uint8_t* calcAddr_(const addr_t addr) const
            {
                // DMI is generally in the absolute most critical path. So if we can ignore safety checking, we should.
#ifdef SAFE_DMI
                sparta_assert(data_ != nullptr);
                sparta_assert(inRange(addr));
#endif
                return data_ + (addr - addr_);
            }

        protected:

            //! \name Access and Query Implementations
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Implements tryRead
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \throw Never throws. An implementation which throws is invalid
             *
             *
             */
            virtual bool tryRead_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf,
                                  const void *,
                                  void *) noexcept
            {
                memcpy(buf, calcAddr_(addr), size);
                return true;
            }

            /*!
             * \brief Implements tryWrite
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \throw Never throws. An implementation which throws is invalid
             *
             */
            virtual bool tryWrite_(addr_t addr,
                                   addr_t size,
                                   const uint8_t *buf,
                                   const void *,
                                   void *) noexcept
            {
                memcpy(calcAddr_(addr), buf, size);
                return true;
            }

            /*!
             * \brief Override of DebugMemoryIF::tryPoke_ which forwards the call to tryWrite_
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \throw Never throws. An implementation which throws is invalid
             *
             */
            virtual bool tryPoke_(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf) noexcept
            {
                memcpy(calcAddr_(addr), buf, size);
                return true;
            }

            //! Override DebugMemoryIF::tryPeek_
            virtual bool tryPeek_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf) const noexcept
            {
                memcpy(buf, calcAddr_(addr), size);
                return true;
            }


        private:
            uint8_t* data_ = nullptr;
            const addr_t addr_;
            const uint32_t size_;

        };
    } // namespace memory
} // namespace sparta

#endif //__SPARTA_DMI_DEPRECATED_H__
