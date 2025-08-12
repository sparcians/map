// <DatabaseCheckpointBase> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/CheckpointBase.hpp"
#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"
#include "sparta/serialization/checkpoint/VectorStorage.hpp"

#include <unordered_set>

namespace sparta::serialization::checkpoint
{
    class DatabaseCheckpointer;

    /*!
    * \brief Checkpoint class optimized for use with database-backed
    * checkpointers.
    */
    class DatabaseCheckpointBase : public CheckpointBase
    {
    public:
        /*!
         * \brief Forwarding constructor
         */
        template <typename... Args>
        DatabaseCheckpointBase(Args&&... args)
            : CheckpointBase(std::forward<Args>(args)...)
        {}

        /*!
         * \brief Destructor
         */
        virtual ~DatabaseCheckpointBase() = default;

        /*!
         * \brief Returns a stack of checkpoints from this checkpoint as far
         * back as possible until no previous link is found. This is a superset
         * of getRestoreChain and contains checkpoints that do not actually need
         * to be inspected for restoring this checkpoint's data. This may reach
         * the head checkpoint if no gaps are encountered.
         */
        virtual std::stack<chkpt_id_t> getHistoryChain() const = 0;

        /*!
         * \brief Returns a stack of checkpoints that must be restored from
         * top-to-bottom to fully restore the state associated with this
         * checkpoint.
         */
        virtual std::stack<chkpt_id_t> getRestoreChain() const = 0;

        /*!
         * \brief Can this checkpoint be deleted
         * Cannot be deleted if:
         * \li This checkpoint has any ancestors which are not deletable and not snapshots
         * \li This checkpoint was not flagged for deletion with flagDeleted
         * \warning This is a recursive search of a checkpoint tree which has potentially many
         * branches and could have high time cost
         */
        virtual bool canDelete() const noexcept = 0;

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
        virtual void flagDeleted() = 0;

        /*!
         * \brief Indicates whether this checkpoint has been flagged deleted.
         * \note Does not imply that the checkpoint can safely be deleted;
         * only that it was flagged for deletion.
         * \note If false, Checkpoint ID will also be UNIDENTIFIED_CHECKPOINT
         * \see flagDeleted()
         */
        virtual bool isFlaggedDeleted() const noexcept = 0;

        /*!
         * \brief Return the ID had by this checkpoint before it was deleted
         * If this checkpoint has not been flagged for deletion, this will be
         * UNIDENTIFIED_CHECKPOINT
         */
        virtual chkpt_id_t getDeletedID() const noexcept = 0;

        /*!
         * \brief Is this checkpoint a snapshot (contains ALL simulator state)
         */
        virtual bool isSnapshot() const noexcept = 0;

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
        virtual uint32_t getDistanceToPrevSnapshot() const noexcept = 0;

        /*!
         * \brief Loads delta state of this checkpoint to root.
         * Does not look at any other checkpoints checkpoints.
         * \see load
         */
        virtual void loadState(const std::vector<ArchData*>& dats) = 0;
    };

} // namespace sparta::serialization::checkpoint
