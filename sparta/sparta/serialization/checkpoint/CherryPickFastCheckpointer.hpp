// <CherryPickFastCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "simdb/apps/App.hpp"
#include "simdb/utils/ConcurrentQueue.hpp"
#include "simdb/sqlite/Iterator.hpp"
#include <map>

namespace sparta
{
    class TreeNode;
    class Scheduler;
}

namespace simdb::pipeline
{
    class Flusher;
}

namespace sparta::serialization::checkpoint
{

class CherryPickFastCheckpointer final : public simdb::App
{
public:
    using checkpoint_type = typename FastCheckpointer::checkpoint_type;
    using checkpoint_ptr = typename FastCheckpointer::checkpoint_ptr;
    using checkpoint_ptrs = typename FastCheckpointer::checkpoint_ptrs;
    using chkpt_id_t = typename Checkpointer::chkpt_id_t;
    using arch_id_t = uint64_t;
    using tick_t = typename Checkpointer::tick_t;

    static constexpr auto NAME = "cherry-pick-fast-checkpointer";

    /*!
     * \brief CherryPickFastCheckpointer constructor
     *
     * \param db_mgr SimDB instance to use as a backing store for all checkpoints.
     *
     * \param roots TreeNodes at which checkpoints will be taken.
     *              These cannot be changed later. These do not
     *              necessarily need to be RootTreeNodes. Before
     *              the first checkpoint is taken, these nodes must
     *              be finalized (see sparta::TreeNode::isFinalized).
     *              At the point of construction, the nodes do not
     *              need to be finalized.
     *
     * \param sched Scheduler to read and restart on checkpoint restore (if
     *              not nullptr)
     */
    CherryPickFastCheckpointer(simdb::DatabaseManager* db_mgr, const std::vector<TreeNode*> & roots,
                               Scheduler* sched = nullptr);

    /*!
     * \brief This AppFactory specialization is provided since we have to initialize the FastCheckpointer
     * which takes the ArchData root(s) / Scheduler, and thus cannot have the default app subclass ctor
     * signature that only takes the DatabaseManager like most other apps.
     */
    class AppFactory : public simdb::AppFactoryBase
    {
    public:
        using AppT = serialization::checkpoint::CherryPickFastCheckpointer;

        /// \brief Sets the ArchData root(s) and Scheduler for a given instance of the checkpointer.
        /// \param roots TreeNode(s) at which ArchData will be taken
        /// \param sched Scheduler to use for the checkpoint's tick numbers
        /// \note This is required before createEnabledApps() is called
        void parameterize(const std::vector<TreeNode*>& roots, Scheduler* sched = nullptr)
        {
            roots_ = roots;
            scheduler_ = sched;
        }

        AppT* createApp(simdb::DatabaseManager* db_mgr) override
        {
            // Make the ctor call that the default AppFactory cannot make.
            return new AppT(db_mgr, roots_, scheduler_);
        }

        void defineSchema(simdb::Schema& schema) const override
        {
            AppT::defineSchema(schema);
        }

    private:
        std::vector<TreeNode*> roots_;
        Scheduler* scheduler_ = nullptr;
    };

    /*!
     * \brief Define the SimDB schema for this checkpointer.
     */
    static void defineSchema(simdb::Schema& schema);

    /*!
     * \brief Instantiate the async processing pipeline to save checkpoints to the DB.
     */
    void createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr) override;

    /*!
     * \brief Use the FastCheckpointer to create checkpoints / checkpoint branches.
     */
    FastCheckpointer& getFastCheckpointer() noexcept {
        return checkpointer_;
    }

    /*!
     * \brief When satisfied with the outstanding/uncommitted checkpoints, call this
     * method to commit them to the database.
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
     *
     * \param committed_checkpoints If not nullptr, this vector will be filled with
     * the IDs of the checkpoints that were sent down the pipeline.
     */
    void commitCurrentBranch(bool force_new_head_chkpt = false,
                             std::vector<chkpt_id_t>* committed_checkpoints = nullptr);

    /*!
     * \brief Send the committed checkpoints down the pipeline to the database.
     */
    void saveCheckpoints(checkpoint_ptrs&& checkpoints);

    /*!
     * \brief Called at the end of simulation just prior to shutting down threads
     */
    void preTeardown() override;

    //! \brief Forward declaration for DatabaseCheckpointLoader
    struct ChkptWindow;

    //! \brief Arch ID range at a particular tick
    struct ArchIdsForTick {
        arch_id_t start_arch_id = UINT64_MAX;
        arch_id_t end_arch_id = UINT64_MAX;
        tick_t tick = UINT64_MAX;
    };

    /*!
     * \class DatabaseCheckpointLoader
     * \brief This class retrieves checkpoints from disk for post-simulation
     * use cases.
     */
    class DatabaseCheckpointLoader
    {
    public:
        //! \brief Construct with one root from which we'll find the ArchData's
        DatabaseCheckpointLoader(simdb::DatabaseManager* db_mgr, TreeNode* root) :
            DatabaseCheckpointLoader(db_mgr, std::vector<TreeNode*>({root}))
        {}

        //! \brief Construct with multiple roots from which we'll find the ArchData's
        DatabaseCheckpointLoader(simdb::DatabaseManager* db_mgr, const std::vector<TreeNode*>& roots);

        //! \brief Get the total number of committed checkpoints
        //! \note This is used as the max value for the linear index
        //! passed to loadCheckpoint()
        uint64_t getNumCheckpoints() const { return num_checkpoints_; }

