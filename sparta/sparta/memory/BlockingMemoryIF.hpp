
/**
 * \file   BlockingMemoryIF.hpp
 * \brief  File that contains BlockingMemoryIF
 */

#ifndef __BLOCKING_MEMORY_IF_H__
#define __BLOCKING_MEMORY_IF_H__

#include "sparta/memory/MemoryExceptions.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/memory/DebugMemoryIF.hpp"
#include "sparta/memory/DMI.hpp"

namespace sparta
{
    namespace memory
    {
        class DMI_DEPRECATED;
        class TranslationIF;

        /*!
         * \brief Pure-virtual memory interface which represents a simple,
         * immediately accessible (blocking) address-space with meaningful read
         * and write accees support. Partially implements DebugMemoryIF, which
         * adds peek and poke support as well as access addr and size
         * validation.
         *
         * This interface and its subclasses \b must reject accesses where the
         * first and last address read are on different memory blocks (based on
         * the block size specified at construction)
         *
         * This interface operates on post-translated addresses from
         * the TranslationIF available through getTranslationIF
         *
         * Through DebugMemoryIF, this class is associated with, but does not
         * use a TranslationIF
         *
         * \note The memory behind this interface is not required to have a
         * contiguous representation in simulator host memory. This interface
         * makes no assumptions about storage.
         *
         * This interface does not support non-blocking accesses or access
         * attributes.
         *
         * Example
         * \code
         * using sparta::memory;
         * // BlockingMemoryIF* bmi;
         * // addr_t varrd;
         * // const uint8_t data[4];
         * // uint8_t buf[4];
         * addr_t paddr = bmi->getTranslationIF()->translate(vaddr);
         * bmi->write(paddr, 4, data);
         * bmi->read(paddr, 4, buf);
         * bmi->peek(paddr, 4, buf);
         * // Note: Translation is only required if the interface does not
         * // represent direct access to physical memory
         * \endcode
         */
        class BlockingMemoryIF : public sparta::memory::DebugMemoryIF
        {
        public:

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            BlockingMemoryIF() = delete;

            /*!
             * \brief Construct a blocking memory interface
             * \param desc Refer to DebugMemoryIF::DebugMemoryIF
             * \param block_size Refer to DebugMemoryIF::DebugMemoryIF
             * \param window Refer to DebugMemoryIF::DebugMemoryIF
             * \param transif Refer to DebugMemoryIF::DebugMemoryIF
             *
             * \todo Support vectors of windows with holes
             */
            BlockingMemoryIF(const std::string& desc,
                             addr_t block_size,
                             const DebugMemoryIF::AccessWindow& window,
                             TranslationIF* transif=nullptr) :
                DebugMemoryIF(desc, block_size, window, transif)
            { }

            virtual ~BlockingMemoryIF() {}

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Memory Access
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Attempt to read memory of size \a size at address \a addr.
             * \note Unless the underlying memory object can reject accesses
             * within the given access window for some reason and the caller
             * needs to be able to test this, the read function is preferred.
             * \param addr Post-translated address from which to read (see
             * getTranslationIF)
             * \param size Size of read (in bytes). \a addr and \a addr + \a
             * size must not land on different sides of a block boundary.
             * Must be > 0.
             * \param buf Buffer of data to populate with \a size bytes from memory
             * object. Content of \a buf is undefined if read fails
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \return true if read successful, false if not
             * \post \a buf will be populated with read results. Even if the read
             * was unsuccessful (returned false) the first \a size bytes may have
             * been modified. Other memory state may change if the memory
             * implementation has actions triggered on writes
             */
            virtual bool tryRead(addr_t addr,
                                 addr_t size,
                                 uint8_t *buf,
                                 const void *in_supplement=nullptr,
                                 void *out_supplement=nullptr) {
                if(__builtin_expect(doesAccessSpan(addr, size), 0)){
                    return false;
                    //throw MemoryReadError(addr, size, "addr is in a different block than addr+size");
                }
                if(__builtin_expect(isInAccessWindows(addr, size) == false, 0)){
                    return false;
                }

                return tryRead_(addr, size, buf, in_supplement, out_supplement);
            }

            /*!
             * \brief Attempts to read memory.
             * \param addr Post-translated address from which to read (see
             * getTranslationIF)
             * \param size Number of bytes to read into \a buf.
             * Note that \a addr and \a size cannot define an access which spans
             * multiple blocks
             * \param buf buffer whose content will be overwritten by \a size
             * bytes read from memory
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \throw SpartaException if the access is invalid as defined by this
             * interface or the underyling memory implementaiton
             * \post Memory state may change if memory implementation has
             * actions triggered on reads
             */
            void read(addr_t addr,
                      addr_t size,
                      uint8_t *buf,
                      const void *in_supplement=nullptr,
                      void *out_supplement=nullptr) {
                if(!tryRead(addr, size, buf, in_supplement, out_supplement)){
                    verifyNoBlockSpan(addr, size);
                    verifyInAccessWindows(addr, size);
                    throw MemoryReadError(addr, size, "Unknown reason");
                }
            }

