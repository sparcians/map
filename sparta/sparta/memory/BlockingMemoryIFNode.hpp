
/**
 * \file   BlockingMemoryIFNode.hpp
 * \brief  File that contains BlockingMemoryIFNode
 */

#ifndef __BLOCKING_MEMORY_IF_NODE_H__
#define __BLOCKING_MEMORY_IF_NODE_H__

#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/memory/MemoryExceptions.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/memory/BlockingMemoryIF.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief Pure-virtual memory interface that builds on the
         * BlockingMemoryIF, acting as a TreeNode in the SPARTA device tree
         * through which clients such as tools and UIs can discover and interact
         * with the interface. Includes SPARTA Tree notification support,
         * and sparta::Counter representation of access statistics.
         *
         * Clients can register SPARTA notifications on this interface's device
         * tree node
         *
         * Access counters are in a StatisticSet node which is a child of this
         * node. This StatisticSet can be accessed via getStatisticSet()
         *
         * sparta::memory::BlockingMemoryObjectIF provides an implementation of
         * this class which binds to a MemoryObject instance.
         */
        class BlockingMemoryIFNode : public sparta::TreeNode, public BlockingMemoryIF
        {
        public:

            //! \name Local Types
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Structure containing data for a Memory pre- or post-read
             * notification
             *
             * The data in this structure is only guaranteed to be valid within
             * the notification callback where it was an argument
             */
            struct ReadAccess {

                ReadAccess(DebugMemoryIF* _mem) :
                    mem(_mem),
                    addr(0),
                    size(0),
                    data(nullptr),
                    in_supplement(nullptr),
                    out_supplement(nullptr)
                { }

                /*!
                 * \brief Memory interface on which the access took place
                 */
                DebugMemoryIF* const mem;

                /*!
                 * \brief Address of the access from the perspective of the
                 * memory interface
                 */
                addr_t addr;

                /*!
                 * \brief Size in bytes of the access
                 */
                addr_t size;

                /*!
                 * \brief Data just read from \a mem. Size is \a size field
                 */
                const uint8_t* data;

                /*!
                 * \brief Supplementary data-object associated with the memory transaction which
                 * caused this notification
                 */
                const void* in_supplement;

                /*!
                 * \brief Supplementary data-object associated with the memory transaction which
                 * caused this notification. Used to send information back about the transaction.
                 */
                void* out_supplement;
            };

            /*!
             * \brief Structure containing data for a Memory post-write notification
             *
             * The data in this structure is only guaranteed to be valid within
             * the notification callback where it was an argument
             */
            struct PostWriteAccess {

                PostWriteAccess(DebugMemoryIF* _mem) :
                    mem(_mem),
                    addr(0),
                    size(0),
                    prior(nullptr),
                    tried(nullptr),
                    in_supplement(nullptr),
                    out_supplement(nullptr)
                { }

                /*!
                 * \brief Register on which the write access took place
                 *
                 * The final value can be peeked through this interface
                 */
                DebugMemoryIF* const mem;

                /*!
                 * \brief Address of the access on this object
                 */
                addr_t addr;

                /*!
                 * \brief Size in bytes of the access
                 */
                addr_t size;

                /*!
                 * \brief Value which was written to \a mem. Size is \a size
                 * field
                 */
                const uint8_t* prior;

                /*!
                 * \brief Value that the write access attempted to write. Size
                 * is \a size field
                 */
                const uint8_t* tried;

                /*!
                 * \brief Supplementary data-object associated with the memory transaction which
                 * caused this notification
                 */
                const void* in_supplement;

                /*!
                 * \brief Supplementary data-object associated with the memory transaction which
                 * caused this notification. Used to send information back about the transaction.
                 */
                void* out_supplement;
            };

            typedef NotificationSource<ReadAccess> ReadNotiSrc;           //!< Notification type for memory read accesses
            typedef NotificationSource<PostWriteAccess> PostWriteNotiSrc; //!< Notification type for memory write accesses

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            BlockingMemoryIFNode() = delete;

            /*!
             * \brief Construct a blocking memory interface that is also a
             * sparta::TreeNode subclass
             * \param parent Parent TreeNode. Must not be null
             * \param group Group name. Must not be empty. See
             * sparta::TreeNode for rules
             * \param group_idx Group index. See sparta::TreeNode
             * \param desc Description of this interface. Must not be empty. See
             * sparta::TreeNode
             * \param block_size Size for all blocks that are accessible through
             * this interfaces
             * \param windows See BlockingMemoryIF::BlockingMemoryIF
             * \param transif See BlockingMemoryIF::BlockingMemoryIF
             */
            BlockingMemoryIFNode(sparta::TreeNode* parent,
                                 const std::string& name,
                                 const std::string& group,
                                 group_idx_type group_idx,
                                 const std::string& desc,
                                 addr_t block_size,
                                 const DebugMemoryIF::AccessWindow& window,
                                 TranslationIF* transif=nullptr) :
                TreeNode(name, group, group_idx, desc),
                BlockingMemoryIF(desc, block_size, window, transif),
                prior_val_buffer_(new uint8_t[block_size]), // block_size assured nonzero from DebugMemoryIF ctor
                sset_(this),
                post_write_noti_(this, "post_write", "Notification immediately after the memory interface has been written", "post_write"),
                post_write_noti_data_(this),
                post_read_noti_(this, "post_read", "Notification immediately after the memory interface has been read", "post_read"),
                post_read_noti_data_(this)
            {
                if(!parent){
                    throw SpartaException("BlockingMemoryIFNode must be constructed with a non-null parent");
                }
                setExpectedParent_(parent);

                ctr_writes_ = &sset_.createCounter<sparta::Counter>("num_writes", "Number of writes attempted (num write calls)",
                                                                  sparta::Counter::COUNT_NORMAL);
                ctr_reads_ = &sset_.createCounter<sparta::Counter>("num_reads", "Number of reads attempted (num read calls)",
                                                                 sparta::Counter::COUNT_NORMAL);

                parent->addChild(this);
            }

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
            BlockingMemoryIFNode(sparta::TreeNode* parent,
                                 const std::string& name,
                                 const std::string& desc,
                                 addr_t block_size,
                                 const AccessWindow& window,
                                 TranslationIF* transif=nullptr) :
                BlockingMemoryIFNode(parent,
                                     name,
                                     sparta::TreeNode::GROUP_NAME_NONE,
                                     sparta::TreeNode::GROUP_IDX_NONE,
                                     desc,
                                     block_size,
                                     window,
                                     transif)
            { }

            virtual ~BlockingMemoryIFNode() {}

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name Observation
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Returns the post-write notification-source node for this
             * memory interface which can be used to observe writes.
             * This notification is posted immediately after the memory has
             * been written and populates a PostWriteAccess object with the
             * results.
             *
             * Refer to PostWriteNotiSrc (sparta::NotificationSource) for more
             * details. Use the sparta::NotificationSource::registerForThis and
             * sparta::NotificationSource::deregisterForThis methods.
             */
            PostWriteNotiSrc& getPostWriteNotificationSource() {
                return post_write_noti_;
            }

            /*!
             * \brief Returns the read notification-source node for this
             * memory interface which can be used to observe writes.
             * This notification is posted immediately after the memory has
             * been read and populates a ReadAccess object
             * with the results.
             *
             * Refer to ReadNotiSrc (sparta::NotificationSource) for more
             * details. Use the sparta::NotificationSource::registerForThis and
             * sparta::NotificationSource::deregisterForThis methods.
             */
            ReadNotiSrc& getReadNotificationSource() {
                return post_read_noti_;
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            //! \name General Queries
            //! @{
            ////////////////////////////////////////////////////////////////////////

            //bool isWriteValid(addr_t addr, addr_t size) const noexcept {
            //    return isWriteValid_(addr, size);
            //}
            //bool isReadValid(addr_t addr, addr_t size) const noexcept {
            //    return isReadValid_(addr, size);
            //}

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
             * \param supplement caller-defined object identifying this memory access
             * for notifications & debug purposes. This object is passed to any
             * subsequent memory interfaces. Can be any pointer, and nullptr
             * indicates no info.
             * \param buf Buffer of data to populate with \a size bytes from memory
             * object. Content of \a buf is undefined if read fails
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
                                 void *out_supplement=nullptr) override {
                ++*ctr_reads_;
                if(__builtin_expect(doesAccessSpan(addr, size), 0)){
                    //BlockingMemoryIF base class seems to imply that throwing from a tryRead is invalid
                    //throw MemoryReadError(addr, size, "addr is in a different block than addr+size");
                    return false;
                }
                if(__builtin_expect(isInAccessWindows(addr, size) == false, 0)){
                    return false;
                }
                if(__builtin_expect(post_read_noti_.observed() == false, 1)){
                    return tryRead_(addr, size, buf, in_supplement, out_supplement); // Read and return if unobserved
                }

                // XXX what implementation of tryRead_ is used here?  It is pure-virtual in the BlockingMemoryIF base.
                bool result = tryRead_(addr, size, buf, in_supplement, out_supplement);

                if(result){
                    ReadNotiSrc::data_type post_read_noti_data(this);
                    post_read_noti_data.addr = addr;
                    post_read_noti_data.size = size;
                    post_read_noti_data.data = buf;
                    post_read_noti_data.in_supplement = in_supplement;
                    post_read_noti_.postNotification(post_read_noti_data);
                }

                return result;
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
                                  void *out_supplement=nullptr) override {
                ++*ctr_writes_;
                if(__builtin_expect(doesAccessSpan(addr, size), 0)){
                    // BlockingMemoryIF base class claims that tryRead and tryWrite cannot throw
                    // throw MemoryWriteError(addr, size, "addr is in a different block than addr+size");
                    return false;
                }
                if(__builtin_expect(isInAccessWindows(addr, size) == false, 0)){
                    return false;
                }
                if(__builtin_expect(post_write_noti_.observed() == false, 1)){
                    return tryWrite_(addr, size, buf, in_supplement, out_supplement); // Write and return if unobserved
                }

                // peek into block-size buffer
                if(!tryPeek(addr, size, prior_val_buffer_.get())){
                    return false;
                }

                bool result = tryWrite_(addr, size, buf, in_supplement, out_supplement);

                if(result){
                    PostWriteNotiSrc::data_type post_write_noti_data(this);
                    post_write_noti_data.addr = addr;
                    post_write_noti_data.size = size;
                    post_write_noti_data.prior = prior_val_buffer_.get();
                    post_write_noti_data.tried = buf;
                    post_write_noti_data.in_supplement = in_supplement;
                    // A getInternalBuffer method seems appropriate here but is
                    // not used because no assumptions should be made about
                    // storage. So Memory implementation may not HAVE an
                    // internal buffer for this memory (e.g. memory mapped registers not
                    // contiguous)
                    // Instead, caller must peek themselves.
                    post_write_noti_.postNotification(post_write_noti_data);
                }

                return result;
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


            ////////////////////////////////////////////////////////////////////////
            //! @}

            /*!
             * \brief Returns the StatisticSet published by this Node.
             * \note This StatisticSet is also a child TreeNode
             */
            StatisticSet& getStatisticSet() {
                return sset_;
            }

            /*!
             * \brief Render description of this BlockingMemoryIF as a string
             * \return String description of the form
             * \verbatim
             * <location size:sizeB>
             * \endverbatim
             * where \a location is the dotted location in a device tree and
             * \a sizeB is the total range of bytes covered by ths interface
             * in hex suffixed with "bytes"
             */
            virtual std::string stringize(bool pretty=false) const override {
                (void) pretty;
                std::stringstream ss;
                ss << "<" << getLocation() << " size:0x" << std::hex << total_range_ << " bytes>";
                return ss.str();
            }

            /*!
             * \brief Buffer for holding prior memory data during post-write
             * notifications in order to avoid re-allocating each time.
             *
             * Size will be that of a single block (maximum read size)
             */
            std::unique_ptr<uint8_t[]> prior_val_buffer_;

            //! \name Interface instrumentation
            //! @{
            ////////////////////////////////////////////////////////////////////////

            StatisticSet sset_;
            Counter* ctr_writes_;
            Counter* ctr_reads_;

            ////////////////////////////////////////////////////////////////////////
            //! @}

            PostWriteNotiSrc post_write_noti_; //!< NotificationSource for post-write notifications
            PostWriteNotiSrc::data_type post_write_noti_data_; //!< Data associated with a post-write notification
            ReadNotiSrc post_read_noti_; //!< NotificationSource for post-read notifications
            ReadNotiSrc::data_type post_read_noti_data_; //!< Data associated with a post-read notification

        }; // class BlockingMemoryIFNode
    }; // namespace memory
}; // namespace sparta

#endif // __BLOCKING_MEMORY_IF_NODE_H__
