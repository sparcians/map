// <DatabaseCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/Checkpointer.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpoint.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpointAccessor.hpp"
#include "simdb/apps/App.hpp"
#include "simdb/pipeline/Pipeline.hpp"

//! Default threshold for creating snapshots
#ifndef DEFAULT_SNAPSHOT_THRESH
#define DEFAULT_SNAPSHOT_THRESH 20
#endif

namespace sparta::serialization::checkpoint
{

class DatabaseCheckpointer;
class DatabaseCheckpointQuery;

/*!
 * \brief Implementation of the FastCheckpointer which only holds
 * a "window" of checkpoints in memory at any given time, and sends
 * checkpoints outside this window to/from SimDB.
 */
class DatabaseCheckpointer : public simdb::App, public Checkpointer
{
public:
    static constexpr auto NAME = "db-checkpointer";

    using checkpoint_type = DatabaseCheckpoint;

    /*!
     * \brief FastCheckpointer Constructor
     *
     * \param db_mgr SimDB instance to use as a backing store for all checkpoints.
     *
     * \param root TreeNode at which checkpoints will be taken.
     *             This cannot be changed later. This does not
     *             necessarily need to be a RootTreeNode. Before
     *             the first checkpoint is taken, this node must
     *             be finalized (see
     *             sparta::TreeNode::isFinalized). At this point,
     *             the node does not need to be finalized
     *
     * \param sched Scheduler to read and restart on checkpoint restore (if
     *              not nullptr)
     */
    DatabaseCheckpointer(simdb::DatabaseManager* db_mgr, TreeNode& root, Scheduler* sched=nullptr);

    /*!
     * \brief Define the SimDB schema for this checkpointer.
     */
    static void defineSchema(simdb::Schema& schema);

    /*!
     * \brief Instantiate the async processing pipeline to save/load checkpoints.
     */
    std::unique_ptr<simdb::pipeline::Pipeline> createPipeline(
        simdb::pipeline::AsyncDatabaseAccessor* db_accessor) override;

    /*!
     * \brief Returns the next-shapshot threshold.
     *
     * This represents the distance between two checkpoints required for the
     * checkpointer to automatically place a snapshot checkpoint instead of
     * a delta. A threshold of 0 or 1 results in all checkpoints being
     * snapshots. A value of 10 results in every 10th checkpoint being a
     * snapshot. Explicit snapshot creation using createCheckpoint can interrupt
     * and restart this pattern.
     *
     * This value is a performance/space tradeoff knob.
     */
    uint32_t getSnapshotThreshold() const noexcept;

    /*!
     * \brief Sets the snapshot threshold
     * \see getSnapshotThreshold
     */
    void setSnapshotThreshold(uint32_t thresh) noexcept;

    /*!
     * \brief Computes and returns the memory usage by this checkpointer at
     * this moment including any framework overhead
     * \note This is an approxiation and does not include some of
     * minimal dynamic overhead from stl containers.
     */
    uint64_t getTotalMemoryUse() const noexcept override;

    /*!
     * \brief Computes and returns the memory usage by this checkpointer at
     * this moment purely for the checkpoint state being held
     */
    uint64_t getContentMemoryUse() const noexcept override;

    /*!
     * \brief Deletes a checkpoint by ID.
     * \param id ID of checkpoint to delete. Must not be
     * Checkpoint::UNIDENTIFIED_CHECKPOINT and must not be equal to the
     * ID of the head checkpoint.
     * \throw CheckpointError if this manager has no checkpoint with given
     * id. Test with hasCheckpoint first. If id ==
     * Checkpoint::UNIDENTIFIED_CHECKPOINT, always throws.
     * Throws if id == getHeadID(). Head cannot be deleted
     *
     * Internally, this deletion may be effective-only and actual data may
     * still exist in an incaccessible form as part of the checkpoint
     * tree implementation.
     *
     * If the current checkpoint is deleted, current will be updated back
     * along the current checkpoints previous checkpoint chain until a non
     * deleted checkpoint is found. This will become the new current
     * checkpoint
     */
    void deleteCheckpoint(chkpt_id_t id) override;

    /*!
     * \brief Loads state from a specific checkpoint by ID
     * \note Does not delete checkpoints. Checkpoints must be explicitly
     * deleted by deleteCheckpoint
     * \throw CheckpointError if id does not refer to checkpoint that exists
     * or if checkpoint could not be load.
     * \warning If checkpoint fails during loading for reasons other than an
     * invalid ID, the simulation state could be corrupt
     * \post current checkpoint is now the checkpoint specified by id
     * \post Sets scheduler current tick to the checkpoint's tick using
     * Scheduler::restartAt
     */
    void loadCheckpoint(chkpt_id_t id) override;

