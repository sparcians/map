// <DatabaseCheckpointQuery> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/Checkpointer.hpp"

namespace simdb
{
    class DatabaseManager;
}

namespace sparta::serialization::checkpoint
{

/*!
 * \brief SQLite query object to "extend" the checkpoint search space from just the
 * cache to include the database. Combinations of in-memory checkpoints, recreated
 * checkpoints, and database schema/query optimizations are used for performance.
 */
class DatabaseCheckpointQuery : public Checkpointer
{
public:
    DatabaseCheckpointQuery(simdb::DatabaseManager* db_mgr, TreeNode& root, sparta::Scheduler* sched=nullptr)
        : Checkpointer(root, sched)
        , db_mgr_(db_mgr)
    {}

    uint64_t getTotalMemoryUse() const noexcept override;

    uint64_t getContentMemoryUse() const noexcept override;

    bool hasCheckpoint(chkpt_id_t id) const noexcept override;

    void deleteCheckpoint(chkpt_id_t id) override;

    void loadCheckpoint(chkpt_id_t id) override;

    std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const override;

    std::vector<chkpt_id_t> getCheckpoints() const override;

    uint32_t getNumCheckpoints() const noexcept override;

    std::deque<chkpt_id_t> getCheckpointChain(chkpt_id_t id) const override;

    void dumpList(std::ostream& o) const override;

    void dumpData(std::ostream& o) const override;

    void dumpAnnotatedData(std::ostream& o) const override;

    void traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size) override;

    chkpt_id_t getPrevID(chkpt_id_t id) const;

    std::vector<chkpt_id_t> getNextIDs(chkpt_id_t id) const;

    uint32_t getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept;

    bool canDelete(chkpt_id_t id) const noexcept;

private:
    void createHead_() override;

    chkpt_id_t createCheckpoint_(bool force_snapshot=false) override;

    void dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) const override;

    std::vector<chkpt_id_t> getNextIDs_(chkpt_id_t id) const override;

    //! \brief SimDB instance
    simdb::DatabaseManager* db_mgr_ = nullptr;
};

} // namespace sparta::serialization::checkpoint
