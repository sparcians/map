// <FastCheckpointer> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>
#include <stack>
#include <queue>
#include <unordered_set>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

#include "sparta/serialization/checkpoint/DeltaCheckpoint.hpp"

//! Default threshold for creating snapshots
#ifndef DEFAULT_SNAPSHOT_THRESH
#define DEFAULT_SNAPSHOT_THRESH 20
#endif

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Implements quick checkpointing through delta-checkpoint trees
     * which store state-deltas in a compact format. State is retrieved through
     * a sparta tree from ArchDatas associated with any TreeNodes.
     *
     * With the goal of checkpoint saving and loading speed, this class does not
     * allow persistent checkpoint files (saved between session) because the
     * data format subject to change and very sensitive to the exact device tree
     * configuration
     *
     * A checkpoint tree may look something like the following, where each
     * checkpoint is shown by its simulation tick number (not ID)
     * \verbatim
     * t=0 (head/snapshot) --> t=100 +-> t=300
     *                     |
     *                     `-> t=320 --> t=400 +-> t=500
     *                     |                   `-> t=430
     *                     `-> t=300
     * \endverbatim
     *
     * The procedure for using the FastCheckpointer is generally:
     * \li Create SPARTA Tree
     * \li Construct FastCheckpointer
     * \li Finalize SPARTA Tree
     * \li Initialize simulation
     * \li FastCheckpointer::createHead
     *
     * Then:
     * \li <run simulation>
     * \li FastCheckpointer::createCheckpoint
     * \li <run simulation>
     * \li FastCheckpointer::loadCheckpoint
     * \li repeat in any order necessary
     * \endverbatim
     *
     * \todo Implement reverse delta storage for backward checkpoint loading
     * \todo Tune ArchData line size based on checkpointer performance
     * \todo More profiling
     * \todo Compression
     * \todo Saving to disk using a templated checkpoint object storage class (allowing for non-binary)
     */
    class FastCheckpointer : public Checkpointer
    {
    public:

        using checkpoint_type = DeltaCheckpoint<storage::VectorStorage>;
        using checkpoint_ptr = std::unique_ptr<checkpoint_type>;
        using checkpoint_ptrs = std::vector<checkpoint_ptr>;

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief FastCheckpointer Constructor
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
        FastCheckpointer(TreeNode& root, Scheduler* sched=nullptr) :
            Checkpointer(root, sched),
            snap_thresh_(DEFAULT_SNAPSHOT_THRESH),
            next_chkpt_id_(checkpoint_type::MIN_CHECKPOINT),
            num_alive_checkpoints_(0),
            num_alive_snapshots_(0),
            num_dead_checkpoints_(0)
        { }

        /*!
         * \brief FastCheckpointer Constructor
         *
         * \param roots TreeNodes at which checkpoints will be taken.
         *              These cannot be changed later. These do not
         *              necessarily need to be RootTreeNodes. Before
         *              the first checkpoint is taken, these nodes must
         *              be finalized (see
         *              sparta::TreeNode::isFinalized). At this point,
         *              the nodes do not need to be finalized
         *
         * \param sched Scheduler to read and restart on checkpoint restore (if
         *              not nullptr)
         */
        FastCheckpointer(const std::vector<sparta::TreeNode*>& roots, Scheduler* sched=nullptr) :
            Checkpointer(roots, sched),
            snap_thresh_(DEFAULT_SNAPSHOT_THRESH),
            next_chkpt_id_(checkpoint_type::MIN_CHECKPOINT),
            num_alive_checkpoints_(0),
            num_alive_snapshots_(0),
            num_dead_checkpoints_(0)
        { }

        /*!
         * \brief Destructor
         *
         * Frees all checkpoint data
         */
        ~FastCheckpointer() {
            // Reverse iterate and flag all as free
            for(auto itr = chkpts_.rbegin(); itr != chkpts_.rend(); ++itr){
                checkpoint_type* d = static_cast<checkpoint_type*>(itr->second.get());
                if(!d->isFlaggedDeleted()){
                    d->flagDeleted();
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

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
        uint32_t getSnapshotThreshold() const noexcept { return snap_thresh_; }

        /*!
         * \brief Sets the snapshot threshold
         * \see getSnapshotThreshold
         */
        void setSnapshotThreshold(uint32_t thresh) noexcept {
            snap_thresh_ = thresh;
        }

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment including any framework overhead
         * \note This is an approxiation and does not include some of
         * minimal dynamic overhead from stl containers.
         */
        uint64_t getTotalMemoryUse() const noexcept override {
            uint64_t mem = 0;
            for(auto& cp : chkpts_){
                mem += cp.second->getTotalMemoryUse();
            }
            return mem;
        }

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment purely for the checkpoint state being held
         */
        uint64_t getContentMemoryUse() const noexcept override {
            uint64_t mem = 0;
            for(auto& cp : chkpts_){
                mem += cp.second->getContentMemoryUse();
            }
            return mem;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Checkpointing Actions & Queries
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Deletes a checkpoint by ID.
         * \param id ID of checkpoint to delete. Must not be
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT and must not be equal to the
         * ID of the head checkpoint.
         * \throw CheckpointError if this manager has no checkpoint with given
         * id. Test with hasCheckpoint first. If id ==
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT, always throws.
         * Throws if id == getHeadID(). Head cannot be deleted
         *
         * Internally, this deletion may be effective-only and actual data may
         * still exist in an incaccessible form as part of the checkpoint
         * delta-tree implementation.
         *
         * If the current checkpoint is deleted, current will be updated back
         * along the current checkpoints previous-delta chain until a non
         * deleted checkpoint is found. This will become the new current
         * checkpoint
         */
        void deleteCheckpoint(chkpt_id_t id) override {

            // Flag checkpoint as deleted
            checkpoint_type* d = findCheckpoint_(id);
            if(!d){
                throw CheckpointError("Could not delete checkpoint ID=")
                    << id << " because no checkpoint by this ID was found";
            }

            // Allow deletion and change ID to UNIDENTIFIED_CHECKPOINT.
            // This is still part of a chain though until there are no
            // dependencies on it.
            if(!d->isFlaggedDeleted()){
                num_dead_checkpoints_++;
                if(d->isSnapshot()){
                    num_alive_snapshots_--;
                }
                num_alive_checkpoints_--;
                d->flagDeleted();
            }

            // Delete this and all contiguous previous checkpoint which were
            // flagged deleted if possible. Stop if current_ is encountered
            cleanupChain_(d);
        }

        /*!
         * \brief Loads state from a specific checkpoint by ID
         * \note Does not delete checkpoints. Checkpoints must be explicitly
         * deleted by deleteCheckpoint
         * \throw CheckpointError if id does not refer to checkpoint that exists
         * or if checkpoint could not be load.
         * \warning If checkpoint fails during loading for reasons other than an
         * invalid ID, the simulation state could be corrupt
         * \post current checkpoint is now the checkpoint specified by id
         * \post If this checkpointer has was constructed with a pointer to a
         * scheduler, sets that scheduler's current tick to the checkpoint's
         * tick using Scheduler::restartAt
         */
        void loadCheckpoint(chkpt_id_t id) override {
            checkpoint_type* d = findCheckpoint_(id);
            if(!d){
                throw CheckpointError("Could not load checkpoint ID=")
                    << id << " because no checkpoint by this ID was found";
            }

            d->load(getArchDatas());

            // Move current to another checkpoint. Anything between head and the
            // old current_ is fair game for removal if allowed
            checkpoint_type* rmv = static_cast<checkpoint_type*>(getCurrent_());
            setCurrent_(d);

            // Restore scheduler tick number
            if(sched_){
                sched_->restartAt(getCurrentTick());
            }

            // Remove all checkpoints which can be. Stop if the new current_ is
            // encountered again.
            // Note that is is OK if current_ was moved to a later position in
            // the chain. No important checkpoints will be removed. The
            // important thing is never to remove current_.
            cleanupChain_(rmv);
        }

        /*!
         * \brief Gets all checkpoints taken at tick t on any timeline.
         * \param t Tick number at which checkpoints should found.
         * \return vector of valid checkpoint IDs (never
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in the
         * critical path.
         */
        std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) override {
            std::vector<chkpt_id_t> results;
            for(auto& p : chkpts_){
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if(cp->getTick() == t && !dcp->isFlaggedDeleted()){
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        /*!
         * \brief Gets all checkpoint IDs available on any timeline sorted by
         * tick (or equivalently checkpoint ID).
         * \return vector of valid checkpoint IDs (never
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in the
         * critical path.
         */
        std::vector<chkpt_id_t> getCheckpoints() override {
            std::vector<chkpt_id_t> results;
            for(auto& p : chkpts_){
                const Checkpoint* cp = p.second.get();
                const checkpoint_type* dcp = static_cast<const checkpoint_type*>(cp);
                if(!dcp->isFlaggedDeleted()){
                    results.push_back(cp->getID());
                }
            }
            return results;
        }

        /*!
         * \brief Gets the current number of checkpoints having valid IDs
         */
        uint32_t getNumCheckpoints() const noexcept override {
            return num_alive_checkpoints_;
        }

        /*!
         * \brief Gets the current number of snapshots with valid IDs
         */
        uint32_t getNumSnapshots() const noexcept {
            return num_alive_snapshots_;
        }

        /*!
         * \brief Gets the current number of delta checkpoints with valid IDs
         */
        uint32_t getNumDeltas() const noexcept {
            return getNumCheckpoints() - getNumSnapshots();
        }

        /*!
         * \brief Gets the curent number of checkpoints (delta or snapshot)
         * withOUT valid IDs.
         */
        uint32_t getNumDeadCheckpoints() const noexcept {
            return num_dead_checkpoints_;
        }

        /*!
         * \brief Debugging utility which gets a deque of checkpoints
         * representing a chain starting at the checkpoint head and ending at
         * the checkpoint specified by \a id. Ths results can contain
         * checkpoint_type::UNIDENTIFIED_CHECKPOINT to represent temporary
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
        std::deque<chkpt_id_t> getCheckpointChain(chkpt_id_t id) override {
            std::deque<chkpt_id_t> results;
            if(!getHead()){
                return results;
            }
            const checkpoint_type* d = findCheckpoint_(id);
            if(!d){
                throw CheckpointError("There is no checkpoint with ID ") << id;
            }
            while(d){
                results.push_back(d->getID());
                d = static_cast<checkpoint_type*>(d->getPrev());
            }
            return results;
        }

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
        checkpoint_type* findLatestCheckpointAtOrBefore(tick_t tick,
                                                        chkpt_id_t from) {
            checkpoint_type* d = findCheckpoint_(from);
            if(!d){
                throw CheckpointError("There is no checkpoint with ID ") << from;
            }

            // Search backward
            do{
                if(d->getTick() <= tick){
                    break;
                }
                d = static_cast<checkpoint_type*>(d->getPrev());
            }while(d);

            return d;
        }

        /*!
         * \brief Finds a checkpoint by its ID
         * \param id ID of checkpoint to find. Guaranteed not to be flagged as
         * deleted
         * \return Checkpoint with ID of \a id if found or nullptr if not found
         */
        const checkpoint_type* findCheckpoint(chkpt_id_t id) noexcept {
            auto it = chkpts_.find(id);
            if (it != chkpts_.end()) {
                return static_cast<checkpoint_type*>(it->second.get());
            }
            return nullptr;
        }

        /*!
         * \brief Tests whether this checkpoint manager has a checkpoint with
         * the given id.
         * \return True if id refers to a checkpoint held by this checkpointer
         * and false if not. If id == Checkpoint::UNIDENTIFIED_CHECKPOINT,
         * always returns false
         */
        bool hasCheckpoint(chkpt_id_t id) noexcept override {
            return chkpts_.find(id) != chkpts_.end();
        }

        /*!
         * \brief Returns IDs of the checkpoints immediately following the given checkpoint.
         */
        std::vector<chkpt_id_t> getNextIDs(chkpt_id_t id) override final {
            std::vector<chkpt_id_t> next_ids;
            if (const auto chkpt = findCheckpoint_(id)) {
                for (const auto next : chkpt->getNexts()) {
                    const auto dcp = static_cast<checkpoint_type*>(next);
                    if (!dcp->isFlaggedDeleted()) {
                        next_ids.push_back(next->getID());
                    }
                }
            }
            return next_ids;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Printing Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns a string describing this object
         */
        std::string stringize() const override {
            std::stringstream ss;
            ss << "<FastCheckpointer on ";
            for (size_t i = 0; i < getRoots().size(); ++i) {
                TreeNode* root = getRoots()[i];
                if (i != 0) {
                    ss << ", ";
                }
                ss << root->getLocation();
            }
            ss << '>';
            return ss.str();
        }

        /*!
         * \brief Dumps this checkpointer's flat list of checkpoints to an
         * ostream with a newline following each checkpoint
         * \param o ostream to dump to
         */
        void dumpList(std::ostream& o) override {
            for(auto& cp : chkpts_){
                o << cp.second->stringize() << std::endl;
            }
        }

        /*!
         * \brief Dumps this checkpointer's data to an ostream with a newline
         * following each checkpoint
         * \param o ostream to dump to
         */
        void dumpData(std::ostream& o) override {
            for(auto& cp : chkpts_){
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

        /*!
         * \brief Dumps this checkpointer's data to an
         * ostream with annotations between each ArchData and a newline
         * following each checkpoint description and each checkpoint data dump
         * \param o ostream to dump to
         */
        void dumpAnnotatedData(std::ostream& o) override {
            for(auto& cp : chkpts_){
                o << cp.second->stringize() << std::endl;
                cp.second->dumpData(o);
                o << std::endl;
            }
        }

        /*!
         * \brief Forwards debug/trace info onto checkpoint by ID
         */
        void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) override {
            checkpoint_type* dcp = findCheckpoint_(id);
            o << "trace: Searching for 0x" << std::hex << offset << " (" << std::dec << size
              << " bytes) in ArchData " << (const void*)container << " when loading checkpoint "
              << std::dec << id << std::endl;
            if(!dcp){
                o << "trace: Checkpoint " << id << " not found" << std::endl;
            }else{
                dcp->traceValue(o, getArchDatas(), container, offset, size);
            }
        }

        /*!
         * \brief When satisfied with the outstanding/uncommitted checkpoints, call this
         * method to commit them to the DiskT object.
         *
         * \param force_new_head_chkpt If false, then this checkpoint chain:
         *   S1 -> D1 -> D2 -> D3 -> S2 -> D4 -> D5 (current)
         *   Would result in this being saved to disk:
         *   S1 -> D1 -> D2 -> D3
         *   While the S2 checkpoint becomes the new head checkpoint in memory,
         *   and D4/D5 are retained in the FastCheckpointer.
         *
         *   If true, then everything from S1 to D5 will be saved to disk, and a new
         *   head checkpoint S3 will be created in memory at the current tick.
         */
        template <typename DiskT>
        void squashCurrentBranch(DiskT& disk, bool force_new_head_chkpt = false) {
            auto disk_checkpoints = squashCurrentBranch_(force_new_head_chkpt);
            if (!disk_checkpoints.empty()) {
                disk.saveCheckpoints(std::move(disk_checkpoints));
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief Squash the current branch and discard all those checkpoints
         * that do not exist on the current branch. Returns the checkpoints
         * that need to be saved to disk.
         *
         * \param force_new_head_chkpt See public method squashCurrentBranch()
         */
        checkpoint_ptrs squashCurrentBranch_(bool force_new_head_chkpt = false) {
            if (!getCurrent_()) {
                return {};
            }

            checkpoint_ptrs to_save;

            // "Force new head checkpoint" means that we should release every
            // checkpoint from the current checkpoint back to the head checkpoint,
            // delete all remaining checkpoints on other branches, and retake the
            // head checkpoint.
            if (force_new_head_chkpt) {
                auto current_id = getCurrentID();
                auto chkpt_chain = getCheckpointChain(current_id);

                // Checkpoint chains are returned from "right to left", i.e. the
                // first id is the current checkpoint, and the last is the head
                // checkpoint. Reverse this list so that we serialize the checkpoints
                // to disk in the order they were created.
                std::reverse(chkpt_chain.begin(), chkpt_chain.end());

                for (auto id : chkpt_chain) {
                    sparta_assert(hasCheckpoint(id));
                    auto chkpt = std::move(chkpts_[id]);

                    sparta_assert(num_alive_checkpoints_-- > 0);
                    if (chkpt->isSnapshot()) {
                        sparta_assert(num_alive_snapshots_-- > 0);
                    }

                    to_save.emplace_back(std::move(chkpt));
                    chkpts_.erase(id);
                }

                for (auto& [id, chkpt] : chkpts_) {
                    chkpt->flagDeleted();
                }
                chkpts_.clear();

                createHead_();
            }

            // If we are not forcing a new head checkpoint, then we can only
            // remove checkpoints along the current chain if there exists more
            // than one snapshot. The most recent of these will become the new
            // head checkpoint. Say we have these checkpoints:
            //
            //   S1 -> 2 -> 3 -> S2 -> 4 -> 5
            //         |
            //         | -> 6 -> S3 -> 7
            //                    |
            //                    8 -> 9
            //
            // If the current checkpoint is id 7, then the checkpoint chain is
            //   S1 -> 2 -> 6 -> S3 -> 7
            //
            // And the restore chain is:
            //   S3 -> 7
            //
            // Since the checkpoint chain (back to head) is different than
            // the restore chain (back to the last snapshot), that means
            // that we have at least two snapshots, and can therefore return
            // the chain S1 -> 2 -> 6 for writing to disk, while retaining
            // the chain S3 -> 7 and deleting everything else (uncommitted
            // branches). Then set the new head checkpoint to S3.
            //
            // On the other hand, if our checkpoints are like this:
            //
            //   S1 -> 2 -> 3
            //
            // Then the checkpoint chain and restore chain are the same, and
            // we only have one snapshot. We therefore can only remove checkpoints
            // that are NOT along the checkpoint/restore chains, and there are
            // no checkpoints returned to the caller for saving to disk, and
            // the head checkpoint stays the same at S1.
            else {
                auto current_id = getCurrentID();
                auto chkpt_chain = getCheckpointChain(current_id);
                auto restore_chain = static_cast<checkpoint_type*>(getCurrent_())->getRestoreChain();

                // We can speed up the comparison by just checking the chain lengths.
                // If they are the same then we can only remove uncommitted branches.
                if (chkpt_chain.size() == restore_chain.size()) {
                    std::unordered_set<chkpt_id_t> to_keep(chkpt_chain.begin(), chkpt_chain.end());
                    std::vector<chkpt_id_t> to_delete;
                    for (auto& [id, chkpt] : chkpts_) {
                        if (!to_keep.count(id)) {
                            chkpt->flagDeleted();
                            to_delete.emplace_back(id);
                        }                        
                    }

                    for (auto id : to_delete) {
                        auto chkpt = std::move(chkpts_[id]);

                        sparta_assert(num_alive_checkpoints_-- > 0);
                        if (chkpt->isSnapshot()) {
                            sparta_assert(num_alive_snapshots_-- > 0);
                        }

                        chkpts_.erase(id);
                    }

                    // Sanity check
                    sparta_assert(chkpts_.find(getHeadID()) != chkpts_.end());
                    sparta_assert(static_cast<const checkpoint_type*>(getHead())->isSnapshot());
                }

                // If the chains are different, then we have at least two snapshots
                // and can save all but the latest checkpoint window (snapshot plus
                // deltas).
                else {
                    // Say the checkpoint chain is this (recall its first checkpoint
                    // is the current/newest, i.e. in backwards order):
                    //
                    //   7 -> S3 -> 6 -> 2 -> S1
                    //
                    // And the restore chain is this (recall that its first checkpoint
                    // is the most recent snapshot leading up to the current checkpoint):
                    //
                    //   S3 -> 7
                    //
                    // The algorithm goes like this:
                    //   1) Reverse the checkpoint chain into e.g.
                    //      S1 -> 2 -> 6 -> S3 -> 7
                    //   2) Loop over the checkpoint chain, and any id not at
                    //      the restore chain (stack) "top" should be added
                    //      to the list of checkpoints we will save to disk.
                    //      If the id is the same as the restore chain "top",
                    //      then store the new head checkpoint if it is a full
                    //      snapshot and pop() the stack.
                    //   3) Delete all checkpoints that are neither saved to disk
                    //      or left in the checkpointer (uncommitted branches).
                    std::reverse(chkpt_chain.begin(), chkpt_chain.end());
                    std::unordered_set<chkpt_id_t> to_keep;

                    for (auto id : chkpt_chain) {
                        if (id != restore_chain.top()->getID() && findCheckpoint(id) != nullptr) {
                            auto chkpt = std::move(chkpts_[id]);

                            sparta_assert(num_alive_checkpoints_-- > 0);
                            if (chkpt->isSnapshot()) {
                                sparta_assert(num_alive_snapshots_-- > 0);
                            }

                            to_save.emplace_back(std::move(chkpt));
                            chkpts_.erase(id);
                        } else {
                            if (restore_chain.top()->isSnapshot()) {
                                setHead_(restore_chain.top());
                            }
                            to_keep.insert(restore_chain.top()->getID());
                            restore_chain.pop();

                            if (restore_chain.empty()) {
                                break;
                            }
                        }
                    }

                    std::vector<chkpt_id_t> to_delete;
                    for (auto& [id, chkpt] : chkpts_) {
                        if (!to_keep.count(id)) {
                            to_delete.emplace_back(id);
                        }
                    }

                    for (auto id : to_delete) {
                        auto & chkpt = chkpts_[id];
                        chkpt->flagDeleted();

                        sparta_assert(num_alive_checkpoints_-- > 0);
                        if (chkpt->isSnapshot()) {
                            sparta_assert(num_alive_snapshots_-- > 0);
                        }
                    }

                    for (auto id : to_delete) {
                        chkpts_.erase(id);
                    }
                }
            }

            sparta_assert(getHead() != nullptr);
            sparta_assert(getHeadID() != checkpoint_type::UNIDENTIFIED_CHECKPOINT);

            return to_save;
        }

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
        void cleanupChain_(checkpoint_type* d) {

            // In order to truly delete any checkpoints, we must traverse back
            // to the previous snapshot (or the head) and forward to the another
            // snapshot or the end of the chain.
            // ONLY if both of those points can be reached without encountering
            // a living checkpoint or the current checkpoint (forward
            // only) can the whole chain (including the leading shapshot) be
            // deleted.

            //! \todo Support compression

            if(d == getHead()){
                // Cannot delete head of checkpoint tree
                return;
            }

            // Walk forward to another snapshot or current
            const bool needed_later = (getCurrent_() == d) || recursForwardFindAlive_(d);
            if(needed_later) {
                // Cannot delete because a later living checkpoint (or current) depends on this
                if(d->isSnapshot()){
                    // This snapshot is needed later. Move to previous delta and work from there
                    d = static_cast<checkpoint_type*>(d->getPrev());
                }else{
                    return; // This delta is needed. Therefore all preceeding deltas are needed
                }
            }

            // Delete backward until current, head, or a non-flagged-deleted checkpoint is hit.
            // It is possible to fracture the checkpoint tree by deleting a segment
            // between two snapshots, so prev can end up with nothing leading up to it
            while(d && d != getHead() && d->isFlaggedDeleted()){

                // If the checkpoint to delete is the current checkpoint, then
                // We cannot just set current to the previous checkpoint because
                // we may have run forward and storing a checkpoint in the
                // future would depend on the checkpoint we are about to delete.
                // This could be fixed by requiring the next checkpoint to be a
                // spapshot. Instead, point to the flagged-deleted checkpoint
                // and do not delete
                if(getCurrent_() == d){
                    return;
                }

                checkpoint_type* prev = static_cast<checkpoint_type*>(d->getPrev());

                // If nothing later in the chain (tree) depends on d's data, it can be deleted.
                // This also patches the checkpoint tree around the deleted checkpoint
                //! \todo canDelete is recursive at worst and might benefit from optimization
                if(d->canDelete()) {
                    // Get checkpoint id regardless of whether alive or dead
                    chkpt_id_t id = d->getID();
                    if (d->isFlaggedDeleted()) {
                        id = d->getDeletedID();
                    }

                    num_dead_checkpoints_--;

                    // Erase element in the map
                    auto itr = chkpts_.find(id);
                    sparta_assert(itr != chkpts_.end());
                    chkpts_.erase(itr);
                }

                d = prev; // Continue until head is reached
            }
        }

        /*!
         * \brief Look forward to see if any future checkpoints depend on \a d.
         * \param d checkpoint to inspect and recursively search
         * \return true if the current checkpoint or any live checkpoints
         * are hit in the search. Search terminates on each branch when a
         * snapshot or the end of the branch is reached. The branch to inspect
         * (\a d) will not be checked itself since the point is to determine
         * which branches down-chain depend on it.
         */
        bool recursForwardFindAlive_(checkpoint_type* d) const
        {
            const std::vector<Checkpoint*> & nexts = d->getNexts();
            for(const auto & chkpt : nexts)
            {
                checkpoint_type* dc = static_cast<checkpoint_type*>(chkpt);
                // Only check descendants for snapshot-ness
                if(dc->isSnapshot()){
                    // Found a live snapshot that ends this branch. d is not needed
                    // after this
                    return false;
                }
                if(dc == getCurrent_()){
                    // Found current in this search chain
                    return true;
                }
                if(dc->isFlaggedDeleted() == false){
                    // Encountered a checkpoint later in the chain that still
                    // depends on this.
                    return true;
                }

                // Continue the search recursively
                if(recursForwardFindAlive_(dc)){
                    return true;
                }
            }

            // Found nothing alive.
            return false;
        }

        /*!
         * \brief Attempts to find a checkpoint within this checkpointer by ID.
         * \param id Checkpoint ID to search for
         * \return Pointer to found checkpoint with matchind ID. If not found,
         * returns nullptr.
         * \todo Faster lookup?
         */
        checkpoint_type* findCheckpoint_(chkpt_id_t id) noexcept {
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        /*!
         * \brief const variant of findCheckpoint_
         */
        const checkpoint_type* findCheckpoint_(chkpt_id_t id) const noexcept {
            auto itr = chkpts_.find(id);
            if (itr != chkpts_.end()) {
                return static_cast<checkpoint_type*>(itr->second.get());
            }
            return nullptr;
        }

        /*!
         * \brief Implements Checkpointer::dumpCheckpointNode_
         */
        void dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) override {
            static std::string SNAPSHOT_NOTICE = "(s)";
            auto cp = findCheckpoint_(id);

            // Draw data for this checkpoint
            if(cp->isFlaggedDeleted()){
                o << cp->getDeletedRepr();
            }else{
                o << cp->getID();
            }
            // Show that this is a snapshot
            if(cp->isSnapshot()){
                o << ' ' << SNAPSHOT_NOTICE;
            }
        }

    private:

        /*!
         * \brief Implements Checkpointer::createHead_
         */
        void createHead_() override {
            tick_t tick = 0;
            if(sched_){
                tick = sched_->getCurrentTick();
            }

            for (auto root : getRoots()) {
                if(root->isFinalized() == false){
                    CheckpointError exc("Cannot create a checkpoint until the tree is finalized. Attempting to checkpoint from node ");
                    exc << root->getLocation() << " at tick ";
                    if(sched_){
                        exc << tick;
                    }else{
                        exc << "<no scheduler>";
                    }
                    throw exc;
                }
            }

            checkpoint_type* dcp = new checkpoint_type(getArchDatas(), next_chkpt_id_++, tick, nullptr, true);
            chkpts_[dcp->getID()].reset(dcp);
            setHead_(dcp);
            num_alive_checkpoints_++;
            num_alive_snapshots_++;
            setCurrent_(dcp);
        }

        chkpt_id_t createCheckpoint_(bool force_snapshot=false) override {
            bool is_snapshot;
            checkpoint_type* prev;

            if(next_chkpt_id_ == checkpoint_type::UNIDENTIFIED_CHECKPOINT){
                throw CheckpointError("Exhausted all ")
                    << checkpoint_type::UNIDENTIFIED_CHECKPOINT << " possible checkpoint IDs. "
                    << "This is likely a gross misuse of checkpointing";
            }

            // Caller guarantees a head
            sparta_assert(getHead() != nullptr);

            tick_t tick;
            if(sched_){
                tick = sched_->getCurrentTick();
            }else{
                tick = 0;
            }

            if(sched_ && (tick < getHead()->getTick())){
                throw CheckpointError("Cannot create a new checkpoint at tick ")
                    << tick << " because this tick number is smaller than the tick number of the head checkpoint at: "
                    << getHead()->getTick() << ". The head checkpoint cannot be reset once created, so it should be done "
                    << "at the start of simulation before running. The simulator front-end should do this so this must "
                    << "likely be fixed in the simulator.";
            }

            if(nullptr == getCurrent_()){
                // Creating a delta from the head
                prev = static_cast<checkpoint_type*>(getHead_());
                is_snapshot = false;
            }else{
                if(sched_ && (tick < getCurrent_()->getTick())){
                    throw CheckpointError("Current tick number from sparta scheduler (")
                        << tick << " ) is less than the current checkpoint's tick number ("
                        << getCurrent_()->getTick() << " To create a checkpoint with an earlier tick number, an "
                        << "older checkpoint having a tick number <= the tick number specified here must first be "
                        << "loaded";
                }

                // Find latest checkpoint <= tick

                prev = static_cast<checkpoint_type*>(getCurrent_());
                is_snapshot = prev->getDistanceToPrevSnapshot() >= getSnapshotThreshold();
            }

            checkpoint_type* dcp = new checkpoint_type(getArchDatas(), // Created during createHead
                                                       next_chkpt_id_++,
                                                       tick,
                                                       prev,
                                                       force_snapshot || is_snapshot);
            chkpts_[dcp->getID()].reset(dcp);
            num_alive_checkpoints_++;
            num_alive_snapshots_ += (dcp->isSnapshot() == true);
            setCurrent_(dcp);

            if (dcp->isSnapshot()){
                // Clean up starting with this snapshot and moving back.
                // May have an opportunity to free older deltas right now
                // (instead of upon next deletion)
                cleanupChain_(dcp);
            }

            return dcp->getID();
        }

        /*!
         * \brief All checkpoints sorted by ascending tick number (or
         * equivalently ascending checkpoint ID since both are monotonically
         * increasing)
         *
         * This map must still be explicitly torn down in reverse order by a
         * subclass of Checkpointer
         */
        std::map<chkpt_id_t, checkpoint_ptr> chkpts_;

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
