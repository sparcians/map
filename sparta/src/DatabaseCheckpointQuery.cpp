#include "sparta/serialization/checkpoint/DatabaseCheckpointQuery.hpp"

namespace sparta::serialization::checkpoint
{

using chkpt_id_t = typename DatabaseCheckpointQuery::chkpt_id_t;
using tick_t = typename DatabaseCheckpointQuery::tick_t;

bool DatabaseCheckpointQuery::hasCheckpoint(chkpt_id_t id) const noexcept
{
    //TODO cnyce
    (void)id;
    return false;
}

chkpt_id_t DatabaseCheckpointQuery::getPrevID(chkpt_id_t id) const
{
    //TODO cnyce
    (void)id;
    return 0;
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getNextIDs(chkpt_id_t id) const
{
    //TODO cnyce
    (void)id;
    return {};
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getCheckpointsAt(tick_t t) const
{
    //TODO cnyce
    (void)t;
    return {};
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getCheckpoints() const
{
    //TODO cnyce
    return {};
}

uint32_t DatabaseCheckpointQuery::getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept
{
    //TODO cnyce
    (void)id;
    return 0;
}

bool DatabaseCheckpointQuery::canDelete(chkpt_id_t id) const noexcept
{
    //TODO cnyce
    (void)id;
    return false;
}

} // namespace sparta::serialization::checkpoint
