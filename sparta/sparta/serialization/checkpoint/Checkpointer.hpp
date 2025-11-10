// <Checkpointer> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>
#include <stack>
#include <queue>
#include <list>
#include <map>
#include <string>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/serialization/checkpoint/Checkpoint.hpp"

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Checkpointer interface. Defines an ID-based checkpointing API
     * for tree of related checkpoints which could be stored as ordered deltas
     * internally.
     *
     * Internal storage and structure are to be defined by implementations of
     * this interface
     *
     * A checkpoint tree may look something like the following, where each
     * checkpoint is shown here by its simulation tick number (not ID)
     * \verbatim
     * t=0 (head) --> t=100 +-> t=300
     *                      |
     *                      `-> t=320 --> t=400 +-> t=500
     *                      |                   `-> t=430
     *                      `-> t=300
     * \endverbatim
     *
     * The procedure for using a Checkpointer is generally:
     * \li Create SPARTA Tree
     * \li Construct Checkpointer
     * \li Finalize SPARTA Tree
     * \li Initialize simulation
     * \li Checkpointer::createHead
     *
     * Then:
     * \li <run simulation>
     * \li Checkpointer::createCheckpoint
     * \li <run simulation>
     * \li Checkpointer::loadCheckpoint
     * \li repeat in any order necessary
     * \endverbatim
     */
    class Checkpointer
    {
    public:

        //! \name Local Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief tick_t Tick type to which checkpoints will refer
        typedef Checkpoint::tick_t tick_t;

        //! \brief tick_t Tick type to which checkpoints will refer
        typedef Checkpoint::chkpt_id_t chkpt_id_t;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Checkpointer Constructor
         * \param root TreeNode at which checkpoints will be taken.
         * This cannot be changed later. This does not necessarily need to be a
         * RootTreeNode. Before the first checkpoint is taken, this node must be
         * finalized (see sparta::TreeNode::isFinalized). At this point, the node
         * does not need to be finalized
         * \param sched Relevant scheduler. If nullptr (default), the
         * checkpointer will not touch attempt to roll back the scheduler on
         * checkpoint restores
         */
        Checkpointer(TreeNode& root, sparta::Scheduler* sched=nullptr) :
            sched_(sched),
            root_(root)
        { }

        /*!
         * \brief Destructor
         */
        virtual ~Checkpointer() {
        }

        /*!
         * \brief Add additional root nodes from which ArchData checkpoints will be taken.
         * \pre Must be called before createHead
         */
        void addRoot(TreeNode& root) {
            if (head_) {
                throw CheckpointError("Cannot add additional checkpoint roots after head has been created");
            }
            additional_roots_.push_back(&root);
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns the root associated with this checkpointer
         */
        const TreeNode& getRoot() const noexcept { return root_; }

        /*!
         * \brief Non-const variant of getRoot
         */
        TreeNode& getRoot() noexcept { return root_; }

        /*!
         * \brief Returns the sheduler associated with this checkpointer
         */
        const Scheduler* getScheduler() const noexcept { return sched_; }

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment including any framework overhead
         * \note This is an approxiation and does not include some of
         * minimal dynamic overhead from stl containers.
         */
        virtual uint64_t getTotalMemoryUse() const noexcept = 0;

        /*!
         * \brief Computes and returns the memory usage by this checkpointer at
         * this moment purely for the checkpoint state being held
         */
        virtual uint64_t getContentMemoryUse() const noexcept = 0;

        /*!
         * \brief Returns the total number of checkpoints which have been
         * created by this checkpointer. This is unrelated to the current number
         * of checkpoints in existance. Includes the head checkpoint if created
         */
        uint64_t getTotalCheckpointsCreated() const noexcept {
            return total_chkpts_created_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Checkpointing Actions & Queries
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Creates a head without taking an identified checkpoint.
         * Cannot already have a head.
         * \pre Must not have called createHead or createCheckpoint before this.
         * \pre Root must be finalized at this time.
         * \post Head will be created at given tick and getHead will return
         * this head.
         * \post Head will become the current checkpoint. See getCurrent
         * \throw CheckpointError if root is not yet finalized or a head already
         * exists for this checkpointer
         * \post Tick number from scheduler is recorded at this time. Future
         * checkpoints cannot be created at tick numbers lower than this. Equal
         * tick numbers are allowed
         *
         * Generally, this is called after all simulation state is initialized
         * (e.g. resisters set) since it makes little sense to go back to a
         * pre-initialized state.
         *
         * Test for a head if necessary with getHead(). Typically, this should
         * be called before running the simulation.
         */
        void createHead() {
            tick_t tick = 0;
            if(sched_){
                tick = sched_->getCurrentTick();
            }
            if(head_){
                CheckpointError exc("Cannot create head at ");
                if(sched_){
                    exc << tick;
                }else{
                    exc << "<no scheduler>";
                }
                exc << " because a head already exists in this checkpointer";
                throw exc;
            }
            if(root_.isFinalized() == false){
                CheckpointError exc("Cannot create a checkpoint until the tree is finalized. Attempting to checkpoint from node ");
                exc << root_.getLocation() << " at tick ";
                if(sched_){
                    exc << tick;
                }else{
                    exc << "<no scheduler>";
                }
                throw exc;
            }

            enumerateArchDatas_(); // Determines which ArchDatas are required and populates adatas_

            createHead_();

            sparta_assert(head_ != nullptr, "A call to createHead_ must create a head and invoke setHead_ or throw an exception");

            total_chkpts_created_++;
        }

        /*!
         * \brief Creates a checkpoint at the given scheduler's current tick
         * with a new checkpoint ID some point after the current checkpoint
         * (see getCurrentID). If the current checkpoint already has
         * other next checkpoints, the new checkpoint will be an alternate
         * branch of the current checkpoint. This snapshot may be stored as a
         * full snapshot if the checkpointer requires it, or if the snapshot
         * threshold is exceeded, or if the \a force_snapshot argument is true
         * Current tick will be read from scheduler (if not null) and must be
         * >= the head checkpoint's tick. The current tick must also be >= the
         * current checkpoints tick (See getCurrenTick).
         * \param force_snapshot Forces the newly-created checkpoint to be a
         * full snapshot instead of allowing the checkpointer to decide based on
         * getSnapshotThreshold. A snapshot takes more time and space to store,
         * but this can be a performance optimization if this checkpoint will be
         * re-loaded very frequently
         * \pre Root must be finalized at this time.
         * \post If no head exists, one will be created. See getHead
         * \post Sets the newly created checkpoint as the current. See
         * getCurrentID
         * \throw CheckpointError if a head checkpoint exists and the current
         * tick is less than the head checkpoint's tick, or if the root TreeNode
         * is not finalized.
         * Also throws if the number of checkpoint IDs has been exhausted (i.e.
         * all values of a chkpt_id_t have been used as IDs). This is unlikely
         * and has a special exception message.
         */
        chkpt_id_t createCheckpoint(bool force_snapshot=false) {
            if(!head_){
                createHead();
            }

            chkpt_id_t id = createCheckpoint_(force_snapshot);
            total_chkpts_created_++;
            return id;
        }

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
        virtual void deleteCheckpoint(chkpt_id_t id) = 0;

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
        virtual void loadCheckpoint(chkpt_id_t id) = 0;

        /*!
         * \brief Forgets the current checkpoint and current checkpoint
         * (resetting to the head checkpoint) so that checkpoints can be taken
         * at a different time without assuming simulation state continutiy with
         * this checkpointers. This is ONLY to be used by a simulator IFF another
         * checkpointer restores state at another cycle or the simulator resets
         * but this checkpointer's tree is still expected to exist.
         * \warning Read the documentation for this method carefully. If it is
         * being used in a simulator with only 1 checkpointer operating at a
         * time, it is very likely that loadCheckpoint should be used instead
         * of this.
         * \post getCurrentTick() and getCurrentID() will refer to the head.
         * \note No checkpoints are deleted or created or reorganized in
         * any way.
         * \note New checkpoints on this scheduler must still be created at a
         * tick number >= this scheduler's head checkpoint tick.
         * \note has no effect if the head has no been created.
         */
        void forgetCurrent() {
            if(head_){
                current_ = head_;
            }
        }

        /*!
         * \brief Gets all checkpoints taken at tick t on any timeline.
         * \param t Tick number at which checkpoints should found.
         * \return vector of valid checkpoint IDs (never
         * Checkpoint::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in a
         * performance-critical path.
         */
        virtual std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) = 0;

        /*!
         * \brief Gets all known checkpoint IDs available on any timeline sorted
         * by tick (or equivalently checkpoint ID).
         * \return vector of valid checkpoint IDs (never
         * Checkpoint::UNIDENTIFIED_CHECKPOINT)
         * \note Makes a new vector of results. This should not be called in a
         * performance-critical path.
         */
        virtual std::vector<chkpt_id_t> getCheckpoints() = 0;

        /*!
         * \brief Gets the current number of checkpoints having valid IDs
         * which a client of this interface can refer to.
         *
         * Ignores any internal temporary or deleted checkpoints without
         * visible IDs
         */
        virtual uint32_t getNumCheckpoints() const noexcept = 0;

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
        virtual std::deque<chkpt_id_t> getCheckpointChain(chkpt_id_t id) = 0;

        /*!
         * \brief Tests whether this checkpoint manager has a checkpoint with
         * the given id.
         * \return True if id refers to a checkpoint held by this checkpointer
         * and false if not. If id == Checkpoint::UNIDENTIFIED_CHECKPOINT,
         * always returns false
         */
        virtual bool hasCheckpoint(chkpt_id_t id) noexcept = 0;

        /*!
         * \brief Returns the head checkpoint which is equivalent to the
         * earliest checkpoint taken
         * \return Head checkpoint. nullptr if 0 checkpoints have been taken
         * with this manager. Once the first checkpoint is taken or the head is
         * explicitly created, will always be non-nullptr
         *
         * The head checkpoint has an ID of
         * Checkpoint::UNIDENTIFIED_CHECKPOINT and can never be deleted.
         */
        const CheckpointBase* getHead() const noexcept {
            return head_;
        }

        /*!
         * \brief Returns the checkpoint ID of the head checkpoint (if it
         * exists) which is equivalent to the earliest checkpoint taken
         * \return Head checkpoint ID. Checkpoint::UNIDENTIFIED_CHECKPOINT
         * if there is no head. Once the first checkpoint is taken or the head is
         * explicitly created, will always be a valid checkpoint identifier.
         *
         * The head checkpoint can never be deleted.
         */
        chkpt_id_t getHeadID() const noexcept {
            if(!head_){
                return Checkpoint::UNIDENTIFIED_CHECKPOINT;
            }
            return head_->getID();
        }

        /*!
         * \brief Returns the current checkpoint ID. This is mainly a debugging
         * utility as the current ID changes when adding, deleting, and loading
         * checkpoints based on whether the checkpoints take were deltas or
         * snapshots. A correct integration of the checkpointer by a simulator
         * should not depend on this method for behavior decisions.
         * \return ID of current checkpoint. If there is no current checkpoint,
         * or the current checkpoint is an internal-only checkpoint, returns
         * Checkpoint::UNIDENTIFIED_CHECKPOINT. A return value of
         * Checkpoint::UNIDENTIFIED_CHECKPOINT usually only happens
         * before the head is created and when the current checkpoint is
         * deleted.
         *
         * The current checkpoint refers to the most recent checkpoint either
         * loaded or created. This value is used to refer to the
         * previous-checkpoint when creating a delta checkpoint.
         *
         * Loading, creating, and deleting a checkpoint updates the current
         * checkpoint.
         *
         * If the previous current checkpoint was deleted, current is updated to
         * refer back to the most recent previous checkpoint in the same
         * previous-checkpoint chain of one which was deleted.
         */
        chkpt_id_t getCurrentID() const {
            if(current_){
                return current_->getID();
            }
            sparta_assert(!head_); // If there was no current, it can only be because there is no head yet
            return Checkpoint::UNIDENTIFIED_CHECKPOINT;
        }

        /*!
         * \brief Gets the tick number of the current checkpoint (see
         * getCurrentID). This is the tick number of the latest checkpoint
         * either saved or written through this checkpointer. The next
         * checkpoint taken will be on the same chain as a checkpoint taken at
         * this tick.
         * \note The next call to createCheckpoint will require the scheduler
         * current tick to be greater than or equal to the tick returned by this
         * method
         * \return tick number of 'current' checkpoint. If there is no current
         * checkpoint, returns 0
         */
        tick_t getCurrentTick() const {
            if(current_){
                return current_->getTick();
            }
            sparta_assert(!head_);
            return 0;
        }

        /*!
         * \brief Returns IDs of the checkpoints immediately following the given checkpoint.
         */
        virtual std::vector<chkpt_id_t> getNextIDs(chkpt_id_t id) = 0;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Printing Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns a string describing this object
         */
        virtual std::string stringize() const {
            std::stringstream ss;
            ss << "<Checkpointer on " << root_.getLocation() << '>';
            return ss.str();
        }

        /*!
         * \brief Dumps this checkpointer's flat list of checkpoints to an
         * ostream with a newline following each checkpoint
         * \param o ostream to dump to
         */
        virtual void dumpList(std::ostream& o) = 0;

        /*!
         * \brief Dumps this checkpointer's data to an ostream with a newline
         * following each checkpoint
         * \param o ostream to dump to
         */
        virtual void dumpData(std::ostream& o) = 0;

        /*!
         * \brief Dumps this checkpointer's data to an
         * ostream with annotations between each ArchData and a newline
         * following each checkpoint description and each checkpoint data dump
         * \param o ostream to dump to
         */
        virtual void dumpAnnotatedData(std::ostream& o) = 0;

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
        virtual void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) = 0;

        /*!
         * \brief Dumps this checkpointer's tree to an ostream with a line for
         * each branch. Printout timescale is not relevant. Multi-line printouts
         * for deep branches will be difficult to read
         * \param o ostream to dump to
         */
        void dumpTree(std::ostream& o) {
            std::deque<uint32_t> c;
            dumpBranch(o, getHeadID(), 0, 0, c);
            o << '\n';
        }

        /*!
         * \brief Recursively dumps one branch (and sub-branches) to an ostream
         * with a line for each branch
         * \param o ostream to dump to
         * \param chkpt Checkpoint to start printint at
         * \param indent Number of spaces to indent before printing this branch
         * \param pos Position on line.
         * \param continues Vector of continue indent points where '|'
         * characters should be printed on lines whose indent amount is greater
         * than each particular indent point. This creates the vertical lines
         * expected in directory-like tree-view displays
         */
        void dumpBranch(std::ostream& o,
                        const chkpt_id_t chkpt,
                        uint32_t indent,
                        uint32_t pos,
                        std::deque<uint32_t>& continues) {
            //! \todo Move the constants somewhere static outside this function (especially the assert)
            static const std::string SEP_STR = "-> "; // Normal checkpoint chain
            static const std::string CONT_SEP_STR = "`> "; // Checkpoint branch from higher line
            assert(SEP_STR.size() == CONT_SEP_STR.size()); // Must be the same length so the layout looks OK

            // Walk through indent and draw continuations
            uint32_t i = pos;
            std::remove_reference<decltype(continues)>::type::const_iterator next_cont = continues.begin();
            for(; i < indent; ++i){
                if(next_cont != continues.end() && i == *next_cont){
                    o << '|';
                    while(next_cont != continues.end() && *next_cont == i){ // Skip duplicates and move on to next
                        ++next_cont;
                    }
                }else{
                    o << ' ';
                }
            }

            auto nexts = getNextIDs(chkpt);
            std::stringstream ss;

            // Draw separator between prev checkpoint and this
            if(next_cont != continues.end() && *next_cont == indent && indent != pos){
                ss << CONT_SEP_STR;
            }else{
                ss << SEP_STR;
            }

            // Draw box around object if it is current
            if(current_ && current_->getID() == chkpt){
                ss << "[ ";
            }

            dumpCheckpointNode_(chkpt, ss);
            ss << ' ';

            if(current_ && current_->getID() == chkpt){
                ss << ']';
            }

            o << ss.str();
            i += ss.str().size();

            // Draw all next checkpoints recursively
            if(nexts.size() > 0){
                if(nexts.size() > 1){
                    continues.push_back(i); // Push continues first
                }
                dumpBranch(o, nexts.front(), i, i, continues); // No indent, no newline
                decltype(nexts)::const_iterator itr = nexts.begin() + 1;
                if(itr == nexts.end()){
                    //continues.pop_back();
                }else{
                    while(itr < nexts.end()){
                        if(itr+1 == nexts.end()){
                            continues.pop_back(); // Do not show this continuation here
                        }
                        o << '\n';
                        dumpBranch(o, *itr, i, 0, continues); // Indent
                        ++itr;
                    }
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

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
        virtual void createHead_() = 0;

        /*!
         * \brief Create a checkpoint
         * \pre Guaranteed to have a valid head at this time
         * (getHead() != nullptr)
         * \post Must create a checkpoint
         * \return Must return a checkpoint ID not currently in use
         * \note invoked by createHead
         */
        virtual chkpt_id_t createCheckpoint_(bool force_snapshot=false) = 0;

        virtual void dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) {
            o << id;
        }

        /*!
         * \brief Returns ArchDatas enumerated by this Checkpointer for
         * iteration when saving or loading checkpoint data
         */
        const std::vector<ArchData*>& getArchDatas() const {
            return adatas_;
        }

        /*!
         * \brief Non-const variant of getHead_
         */
        CheckpointBase* getHead_() noexcept {
            return head_;
        }

        /*!
         * \brief Gets the head checkpoint. Returns nullptr if none created yet
         */
        const CheckpointBase* getHead_() const noexcept {
            return head_;
        }

        /*!
         * \brief Sets the head checkpointer pointer to \a head for the first
         * time
         * \param head New head checkpoint pointer. Must not be nullptr
         * \pre Internal head pointer must be nullptr.
         * \note This can only be done once
         */
        void setHead_(CheckpointBase* head) {
            sparta_assert(head != nullptr, "head argument in setHead_ cannot be nullptr");
            sparta_assert(head_ == nullptr, "Cannot setHead_ again on a Checkpointer once heas is already set");
            head_ = head;
        }

        /*!
         * \brief Gets the current checkpointer pointer. Returns nullptr if
         * there is no current checkpoint object
         */
        CheckpointBase* getCurrent_() const noexcept {
            return current_;
        }

        /*!
         * \brief Sets the current checkpoint pointer.
         * \param current Pointer to set as current checkpoint. The next
         * checkpoint created will follow the current checkpoint set here.
         * Cannot be nullptr
         */
        void setCurrent_(CheckpointBase* current) {
            sparta_assert(current != nullptr,
                        "Can never setCurrent_ to nullptr except. A null current is a valid state at initialization only")
            current_ = current;
        }

        /*!
         * \brief Scheduler whose tick count will be set and read. Cannnot be
         * updated after first checkpoint without bad side effects. Keeping this
         * const for simplicity.
         */
        Scheduler* const sched_;

    private:

        /*!
         * \brief Enumerates all ArchDatas in the root and adds them to a vector
         * for fast iteration during checkpoint/restore
         * \pre Must not have been called before
         */
        void enumerateArchDatas_() {
            sparta_assert(adatas_.size() == 0, "FastCheckpointer already has a vector of ArchDatas. Cannot re-enumerate");

            adatas_.clear(); // Clear in case invoked again

            // This is a helper for building the adatas_ vector while detecting
            // dupicate ArchDatas.
            // Maps an ArchData key to the TreeNode which pointed to it.
            std::map<ArchData*,TreeNode*> adatas_helper;

            // Recursively walk the tree and add all ArchDatas to adatas_
            recursAddArchData_(&root_, adatas_helper);
            for(TreeNode* additional_root : additional_roots_){
                recursAddArchData_(additional_root, adatas_helper);
            }
        }

        /*!
         * \brief Appends each ArchData found in the tree to adatas_
         * \param n Node to add first and recurs below
         * \param adatas_helper Tracks ArchData references during recursion to
         * check for multiple references to the same ArchData from different TreeNodes
         * \throw CheckpointError
         */
        void recursAddArchData_(TreeNode* n, std::map<ArchData*,TreeNode*>& adatas_helper) {
            assert(n);
            auto adatas = n->getAssociatedArchDatas();
            for(ArchData* ad : adatas){
                if(ad != nullptr){
                    auto itr = adatas_helper.find(ad);
                    if(itr != adatas_helper.end()){
                        throw CheckpointError("Found a second reference to ArchData ")
                            << ad << " in the tree: " << root_.stringize() << " . First reference found throgh "
                            << itr->second->getLocation() << " and second found through " << n->getLocation()
                            << " . An ArchData should be findable throug exactly 1 TreeNode";
                    }
                    adatas_.push_back(ad);
                    adatas_helper[ad] = n; // Add to helper map
                }
            }
            for(TreeNode* child : TreeNodePrivateAttorney::getAllChildren(n)){
                recursAddArchData_(child, adatas_helper);
            }
        }

        TreeNode& root_; //!< Root of tree at which checkpoints will be taken
        std::vector<TreeNode*> additional_roots_; //!< Additional root nodes for checkpointing

        /*!
         * \brief Head checkpoint. This is the first checkpoint taken but cannot
         * be deleted. Head checkpoint memory is owned by checkpointer subclass.
         */
        CheckpointBase* head_ = nullptr;

        /*!
         * \brief ArchDatas required to checkpoint for this checkpointiner based
         * on the root TreeNode
         */
        std::vector<ArchData*> adatas_;

        /*!
         * \brief Most recent checkpoint created or loaded
         */
        CheckpointBase* current_ = nullptr;

        /*!
         * \brief Total checkpoint ever created by this instance. Monotonically
         * increasing. Includes the head checkpoint
         */
        uint64_t total_chkpts_created_ = 0;
    };

} // namespace sparta::serialization::checkpoint


//! ostream insertion operator for sparta::Register
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::Checkpointer& cpr){
    o << cpr.stringize();
    return o;
}

//! ostream insertion operator for sparta::Register
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::Checkpointer* cpr){
    if(cpr == 0){
        o << "null";
    }else{
        o << cpr->stringize();
    }
    return o;
}