    /*!
     * \brief Gets all checkpoints taken at tick t on any timeline.
     * \param t Tick number at which checkpoints should found.
     * \return vector of valid checkpoint IDs (never
     * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
     * \note Makes a new vector of results. This should not be called in the
     * critical path.
     */
    std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override;

    /*!
     * \brief Gets all checkpoint IDs available on any timeline sorted by
     * tick (or equivalently checkpoint ID).
     * \return vector of valid checkpoint IDs (never
     * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
     * \note Makes a new vector of results. This should not be called in the
     * critical path.
     */
    std::vector<chkpt_id_t> getCheckpoints() const override;

    /*!
     * \brief Gets the current number of checkpoints having valid IDs
     */
    uint32_t getNumCheckpoints() const noexcept override;

    /*!
     * \brief Gets the current number of snapshots with valid IDs
     */
    uint32_t getNumSnapshots() const noexcept;

    /*!
     * \brief Gets the current number of delta checkpoints with valid IDs
     */
    uint32_t getNumDeltas() const noexcept;

    /*!
     * \brief Gets the curent number of checkpoints (delta or snapshot)
     * withOUT valid IDs.
     */
    uint32_t getNumDeadCheckpoints() const noexcept;

    /*!
     * \brief Debugging utility which gets a deque of checkpoints
     * representing a chain starting at the checkpoint head and ending at
     * the checkpoint specified by \a id. Ths results can contain
     * Checkpoint::UNIDENTIFIED_CHECKPOINT to represent temporary
     * deleted checkpoints in the chain.
     * \param id ID of checkpoint that terminates the chain
     * \return dequeue of checkpoint IDs where the front is always the head
     * and the back is always the checkpoint described by \a id. If there is
     * no checkpoint head, returns an empty result
     * \throw CheckpointError if \a id does not refer to a valid
     * checkpoint.
     * \note Makes a new vector of results. This should not be called in the
     * critical path.
     */
    std::deque<chkpt_id_t> getCheckpointChain(chkpt_id_t id) const override;

    /*!
     * \brief Finds the latest checkpoint at or before the given tick
     * starting at the \a from checkpoint and working backward.
     * If no checkpoints before or at tick are found, returns nullptr.
     * \param tick Tick to search for
     * \param from Checkpoint at which to begin searching for a tick.
     * Must be a valid checkpoint known by this checkpointer.
     * See hasCheckpoint.
     * \return The latest checkpoint with a tick number less than or equal
     * to the \a tick argument. Returns nullptr if no checkpoints before \a
     * tick were found. It is possible for the checkpoint identified by \a
     * from could be returned.
     * \warning This is not a high-performance method. Generally,
     * a client of this interface knows a paticular ID.
     * \throw CheckpointError if \a from does not refer to a valid
     * checkpoint.
     */
    DatabaseCheckpointAccessor<false> findLatestCheckpointAtOrBefore(tick_t tick, chkpt_id_t from);

    /*!
     * \brief Finds a checkpoint by its ID
     * \param id ID of checkpoint to find. Guaranteed not to be flagged as
     * deleted
     * \return Checkpoint with ID of \a id if found or nullptr if not found
     */
    DatabaseCheckpointAccessor<false> findCheckpoint(chkpt_id_t id);

    /*!
     * \brief Tests whether this checkpoint manager has a checkpoint with
     * the given id.
     * \return True if id refers to a checkpoint held by this checkpointer
     * and false if not. If id == Checkpoint::UNIDENTIFIED_CHECKPOINT,
     * always returns false
     */
    bool hasCheckpoint(chkpt_id_t id) const noexcept override;

    /*!
     * \brief Dumps the restore chain for this checkpoint.
     * \see getRestoreChain()
     * \param o ostream to which chain data will be dumped
     * \param id ID of starting checkpoint
     */
    void dumpRestoreChain(std::ostream& o, chkpt_id_t id) const;

    /*!
     * \brief Returns a stack of checkpoints from this checkpoint as far
     * back as possible until no previous link is found. This is a superset
     * of getRestoreChain and contains checkpoints that do not actually need
     * to be inspected for restoring this checkpoint's data. This may reach
     * the head checkpoint if no gaps are encountered.
     */
    std::stack<chkpt_id_t> getHistoryChain(chkpt_id_t id) const;

    /*!
     * \brief Returns a stack of checkpoints that must be restored from
     * top-to-bottom to fully restore the state associated with this
     * checkpoint.
     */
    std::stack<chkpt_id_t> getRestoreChain(chkpt_id_t id) const;

    /*!
     * \brief Returns next checkpoint following *this. May be an empty
     * vector if there are no later checkpoints.
     */
    std::vector<chkpt_id_t> getNextIDs(chkpt_id_t id) const;

