// <DatabaseCheckpoint> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/CheckpointBase.hpp"
#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"
#include "sparta/serialization/checkpoint/VectorStorage.hpp"

namespace sparta::serialization::checkpoint
{
    class DatabaseCheckpointer;
    class DatabaseCheckpoint;

    struct ChkptWindowBytes {
        using chkpt_id_t = CheckpointBase::chkpt_id_t;
        std::vector<char> chkpt_bytes;
        chkpt_id_t start_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        chkpt_id_t end_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        uint64_t start_tick = 0;
        uint64_t end_tick = 0;
    };

    struct ChkptWindow {
        using chkpt_id_t = CheckpointBase::chkpt_id_t;
        std::vector<std::shared_ptr<DatabaseCheckpoint>> chkpts;
        chkpt_id_t start_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        chkpt_id_t end_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        uint64_t start_tick = 0;
        uint64_t end_tick = 0;

        //! \brief Support boost::serialization
        template <typename Archive>
        void serialize(Archive& ar, const unsigned int /*version*/);
    };

    /*!
    * \brief Checkpoint class optimized for use with database-backed
    * checkpointers.
    */
    class DatabaseCheckpoint : public CheckpointBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Default constructable required for boost::serialization
        DatabaseCheckpoint() = default;

        //! \brief Not copy constructable
        DatabaseCheckpoint(const DatabaseCheckpoint&) = delete;

        //! \brief Non-assignable
        DatabaseCheckpoint& operator=(const DatabaseCheckpoint&) = delete;

        //! \brief Move constructor
        DatabaseCheckpoint(DatabaseCheckpoint&&) = default;

        //! \brief Not move-assignable
        DatabaseCheckpoint& operator=(DatabaseCheckpoint&&) = delete;

    private:

        //! \brief Construction to be performed by friend class DatabaseCheckpointer
        DatabaseCheckpoint(TreeNode& root,
                           const std::vector<ArchData*>& dats,          
                           chkpt_id_t id,
                           tick_t tick,
                           DatabaseCheckpoint* prev,
                           bool is_snapshot,
                           DatabaseCheckpointer* checkpointer);

        //! \brief This constructor is called during checkpoint cloning
        DatabaseCheckpoint(chkpt_id_t id,
                           tick_t tick,
                           chkpt_id_t prev_id,
                           const std::vector<chkpt_id_t>& next_ids,
                           chkpt_id_t deleted_id,
                           bool is_snapshot,
                           const storage::VectorStorage& storage,
                           DatabaseCheckpointer* checkpointer);

        ////////////////////////////////////////////////////////////////////////
        //! @}

        friend class DatabaseCheckpointer;

