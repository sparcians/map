
#pragma once

#include <string.h>
#include "sparta/memory/BlockingMemoryIF.hpp"

namespace sparta {
    namespace memory {

        /**
         * \class DMIBlockingMemoryIF
         * \brief Class that provides a BlockingMemoryIF over a raw pointer.
         *
         * Instances of this class are returned from
         * BlockingMemoryIFNode::getDMI which allows a user to gain
         * "backdoor" access to memory in the sparta::ArchData memory
         * pool.  Think of a DMIBlockingMemoryIF instance as a "view"
         * into a segment of memory between start_addr and (start_addr
         * + size)
         *
         * The user of this class should be aware of the following
         * caveats:
         *
         * #. The DMIBlockingMemoryIF can be invalidated at any time.
         *    Invalidations could result from changes in mappings,
         *    permissions, etc.  Users that derive from
         *    BlockingMemoryIFNode and provide their own DMI memory
         *    access can invalidate at any time.
         *
         * #. Using the method `getRawDataPtr` is a means to improve
         *    simulation speed, but overruns are possible and not
         *    checked.  The suggestion to use read/write methods is
         *    strongly encouraged.
         *
         * #. The DMI's access window, if returned from
         *    BlockingMemoryIFNode, is soley based on the size of the
         *    sparta::memory::MemoryObject originally programmed in
         *    BlockingMemoryIFNode and not total memory.  For example,
         *    if the user creates sparta::memory::MemoryObject with
         *    block size of 64 bytes and a total of 1024 bytes of
         *    memory, a DMI object can _only_ access 64 bytes of
         *    memory at a time.
         *
         * #. Using a DMI will bypass read/write counts in
         *    BlockingMemoryIFNode showing fewer reads/writes than
         *    actually occurred
         *
         * #. Using a DMI will bypass pre/post read/write
         *    notifications.
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
            //! \throw SpartaException if the pointer is not valid
            //!
            //! This is dangerous to use directly as memory bounds
            //! checking can easily be bypassed
            void * getRawDataPtr() {
                sparta_assert(isValid(), "This DMI pointer is invalid " << this);
                return raw_pointer_;
            }

            //! \return true if the DMI is still valid to use
            bool isValid() const { return valid_; }

            //! \brief Typically called by the creator of the DMI Mem IF
            void clearValid() { valid_ = false; }

            /**
             * \brief Override of sparta::BlockingMemoryIF::tryRead
             * \return false if the DMI is not valid
             */
            bool tryRead(addr_t addr,
                         addr_t size,
                         uint8_t *buf,
                         const void *in_supplement=nullptr,
                         void *out_supplement=nullptr) override final
            {
                if(false == isValid()) { return false; }
                return BlockingMemoryIF::tryRead(addr, size, buf, in_supplement, out_supplement);
            }

            /**
             * \brief Override of sparta::BlockingMemoryIF::tryWrite
             * \return false if the DMI is not valid
             */
            bool tryWrite(addr_t addr,
                          addr_t size,
                          const uint8_t *buf,
                          const void *in_supplement=nullptr,
                          void *out_supplement=nullptr) override final
            {
                if(false == isValid()) { return false; }
                return BlockingMemoryIF::tryWrite(addr, size, buf, in_supplement, out_supplement);
            }

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