            /*!
             * \brief Attempt to write memory of size \a size at address \a addr.
             * \note Unless the underlying memory object can reject accesses
             * within the given access window for some reason and the caller
             * needs to be able to test this, the write function is preferred.
             * \param addr Post-translated address to which to write (see
             * getTranslationIF)
             * \param size Size of write (in bytes). \a addr and \a addr + \a
             * size must not land on different sides of a block boundary.
             * Must be > 0.
             * \param buf Buffer of data to copy \a size bytes into memory object
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \return true if write successful, false if not
             * \post Memory state will reflect the bytes written.
             * \post Data in memory is not expected to be modified upon an
             * illegal write
             * If the write was unsuccessful, memory will not be modified.
             */
            virtual bool tryWrite(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf,
                                  const void *in_supplement=nullptr,
                                  void *out_supplement=nullptr) {
                if(__builtin_expect(doesAccessSpan(addr, size), 0)){
                    return false;
                    //throw MemoryWriteError(addr, size, "addr is in a different block than addr+size");
                }
                if(__builtin_expect(isInAccessWindows(addr, size) == false, 0)){
                    return false;
                }

                return tryWrite_(addr, size, buf, in_supplement, out_supplement);
            }

            /*!
             * \brief Attempts to write memory.
             * \param addr Post-translated address to which to write (see
             * getTranslationIF)
             * \param size Number of bytes to write from \a buf.
             * Note that \a addr and \a size cannot define an access which spans
             * multiple blocks
             * \param buf read-only buffer whose content should be written to
             * memory.
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \throw SpartaException if the access is invalid as defined by this
             * interface or the underyling memory implementaiton
             * \post Memory state will reflect the bytes written.
             * \post Data in memory is not expected to be modified upon an
             * failed write
             */
            void write(addr_t addr,
                       addr_t size,
                       const uint8_t *buf,
                       const void *in_supplement=nullptr,
                       void *out_supplement=nullptr) {
                if(!tryWrite(addr, size, buf, in_supplement, out_supplement)){
                    verifyNoBlockSpan(addr, size);
                    verifyInAccessWindows(addr, size);
                    throw MemoryReadError(addr, size, "Unkonwn reason");
                }
            }

            /**
             * \brief Return a DMI if possible. Default implementation
             * returns false for no dmi
             * \note implementations of getDMI should try to return the largest DMI possible.
             * The start pointer if possible can start below addr. We only require that addr
             * fall in the valid range of the DMI.
             * \param addr the post-translated address which is the start
             * of the DMI.
             * \param size the number of bytes you expect the dmi to span,
             * can be used for error checking that we get a dmi of the size we want.
             * \param dmi gets populated with the correct dmi.
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \return is dmi created valid
             */
            virtual bool getDMI_DEPRECATED(const addr_t addr,
                                           const addr_t size,
                                           DMI_DEPRECATED &dmi,
                                           const void *supplement = nullptr)
            {
                (void)addr;
                (void)size;
                (void)dmi;
                (void)supplement;
                return false;
            }

            /**
             * Returns a (possibly invalid) DMI
             *
             * \param addr A guest physical address that is to be accessed via DMI
             * \param callback Callback that is called when the DMI is invalidated
             * \param supplement caller-defined object identifying this memory access
             */
            virtual DMI getDMI(const addr_t addr,
                               const DMIInvalidationCallback &callback,
                               const void *supplement = nullptr)
            {
                (void)callback; (void) supplement;
                return DMI::makeInvalid(addr);
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            /*!
             * \brief Render description of this BlockingMemoryIF as a string
             */
            std::string stringize(bool pretty=false) const {
                (void) pretty;
                std::stringstream ss;
                ss << "<BlockingMemoryIF size:0x" << std::hex << total_range_ << " bytes>";
                return ss.str();
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
             * Subclasses must override this method
             *
             */
            virtual bool tryRead_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf,
                                  const void *in_supplement,
                                  void *out_supplement) = 0;

            /*!
             * \brief Implements tryWrite
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \throw Never throws. An implementation which throws is invalid
             *
             * Subclasses must override this method
             */
            virtual bool tryWrite_(addr_t addr,
                                   addr_t size,
                                   const uint8_t *buf,
                                   const void *in_supplement,
                                   void *out_supplement) = 0;

            /*!
             * \brief Override of DebugMemoryIF::tryPoke_ which forwards the call to tryWrite_
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \throw Never throws. An implementation which throws is invalid
             *
             * Subclasses must override this method
             */
            virtual bool tryPoke_(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf) override {
                return tryWrite_(addr, size, buf, nullptr, nullptr);
            }

            ////! \brief Implements isWriteValid
            //virtual bool isWriteValid_(addr_t addr, addr_t size) const noexcept = 0;
            //
            ////! \brief Implements isReadValid
            //virtual bool isReadValid_(addr_t addr, addr_t size) const noexcept = 0;

            ////////////////////////////////////////////////////////////////////////
            //! @}


        }; // class BlockingMemoryIF
    }; // namespace memory
}; // namespace sparta


//! \brief BlockingMemoryIF stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::BlockingMemoryIF& mi) {
    out << mi.stringize();
    return out;
}

//! \brief BlockingMemoryIF stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::BlockingMemoryIF* mi) {
    if(nullptr == mi){
        out << "null";
    }else{
        out << mi->stringize();
    }
    return out;
}

#endif // __BLOCKING_MEMORY_IF_H__
