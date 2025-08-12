namespace sparta::serialization::checkpoint
{

using chkpt_id_t = typename CheckpointBase::chkpt_id_t;

template <bool IsConst>
DatabaseCheckpointAccessor<IsConst>::DatabaseCheckpointAccessor(db_checkpointer* checkpointer, chkpt_id_t id)
{
    //TODO cnyce
    (void)checkpointer;
    (void)id;
}

template <bool IsConst>
DatabaseCheckpointAccessor<IsConst>::~DatabaseCheckpointAccessor()
{
    //TODO cnyce
}

template <bool IsConst>
std::string DatabaseCheckpointAccessor<IsConst>::stringize() const
{
    //TODO cnyce
    return "";
}

template <bool IsConst>
void DatabaseCheckpointAccessor<IsConst>::dumpData(std::ostream& o) const
{
    //TODO cnyce
    (void)o;
}

template <bool IsConst>
uint64_t DatabaseCheckpointAccessor<IsConst>::getTotalMemoryUse() const noexcept
{
    //TODO cnyce
    return 0;
}

template <bool IsConst>
uint64_t DatabaseCheckpointAccessor<IsConst>::getContentMemoryUse() const noexcept
{
    //TODO cnyce
    return 0;
}

template <bool IsConst>
void DatabaseCheckpointAccessor<IsConst>::load(const std::vector<ArchData*>& dats)
{
    //TODO cnyce
    (void)dats;
}

template <bool IsConst>
chkpt_id_t DatabaseCheckpointAccessor<IsConst>::getPrevID() const
{
    //TODO cnyce
    return 0;
}

template <bool IsConst>
std::vector<chkpt_id_t> DatabaseCheckpointAccessor<IsConst>::getNextIDs() const
{
    //TODO cnyce
    return {};
}

template <bool IsConst>
std::string DatabaseCheckpointAccessor<IsConst>::getDeletedRepr() const
{
    //TODO cnyce
    return "";
}

template <bool IsConst>
std::stack<chkpt_id_t> DatabaseCheckpointAccessor<IsConst>::getHistoryChain() const
{
    //TODO cnyce
    return {};
}

template <bool IsConst>
std::stack<chkpt_id_t> DatabaseCheckpointAccessor<IsConst>::getRestoreChain() const
{
    //TODO cnyce
    return {};
}

template <bool IsConst>
bool DatabaseCheckpointAccessor<IsConst>::canDelete() const noexcept
{
    //TODO cnyce
    return false;
}

template <bool IsConst>
void DatabaseCheckpointAccessor<IsConst>::flagDeleted()
{
    //TODO cnyce
}

template <bool IsConst>
bool DatabaseCheckpointAccessor<IsConst>::isFlaggedDeleted() const noexcept
{
    //TODO cnyce
    return false;
}

template <bool IsConst>
chkpt_id_t DatabaseCheckpointAccessor<IsConst>::getDeletedID() const noexcept
{
    //TODO cnyce
    return 0;
}

template <bool IsConst>
bool DatabaseCheckpointAccessor<IsConst>::isSnapshot() const noexcept
{
    //TODO cnyce
    return false;
}

template <bool IsConst>
uint32_t DatabaseCheckpointAccessor<IsConst>::getDistanceToPrevSnapshot() const noexcept
{
    //TODO cnyce
    return 0;
}

template <bool IsConst>
void DatabaseCheckpointAccessor<IsConst>::loadState(const std::vector<ArchData*>& dats)
{
    //TODO cnyce
    (void)dats;
}

} // namespace sparta::serialization::checkpoint
