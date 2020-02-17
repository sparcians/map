
/**
 * \file   MemoryExceptions.hpp
 * \brief  File that contains some exception types related to memory interfaces
 */

#ifndef __MEMORY_EXCEPTIONS_H__
#define __MEMORY_EXCEPTIONS_H__

#include "sparta/utils/SpartaException.hpp"
#include "sparta/memory/AddressTypes.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Indicates that there was an issue translating an address in
         * the SPARTA framework.
         *
         * This is intended to communicate to a (typically external) client of
         * the framework that a translation was not possible (e.g. out of
         * memory bounds, bad alignment, no translation, etc.)
         */
        class MemoryTranslationError : public SpartaException
        {
        public:
            MemoryTranslationError(addr_t addr,
                                   const std::string& why) :
                SpartaException(((std::stringstream&)(std::stringstream() << "Invalid translation from " << std::hex
                                                    << addr << " : " << why)).str())
            { }
        };

        /*!
         * \brief Indicates that there was an issue accessing a SPARTA memory
         * object or interface.
         *
         * This is intended to communicate to a (typically external) client of
         * the framework that an access was not allowed (e.g. out of bounds, bad
         * alignment, spans blocks, no translation, etc.)
         */
        class MemoryAccessError : public SpartaException
        {
        public:
            MemoryAccessError(addr_t addr,
                              addr_t size,
                              const std::string& access,
                              const std::string& why) :
                SpartaException(((std::stringstream&)(std::stringstream() << "Invalid " << access << " access at 0x" << std::hex
                                                    << addr << " of size: " << std::dec << size << ": " << why)).str())
            { }
        };

        /*!
         * \brief Error while attempting to read some memory object or interface
         */
        class MemoryReadError : public MemoryAccessError
        {
        public:
            MemoryReadError(addr_t addr,
                            addr_t size,
                            const std::string& why) :
                MemoryAccessError(addr, size, "read", why)
            { }
        };

        /*!
         * \brief Error while attempting to write some memory object or interface
         */
        class MemoryWriteError : public MemoryAccessError
        {
        public:
            MemoryWriteError(addr_t addr,
                             addr_t size,
                             const std::string& why) :
                MemoryAccessError(addr, size, "write", why)
            { }
        };

        /*!
         * \brief Error while attempting to peek some memory object or interface
         */
        class MemoryPeekError : public MemoryAccessError
        {
        public:
            MemoryPeekError(addr_t addr,
                            addr_t size,
                            const std::string& why) :
                MemoryAccessError(addr, size, "peek", why)
            { }
        };

        /*!
         * \brief Error while attempting to poke some memory object or interface
         */
        class MemoryPokeError : public MemoryAccessError
        {
        public:
            MemoryPokeError(addr_t addr,
                            addr_t size,
                            const std::string& why) :
                MemoryAccessError(addr, size, "poke", why)
            { }
        };

    }; // namespace memory
}; // namespace sparta

#endif // __MEMORY_EXCEPTIONS_H__