    public:

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version) {
            sparta_assert(deleted_id_ == CheckpointBase::UNIDENTIFIED_CHECKPOINT,
                          "Cannot serialize a DatabaseCheckpoint that was already deleted");

            CheckpointBase::serialize(ar, version);
            ar & prev_id_;
            ar & next_ids_;
            ar & is_snapshot_;
            ar & data_;
        }

        /*!
         * \brief Returns a string describing this object
         */
        std::string stringize() const override;

        /*!
         * \brief Writes all checkpoint raw data to an ostream
         * \param o ostream to which raw data will be written
         * \note No newlines or other extra characters will be appended
         */
        void dumpData(std::ostream& o) const override;

        /*!
         * \brief Returns memory usage by this checkpoint
         */
        uint64_t getTotalMemoryUse() const noexcept override;

        /*!
         * \brief Returns memory usage by the content of this checkpoint
         */
        uint64_t getContentMemoryUse() const noexcept override;

        /*!
         * \brief Returns a stack of checkpoints from this checkpoint as far
         * back as possible until no previous link is found. This is a superset
         * of getRestoreChain and contains checkpoints that do not actually need
         * to be inspected for restoring this checkpoint's data. This may reach
         * the head checkpoint if no gaps are encountered.
         */
        std::stack<chkpt_id_t> getHistoryChain() const;

        /*!
         * \brief Returns a stack of checkpoints that must be restored from
         * top-to-bottom to fully restore the state associated with this
         * checkpoint.
         */
        std::stack<chkpt_id_t> getRestoreChain() const;

        /*!
         * \brief Get the ID of our previous checkpoint. Returns UNIDENTIFIED_CHECKPOINT
         * if we have no previous checkpoint, as is the case with the head checkpoint
         * and snapshots.
         */
        chkpt_id_t getPrevID() const override;

        /*!
         * \brief Returns next checkpoint following *this. May be an empty
         * vector if there are no later checkpoints.
         */
        std::vector<chkpt_id_t> getNextIDs() const override;

        /*!
         * \brief Attempts to restore this checkpoint including any previous
         * deltas (dependencies).
         *
         * Uses loadState to restore state from each checkpoint in the
         * restore chain.
         */
        void load(const std::vector<ArchData*>& dats) override;

        /*!
         * \brief Indicates whether this checkpoint has been flagged deleted.
         * \note Does not imply that the checkpoint can safely be deleted;
         * only that it was flagged for deletion.
         * \note If false, Checkpoint ID will also be UNIDENTIFIED_CHECKPOINT
         * \see flagDeleted()
         */
        bool isFlaggedDeleted() const noexcept;

        /*!
         * \brief Return the ID had by this checkpoint before it was deleted
         * If this checkpoint has not been flagged for deletion, this will be
         * UNIDENTIFIED_CHECKPOINT
         */
        chkpt_id_t getDeletedID() const noexcept;

        /*!
         * \brief Gets the representation of this deleted checkpoint as part of
         * a checkpoint chain (if that checkpointer supports deletion)
         * \return "D-" concatenate with ID copied when being deleted. Returns
         * the ID if not yet deleted
         */
        std::string getDeletedRepr() const override;

        /*!
         * \brief Is this checkpoint a snapshot (contains ALL simulator state)
         */
        bool isSnapshot() const noexcept;

        /*!
         * \brief Determines how many checkpoints away the closest, earlier
         * snapshot is.
         * \return distance to closest snapshot. If this node is a snapshot,
         * returns 0; if immediate getPrev() is a snapshot, returns 1; and
         * so on.
         *
         * \note This is a noexcept function, which means that the exception if
         * no snapshot is encountered is uncatchable. This is intentional.
         */
        uint32_t getDistanceToPrevSnapshot() const noexcept;

        /*!
         * \brief Loads delta state of this checkpoint to root.
         * Does not look at any other checkpoints checkpoints.
         * \see load
         */
        void loadState(const std::vector<ArchData*>& dats);

        /*!
         * \brief Create a deep copy of this checkpoint.
         */
        std::unique_ptr<DatabaseCheckpoint> clone() const;

    private:

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage
         * \pre Must not have already stored data for this checkpoint
         * This should only be called at construction
         */
        void storeSnapshot_(const std::vector<ArchData*>& dats);

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage
         * \pre Must not have already stored data for this checkpoint
         * This should only be called at construction
         */
        void storeDelta_(const std::vector<ArchData*>& dats);

        /*!
         * \brief Allows this checkpoint to be deleted if it is no longer a
         * previous delta of some other delta (i.e. getNexts() returns an
         * empty vector). Sets the checkpoint ID to invalid. Calling multiple
         * times has no effect
         * \pre Must not already be flagged deleted
         * \post isFlaggedDeleted() will return true
         * \post getDeletedID() will return the current ID (if any)
         * \see canDelete
         * \see isFlaggedDeleted
         */
        void flagDeleted_();

        /*!
         * \brief ID of the previous checkpoint.
         */
        chkpt_id_t prev_id_;

        /*!
         * \brief IDs of the next checkpoints.
         */
        std::vector<chkpt_id_t> next_ids_;

        /*!
         * \brief ID of the checkpoint before it was deleted. This is invalid
         * until deletion. Prevents misuse of checkpoint ID or any confusion
         * about whether it is deleted or not.
         */
        chkpt_id_t deleted_id_;

        //! \brief Is this node a snapshot?
        bool is_snapshot_;

        //! \brief Storage implementation
        storage::VectorStorage data_;

        //! \brief Checkpointer who created us
        DatabaseCheckpointer* checkpointer_ = nullptr;
    };

    //! Defined down here for "new DatabaseCheckpoint"
    template <typename Archive>
    inline void ChkptWindow::serialize(Archive& ar, const unsigned int /*version*/) {
        ar & start_chkpt_id;
        ar & end_chkpt_id;
        ar & start_tick;
        ar & end_tick;

        if (chkpts.empty()) {
            // We are loading a checkpoint window from disk
            const auto num_chkpts = end_chkpt_id - start_chkpt_id + 1;
            for (size_t i = 0; i < num_chkpts; ++i) {
                chkpts.emplace_back(new DatabaseCheckpoint);
                ar & *chkpts.back();
            }
        } else {
            // We are saving a checkpoint window to disk
            for (auto& chkpt : chkpts) {
                ar & *chkpt;
            }
        }
    }

} // namespace sparta::serialization::checkpoint
