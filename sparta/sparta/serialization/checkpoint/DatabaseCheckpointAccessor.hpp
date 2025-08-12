// <DatabaseCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/DatabaseCheckpointBase.hpp"

namespace sparta::serialization::checkpoint
{

class DatabaseCheckpoint;
class DatabaseCheckpointer;

/*!
 * \brief This class wraps a DatabaseCheckpoint and recreates it from disk
 * if the checkpoint no longer exists in the checkpointer in memory.
 */
template <bool IsConst=false>
class DatabaseCheckpointAccessor : public DatabaseCheckpointBase
{
public:
    using db_checkpointer = std::conditional_t<IsConst, const DatabaseCheckpointer, DatabaseCheckpointer>;
    using db_checkpoint = std::conditional_t<IsConst, const DatabaseCheckpoint, DatabaseCheckpoint>;

    //! Constructor
    DatabaseCheckpointAccessor(db_checkpointer* checkpointer, chkpt_id_t id);

    //! Moves allowed
    DatabaseCheckpointAccessor(DatabaseCheckpointAccessor&&) = default;

    //! Copies disallowed
    DatabaseCheckpointAccessor(const DatabaseCheckpointAccessor&) = delete;

    //! Move assignment disallowed
    DatabaseCheckpointAccessor& operator=(DatabaseCheckpointAccessor&&) = delete;

    //! Copy assignment disallowed
    DatabaseCheckpointAccessor& operator=(const DatabaseCheckpointAccessor&) = delete;

    //! For parity with all the other in-memory checkpoint types.
    DatabaseCheckpointAccessor* operator->() { return this; }

    //! For parity with all the other in-memory checkpoint types.
    const DatabaseCheckpointAccessor* operator->() const { return this; }

    //! Destructor
    ~DatabaseCheckpointAccessor();

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
     * \brief Returns memory usage by this checkpoint including any
     * framework data structures
     */
    uint64_t getTotalMemoryUse() const noexcept override;

    /*!
     * \brief Returns memory usage by this checkpoint solely for the
     * checkpointed content.
     */
    uint64_t getContentMemoryUse() const noexcept override;

    /*!
     * \brief Attempts to restore this checkpoint state to the simulation
     * state (ArchData) objects given to this Checkpoint at construction
     */
    void load(const std::vector<ArchData*>& dats) override;

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
     * \brief Gets the representation of this deleted checkpoint as part of
     * a checkpoint chain (if that checkpointer supports deletion)
     */
    std::string getDeletedRepr() const override;

    /*!
     * \brief Returns a stack of checkpoints from this checkpoint as far
     * back as possible until no previous link is found. This is a superset
     * of getRestoreChain and contains checkpoints that do not actually need
     * to be inspected for restoring this checkpoint's data. This may reach
     * the head checkpoint if no gaps are encountered.
     */
    std::stack<chkpt_id_t> getHistoryChain() const override;

    /*!
     * \brief Returns a stack of checkpoints that must be restored from
     * top-to-bottom to fully restore the state associated with this
     * checkpoint.
     */
    std::stack<chkpt_id_t> getRestoreChain() const override;

    /*!
     * \brief Can this checkpoint be deleted
     * Cannot be deleted if:
     * \li This checkpoint has any ancestors which are not deletable and not snapshots
     * \li This checkpoint was not flagged for deletion with flagDeleted
     * \warning This is a recursive search of a checkpoint tree which has potentially many
     * branches and could have high time cost
     */
    bool canDelete() const noexcept override;

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
    void flagDeleted() override;

    /*!
     * \brief Indicates whether this checkpoint has been flagged deleted.
     * \note Does not imply that the checkpoint can safely be deleted;
     * only that it was flagged for deletion.
     * \note If false, Checkpoint ID will also be UNIDENTIFIED_CHECKPOINT
     * \see flagDeleted()
     */
    bool isFlaggedDeleted() const noexcept override;

    /*!
     * \brief Return the ID had by this checkpoint before it was deleted
     * If this checkpoint has not been flagged for deletion, this will be
     * UNIDENTIFIED_CHECKPOINT
     */
    chkpt_id_t getDeletedID() const noexcept override;

    /*!
     * \brief Is this checkpoint a snapshot (contains ALL simulator state)
     */
    bool isSnapshot() const noexcept override;

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
    uint32_t getDistanceToPrevSnapshot() const noexcept override;

    /*!
     * \brief Loads delta state of this checkpoint to root.
     * Does not look at any other checkpoints checkpoints.
     * \see load
     */
    void loadState(const std::vector<ArchData*>& dats) override;

private:
    db_checkpointer* checkpointer_;
    chkpt_id_t id_;
};

} // namespace sparta::serialization::checkpoint

#include "sparta/serialization/checkpoint/DatabaseCheckpointAccessor.tpp"
