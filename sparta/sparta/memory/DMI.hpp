
#pragma once

#include <vector>
#include <functional>

#include "sparta/memory/AddressTypes.hpp"

namespace sparta {
    namespace memory {

        /**
         * \class DMI
         * \brief Represents a memory range that can be accessed via DMI
         */
        class DMI
        {
        public:
            /**
             * \brief Construct an invalid DMI
             *
             * Some users of this DMI API maintain a list of memory
             * regions that do not support DMI. When requesting a DMI
             * for a memory address in a region that does not support
             * DMI they need to know the address and size of the
             * region so they can track which memory regions do not
             * support DMI. Ideally the largest region including the
             * requested address should be returned. This is not
             * always possible. Sometimes the size is not known, in
             * those cases the best we can do is to report the size to
             * be one byte.
             */
            DMI(const addr_t addr, const addr_t size = 1)
                : DMI(nullptr, addr, size, false, false)
            { }

            /**
             * \brief Construct a DMI pointer
             *
             * \param dmi_ptr Pointer to raw memory
             * \param addr Starting adddress of this DMI region
             * \param size The size this DMI support
             */
            DMI(void *dmi_ptr, const addr_t addr, const addr_t size)
                : DMI(dmi_ptr, addr, size, true, true)
            { }

            /**
             * \brief Construct a DMI pointer
             *
             * \param dmi_ptr Pointer to raw memory
             * \param addr Starting adddress of this DMI region
             * \param size The size this DMI support
             * \param read_permission Does this region have read perms?
             * \param write_permission Does this region have write perms?
             */
            DMI(void *dmi_ptr,
                const addr_t addr,
                const addr_t size,
                const bool read_permission,
                const bool write_permission) :
                dmi_ptr_(dmi_ptr), addr_(addr), size_(size),
                read_permission_(read_permission),
                write_persmission_(write_permission)
            {}

            //! \return The raw memory pointer
            void *getRawPtr() const { return dmi_ptr_; }

            //! \return The starting address
            addr_t getAddr() const { return addr_; }

            //! \return The size of this DMI access
            addr_t getSize() const { return size_; }

            //! \return The read permissions
            bool hasReadPermission() const { return read_permission_; }

            //! \return The write permissions
            bool hasWritePermission() const { return write_persmission_; }

            //! \return Is this DMI valid to use?
            bool isValid() const { return (dmi_ptr_ != nullptr); }

            /**
             * \brief Is the given address and size in range?
             *
             * \param addr The address to check
             * \param size The size
             * \return true if in ranage; false otherwise
             */
            bool inRange(const addr_t addr, const addr_t size = 0) const {
                return (getAddr() <= addr) && (addr + size < (getAddr() + getSize()));
            }

        private:
            /** Pointer to backing storage for the memory range covered by this DMI */
            void *const dmi_ptr_ = nullptr;

            /** Guest physical address of the memory range covered by this DMI */
            const addr_t addr_ = 0;

            /** Size in bytes of the of the memory range covered by this DMI */
            const addr_t size_ = 0;

            /** True if the memory range covered by this DMI can be read */
            const bool read_permission_ = true;

            /** True if the memory range covered by this DMI can be written */
            const bool write_persmission_ = true;

            /** True if this DMI pointer is valid */
            const bool valid_ = true;
        };

        static bool operator==(const DMI &rhs, const DMI &lhs)
        {
            auto equal = true;

            equal = equal && (lhs.isValid() == rhs.isValid());
            equal = equal && (lhs.getRawPtr() == rhs.getRawPtr());
            equal = equal && (lhs.getSize() == rhs.getSize());
            equal = equal && (lhs.hasReadPermission() == rhs.hasReadPermission());
            equal = equal && (lhs.hasWritePermission() == rhs.hasWritePermission());

            return equal;
        }

        typedef std::function<void(const DMI&)> DMIInvalidationCallback;

    } // namespace memory
} // namespace sparta