    /*!
     * \brief Attempts to restore this checkpoint including any previous
     * deltas (dependencies).
     *
     * Uses loadState to restore state from each checkpoint in the
     * restore chain.
     */
    void load(const std::vector<ArchData*>& dats, chkpt_id_t id);

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
    uint32_t getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept;

    /*!
     * \brief Check if the given checkpoint is a snapshot (not a delta).
     * \return Returns false if not a snapshot or the id is not a checkpoint.
     */
    bool isSnapshot(chkpt_id_t id) const noexcept;

    /*!
     * \brief Can this checkpoint be deleted
     * Cannot be deleted if:
     * \li This checkpoint has any ancestors which are not deletable and not snapshots
     * \li This checkpoint was not flagged for deletion with flagDeleted
     * \warning This is a recursive search of a checkpoint tree which has potentially many
     * branches and could have high time cost
     */
    bool canDelete(chkpt_id_t id) const noexcept;

    /*!
     * \brief Returns a string describing this object
     */
    std::string stringize() const override;

    /*!
     * \brief Dumps this checkpointer's flat list of checkpoints to an
     * ostream with a newline following each checkpoint
     * \param o ostream to dump to
     */
    void dumpList(std::ostream& o) const override;

    /*!
     * \brief Dumps this checkpointer's data to an ostream with a newline
     * following each checkpoint
     * \param o ostream to dump to
     */
    void dumpData(std::ostream& o) const override;

    /*!
     * \brief Dumps this checkpointer's data to an
     * ostream with annotations between each ArchData and a newline
     * following each checkpoint description and each checkpoint data dump
     * \param o ostream to dump to
     */
    void dumpAnnotatedData(std::ostream& o) const override;

    /*!
     * \brief Debugging utility which dumps values in some bytes across a
     * chain of checkpoints. The intent is to show the values loaded when
     * attempting to restore needed to restore the given value in the
     * selected checkpoint
     * \param o ostream with each value and checkpoint ID will be printed
     * \param id ID of checkpoint to "restore" value from
     * \param container ArchData in which the data being traced lives
     * \param offset Offset into \a container
     * \param size Bytes to read at \a offset
     * \warning This may change checkpoint data read/write state and should
     * only be done between completed checkpoints saves/restores in order to
     * not interfere.
     */
    void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) override;

private:

    /*!
     * \brief Create a head node.
     * \pre ArchDatas for tree root are already enumerated
     * \pre Tree of getRoot() is already finalized
     * \pre Guaranteed to have a null head at this time
     * (getHead() == nullptr)
     * \post Must create a head checkpoint
     * \post Must invoke setHead_
     * \note invoked by createHead
     */
    void createHead_() override;

    /*!
     * \brief Create a checkpoint
     * \pre Guaranteed to have a valid head at this time
     * (getHead() != nullptr)
     * \post Must create a checkpoint
     * \return Must return a checkpoint ID not currently in use
     * \note invoked by createHead
     */
    chkpt_id_t createCheckpoint_(bool force_snapshot=false) override;

    /*!
     * \brief Delete given checkpoint and all contiguous previous
     * checkpoints which can be deleted (See checkpoint_type::canDelete).
     * This is the only place where checkpoint objects are actually freed
     * (aside from destruction) and it ensures that they will not disrupt
     * the checkpoint delta chains. All other deletion is simply flagging
     * and re-identifying checkpoints
     * \param d Checkpoint to attempt to delete first. Function will then
     * move through each previous checkpoint until reaching head.
     * \post Head checkpoint will never be deleted by this function
     * \note Never flags any new checkpoints as deleted
     */
    void cleanupChain_(chkpt_id_t id);

    /*!
     * \brief Look forward to see if any future checkpoints depend on \a d.
     * \param d checkpoint to inspect and recursively search
     * \return true if the current checkpoint or any live checkpoints
     * are hit in the search. Search terminates on each branch when a
     * snapshot or the end of the branch is reached. The branch to inspect
     * (\a d) will not be checked itself since the point is to determine
     * which branches down-chain depend on it.
     */
    bool recursForwardFindAlive_(chkpt_id_t id) const;

    /*!
     * \brief Attempts to find a checkpoint within this checkpointer by ID.
     * \param id Checkpoint ID to search for
     * \return Pointer to found checkpoint with matchind ID. If not found,
     * returns nullptr.
     * \todo Faster lookup?
     */
    DatabaseCheckpointAccessor<false> findCheckpoint_(chkpt_id_t id) noexcept;

    /*!
     * \brief Const version of findCheckpoint_()
     */
    DatabaseCheckpointAccessor<true> findCheckpoint_(chkpt_id_t id) const noexcept;

