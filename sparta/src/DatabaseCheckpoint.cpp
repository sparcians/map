// <DatabaseCheckpoint> -*- C++ -*-

#include "sparta/serialization/checkpoint/DatabaseCheckpoint.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpointer.hpp"

namespace sparta::serialization::checkpoint
{

using tick_t = typename CheckpointBase::tick_t;
using chkpt_id_t = typename CheckpointBase::chkpt_id_t;
using checkpoint_type = DatabaseCheckpoint;
using checkpoint_ptr = std::shared_ptr<DatabaseCheckpoint>;

DatabaseCheckpoint::DatabaseCheckpoint(TreeNode& root,
                                       const std::vector<ArchData*>& dats,          
                                       chkpt_id_t id,
                                       tick_t tick,
                                       DatabaseCheckpoint* prev,
                                       bool is_snapshot,
                                       DatabaseCheckpointer* checkpointer)
    : DatabaseCheckpointBase(id, tick)
    , prev_id_(prev ? prev->getID() : UNIDENTIFIED_CHECKPOINT)
    , deleted_id_(UNIDENTIFIED_CHECKPOINT)
    , is_snapshot_(is_snapshot)
    , checkpointer_(checkpointer)
{
    (void)root;
    if (prev_id_ == UNIDENTIFIED_CHECKPOINT) {
        if (is_snapshot == false) {
            throw CheckpointError("Cannot create a DatabaseCheckpoint id=")
                << id << " at tick=" << tick << " which has no prev_delta and is not a snapshot";
        }
    }

    if (prev) {
        prev->next_ids_.push_back(getID());
    }

    // Store the checkpoint from root
    if (is_snapshot) {
        storeSnapshot_(dats);
    } else {
        storeDelta_(dats);
    }
}

DatabaseCheckpoint::DatabaseCheckpoint(chkpt_id_t prev_id,
                                       const std::vector<chkpt_id_t>& next_ids,
                                       chkpt_id_t deleted_id,
                                       bool is_snapshot,
                                       const storage::VectorStorage& storage,
                                       DatabaseCheckpointer* checkpointer)
    : DatabaseCheckpointBase(getID(), getTick())
    , prev_id_(prev_id)
    , next_ids_(next_ids)
    , deleted_id_(deleted_id)
    , is_snapshot_(is_snapshot)
    , data_(storage)
    , checkpointer_(checkpointer)
{
}

std::string DatabaseCheckpoint::stringize() const
{
    std::stringstream ss;
    ss << "<DatabaseCheckpoint id=";
    if (isFlaggedDeleted()) {
        ss << "DELETED";
    } else {
        ss << getID();
    }
    ss << " at t=" << getTick();
    if (isSnapshot()) {
        ss << "(snapshot)";
    }
    ss << ' ' << getTotalMemoryUse()/1000.0f << "kB (" << getContentMemoryUse()/1000.0f << "kB Data)";
    ss << '>';
    return ss.str();
}

void DatabaseCheckpoint::dumpData(std::ostream& o) const
{
    data_.dump(o);
}

uint64_t DatabaseCheckpoint::getTotalMemoryUse() const noexcept
{
    return getContentMemoryUse() \
        + sizeof(decltype(*this)) \
        + (getNextIDs().size() * sizeof(typename std::remove_reference<decltype(*this)>::type*));
}

uint64_t DatabaseCheckpoint::getContentMemoryUse() const noexcept
{
    return data_.getSize();
}

std::stack<chkpt_id_t> DatabaseCheckpoint::getHistoryChain() const
{
    return checkpointer_->getHistoryChain(getID());
}

std::stack<chkpt_id_t> DatabaseCheckpoint::getRestoreChain() const
{
    return checkpointer_->getRestoreChain(getID());
}

chkpt_id_t DatabaseCheckpoint::getPrevID() const
{
    return prev_id_;
}

std::vector<chkpt_id_t> DatabaseCheckpoint::getNextIDs() const
{
    return next_ids_;
}

void DatabaseCheckpoint::load(const std::vector<ArchData*>& dats)
{
    checkpointer_->load(dats, getID());
}

bool DatabaseCheckpoint::canDelete() const noexcept
{
    return checkpointer_->canDelete(getID());
}

void DatabaseCheckpoint::flagDeleted()
{
    sparta_assert(!isFlaggedDeleted(),
                  "Cannot delete a checkpoint when it is already deleted: " << this);
    deleted_id_ = getID();
    setID_(UNIDENTIFIED_CHECKPOINT);
}

bool DatabaseCheckpoint::isFlaggedDeleted() const noexcept
{
    return getID() == UNIDENTIFIED_CHECKPOINT;
}

chkpt_id_t DatabaseCheckpoint::getDeletedID() const noexcept
{
    return deleted_id_;
}

std::string DatabaseCheckpoint::getDeletedRepr() const
{
    std::stringstream ss;
    if (isFlaggedDeleted()) {
        ss << "*" << getDeletedID();
    } else {
        ss << getID();
    }
    return ss.str();
}

bool DatabaseCheckpoint::isSnapshot() const noexcept
{
    return is_snapshot_;
}

uint32_t DatabaseCheckpoint::getDistanceToPrevSnapshot() const noexcept
{
    return checkpointer_->getDistanceToPrevSnapshot(getID());
}

void DatabaseCheckpoint::loadState(const std::vector<ArchData*>& dats)
{
    data_.prepareForLoad();
    sparta_assert(data_.good(),
                  "Attempted to loadState from a DeltaCheckpoint with a bad data buffer");
    if(isSnapshot()){
        for(ArchData* ad : dats){
            ad->restoreAll(data_);
        }
    }else{
        for(ArchData* ad : dats){
            ad->restore(data_);
        }
    }
}

std::unique_ptr<DatabaseCheckpoint> DatabaseCheckpoint::clone() const
{
    auto clone = new DatabaseCheckpoint(prev_id_, next_ids_, deleted_id_, is_snapshot_, data_, checkpointer_);
    return std::unique_ptr<DatabaseCheckpoint>(clone);
}

void DatabaseCheckpoint::storeSnapshot_(const std::vector<ArchData*>& dats)
{
    sparta_assert(data_.good(),
                  "Attempted to storeSnapshot_ from a DatabaseCheckpoint with a bad data buffer");

    for (ArchData* ad : dats) {
        ad->saveAll(data_);
    }
}

void DatabaseCheckpoint::storeDelta_(const std::vector<ArchData*>& dats)
{
    sparta_assert(data_.good(),
                  "Attempted to storeDelta_ from a DatabaseCheckpoint with a bad data buffer");

    for (ArchData* ad : dats) {
        ad->save(data_);
    }
}

} // namespace sparta::serialization::checkpoint
