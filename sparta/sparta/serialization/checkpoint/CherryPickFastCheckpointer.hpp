// <CherryPickFastCheckpointer> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "simdb/apps/App.hpp"
#include "simdb/utils/ConcurrentQueue.hpp"
#include <map>

namespace sparta
{
    class TreeNode;
    class Scheduler;
}

namespace simdb::pipeline
{
    class RunnableFlusher;
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
     */
    void commitCurrentBranch(bool force_new_head_chkpt = false);

    /*!
     * \brief Send the committed checkpoints down the pipeline to the database.
     */
    void saveCheckpoints(checkpoint_ptrs&& checkpoints);

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

private:
    FastCheckpointer checkpointer_;
    simdb::DatabaseManager* db_mgr_ = nullptr;

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
        size_t num_chkpts = 0;
        std::vector<char> chkpt_bytes;
    };

    simdb::ConcurrentQueue<ChkptWindow>* pipeline_head_ = nullptr;
    std::unique_ptr<simdb::pipeline::RunnableFlusher> pipeline_flusher_;
};

} // namespace sparta::serialization::checkpoint

namespace simdb
{

/*!
 * \brief This AppFactory specialization is provided since we have to initialize the FastCheckpointer
 * which takes the ArchData root(s) / Scheduler, and thus cannot have the default app subclass ctor
 * signature that only takes the DatabaseManager like most other apps.
 */
template <>
class AppFactory<sparta::serialization::checkpoint::CherryPickFastCheckpointer> : public AppFactoryBase
{
public:
    using AppT = sparta::serialization::checkpoint::CherryPickFastCheckpointer;

    /// \brief Sets the ArchData root(s) for a given instance of the checkpointer.
    /// \param instance_num 0 if using one checkpointer instance, else the instance number (1-based)
    /// \param roots TreeNode(s) at which ArchData will be taken
    /// \note Scheduler must be set separately via setScheduler()
    /// \note This is required before createEnabledApps() is called
    void setArchDataRoots(size_t instance_num, const std::vector<sparta::TreeNode*>& roots)
    {
        roots_by_inst_num_[instance_num] = roots;
    }

    /// \brief Sets the Scheduler for all instances of the checkpointer.
    /// \param sched Scheduler to use for the checkpoint's tick numbers
    /// \note This is optional if ticks are not needed
    void setScheduler(sparta::Scheduler& sched)
    {
        scheduler_ = &sched;
    }

    AppT* createApp(DatabaseManager* db_mgr, size_t instance_num = 0) override
    {
        auto it = roots_by_inst_num_.find(instance_num);
        if (it == roots_by_inst_num_.end()) {
            throw sparta::SpartaException(
                "No TreeNode (ArchData root) set for DatabaseCheckpointer instance number ")
                << instance_num << ". Did you forget to call setArchDataRoot()?";
        }

        const auto & roots = it->second;

        // Make the ctor call that the default AppFactory cannot make.
        return new AppT(db_mgr, roots, scheduler_);
    }

    void defineSchema(Schema& schema) const override
    {
        AppT::defineSchema(schema);
    }

private:
    sparta::Scheduler* scheduler_ = nullptr;
    std::map<size_t, std::vector<sparta::TreeNode*>> roots_by_inst_num_;
};

} // namespace simdb
