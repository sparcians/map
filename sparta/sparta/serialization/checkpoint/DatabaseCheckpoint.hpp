// <DatabaseCheckpoint> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/CheckpointBase.hpp"
#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"
#include "sparta/serialization/checkpoint/VectorStorage.hpp"

namespace sparta::serialization::checkpoint
{
    class DatabaseCheckpointer;
    class DatabaseCheckpoint;

    /*!
     * \brief A window of checkpoints to be sent to/from the database as a unit.
     * \note A "window" is defined as a group of (snap_thresh_ + 1) checkpoints,
     * where the first checkpoint in the window is a snapshot and the remaining
     * checkpoints in the window are deltas. Checkpoints are processed this way
     * to enable various performance optimizations.
     */
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
     * \brief Compressed version of ChkptWindow to be stored in the database.
     */
    struct ChkptWindowBytes {
        using chkpt_id_t = CheckpointBase::chkpt_id_t;
        std::vector<char> chkpt_bytes;
        chkpt_id_t start_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        chkpt_id_t end_chkpt_id = CheckpointBase::UNIDENTIFIED_CHECKPOINT;
        uint64_t start_tick = 0;
        uint64_t end_tick = 0;
    };

    /*!
    * \brief Checkpoint class optimized for use with database-backed
    * checkpointers.
    */
    class DatabaseCheckpoint final : public CheckpointBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not copy constructable
        DatabaseCheckpoint(const DatabaseCheckpoint&) = delete;

        //! \brief Non-assignable
        DatabaseCheckpoint& operator=(const DatabaseCheckpoint&) = delete;

        //! \brief Not move constructable
        DatabaseCheckpoint(DatabaseCheckpoint&&) = delete;

        //! \brief Not move assignable
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

        //! \brief Default constructable required for boost::serialization of ChkptWindow
        DatabaseCheckpoint() = default;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        friend class ChkptWindow;
        friend class DatabaseCheckpointer;

    public:

        /*
         * \brief Support boost::serialization
         */
        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version) {
            CheckpointBase::serialize(ar, version);
            ar & prev_id_;
            ar & next_ids_;
            ar & is_snapshot_;
            ar & data_;
        }

        /*!
         * \brief Returns a string describing this object.
         */
        std::string stringize() const override;

        /*!
         * \brief Writes all checkpoint raw data to an ostream.
         * \param o ostream to which raw data will be written.
         * \note No newlines or other extra characters will be appended.
         */
        void dumpData(std::ostream& o) const override;

        /*!
         * \brief Returns memory usage by this checkpoint.
         */
        uint64_t getTotalMemoryUse() const noexcept override;

        /*!
         * \brief Returns memory usage by the content of this checkpoint.
         */
        uint64_t getContentMemoryUse() const noexcept override;

        /*!
         * \brief Returns a stack of checkpoints from this checkpoint as far
         * back as possible until no previous link is found.
         *
         * \note Since this checkpointer enforces a linear chain of checkpoints
         * with no gaps, this always reaches the head checkpoint.
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
         * only for the head checkpoint.
         */
        chkpt_id_t getPrevID() const override;

        /*!
         * \brief Returns next checkpoints following this one. May be an empty
         * vector if there are no later checkpoints.
         * 
         * \note Since this checkpointer enforces a linear chain of checkpoints
         * with no gaps, this vector will always have 0 or 1 elements.
         */
        std::vector<chkpt_id_t> getNextIDs() const override;

        /*!
         * \brief Attempts to restore this checkpoint including any previous
         * deltas (dependencies).
         *
         * \note Uses loadState to restore state from each checkpoint in the
         * restore chain.
         */
        void load(const std::vector<ArchData*>& dats) override;

        /*!
         * \brief Is this checkpoint a snapshot? If true, this checkpoint has
         * no dependencies and contains all simulator state.
         */
        bool isSnapshot() const noexcept;

        /*!
         * \brief Determines how many checkpoints away the closest, earlier
         * snapshot is.
         * 
         * \return distance to closest snapshot. If this node is a snapshot,
         * returns 0; if immediate getPrev() is a snapshot, returns 1; and
         * so on.
         */
        uint32_t getDistanceToPrevSnapshot() const noexcept;

        /*!
         * \brief Loads delta state of this checkpoint to root.
         * \note Does not look at any other checkpoints.
         * \see DatabaseCheckpointer::load
         */
        void loadState(const std::vector<ArchData*>& dats);

    private:

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage.
         *
         * \pre Must not have already stored data for this checkpoint.
         * 
         * \note This should only be called at construction
         */
        void storeSnapshot_(const std::vector<ArchData*>& dats);

        /*!
         * \brief Writes checkpoint data starting from current root to
         * checkpoint storage.
         * 
         * \pre Must not have already stored data for this checkpoint.
         * 
         * \note This should only be called at construction
         */
        void storeDelta_(const std::vector<ArchData*>& dats);

        //! \brief ID of the previous checkpoint.
        chkpt_id_t prev_id_;

        /*!
         * \brief IDs of the next checkpoints. Since this checkpointer
         * enforces a linear chain of checkpoints with no gaps, this vector
         * will always have 0 or 1 elements.
         */
        std::vector<chkpt_id_t> next_ids_;

        //! \brief Is this node a snapshot?
        bool is_snapshot_;

        //! \brief Storage implementation.
        storage::VectorStorage data_;

        //! \brief Checkpointer who created us.
        DatabaseCheckpointer* checkpointer_ = nullptr;
    };

    /*!
     * \brief Support boost::serialization for ChkptWindow.
     * \note Defined down here for "new DatabaseCheckpoint".
     */
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