        //! \brief Load a checkpoint with an index between [0,getNumCheckpoints())
        //! \throw Throws if index is out of range
        void loadCheckpoint(uint64_t checkpoint_idx) {
            if (checkpoint_idx >= num_checkpoints_) {
                throw SpartaException("Checkpoint index out of range");
            }
            loadCheckpoint_(checkpoint_idx);
        }

    private:
        //! \brief Find all ArchData's from the given roots (recursive)
        static std::vector<ArchData*> enumerateArchDatas_(const std::vector<TreeNode*>& roots);

        //! \brief Get the exact checkpoint from the given window
        static checkpoint_type* getChkptFromWindow_(arch_id_t arch_id, tick_t tick, ChkptWindow& window);

        //! \brief Get the exact checkpoint from our cache
        checkpoint_type* getChkptFromCache_(arch_id_t arch_id, tick_t tick);

        //! \brief Go to the database looking for the exact checkpoint with
        //! this arch ID and tick
        //! \note If found, this will overwrite the cached window
        checkpoint_type* getChkptFromDisk_(arch_id_t arch_id, tick_t tick);

        //! \brief Load a checkpoint with an index between [0,getNumCheckpoints())
        void loadCheckpoint_(uint64_t checkpoint_idx);

        //! \brief Database instance
        simdb::DatabaseManager *const db_mgr_;

        //! \brief ArchData's from the roots given to our ctor
        const std::vector<ArchData*> adatas_;

        //! \brief Hold onto one checkpoint window for performance
        std::unique_ptr<ChkptWindow> cached_window_;

        //! \brief Hold onto a linear history of arch IDs and their ticks
        std::vector<ArchIdsForTick> tick_runs_;

        //! \brief Max linear index into the unique (arch ID, tick) pair
        //! \note This is the total number of committed checkpoints
        uint64_t num_checkpoints_ = 0;
    };

    /*!
     * \class DatabaseCheckpointReplayer
     * \brief To assist with post-simulation replay, this class implements
     * a step() method
     */
    class DatabaseCheckpointReplayer
    {
    public:
        //! \brief Construct with one root from which we'll find the ArchData's
        DatabaseCheckpointReplayer(simdb::DatabaseManager* db_mgr, TreeNode* root) :
            DatabaseCheckpointReplayer(db_mgr, std::vector<TreeNode*>({root}))
        {}

        //! \brief Construct with multiple roots from which we'll find the ArchData's
        DatabaseCheckpointReplayer(simdb::DatabaseManager* db_mgr, const std::vector<TreeNode*>& roots) :
            chkpt_loader_(db_mgr, roots)
        {
            // Load the initial simulation state
            constexpr size_t first_head_chkpt_id = 0;
            chkpt_loader_.loadCheckpoint(first_head_chkpt_id);
        }

        //! \brief Get the total number of committed checkpoints
        uint64_t getNumCheckpoints() const { return chkpt_loader_.getNumCheckpoints(); }

        //! \brief Go to the next checkpoint and fill in the ArchData's from the roots
        //! \return Returns true if successful, false if we have no more checkpoints
        bool step();

    private:
        //! \brief Checkpoint loader to do the heavy lifting
        DatabaseCheckpointLoader chkpt_loader_;

        //! \brief Linear index of the next checkpoint to load
        //! \note Starts at 1 since the first checkpoint taken is the first head
        //! checkpoint, i.e. the simulator's starting state, which is loaded
        //! in our constructor.
        uint64_t next_checkpoint_idx_ = 1;
    };

    /*!
     * \brief Get the total number of checkpoints sent to the database thus far.
     * \note Do not call this in the critical path, as it will flush the whole
     * pipeline before querying the database.
     */
    size_t getNumCheckpoints() const;

    /*!
     * \brief Returns a string describing this object
     */
    std::string stringize() const;

    struct ChkptWindow {
        arch_id_t start_arch_id = UINT64_MAX;
        arch_id_t end_arch_id = UINT64_MAX;
        tick_t start_tick = UINT64_MAX;
        tick_t end_tick = UINT64_MAX;
        checkpoint_ptrs checkpoints;

        template <typename Archive>
        void serialize(Archive & ar, const unsigned int /*version*/) {
            ar & start_arch_id;
            ar & end_arch_id;
            ar & start_tick;
            ar & end_tick;

            if (checkpoints.empty()) {
                // We are loading a checkpoint window from disk
                const auto num_chkpts = end_arch_id - start_arch_id + 1;
                for (size_t i = 0; i < num_chkpts; ++i) {
                    checkpoints.emplace_back(new checkpoint_type);
                    ar & *checkpoints.back();
                }
            } else {
                // We are saving a checkpoint window to disk
                for (auto& chkpt : checkpoints) {
                    ar & *chkpt;
                }
            }
        }
    };

    struct ChkptWindowBytes {
        arch_id_t start_arch_id = UINT64_MAX;
        arch_id_t end_arch_id = UINT64_MAX;
        tick_t start_tick = UINT64_MAX;
        tick_t end_tick = UINT64_MAX;
        std::vector<char> chkpt_bytes;
    };

private:
    FastCheckpointer checkpointer_;
    simdb::DatabaseManager* db_mgr_ = nullptr;
    simdb::ConcurrentQueue<ChkptWindow>* chkpt_window_head_ = nullptr;
    simdb::ConcurrentQueue<ArchIdsForTick>* tick_runs_head_ = nullptr;
    std::queue<ArchIdsForTick> tick_runs_;
    uint64_t committed_arch_id_ = 0;
    std::unique_ptr<simdb::pipeline::Flusher> pipeline_flusher_;
    std::vector<chkpt_id_t> committed_checkpoints_;
};

} // namespace sparta::serialization::checkpoint
