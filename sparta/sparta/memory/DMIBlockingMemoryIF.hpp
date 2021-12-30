
#pragma once

#include <string.h>
#include "sparta/memory/BlockingMemoryIF.hpp"

namespace sparta {
    namespace memory {

        /**
         * \class DMIBlockingMemoryIF
         * \brief Class that provides DMI access to backend memory
         */
        class DMIBlockingMemoryIF final : public BlockingMemoryIF
        {
        public:
            /**
             * \brief Wraps a raw pointer and provides BlockingMemoryIF to it
             * \param raw_pointer The raw data pointer with memory to access
             * \param start_addr  The expected "start address" of this raw pointer
             * \param size        The expected size of the data
             *
             * Typically created by BlockingMemoryObjectIFNode when
             * DMI request is made, this class will wrap a raw data
             * pointer and provide the functionality of the
             * BlockingMemoryIF.  This includes window access checking
             * and bounds checking for the raw pointer.
             *
             * Note that memory observeration is _completely bypassed_
             * when using a DMI interface.
             */
            DMIBlockingMemoryIF(void * raw_pointer,
                                addr_t start_addr,
                                addr_t size) :
                BlockingMemoryIF("DMI", size, {start_addr, start_addr + size}),
                start_addr_(start_addr),
                raw_pointer_(raw_pointer)
            { }

            //! \return The internal raw pointer.
            //!
            //! This is dangerous to use directly as memory bounds
            //! checking can easily be bypassed
            void * getRawDataPtr() { return raw_pointer_; }

            //! \return true if the DMI is still valid to use
            bool isValid() const { return valid_; }

            //! \brief Typically called by the creator of the DMI Mem IF
            void clearValid() { valid_ = false; }

        private:

            //! Try to disallow this from being called
            DMIBlockingMemoryIF * getDMI(addr_t, addr_t) override final {
                sparta_assert(false, "You cannot get a DMI interface from a DMI interface!");
                return nullptr;
            }

            void *computeHostAddress_(const addr_t addr) const
            {
                return (uint8_t *)raw_pointer_ + (addr - start_addr_);
            }

            // When this method is called, access windowing is alraedy
            // checked as well as address spanning
            bool tryRead_(addr_t addr,
                          addr_t size,
                          uint8_t *buf,
                          const void *,
                          void *) noexcept override
            {
                ::memcpy(buf, computeHostAddress_(addr), size);
                return true;
            }

            // When this method is called, access windowing is alraedy
            // checked as well as address spanning
            bool tryWrite_(addr_t addr,
                           addr_t size,
                           const uint8_t *buf,
                           const void *,
                           void *) noexcept override
            {
                ::memcpy(computeHostAddress_(addr), buf, size);
                return true;
            }

            // When this method is called, access windowing is alraedy
            // checked as well as address spanning
            bool tryPeek_(addr_t addr, addr_t size, uint8_t *buf) const noexcept override
            {
                ::memcpy(buf, computeHostAddress_(addr), size);
                return true;
            }

            // When this method is called, access windowing is alraedy
            // checked as well as address spanning
            bool tryPoke_(addr_t addr, addr_t size, const uint8_t *buf) noexcept override
            {
                ::memcpy(computeHostAddress_(addr), buf, size);
                return true;
            }

            addr_t start_addr_;
            void * raw_pointer_ = nullptr;
            bool valid_ = true;
        };

    } // namespace memory
} // namespace sparta
