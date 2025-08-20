// <DatabaseCheckpointQuery> -*- C++ -*-

#pragma once

#include <stdint.h>
#include <vector>

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
class DatabaseCheckpointQuery
{
public:
    //! \brief Construct with a SimDB instance
    DatabaseCheckpointQuery(simdb::DatabaseManager* db_mgr)
        : db_mgr_(db_mgr)
    {}

    using chkpt_id_t = uint64_t;
    using tick_t = uint64_t;

    bool hasCheckpoint(chkpt_id_t id) const noexcept;

    chkpt_id_t getPrevID(chkpt_id_t id) const;

    std::vector<chkpt_id_t> getNextIDs(chkpt_id_t id) const;

    std::vector<chkpt_id_t> getCheckpointsAt(tick_t t) const;

    std::vector<chkpt_id_t> getCheckpoints() const;

    uint32_t getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept;

    bool canDelete(chkpt_id_t id) const noexcept;

private:
    //! \brief SimDB instance
    simdb::DatabaseManager* db_mgr_ = nullptr;
};

} // namespace sparta::serialization::checkpoint
