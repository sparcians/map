#pragma once

#include <list>
#include <cinttypes>
#include <string>

#include "sparta/memory/MemoryObject.hpp"
#include "sparta/memory/BlockingMemoryIF.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta::memory
{
    /**
     * \class StoreData Helper class for CachedMemory
     *
     * This class is the default helper class for CachedMemory, but is
     * not required to be used.
     *
     * However, the API it provides must be implemented:
     * \code
     *     // Expected construction
     *     MemoryAccessWrite(uint64_t        write_id,
     *                       addr_t          paddr,
     *                       size_t          size,
     *                       const uint8_t * data,
     *                       const void    * in_supplement,
     *                       void          * out_supplement);
     *
     *     uint64_t getWriteID() const;
     *     addr_t   getPAddr()   const;
     *     size_t   getSize()    const;
     *
     *     uint8_t * getStoreDataPtr();
     *     uint8_t * getPrevDataPtr();
     *     const uint8_t * getStoreDataPtr() const;
     *     const uint8_t * getPrevDataPtr()  const;
     * \endcode
     *
     * This class is the default helper class for CachedMemory.
     * Specifically, when a write occurs in CachedMemory, an instance
     * of this class is created to hold the data to be written as well
     * as the previous data at the physical location.
     *
     * When a write is committed, downsteam memory is updated using
     * this class to write the data (and not the data from the cache).
     *
     */
    class StoreData
    {
    public:

        /**
         * \brief Construct a StoreData object
         * \param write_id The unique ID for this StoreData
         * \param paddr    The physical address of the data it contains
         * \param size     The size of the data contained
         * \param data     The data to be copied into this class
         * \param in_supplement  (ignored)
         * \param out_supplement (ignored)
         */
        StoreData(uint64_t        write_id,
                  addr_t          paddr,
                  size_t          size,
                  const uint8_t * data,
                  const void    * in_supplement,
                  void          * out_supplement) :
            write_id_(write_id),
            paddr_(paddr),
            size_(size),
            stored_value_(size),
            prev_value_(size)
        {
            (void) in_supplement;
            (void) out_supplement;

            // Cache the memory being written to eventually be committed
            ::memcpy(stored_value_.data(), data, size);
        }

        //! Get the write ID, unique to each write performed
        uint64_t getWriteID() const { return write_id_; }

        //! Get the physical address this write corresponds to
        addr_t   getPAddr()   const { return paddr_; }

        //! Get the size of the write data (in bytes)
        size_t   getSize()    const { return size_;  }

        //! Get the stored data
        uint8_t * getStoreDataPtr() { return stored_value_.data(); }

        //! Get the previous data at the address (for restore)
        uint8_t * getPrevDataPtr()  { return prev_value_.data(); }

        //! Const methods of above methods
        const uint8_t * getStoreDataPtr() const { return stored_value_.data(); }

        //! Const methods of above methods
        const uint8_t * getPrevDataPtr()  const { return prev_value_.data(); }

    private:
        const uint64_t       write_id_; //!< Unique ID for the write
        const addr_t         paddr_;  //!< Physical address
        const size_t         size_;   //!< doubleword=8, word=4, halfword=2, byte=1
        std::vector<uint8_t> stored_value_;
        std::vector<uint8_t> prev_value_;
    };

    inline std::ostream & operator<<(std::ostream & os, const StoreData & mstore)
    {
        os << std::hex << "wid:" << mstore.getWriteID() << " pa:" << mstore.getPAddr()
           << std::dec << " size:" << mstore.getSize();
        return os;
    }

    /**
     * \class CachedMemory
     * \brief Memory that can be used as a cache for core models
     * \tparam MemoryWriteType Type supplied by the modeler to track the outstanding writes
     *
     * CachedMemory is used by a functional/performance model to
     * "cache" outstanding writes and provide those values back to the
     * core when asked.  It is implemented as full memory model, with
     * its memory always representing the latest stored value that is
     * not committed.
     *
     * When the write is committed by a functional model (like in a
     * cosimulation environment), the data is pushed downstream.
     *
     * If the memory is _not_ to be pushed out (flushing for example),
     * the modeler can "drop" that memory update and CachedMemory will
     * restore the previous value.
     *
     * Use case: data-driven performance modeling where there are
     * speculative stores (storing to a local store commit buffer) and
     * there are dependent loads on that data.  If the store is no
     * longer speculative, the store is officially committed.  If it's
     * flushed, old data can be restored and downstream memory (main
     * memory/IO devices, etc) will never see the data.
     *
     * System memory is responsible for updating other CachedMemory
     * instances in the case of multicore.
     *
     * Since this class acts like a cache off of a core (example), a
     * few rules apply:
     *
     * -# Reads will always be from this block of memory and must be
     *    aligned.  If the read is not in this cache, downstream
     *    memory will be peeked, but the result will not be cached.
     *
     * -# Writes will always cache.  If the data were not in the cache
     *    initially, downsteam memory will be peeked and loaded before
     *    merging the write data.
     *
     * -# Peeking will always be a read on the cache memory.  If the
     *    block is not in the cache, peeking will be forwarded to
     *    downstream memory
     *
     * -# Poking will always write to the cache as well as downsteam
     *    memory.  If the data is not in the cache, it will be loaded
     *    into the cache before being written
     */
    template<class MemoryWriteType = StoreData>
    class CachedMemory : public sparta::memory::BlockingMemoryIF
    {
    public:
        /**
         * \brief Create a CachedMemory object
         *
         * \param name     CachedMemory name
         * \param write_id The ID this CachedMemory to uniquify memory writes
         * \param outstanding_writes_watermark Watermark for assertion if uncomitted writes get too large
         * \param block_size Block size of the cache. (See sparta::MemoryObject::MemoryObject)
         * \param total_size Total size of the cache block (See sparta::MemoryObject::MemoryObject)
         * \param downstream_memory The coherency point past this CachedMemory
         */
        CachedMemory(const std::string & name,
                     uint64_t write_id,
                     uint32_t outstanding_writes_watermark,
                     addr_t   block_size,
                     addr_t   total_size,
                     sparta::memory::BlockingMemoryIF * downstream_memory);

        /**
         * \brief Get the write ID for this cache
         * \return The write ID given at construction
         */
        uint64_t getWriteID() const { return write_id_; }

        /**
         * \brief Get all outstanding writes
         * \return vector of outstanding memory access writes based on the user type
         */
        const std::map<uint64_t, MemoryWriteType> & getOutstandingWrites() const {
            return outstanding_writes_;
        }

        /**
         * \brief Given address, find outstanding writes that are not merged/committed
         * \param paddr Physical address to search for
         * \return vector of outstanding memory writes that match
         *
         * This function will return all MemAccessWrites that it finds
         * that partially (at least/most 1 byte) or completely
         * contains the given paddr.
         *
         * The first element of the vector is the oldest outstanding write
         */
        std::vector<MemoryWriteType> getOutstandingWritesForAddr(addr_t paddr) const;

        /**
         * \brief Return the number of uncomitted writes
         */
        uint32_t getNumOutstandingWrites() const { return outstanding_writes_.size(); }

        /**
         * \brief Commit the write that matches the given MemoryWriteType
         * \param write_to_commit The MemoryAccessWrite to commit
         *
         * There must be an exact match in the cache for that
         * address/size combination for the outstanding write to be
         * removed
         */
        void commitWrite(const MemoryWriteType & write_to_commit);

        /**
         * \brief Merge an incoming write from system memory
         * \param paddr Write's address
         * \param size  Write's size (in bytes)
         * \param buf   Data to store
         *
         * This function will merge the given data into this cache
         * only writing those bytes that do not collide
         * wtih stores that have not been committed.
         */
        void mergeWrite(addr_t paddr, addr_t size, const uint8_t * buf);

        /**
         * \brief Drop the outstanding write that matches the given MemoryWriteType
         * \param write_to_drop The MemoryAccessWrite to drop
         * \throw SpartaException if no commit with the given exists
         */
        void dropWrite(const MemoryWriteType & write_to_drop);

        /**
         * \brief Return the latest outstanding store access
         * \return The latest/newest write access created
         */
        const MemoryWriteType & getLastOutstandingWrite() const {
            sparta_assert(false == outstanding_writes_.empty());
            return outstanding_writes_.rbegin()->second;
        }

    private:
        ////////////////////////////////////////////////////////////////////////////////
        // State
        const uint64_t write_id_;
        const uint32_t outstanding_writes_watermark_;
        std::map<uint64_t, MemoryWriteType> outstanding_writes_;
        uint64_t write_uid_ = 0;

        ////////////////////////////////////////////////////////////////////////////////
        // Derived methods
        bool tryRead_(addr_t paddr, addr_t size, uint8_t *buf,
                      const void *in_supplement, void *out_supplement) override final;
        bool tryWrite_(addr_t paddr, addr_t size, const uint8_t *buf,
                       const void *in_supplement, void *out_supplement) override final;
        bool tryPeek_(addr_t paddr, addr_t size,
                      uint8_t *buf) const override final;
        bool tryPoke_(addr_t paddr, addr_t size, const uint8_t *buf) override final;

        ////////////////////////////////////////////////////////////////////////////////
        // Memory
        sparta::memory::BlockingMemoryIF * downstream_memory_ = nullptr;
        sparta::memory::MemoryObject       cached_memory_;

    };

    template<class MemoryWriteType>
    CachedMemory<MemoryWriteType>::CachedMemory(const std::string & name,
                                                uint64_t write_id,
                                                uint32_t outstanding_writes_watermark,
                                                addr_t block_size,
                                                addr_t total_size,
                                                sparta::memory::BlockingMemoryIF * downstream_memory) :
        sparta::memory::BlockingMemoryIF (name + "_cached_memory", block_size,
                                          sparta::memory::DebugMemoryIF::AccessWindow(0, total_size)),
        write_id_(write_id),
        outstanding_writes_watermark_(outstanding_writes_watermark),
        // Use the write ID at the top byte for uniquenes
        write_uid_(uint64_t(write_id_) << ((sizeof(write_uid_) - 1) * 8)),
        downstream_memory_(downstream_memory),
        cached_memory_(nullptr, block_size, total_size, 0)
    {}

    // Reads always occur on the cached memory.  If it "misses," data
    // will be peeked from downsteam memory but not read loaded into
    // the cached memory.  The cached memory does not maintain notions
    // of RWITM, etc
    template<class MemoryWriteType>
    bool CachedMemory<MemoryWriteType>::tryRead_ (addr_t paddr, addr_t size, uint8_t *buf,
                                                  const void *in_supplement, void *out_supplement)
    {
        if(cached_memory_.tryGetLine(paddr) == nullptr) {
            downstream_memory_->peek(paddr, size, buf);
        }
        else {
            cached_memory_.read(paddr, size, buf);
        }
        return true;
    }

    // Writes always occur on the cached memory.  If not present, the
    // data will first be loaded into the cache and the write merged
    template<class MemoryWriteType>
    bool CachedMemory<MemoryWriteType>::tryWrite_ (addr_t paddr, addr_t size, const uint8_t *buf,
                                                   const void *in_supplement, void *out_supplement)
    {
        sparta_assert(outstanding_writes_.size() != outstanding_writes_watermark_,
                      "Watermark of outstanding writes reached. "
                      "Writes need to be committed or dropped via the CoSim API");

        if(cached_memory_.tryGetLine(paddr) == nullptr) {
            // This block has not been initialized in cached memory yet because
            // it has not been written to before.
            const auto aligned_paddr = paddr & block_mask_;
            uint8_t * cached_mem_block_ptr = cached_memory_.getLine(aligned_paddr).getRawDataPtr(0);
            downstream_memory_->peek(aligned_paddr, block_size_, cached_mem_block_ptr);
        }

        // Store the write into a outstanding chain
        ++write_uid_;

        MemoryWriteType outstanding_write(write_uid_, paddr, size, buf, in_supplement, out_supplement);
        auto * prev_data_store = outstanding_write.getPrevDataPtr();

        // Need to get the previous value of memory to restore if the write is dropped
        cached_memory_.read(paddr, size, prev_data_store);

        // Write the new value of memory
        cached_memory_.write(paddr, size, buf);

        // Store the write
        outstanding_writes_.emplace(std::make_pair(write_uid_, outstanding_write));

        return true;
    }

    // Peeks will always occur on the cached memory as reads.  If the
    // data is not in the cache, downsteam memory is peeked instead
    template<class MemoryWriteType>
    bool CachedMemory<MemoryWriteType>::tryPeek_(addr_t paddr, addr_t size, uint8_t *buf) const
    {
        // Need to check for CI space or possibly magic memory address
        if(cached_memory_.tryGetLine(paddr) == nullptr) {
            downstream_memory_->peek(paddr, size, buf);
        }
        else {
            cached_memory_.read(paddr, size, buf);
        }
        return true;
    }

    // Pokes will always occur on the cached memory and downstream memory.
    template<class MemoryWriteType>
    bool CachedMemory<MemoryWriteType>::tryPoke_(addr_t paddr, addr_t size, const uint8_t *buf)
    {
        if(cached_memory_.tryGetLine(paddr) == nullptr) {
            // This memory was never written (or read) before.
            const auto aligned_paddr = paddr & block_mask_;
            uint8_t * cached_mem_block_ptr = cached_memory_.getLine(aligned_paddr).getRawDataPtr(0);
            downstream_memory_->poke(aligned_paddr, block_size_, cached_mem_block_ptr);
        }
        cached_memory_.write(paddr, size, buf);
        downstream_memory_->poke(paddr, size, buf);
        return true;
    }

    template<class MemoryWriteType>
    std::vector<MemoryWriteType>
    CachedMemory<MemoryWriteType>::getOutstandingWritesForAddr(addr_t paddr) const
    {
        std::vector<MemoryWriteType> matching_stores;
        for(const auto & [wuid, maw] : outstanding_writes_) {
            if((paddr >= maw.getPAddr()) && (paddr < (maw.getPAddr() + maw.getSize()))) {
                // This store access contains the given paddr
                matching_stores.emplace_back(maw);
            }
        }
        return matching_stores;
    }

    template<class MemoryWriteType>
    void CachedMemory<MemoryWriteType>::commitWrite(const MemoryWriteType & write_to_commit)
    {
        sparta_assert(false == outstanding_writes_.empty(),
                      "there are no outstanding writes for commit");

        const auto & mem_write_access = outstanding_writes_.begin()->second;

        if(SPARTA_EXPECT_FALSE(mem_write_access.getWriteID() != write_to_commit.getWriteID()))
        {
            std::stringstream msg;
            msg << __FUNCTION__ << ": error: attempting to commit write out of order: " << write_to_commit
                << " expected to commit write: " << mem_write_access;
            msg << "\nOutstanding writes for write ID " << write_id_ << " (oldest to newest):\n";
            for(const auto & outstanding_write : outstanding_writes_) {
                msg << "\t" << outstanding_write << "\n";
            }
            throw SpartaException(msg.str());
        }

        // Update downstream memory
        downstream_memory_->tryWrite(mem_write_access.getPAddr(),
                                     mem_write_access.getSize(),
                                     mem_write_access.getStoreDataPtr(), (void*)this);
        outstanding_writes_.erase(outstanding_writes_.begin());
    }

    template<class MemoryWriteType>
    void CachedMemory<MemoryWriteType>::dropWrite(const MemoryWriteType & write_to_drop)
    {
        sparta_assert(false == outstanding_writes_.empty(),
                      "There are no outstanding writes for dropping");

        if(SPARTA_EXPECT_FALSE(outstanding_writes_.find(write_to_drop.getWriteID()) == outstanding_writes_.end()))
        {
            std::stringstream msg;
            msg << __FUNCTION__ << ": error: attempting to drop a write that is not known by this CachedMemory: "
                << write_to_drop;
            msg << "\nOutstanding writes for write ID " << write_id_ << " (oldest to newest):\n";
            for(const auto & outstanding_writes : outstanding_writes_) {
                msg << "\t" << outstanding_writes << "\n";
            }
            throw SpartaException(msg.str());
        }

        // Drop all the writes from the newest up to and including the
        // dropped write
        auto currently_flushing_write = outstanding_writes_.rbegin();
        while(currently_flushing_write != outstanding_writes_.rend())
        {
            const uint64_t current_flushed_wuid = currently_flushing_write->first;
            const auto & current_flushed_maw = currently_flushing_write->second;
            cached_memory_.write(current_flushed_maw.getPAddr(),
                                 current_flushed_maw.getSize(),
                                 current_flushed_maw.getPrevDataPtr());

            // Drop the write
            outstanding_writes_.erase((++currently_flushing_write).base());

            if(current_flushed_wuid == write_to_drop.getWriteID()) {
                break;
            }

            currently_flushing_write = outstanding_writes_.rbegin();
        }
    }

    template<class MemoryWriteType>
    void CachedMemory<MemoryWriteType>::mergeWrite(addr_t paddr, addr_t size, const uint8_t * buf)
    {
        if(getNumOutstandingWrites() > 0) {
            // Brute force method, but works.  Take one byte at a time and
            // look for collisions.  If there are any, don't store it, but
            // update that outstanding store's previous value.  Start with
            // the oldest write and move to the newest.
            for(addr_t byte = 0; byte < size; ++byte)
            {
                const auto paddr_offset = paddr + byte;
                bool collision = false;
                for(auto & [wuid, maw] : outstanding_writes_)
                {
                    const auto maw_start_paddr = maw.getPAddr();
                    const auto maw_size  = maw.getSize();
                    if((paddr_offset >= maw_start_paddr) && (paddr_offset < (maw_start_paddr + maw_size)))
                    {
                        // Found a collision with this byte.  Update the
                        // previous value with the merge data
                        collision = true;
                        const uint32_t maw_byte = paddr_offset - maw_start_paddr;
                        maw.getPrevDataPtr()[maw_byte] = *(buf + byte);
                        break;
                    }
                }
                if(!collision) {
                    // Write the one byte of data -- no collisions
                    cached_memory_.write(paddr_offset, 1, buf + byte);
                }
            }
        }
        else {
            cached_memory_.write(paddr, size, buf);
        }
    }


}
