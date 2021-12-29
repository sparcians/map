
/**
 * \file   SimpleMemoryMapNode.hpp
 * \brief  File that contains SimpleMemoryMapNode
 */

#pragma once

#include "sparta/memory/BlockingMemoryIFNode.hpp"
#include "sparta/memory/SimpleMemoryMap.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Memory mapping object which implements BlockingMemoryIFNode.
         * Supports a simple mapping of incoming addresses to addresses within
         * a set of destination BlockingMemoryIFs.
         *
         * Note that this map supports notifications and instrumentation on the
         * Map itself as well as on the destination memory interfaces.
         *
         * Destinations must start and end on block boundaries and accesses
         * cannot span destinations.
         *
         * \see BlockingMemoryIFNode
         * \see SimpleMemoryMap
         */
        class SimpleMemoryMapNode : public BlockingMemoryIFNode, public SimpleMemoryMap
        {
        public:

            using BlockingMemoryIFNode::getBlockSize;

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            SimpleMemoryMapNode() = delete;

            /*!
             * \brief Construct a SimpleMemoryMap that is also a
             * BlockingMemoryIFNode subclass
             * \param parent Parent TreeNode. Must not be null
             * \param group Group name. Must not be empty. See
             * sparta::TreeNode for rules
             * \param group_idx Group index. See sparta::TreeNode
             * \param desc Description of this interface. Must not be empty. See
             * sparta::TreeNode
             * \param block_size Size for all blocks that are accessible through
             * this interfaces
             * \param total_size Total size of the mapping space. This does not
             * need to be entirely packed with mappings. This is required for
             * BlockingMemoryIF
             * \param transif See BlockingMemoryIF::BlockingMemoryIF
             */
            SimpleMemoryMapNode(sparta::TreeNode* parent,
                                const std::string& name,
                                const std::string& group,
                                group_idx_type group_idx,
                                const std::string& desc,
                                addr_t block_size,
                                addr_t total_size,
                                TranslationIF* transif=nullptr) :
                BlockingMemoryIFNode(parent, name, group, group_idx, desc,
                                     block_size,
                                     DebugMemoryIF::AccessWindow(0, total_size),
                                     transif),
                SimpleMemoryMap(block_size)
            { }

            /*!
             * \brief Constructor for SimpleMemoryMapNode without TreeNode group
             * information
             *
             * This is the simplest constructor available for this class
             */
            SimpleMemoryMapNode(sparta::TreeNode* parent,
                                const std::string& name,
                                const std::string& desc,
                                addr_t block_size,
                                addr_t total_size,
                                TranslationIF* transif=nullptr) :
                SimpleMemoryMapNode(parent,
                                    name,
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    desc,
                                    block_size,
                                    total_size,
                                    transif)
            { }

            virtual ~SimpleMemoryMapNode() {}

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Memory Access
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /**
             * \brief Read a block of memory
             *
             * \param addr The unmasked address to read
             * \param size The amount of memory to read
             * \param buf  The buffer to read the data into
             * \param in_supplement  Metadata supplement (in data) to pass to the eventual memory interface
             * \param out_supplement Metadata supplement (out data) to pass to the eventual memory interface
             * \throw MemoryReadError if there's an issue reading memory at that address and size
             *
             * This method will attempt to map the given address to an
             * internal memory interface and read data from that
             * interface.  If the eventual interface fails, this method will throw
             */
            void read(addr_t addr,
                      addr_t size,
                      uint8_t *buf,
                      const void *in_supplement=nullptr,
                      void *out_supplement=nullptr) {
                if(!tryRead(addr, size, buf, in_supplement, out_supplement)){
                    verifyHasMapping(addr, size);
                    verifyNoBlockSpan(addr, size);
                    verifyInAccessWindows(addr, size);
                    throw MemoryReadError(addr, size, "Unknown reason");
                }
            }

            /**
             * \brief Write a block of memory
             *
             * \param addr The unmasked address to write to
             * \param size The amount of memory to write
             * \param buf  The buffer of data to write
             * \param in_supplement  Metadata supplement (in data) to pass to the eventual memory interface
             * \param out_supplement Metadata supplement (out data) to pass to the eventual memory interface
             * \throw MemoryReadError if there's an issue writing memory at that address and size
             *
             * This method will attempt to map the given address to an
             * internal memory interface write read data to that
             * interface.  If the eventual interface fails, this
             * method will throw
             */
            void write(addr_t addr,
                       addr_t size,
                       const uint8_t *buf,
                       const void *in_supplement=nullptr,
                       void *out_supplement=nullptr) {
                if(!tryWrite(addr, size, buf, in_supplement, out_supplement)){
                    verifyHasMapping(addr, size);
                    verifyNoBlockSpan(addr, size);
                    verifyInAccessWindows(addr, size);
                    throw MemoryReadError(addr, size, "Unkonwn reason");
                }
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name DMI Access
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /**
             * \brief Get a DMI blocking interface at the given address and size
             *
             * \param addr The intended address to read/write directly to/from
             * \param size The intended block size
             *
             * \return Pointer a valid DMIBlockingMemoryIF that can
             *         support the given address/size combo.  nullptr if not supported
             *
             * SimpleMemoryMap will find the equivalent mapped memory
             * interface that can support the given address and size
             * combo.  If the end memory interface cannot support DMI
             * or the given address/size, combo, nullptr will be returned.
             */
            DMIBlockingMemoryIF *getDMI(addr_t addr, addr_t size) override {
                return nullptr;
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

        protected:

            //! \name Access and Query Implementations
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Implements tryRead_
             * \param addr Address to read from where 0 is start of this memory
             * object.
             * \warning Does not immediately prohibit accesses spanning blocks or
             * mappings. This is the responsibility of the caller
             */
            virtual bool tryRead_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf,
                                  const void *in_supplement=nullptr,
                                  void *out_supplement=nullptr) override {
                const Mapping* m = findMapping(addr);
                if(!m){
                    return false;
                    //throw MemoryReadError(addr, size, "Could not find mapping at this address");
                }
                return m->memif->tryRead(m->mapAddress(addr), size, buf, in_supplement, out_supplement);
            }

            /*!
             * \brief Implements tryWrite_
             * \param addr Address to write to where 0 is start of this memory
             * object.
             * \warning Does not immediately prohibit accesses spanning blocks or
             * mappings. This is the responsibility of the caller
             */
            virtual bool tryWrite_(addr_t addr,
                                   addr_t size,
                                   const uint8_t *buf,
                                   const void *in_supplement=nullptr,
                                   void *out_supplement=nullptr) override {
                Mapping* m = findMapping(addr);
                if(!m){
                    return false;
                    //throw MemoryWriteError(addr, size, "Could not find mapping at this address");
                }
                return m->memif->tryWrite(m->mapAddress(addr), size, buf, in_supplement, out_supplement);
            }

            /*!
             * \brief Implements tryPeek_
             *
             * These accesses can span mappings and are automatically split
             */
            virtual bool tryPeek_(addr_t addr,
                                  addr_t size,
                                  uint8_t *buf) const override {
                // Note that input peeks are already split into blocks, so each
                // may be translated once. Easy. Recall that peek has no
                // performance requirement either.

                const Mapping* m = findMapping(addr);
                if(!m){
                    return false;
                }
                return m->memif->tryPeek(m->mapAddress(addr), size, buf);
            }

            /*!
             * \brief Implements tryPoke_
             *
             * These accesses can span mappings and are automatically split
             */
            virtual bool tryPoke_(addr_t addr,
                                  addr_t size,
                                  const uint8_t *buf) override {
                // Note that input peeks are already split into blocks, so each
                // may be translated once. Easy. Recall that peek has no
                // performance requirement either.

                Mapping* m = findMapping(addr);
                if(!m){
                    return false;
                }
                return m->memif->tryPoke(m->mapAddress(addr), size, buf);
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

        }; // class SimpleMemoryMapNode
    }; // namespace memory
}; // namespace sparta
