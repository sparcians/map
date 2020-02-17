
/**
 * \file   MemoryObject.hpp
 * \brief  File that contains MemoryObject
 */

#ifndef __MEMORY_OBJECT_H__
#define __MEMORY_OBJECT_H__

#include "sparta/utils/SpartaException.hpp"
#include "sparta/memory/MemoryExceptions.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/memory/BlockingMemoryIFNode.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/memory/DMI_DEPRECATED.hpp"
#include "sparta/memory/DMI.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Memory object with sparse storage for large memory
         * representations. Has direct read/write interface within blocks.
         * Checkpointable.
         *
         * Addresses begin at 0. Has basic bounds checking and a trival,
         * nonvirtual interface.
         *
         * This interface does not support non-blocking accesses or access
         * attributes.
         *
         * A binding class BlockingMemoryObjectIF is provided for placing a
         * BlockingMemoryIF on top of this MemoryObject.
         *
         * For checkpointing support, the owner_node construct argument must
         * be used
         */
        class MemoryObject : public ArchData
        {
        public:

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            MemoryObject() = delete;

            /*!
             * \brief Construct a Memory object with sparse storage
             * \param owner_node Node owning this ArchData. Can be nullptr if
             * checkpointing support is not needed, Should typically have a
             * node. This is the only means by which memory objects are
             * associated with a TreeNode in order to be found by a
             * sparta::serialization::checkpoint::Checkpointer support. Multiple
             * memory objects can be associated with the same node.
             * \param block_size Size of an individual block in this object.
             * Must be a power of 2.
             * \param total_size Size of the memory object. Must be a multiple
             * of block_size. Address 0 refers to first byte. Any address
             * offsetting must be performed externally this functionality is
             * omitted for performance)
             * \param fill Value with which to populate newly-accessed
             * memory. Bytes beyond \a fill_val_size must be 0
             * \param fill_val_size Number of bytes from Value to use for
             * repeating fill. This must be a power of 2 between 1 and 8
             * inclusive.
             */
            MemoryObject(TreeNode* owner_node,
                         addr_t block_size,
                         addr_t total_size,
                         uint64_t fill=0xcc,
                         uint16_t fill_val_size=1) :
                ArchData(owner_node, block_size, fill, fill_val_size)
            {
                // ArchData ctor validates block_size as power of 2
                if(block_size == 0){
                    throw SpartaException("Cannot construct a Memoryobject with a block size of 0. Must be a power of 2 and greater than 0");
                }

                if(total_size == 0){
                    throw SpartaException("Cannot construct a Memoryobject with a total size of 0. Must be a multiple of block_size and greater than 0");
                }

                if(total_size % block_size != 0){
                    throw SpartaException("Cannot construct a MemoryObject with total_size = ")
                        << total_size << " which is not an even muliple of block_size (" << block_size << ")";
                }

                // Perform the layout. At this point, no further resizing can be done
                ArchData::layoutRange(total_size);
            }

            virtual ~MemoryObject() {}

            /*!
             * \brief Render description of this MemoryObject as a
             * string.
             * \note This is NOT a TreeNode override since this class is not a
             * TreeNode
             */
            virtual std::string stringize(bool pretty=false) const {
                (void) pretty;
                std::stringstream ss;
                ss << "<MemoryObject size:0x" << std::hex << getSize() << " bytes, "
                   << std::dec << getNumBlocks() << " blocks, "
                   << getNumAllocatedLines() << " blocks realized>";
                return ss.str();
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Memory Access
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Return a DMI if possible.
             * \param addr the post-translated address which is the start
             * of the DMI.
             * \param size the number of bytes you expect the dmi to span,
             * can be used for error checking that we get a dmi of the size we want.
             * \param dmi gets populated with the correct dmi.
             * \return is dmi created valid.
             */
            bool getDMI_DEPRECATED(const addr_t addr,
                                   const addr_t size,
                                   DMI_DEPRECATED &dmi)
            {
                // Address validation performed in getLine
                // This forces an allocate.
                ArchData::Line& l = ArchData::getLine(addr);
                sparta_assert(size <= l.getLayoutSize());
                dmi.set((void*)l.getRawDataPtr(addr - l.getOffset()));
                return true;
            }

            /**
             * Returns a (possibly invalid) DMI
             *
             * \param addr A guest physical address that is to be accessed via DMI
             * \param callback Callback that is called when the DMI is invalidated
             */
            DMI getDMI(const addr_t addr,
                       const DMIInvalidationCallback &callback)
            {
                auto &line = ArchData::getLine(addr);

                /* Pointer to backing storage of memory region covered by this DMI */
                const auto dmi_ptr = line.getRawDataPtr(0);
                /* Guest physical address of the memory region covered by this DMI */
                const auto guest_addr = line.getOffset();
                /* Size of the memory region covered by this DMI */
                const auto size = line.getLayoutSize();

                (void)callback;
                return DMI(dmi_ptr, guest_addr, size);
            }

            /*!
             * \brief Reads memory
             * \param addr Address to read from where 0 is start of this memory
             * object.
             * \throw SpartaException if access of addr and size is not possible
             * in this storage object. Caller should validate
             */
            void read(addr_t addr,
                      addr_t size,
                      uint8_t *buf) const {
                // Address validation performed in tryGetLine
                const ArchData::Line* l = ArchData::tryGetLine(addr);
                if(!l){
                    checkCanAccess(addr, size); // Acts as if performing a read/write and throws on failure
                    ArchData::fillValue(buf,
                                        size,
                                        getFill(), // Initialze val with default fill
                                        getFillPatternSize(),
                                        (addr % getBlockSize()) % getFillPatternSize()); // Adjust for misalignment with fill pattern. block size is power of 2.
                }else{
                    // Size validation performed in read
                    l->read(addr - l->getOffset(), size, buf);
                }
            }

            /*!
             * \brief Writes memory
             * \param addr Address to write to where 0 is start of this memory
             * object.
             * \throw SpartaException if access of addr and size is not possible
             * in this storage object. Caller should validate
             */
            void write(addr_t addr,
                       addr_t size,
                       const uint8_t *buf) {
                // Address validation performed in getLine
                ArchData::Line& l = ArchData::getLine(addr);

                // Access size validation performed in write
                l.write(addr - l.getOffset(), size, buf);
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Analysis Methods
            //! @{
            ////////////////////////////////////////////////////////////////////////

            //! Gets the line associated with this access.
            //! Performs the same level of validatio non \a addr and \a size that read and write will.
            void _lookupAndValidate(addr_t addr,
                                    addr_t size,
                                    uint8_t *buf) const {
                (void) buf;
                const ArchData::Line* l = ArchData::tryGetLine(addr);
                (void) l;
                checkCanAccess(addr, size); // Acts as if performing a read/write and throws on failure
            }

            //! Determines if memory with the given address and size can be accessed.
            //! Performs the same level of validation on \a addr and \a size that read and write will.150

            void _canAccess(addr_t addr,
                            addr_t size,
                            uint8_t *buf) const {
                (void) buf;
                checkCanAccess(addr, size);
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name General Attributes
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Returns the number of blocks in this memory object
             */
            addr_t getNumBlocks() const {
                return ArchData::getLineIndex(ArchData::getSize());
            };

            addr_t getBlockSize() const {
                return ArchData::getLineSize();
            }

            /*!
             * \brief Get the fill pattern. Fewer than 8 bytes may be used. See
             * getFillPatternSize
             */
            uint64_t getFill() const {
                return ArchData::getInitial();
            }

            /*!
             * \brief Get the number of bytes in the fill pattern
             */
            uint16_t getFillPatternSize() const {
                return ArchData::getInitialValSize();
            }

        }; // class DebugMemoryIF


        /*!
         * \brief BlockingMemoryIFNode implementation with binding to a
         * MemoryObject instantiated separately and specified at
         * BlockingMemoryObjectIFNode construction
         *
         * Forwards accesses to MemoryObject. If memory-access logic beyond
         * simple access-window checking and block-boundary checking is
         * required, a custom subcass for BlockingMemoryIF must be written
         * or this class may be subclassed
         *
         * This class does not to any translation or address offet computation.
         * It merely implements BlockingMemoryIF and forwards calls to a bound
         * MemoryObject
         *
         * This class does not handle checkpointing. Checkpointing operates on
         * MemoryObject since a MemoryObject represents unique memory but can
         * have many interfaces.
         */
        class BlockingMemoryObjectIFNode : public BlockingMemoryIFNode
        {
            MemoryObject& binding_; //!< MemoryObject to which this interface is bound

        public:

            /*!
             * \brief Constructs a BlockingMemoryIFNode bound to a MemoryObject
             *
             * Refer to BlockingMemoryIFNode::BlockingMemoryIFNode for arguments
             */
            BlockingMemoryObjectIFNode(sparta::TreeNode* parent,
                                       const std::string& name,
                                       const std::string& group,
                                       sparta::TreeNode::group_idx_type group_idx,
                                       const std::string& desc,
                                       TranslationIF* transif,
                                       MemoryObject& binding) :
                BlockingMemoryIFNode(parent, name, group, group_idx, desc,
                                     binding.getBlockSize(),
                                     DebugMemoryIF::AccessWindow(0,binding.getSize()),
                                     transif),
                binding_(binding)
            { }

            /*!
             * \brief Constructor for single window without TreeNode group
             * information
             *
             * The \a window parameter may be a single window representing the
             * entire range (if there are no holes in the memory represented by
             * this interface)
             *
             * This is the simplest constructor available for this class
             */
            BlockingMemoryObjectIFNode(sparta::TreeNode* parent,
                                       const std::string& name,
                                       const std::string& desc,
                                       TranslationIF* transif,
                                       MemoryObject& binding) :
                BlockingMemoryObjectIFNode(parent,
                                           name,
                                           sparta::TreeNode::GROUP_NAME_NONE,
                                           sparta::TreeNode::GROUP_IDX_NONE,
                                           desc,
                                           transif,
                                           binding)
            { }

            virtual ~BlockingMemoryObjectIFNode() {}

            MemoryObject* getMemObj() {
                return &binding_;
            }

            /*!
             * \brief Return a DMI if possible.
             * \param addr the post-translated address which is the start
             * of the DMI.
             * \param size the number of bytes you expect the dmi to span,
             * can be used for error checking that we get a dmi of the size we want.
             * \param dmi gets populated with the correct dmi.
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This pointer is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \return is dmi created valid.
             */
            bool getDMI_DEPRECATED(const addr_t addr,
                                   const addr_t size,
                                   DMI_DEPRECATED &dmi,
                                   const void *supplement = nullptr) override
            {
                (void)supplement; // we don't care about supplement.
                return binding_.getDMI_DEPRECATED(addr, size, dmi);
            }

            DMI getDMI(const addr_t addr,
                       const DMIInvalidationCallback &callback,
                       const void *supplement = nullptr) override
            {
                (void) supplement;
                return binding_.getDMI(addr, callback);
            }

        protected:
            //! Override of DebugMemoryIF::tryPeek_
            virtual bool tryPeek_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf) const override {
                binding_.read(addr,size,buf);
                return true;
            }

            //! Override of DebugMemoryIF::tryPoke_
            virtual bool tryPoke_(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf) override {
                binding_.write(addr,size,buf);
                return true;
            }

        private:

            /*!
             * \brief Override of BlockingMemoryIF::tryRead_
             */
            virtual bool tryRead_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf,
                                  const void *in_supplement,
                                  void *out_supplement) override {
                (void) in_supplement;
                (void) out_supplement;
                binding_.read(addr, size, buf);
                return true;
            }

            /*!
             * \brief Override of BlockingMemoryIF::tryWrite_
             */
            virtual bool tryWrite_(addr_t addr,
                                   addr_t size,
                                   const uint8_t *buf,
                                   const void *in_supplement,
                                   void *out_supplement) override {
                (void) in_supplement;
                (void) out_supplement;
                binding_.write(addr, size, buf);
                return true;
            }

        }; // class BlockingMemoryIF

    }; // namespace memory
}; // namespace sparta

//! \brief MemoryObject stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::MemoryObject& mo) {
    out << mo.stringize();
    return out;
}

//! \brief MemoryObject stream operator
inline std::ostream& operator<<(std::ostream& out, const sparta::memory::MemoryObject* mo) {
    if(nullptr == mo){
        out << "null";
    }else{
        out << mo->stringize();
    }
    return out;
}

#endif // __MEMORY_OBJECT_H__
