#include "sparta/serialization/checkpoint/DatabaseCheckpointQuery.hpp"

namespace sparta::serialization::checkpoint
{

using chkpt_id_t = typename DatabaseCheckpointQuery::chkpt_id_t;
using tick_t = typename DatabaseCheckpointQuery::tick_t;

uint64_t DatabaseCheckpointQuery::getTotalMemoryUse() const noexcept
{
    //TODO cnyce
    return 0;
}

uint64_t DatabaseCheckpointQuery::getContentMemoryUse() const noexcept
{
    //TODO cnyce
    return 0;
}

void DatabaseCheckpointQuery::deleteCheckpoint(chkpt_id_t id)
{
    //TODO cnyce
    (void)id;
}

void DatabaseCheckpointQuery::loadCheckpoint(chkpt_id_t id)
{
    //TODO cnyce
    (void)id;
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

uint32_t DatabaseCheckpointQuery::getNumCheckpoints() const noexcept
{
    //TODO cnyce
    return 0;
}

std::deque<chkpt_id_t> DatabaseCheckpointQuery::getCheckpointChain(chkpt_id_t id) const
{
    //TODO cnyce
    (void)id;
    return {};
}

bool DatabaseCheckpointQuery::hasCheckpoint(chkpt_id_t id) const noexcept
{
    //TODO cnyce
    (void)id;
    return false;
}

void DatabaseCheckpointQuery::dumpList(std::ostream& o) const
{
    //TODO cnyce
    (void)o;
}

void DatabaseCheckpointQuery::dumpData(std::ostream& o) const
{
    //TODO cnyce
    (void)o;
}

void DatabaseCheckpointQuery::dumpAnnotatedData(std::ostream& o) const
{
    //TODO cnyce
    (void)o;
}

void DatabaseCheckpointQuery::traceValue(std::ostream& o, chkpt_id_t id, const ArchData* container, uint32_t offset, uint32_t size)
{
    //TODO cnyce
    (void)o;
    (void)id;
    (void)container;
    (void)offset;
    (void)size;
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

void DatabaseCheckpointQuery::createHead_()
{
}

chkpt_id_t DatabaseCheckpointQuery::createCheckpoint_(bool force_snapshot)
{
    //TODO cnyce
    (void)force_snapshot;
    return 0;    
}

void DatabaseCheckpointQuery::dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) const
{
    //TODO cnyce
    (void)id;
    (void)o;
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getNextIDs_(chkpt_id_t id) const
{
    //TODO cnyce
    (void)id;
    return {};
}

} // namespace sparta::serialization::checkpoint