    /*!
     * \brief Implements Checkpointer::dumpCheckpointNode_
     */
    void dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) const override;

    /*!
     * \brief Returns IDs of the checkpoints immediately following the given checkpoint.
     */
    std::vector<chkpt_id_t> getNextIDs_(chkpt_id_t id) const override;

    /*!
     * \brief Intercept calls to Checkpointer::setHead_() and ensure we do not delete it.
     */
    void setHead_(CheckpointBase* head) override;

    /*!
     * \brief Intercept calls to Checkpointer::setCurrent_() and ensure we do not delete it.
     * Also take this time to "unbless" the previous current node.
     */
    void setCurrent_(CheckpointBase* current) override;

    /*!
     * \brief Set ID of head checkpoint.
     */
    void setHeadID_(chkpt_id_t id);

    /*!
     * \brief Set ID of current checkpoint.
     */
    void setCurrentID_(chkpt_id_t id);

    /*!
     * \brief Add the given checkpoint to the cache and start processing it.
     */
    void addToCache_(std::shared_ptr<checkpoint_type> chkpt);

    /*!
     * \brief Clone the next checkpoint that is ready for processing.
     */
    bool cloneNextPipelineHeadCheckpoint_(std::shared_ptr<checkpoint_type>& next);

    //! \brief Checkpointer head ID. Used to prevent the head from being deleted from the cache.
    chkpt_id_t head_id_ = checkpoint_type::UNIDENTIFIED_CHECKPOINT;

    //! \brief Checkpointer current ID. Used to prevent the current node from being deleted from the cache.
    chkpt_id_t current_id_ = checkpoint_type::UNIDENTIFIED_CHECKPOINT;

    //! \brief Subset (or all of) our checkpoints that we currently are holding in memory.
    std::unordered_map<chkpt_id_t, std::shared_ptr<checkpoint_type>> chkpts_cache_;

    //! \brief Ordered running list of checkpoint IDs that come in via calls to createCheckpoint_().
    //! This is used in the pipeline to pick off and start processing checkpoints in the same order
    //! in which they were received, while keeping the cache designed to use an unordered_map for
    //! random access.
    std::queue<chkpt_id_t> chkpt_ids_for_pipeline_head_;

    //! \brief SQLite query object to "extend" the checkpoint search space from just the
    //! cache to include the database. Combinations of in-memory checkpoints, recreated
    //! checkpoints, and database schema/query optimizations are used for performance.
    std::shared_ptr<DatabaseCheckpointQuery> chkpt_query_;

    //! \brief Mutex to protect our checkpoints cache.
    mutable std::mutex mutex_;

    //! \brief SimDB instance
    simdb::DatabaseManager* db_mgr_ = nullptr;

    /*!
     * \brief Snapshot generation threshold. Every n checkpoints in a chain
     * are taken as snapshots instead of deltas
     */
    uint32_t snap_thresh_;

    /*!
     * \brief Next checkpoint ID value
     */
    chkpt_id_t next_chkpt_id_;

    /*!
     * \brief Number of living checkpoints of either snapshot or delta type.
     * (where checkpoint isFlaggedDeleted()=false)
     */
    uint32_t num_alive_checkpoints_;

    /*!
     * \brief Number of living snapshot checkpoints (where checkpoint
     * isFlaggedDeleted()=false). Will be <= num_alive_checkpoints_
     * The number of delta checkpoints (not snapshots) can be computed as
     * num_alive_checkpoints_ - num_alive_snapshots_.
     */
    uint32_t num_alive_snapshots_;

    /*!
     * \brief Number of checkpoints which have been flagged as deleted but
     * still exist in the checkpointer.
     */
    uint32_t num_dead_checkpoints_;
};

} // namespace sparta::serialization::checkpoint

namespace simdb
{

/*!
 * \brief This AppFactory specialization is provided since we have an app that inherits
 * from FastCheckpointer, and thus cannot have the default app subclass ctor signature
 * that only takes the DatabaseManager like most other apps.
 */
template <>
class AppFactory<sparta::serialization::checkpoint::DatabaseCheckpointer> : public AppFactoryBase
{
public:
    using AppT = sparta::serialization::checkpoint::DatabaseCheckpointer;

    void setSpartaElems(sparta::TreeNode& root, sparta::Scheduler* sched = nullptr)
    {
        root_ = &root;
        sched_ = sched;
    }

    AppT* createApp(DatabaseManager* db_mgr) override
    {
        if (!root_) {
            throw sparta::SpartaException("Must set root (and maybe scheduler) before instantiating apps!");
        }

        // Make the ctor call that the default AppFactory cannot make.
        return new AppT(db_mgr, *root_, sched_);
    }

    void defineSchema(Schema& schema) const override
    {
        AppT::defineSchema(schema);
    }

private:
    sparta::TreeNode* root_ = nullptr;
    sparta::Scheduler* sched_ = nullptr;
};

} // namespace simdb
