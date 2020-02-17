
/**
 * \file   DebugMemoryIF.hpp
 * \brief  File that contains DebugMemoryIF
 */

#ifndef __DEBUG_MEMORY_IF_H__
#define __DEBUG_MEMORY_IF_H__

#include <math.h>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/memory/MemoryExceptions.hpp"
#include "sparta/memory/TranslationIF.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/utils/Utils.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Memory interface which represents a simple, immediately
         * accessible (blocking) address-space with support for peek and poke
         * acceeses having no side effects other than changing the desired
         * memory.
         *
         * This interface does not support non-blocking accesses or access
         * attributes.
         *
         * This interface operates on post-translated addresses from
         * the TranslationIF available through getTranslationIF
         *
         * The object implementing this interface can perform any translation
         * or logic to satisfy memory accesses.
         *
         * Peek and Poke accesses within this interface are automatically
         * chunked into block-constrained accesses and re-assembled.
         *
         * This class is intended to be an innocuous interface to some kind of
         * "memory" from the perspective of some simulation unit with which this
         * interface is associated. For example, if attached to a core this
         * interface would most appropriately present the software view of
         * virtual memory. Note that data and instruction memory views from a
         * core may need to use different debugging interfaces. It is possible
         * different interfaces could even be provided for each address-space
         * avaialable per data and instruction depending on that core's mmu.
         *
         * Example
         * \code
         * using sparta::memory;
         * // DebugMemoryIF* dbgmi;
         * // addr_t varrd;
         * // const uint8_t data[4];
         * // uint8_t buf[4];
         * addr_t paddr = dbgmi->getTranslationIF()->translate(vaddr);
         * dbgmi->poke(paddr, 4, data);
         * dbgmi->peek(paddr, 4, buf);
         * // Note: Translation is only required if the interface does not
         * // represent direct access to physical memory
         * \endcode
         */
        class DebugMemoryIF
        {
        public:

            //! \name Types
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Defines an access window within this interface. Accesses
             * through a memory interface are constrained to this window.
             *
             * Non-assignable
             */
            struct AccessWindow {

                AccessWindow() = delete;
                AccessWindow& operator=(const AccessWindow&) = delete;

                /*!
                 * \brief Default copy constructor
                 */
                AccessWindow(const AccessWindow&) = default;

                /*!
                 * \brief Construct address window with range
                 * \param _start Start address of the window in this interface's
                 * post-translated addresses (inclusive)
                 * \param _end End address of the window in this interface's
                 * post-translated addresses (exclusive)
                 * \note start must be < end and both addresses must be
                 * block-aligned within whatever interface the window is
                 * contained
                 */
                AccessWindow(addr_t _start,
                             addr_t _end,
                             const std::string& _name="default") :
                    start(_start), end(_end), name(_name)
                {
                    if(start >= end){
                        throw SpartaException("Cannot construct a Memory AccessWindow \"")
                            << name << "\"where start address ("
                            << std::hex << start << ") >= end address (" << end << ")";
                    }
                }

                /*!
                 * \brief Does this window interval contain the specified
                 * post-translated address \a addr.
                 */
                bool containsAddr(addr_t addr) const noexcept {
                    return addr >= start && addr < end;
                }

                /*!
                 * \brief inclusive start address (block-aligned)
                 */
                const addr_t start;

                /*!
                 * \brief exclusive end address (block-aligned)
                 */
                const addr_t end;

                /*!
                 * \brief What is this window called (for printouts)
                 */
                const std::string name;
            };

            ////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Default constuctor
             */
            DebugMemoryIF() = delete;

            /*!
             * \brief Construct a DebugMemoryInterface
             * \param desc Description of this interface. What is this an
             * interface for and what is the perspective? For example,
             * "core virtual data memory". This exists so that errors generated
             * by this code can indicate which interface the error came from
             * \param block_size Size of a block in this interface. Must be a
             * power of 2 and greater than 0.
             * \param window AccessWindow defining valid range of addresses.
             * Must be block-aligned. These are post-translated addresses ready
             * to be used in access methods
             * \param transif Optional translation interface associated with
             * this memory interface. This must be specified, is managed
             * externally, and must exist for the lifetime of this interface
             * \throw SpartaException if \a block_size or \a window is invalid
             * \todo Support vectors of windows with holes
             */
            DebugMemoryIF(const std::string& desc,
                          addr_t block_size,
                          const AccessWindow& window,
                          TranslationIF* transif=nullptr) :
                block_size_(block_size),
                block_mask_(0),
                block_idx_lsb_(0),
                acc_windows_({window}),
                trans_(transif),
                desc_ptr_(StringManager::getStringManager().internString(desc))
                // total_range_, low_end_, high_end_, and accessible_size_ are initialized in body
            {
                // Validate block_size
                if(block_size_ == 0){
                    throw SpartaException("0 block size specified for DebugMemoryIF: ")
                        << desc;
                }

                if(isPowerOf2(block_size_) == false){
                    throw SpartaException("block size (")
                        << block_size_ << ") specified is not a power of 2 for DebugMemoryIF: "
                        << desc;
                }

                // Compute block mask / idx_lsb
                block_idx_lsb_ = (addr_t)log2(block_size);
                block_mask_ = ~(((addr_t)1 << block_idx_lsb_) - 1);
                sparta_assert(((addr_t)1 << block_idx_lsb_) == block_size_);

                // Validate access windows

                if(acc_windows_.size() == 0){
                    throw SpartaException("0 access windows specified for DebugMemoryIF: ")
                        << desc;
                }

                //! \todo Check for access window overlaps (once multiple windows are supported)

                low_end_ = acc_windows_[0].start;
                high_end_ = acc_windows_[0].end;

                // Check window block alignment
                for(const auto& win : acc_windows_){
                    if(win.start % block_size != 0){
                        throw SpartaException("Memory AccessWindow start address was not block-size (")
                            << block_size << ") aligned. Was " << std::hex << win.start;
                    }
                    if(win.end % block_size != 0){
                        throw SpartaException("Memory AccessWindow end address was not block-size (")
                            << block_size << ") aligned. Was " << std::hex << win.end;
                    }

                    if(win.start < low_end_){
                        low_end_ = win.start;
                    }
                    if(win.end > high_end_){
                        high_end_ = win.end;
                    }
                }

                sparta_assert(acc_windows_.size() == 1); // Assumption for the following code
                //! \todo Create lookup map for identifying holes if more than 1 window is allowed
                total_range_ = acc_windows_[acc_windows_.size()-1].end - acc_windows_[0].start;
                accessible_size_ = total_range_; // For a single window
            }

            /*!
             * \brief Virutal destructor
             */
            virtual ~DebugMemoryIF() {}

            ////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Translation Information
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Gets the translation interface associated with this Debug
             * memory interface (if any).
             *
             * This translation interface, if not nullptr, is intended to be
             * used by a client of this class to translate addresses from some
             * external space into a space suitable for use in memory accesses
             * through this interface.
             *
             * The DebugMemoryIF does not use this translation interface. This
             * association is present as a hint for toos and UIs.
             */
            virtual const TranslationIF* getTranslationIF() {
                return trans_;
            }

            ////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Interface Attributes
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Returns the description specified at construction
             */
            const std::string& getDescription() { return *desc_ptr_; };

            /*!
             * \brief Returns the block size of memory represented by this
             * interface. Read and write accesses must not span block
             * boundaries (where addr % block_size == 0).
             */
            addr_t getBlockSize() const { return block_size_; }

            /*!
             * \brief Gets the total span of this interface's valid address
             * range.
             *
             * This is: 1 + the highest accessible address - the lowest
             * accessible address.
             */
            addr_t getRange() const { return total_range_; }

            /*!
             * \brief Gets the lowest address accessible
             */
            addr_t getLowEnd() const { return low_end_; }

            /*!
             * \brief Gets the highest address accessible + 1
             */
            addr_t getHighEnd() const { return high_end_; }

            /*!
             * \brief Gets the total accessible size of this interface's valid
             * addresses within the total size (getRange) excluding holes between
             * access windows
             */
            addr_t getAccessibleSize() const { return accessible_size_; } // sum of all window sizes

            /*!
             * \brief Gets the vector of windows representing this interface.
             *
             * These windows define the post-translated access space for this
             * interface
             */
            const std::vector<AccessWindow>& getWindows() const {
                return acc_windows_;
            }

            ////////////////////////////////////////////////////////////////////
            //! @}

            //! \name General Queries
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Determines if the given address is in an access window
             * defined for this interface.
             * \return true if in some window, false otherwise
             * \warning This is not a high-performance method.
             * \see verifyInAccessWindows_
             */
            bool isAddressInWindows(addr_t addr) const noexcept {
                for(const auto& win : acc_windows_){
                    if(win.containsAddr(addr)){
                        return true;
                    }
                }
                return false;
            }

            /*!
             * \brief Verifies that the range [addr, addr+size) is within the
             * access windows for this interface
             * \param addr Post-translated address to which to test (see
             * getTranslationIF)
             * \param size Size of access to test
             * \throw MemoryAccessError if the access is not entirely contained
             * in access windows.
             */
            void verifyInAccessWindows(addr_t addr, addr_t size) const {
                // Assumes 1 access window
                sparta_assert(acc_windows_.size() == 1); // This code is invalid once more than 1 window is supported
                if(__builtin_expect(addr < acc_windows_[0].start || addr+size > acc_windows_[0].end, 0)){
                    MemoryAccessError ex(addr, size, "any", "The access at does not fit within access windows: ");
                    ex << "{ [" << acc_windows_[0].start << "," << acc_windows_[0].end << ") }";
                    throw ex;
                }
            }

            /*!
             * \brief Determines if the range [addr, addr+size) is within the
             * access windows for this interface.
             * \param addr Post-translated address to test (see
             * getTranslationIF)
             * \param size Size of access to test
             * \return false if access does not fall entirely within access
             * windows of this interface
             */
            bool isInAccessWindows(addr_t addr, addr_t size) const {
                // Assumes 1 access window
                sparta_assert(acc_windows_.size() == 1); // This code is invalid once more than 1 window is supported
                return addr >= acc_windows_[0].start && addr+size <= acc_windows_[0].end;
            }

            /*!
             * \brief Verifies that the given address does not span block
             * boundaries defined for this interface
             * \param addr Post-translated address to test (see
             * getTranslationIF)
             * \param size Size of access to test
             * \throw MemoryAccessError if  true if access spans a block boundary, false otherwise
             */
            void verifyNoBlockSpan(addr_t addr, addr_t size) const {
                sparta_assert(size > 0);
                if(__builtin_expect(doesAccessSpan(addr,size), 0)){
                    throw MemoryAccessError(addr, size, "any", "The access spans blocks");
                }
            }

            /*!
             * \brief Determines if the given address spans block boundaries
             * defined for this interface. Accesses which span blocks are
             * illegal for read/write accesses, but allowed for peak/poke debug
             * accesses
             * \param addr Post-translated address to test (see
             * getTranslationIF)
             * \param size Size of access to test
             * \return true if access spans a block boundary, false otherwise
             */
            bool doesAccessSpan(addr_t addr, addr_t size) const noexcept {
                return (addr & block_mask_) != ((addr + size - 1) & block_mask_);
            }

            ////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Debug Memory Access
            //! @{
            ////////////////////////////////////////////////////////////////////

            /*!
             * \brief Attempts to 'peek' memory without having any side effects,
             * size-limitations, alignment constraints except that all bytes
             * peeked are inside an access window for this interface.
             * \param addr Post-translated address from which to peek (see
             * getTranslationIF)
             * \param size Number of bytes to read from \a buf. Note that \a
             * addr and \a size can span block boundaries in a peek/poke method
             * \param buf Buffer to which \a size peeked bytes will be copied if
             * the peek was legal. Content of \a buf is undefined if peek is
             * illegal
             * \return true if the peek is legal and false if not. It is up to
             * the caller to determine if the \a addr and \a size are legal
             * values for this interface (i.e. in an access window and not
             * spanning a block boundary). Further restriction on alignment may
             * be imposed by the implementation but generally should not be.
             * \post Has NO side-effects on simulation. Simply gets data if
             * possible.
             * \note Peek is a const-qualified method and is thus allowed in
             * const instances of DebugMemoryIF to provide clients with
             * read-onlydebug access to memory
             * \note Peeking has no performance requirements and may be slower
             * than read.
             *
             * Peeking is intended as a debugger/tool interface to the
             * simulation
             */
            bool tryPeek(addr_t addr,
                         addr_t size,
                         uint8_t *buf) const {
                if(false == __builtin_expect(isInAccessWindows(addr, size), 0)){
                    return false;
                }

                uint8_t* out = buf;
                addr_t a = addr;
                const addr_t end = addr + size;
                while(1){
                    addr_t block_end = block_mask_ & (a + block_size_);
                    if(block_end >= end){
                        return tryPeek_(a, end - a, out);
                    }

                    addr_t size_in_block = block_end - a;
                    if(!tryPeek_(a, size_in_block, out)){
                        return false;
                    }
                    out += size_in_block;
                    a = block_end;
                }
            }

            /*!
             * \brief Wrapper on tryPeek which throws a MemoryAccessError if the
             * peek is not legal
             */
            void peek(addr_t addr,
                      addr_t size,
                      uint8_t *buf) const {
                if(!tryPeek(addr, size, buf)){
                    verifyInAccessWindows(addr, size);
                    throw MemoryPeekError(addr, size, "Cannot peek memory");
                }
            }

            /*!
             * \brief Attempts to 'poke' memory without having any side effects
             * other than changing the bytes within the range
             * [ \a addr , \a addr + \a size ). Poke has no size-limitations or
             * alignment constraints except that all bytes peeked are inside an
             * access window for this interface.
             * \param addr Post-translated address to which to poke (see
             * getTranslationIF)
             * \param size Number of bytes to write from \a buf.
             * Note that \a addr and \a size can span block boundaries in a
             * peek/poke method
             * \param buf read-only buffer whose content should be written to
             * memory.
             * \return true if the poke is legal and false if not. It is up to
             * the caller to determine if the \a addr and \a size are legal
             * values for this interface (i.e. in an access window and not
             * spanning a block boundary). Further restriction on alignment may
             * be imposed by the implementation but generally should not be.
             * \post Memory state will reflect the bytes poked.
             * \post Data in memory is not expected to be modified upon an
             * illegal poke
             * \post Has NO side-effects on simulation but can modify memory
             * storage. Has the effect of simply replacing the specified bytes
             * in the memory model storage. The model storage internal data
             * structures (e.g. sprarse mapping, checkpointing helpers, etc) can
             * be modified to support a poke (i.e. a new block needed
             * allocation) if it is required to store the poked data.
             * \note Poking has no performance requirements and may be slower
             * than write.
             *
             * Poking is intended as a debugger/tool interface to the simulation
             */
            bool tryPoke(addr_t addr,
                         addr_t size,
                         const uint8_t *buf) {
                if(false == __builtin_expect(isInAccessWindows(addr, size), 0)){
                    return false;
                }

                const uint8_t* in = buf;
                addr_t a = addr;
                const addr_t end = addr + size;
                while(1){
                    addr_t block_end = block_mask_ & (a + block_size_);
                    if(block_end >= end){
                        return tryPoke_(a, end - a, in);
                    }

                    addr_t size_in_block = block_end - a;
                    if(!tryPoke_(a, size_in_block, in)){
                        return false;
                    }
                    in += size_in_block;
                    a = block_end;
                }
            }

            /*!
             * \brief Wrapper on tryPoke which throws a MemoryAccessError if the
             * poke is not legal
             */
            void poke(addr_t addr,
                      addr_t size,
                      const uint8_t *buf) {
                if(!tryPoke(addr, size, buf)){
                    verifyInAccessWindows(addr, size);
                    throw MemoryPokeError(addr, size, "Cannot poke memory");
                }
            }

            ////////////////////////////////////////////////////////////////////
            //! @}

        protected:

            /*!
             * \brief Size of a block accessed through this interface
             */
            const addr_t block_size_;

            /*!
             * \brief Mask applied to an address to get only bits representing
             * the block ID.
             */
            addr_t block_mask_;

            /*!
             * \brief rshift applied to an address to get the block ID
             */
            addr_t block_idx_lsb_;

            /*!
             * \brief Vector of access windows representing this memory
             */
            const std::vector<AccessWindow> acc_windows_;

            /*!
             * \brief Translation interface created for this interface.
             * Externally owned
             */
            TranslationIF* trans_;

            /*!
             * \brief Description pointer
             */
            const std::string* const desc_ptr_;

            /*!
             * \brief Range of addresses from highest accessible to lowest
             */
            addr_t total_range_;

            /*!
             * \brief Lowest accessible address
             */
            addr_t low_end_;

            /*!
             * \brief Highest accessible address + 1
             */
            addr_t high_end_;

            /*!
             * \brief Number of bytes accessible through this interface
             */
            addr_t accessible_size_;


        private:

            /*!
             * \brief Implements tryPeek
             * \note Accesses are always within a single block. tryPeek will
             * divide them.
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \note const-qualified
             * \note Has no performance requrement. Implementations should try
             * and be reasonably fast (i.e. do not block on disk accesses), but
             * this method is not considered performance critical
             * \throw Never throws. An implementation which throws is invalid
             *
             * Subclasses must override this method
             */
            virtual bool tryPeek_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf) const = 0;

            /*!
             * \brief Implements tryPoke
             * \note Accesses are always within a single block. tryPeek will
             * divide them.
             * \note Arguments addr and size are guaranteed to be within access
             * window
             * \note Has no performance requrement. Implementations should try
             * and be reasonably fast (i.e. do not block on disk accesses), but
             * this method is not considered performance critical
             * \throw Never throws. An implementation which throws is invalid
             *
             * Subclasses must override this method
             */
            virtual bool tryPoke_(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf) = 0;

        }; // class DebugMemoryIF
    }; // namespace memory
}; // namespace sparta

#endif // __DEBUG_MEMORY_IF_H__
