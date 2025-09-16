#include "sparta/serialization/checkpoint/DatabaseCheckpointQuery.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpoint.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"
#include "simdb/sqlite/Iterator.hpp"
#include "simdb/utils/Compress.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

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
    tagged_deleted_ids_.insert(id);
}

void DatabaseCheckpointQuery::loadCheckpoint(chkpt_id_t)
{
    throw CheckpointError("DatabaseCheckpointQuery::loadCheckpoint() not supported");
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getCheckpointsAt(tick_t t) const
{
    // SELECT ChkptWindowBytesID FROM ChkptWindowTicks WHERE t <= EndTick AND t >= StartTick
    auto query = db_mgr_->createQuery("ChkptWindowTicks");

    query->addConstraintForUInt64("StartTick", simdb::Constraints::LESS_EQUAL, t);
    query->addConstraintForUInt64("EndTick", simdb::Constraints::GREATER_EQUAL, t);

    int window_id;
    query->select("ChkptWindowBytesID", window_id);

    auto results = query->getResultSet();
    if (!results.getNextRecord()) {
        return {};
    }

    // SELECT ChkptID FROM ChkptWindowIDs WHERE ChkptWindowBytesID = <window_id>
    query = db_mgr_->createQuery("ChkptWindowIDs");

    int chkpt_id;
    query->select("ChkptID", chkpt_id);
    query->addConstraintForInt("ChkptWindowBytesID", simdb::Constraints::EQUAL, window_id);

    auto results2 = query->getResultSet();
    std::vector<chkpt_id_t> ids;
    while (results2.getNextRecord()) {
        if (auto chkpt = findCheckpoint(chkpt_id)) {
            if (chkpt->getTick() == t) {
                ids.push_back(chkpt_id);
            }
        }
    }

    return ids;
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getCheckpoints() const
{
    auto query = db_mgr_->createQuery("ChkptWindowIDs");

    int chkpt_id;
    query->select("ChkptID", chkpt_id);

    auto results = query->getResultSet();
    std::vector<chkpt_id_t> ids;
    while (results.getNextRecord()) {
        ids.push_back(chkpt_id);
    }

    return ids;
}

uint32_t DatabaseCheckpointQuery::getNumCheckpoints() const noexcept
{
    auto query = db_mgr_->createQuery("ChkptWindowIDs");
    return query->count();
}

std::deque<chkpt_id_t> DatabaseCheckpointQuery::getCheckpointChain(chkpt_id_t id) const
{
    //TODO cnyce
    (void)id;
    return {};
}

bool DatabaseCheckpointQuery::hasCheckpoint(chkpt_id_t id) const noexcept
{
    auto query = db_mgr_->createQuery("ChkptWindowIDs");
    query->addConstraintForUInt64("ChkptID", simdb::Constraints::EQUAL, id);

    int bytes_id;
    query->select("ChkptWindowBytesID", bytes_id);

    auto results = query->getResultSet();
    return results.getNextRecord();
}

bool DatabaseCheckpointQuery::isSnapshot(chkpt_id_t id) const noexcept
{
    auto chkpt = findCheckpoint(id);
    return chkpt ? chkpt->isSnapshot() : false;
}

bool DatabaseCheckpointQuery::canDelete(chkpt_id_t id) const noexcept
{
    auto chkpt = findCheckpoint(id);
    return chkpt ? chkpt->canDelete() : false;
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
    (void)o;
    (void)id;
    (void)container;
    (void)offset;
    (void)size;

    sparta_assert(false, "Not implemented");
}

std::shared_ptr<DatabaseCheckpoint> DatabaseCheckpointQuery::findCheckpoint(chkpt_id_t id, bool must_exist) const
{
    // "Undo" task 6 (write to the database)
    auto query = db_mgr_->createQuery("ChkptWindowIDs");
    query->addConstraintForUInt64("ChkptID", simdb::Constraints::EQUAL, id);

    int window_id = 404;
    query->select("ChkptWindowBytesID", window_id);

    auto results1 = query->getResultSet();
    if (!results1.getNextRecord()) {
        if (must_exist) {
            throw CheckpointError("There is no checkpoint with ID ") << id;
        }
        return nullptr;
    }

    query = db_mgr_->createQuery("ChkptWindowBytes");
    query->addConstraintForInt("Id", simdb::Constraints::EQUAL, window_id);

    std::vector<char> bytes;
    query->select("WindowBytes", bytes);

    auto results2 = query->getResultSet();
    if (!results2.getNextRecord()) {
        if (must_exist) {
            throw CheckpointError("There is no checkpoint with ID ") << id;
        }
        return nullptr;
    }

    // "Undo" task 5 (zlib compression)
    std::vector<char> uncompressed;
    simdb::decompressData(bytes, uncompressed);

    // "Undo" task 4 (boost::serialization)
    namespace bio = boost::iostreams;
    bio::array_source src(uncompressed.data(), uncompressed.size());
    bio::stream<bio::array_source> is(src);

    boost::archive::binary_iarchive ia(is);
    ChkptWindow window;
    ia >> window;

    for (auto& chkpt : window.chkpts) {
        if (chkpt->getID() == id) {
            return chkpt;
        }
    }

    sparta_assert(false, "Should not be reachable");
    return nullptr;
}

chkpt_id_t DatabaseCheckpointQuery::getPrevID(chkpt_id_t id) const
{
    auto chkpt = findCheckpoint(id, true);
    return chkpt->getPrevID();
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getNextIDs(chkpt_id_t id, bool immediate_only) const
{
    std::vector<chkpt_id_t> next_ids;
    while (true) {
        auto chkpt = findCheckpoint(id, false);
        if (!chkpt) {
            break;
        }

        auto ids = chkpt->getNextIDs();
        if (ids.empty()) {
            break;
        }

        next_ids.insert(next_ids.end(), ids.begin(), ids.end());
        if (immediate_only) {
            break;
        }

        assert(ids.size() == 1);
        id = ids[0];
    }

    return next_ids;
}

uint32_t DatabaseCheckpointQuery::getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept
{
    //TODO cnyce
    (void)id;
    return 0;
}

void DatabaseCheckpointQuery::createHead_()
{
    throw CheckpointError("Cannot create checkpoint head for DatabaseCheckpointQuery");
}

chkpt_id_t DatabaseCheckpointQuery::createCheckpoint_(bool)
{
    throw CheckpointError("Cannot create checkpoint head for DatabaseCheckpointQuery");
}

void DatabaseCheckpointQuery::dumpCheckpointNode_(const chkpt_id_t, std::ostream&) const
{
    throw CheckpointError("Cannot dump checkpoint node for DatabaseCheckpointQuery");
}

std::vector<chkpt_id_t> DatabaseCheckpointQuery::getNextIDs_(chkpt_id_t id) const
{
    return getNextIDs(id);
}

} // namespace sparta::serialization::checkpoint
