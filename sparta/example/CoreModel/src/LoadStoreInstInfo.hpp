#pragma once

#include "MemAccessInfo.hpp"

namespace core_example
{
    class LoadStoreInstInfo;
    using LoadStoreInstInfoPtr = sparta::SpartaSharedPointer<LoadStoreInstInfo>;

    // Keep record of instruction issue information
    class LoadStoreInstInfo
    {
    public:
        enum class IssuePriority : std::uint16_t
        {
            HIGHEST = 0,
            __FIRST = HIGHEST,
            CACHE_RELOAD,   // Receive mss ack, waiting for cache re-access
            CACHE_PENDING,  // Wait for another outstanding miss finish
            MMU_RELOAD,     // Receive for mss ack, waiting for mmu re-access
            MMU_PENDING,    // Wait for another outstanding miss finish
            NEW_DISP,       // Wait for new issue
            LOWEST,
            NUM_OF_PRIORITIES,
            __LAST = NUM_OF_PRIORITIES
        };

        enum class IssueState : std::uint32_t
        {
            READY = 0,          // Ready to be issued
            __FIRST = READY,
            ISSUED,         // On the flight somewhere inside Load/Store Pipe
            NOT_READY,      // Not ready to be issued
            NUM_STATES,
            __LAST = NUM_STATES
        };

        LoadStoreInstInfo() = delete;
        LoadStoreInstInfo(const MemoryAccessInfoPtr & info_ptr) :
            mem_access_info_ptr_(info_ptr),
            rank_(IssuePriority::LOWEST),
            state_(IssueState::NOT_READY) {}

        // This ExampleInst pointer will act as one of the two portals to the ExampleInst class
        // and we will use this pointer to query values from functions of ExampleInst class
        const ExampleInstPtr & getInstPtr() const {
            return mem_access_info_ptr_->getInstPtr();
        }

        // This MemoryAccessInfo pointer will act as one of the two portals to the MemoryAccesInfo class
        // and we will use this pointer to query values from functions of MemoryAccessInfo class
        const MemoryAccessInfoPtr & getMemoryAccessInfoPtr() const {
            return mem_access_info_ptr_;
        }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getInstUniqueID() const {
            const MemoryAccessInfoPtr &mem_access_info_ptr = getMemoryAccessInfoPtr();

            return mem_access_info_ptr == nullptr ? 0 : mem_access_info_ptr->getInstUniqueID();
        }

        void setPriority(const IssuePriority & rank) {
            rank_.setValue(rank);
        }

        const IssuePriority & getPriority() const {
            return rank_.getEnumValue();
        }

        void setState(const IssueState & state) {
            state_.setValue(state);
            }

        const IssueState & getState() const {
            return state_.getEnumValue();
        }


        bool isReady() const { return (getState() == IssueState::READY); }

        bool winArb(const LoadStoreInstInfoPtr & that) const
        {
            if (that == nullptr) {
                return true;
            }

            return (static_cast<uint32_t>(this->getPriority())
                < static_cast<uint32_t>(that->getPriority()));
        }

    private:
        MemoryAccessInfoPtr mem_access_info_ptr_;
        sparta::State<IssuePriority> rank_;
        sparta::State<IssueState> state_;

    };  // class LoadStoreInstInfo
} // namespace core_example

namespace simdb
{

template <>
inline void defineEnumMap<core_example::LoadStoreInstInfo::IssuePriority>(std::string& enum_name, std::map<std::string, uint16_t>& map)
{
    using IssuePriority = core_example::LoadStoreInstInfo::IssuePriority;

    enum_name = "IssuePriority";
    map["HIGHEST"] = static_cast<int32_t>(IssuePriority::HIGHEST);
    map["CACHE_RELOAD"] = static_cast<int32_t>(IssuePriority::CACHE_RELOAD);
    map["CACHE_PENDING"] = static_cast<int32_t>(IssuePriority::CACHE_PENDING);
    map["MMU_RELOAD"] = static_cast<int32_t>(IssuePriority::MMU_RELOAD);
    map["MMU_PENDING"] = static_cast<int32_t>(IssuePriority::MMU_PENDING);
    map["NEW_DISP"] = static_cast<int32_t>(IssuePriority::NEW_DISP);
    map["LOWEST"] = static_cast<int32_t>(IssuePriority::LOWEST);
}

template <>
inline void defineEnumMap<core_example::LoadStoreInstInfo::IssueState>(std::string& enum_name, std::map<std::string, uint32_t>& map)
{
    using IssueState = core_example::LoadStoreInstInfo::IssueState;

    enum_name = "IssueState";
    map["READY"] = static_cast<int32_t>(IssueState::READY);
    map["ISSUED"] = static_cast<int32_t>(IssueState::ISSUED);
    map["NOT_READY"] = static_cast<int32_t>(IssueState::NOT_READY);
}

} // namespace simdb

inline std::ostream& operator<<(std::ostream& os,
    const core_example::LoadStoreInstInfo::IssuePriority& rank){
    switch(rank){
        case core_example::LoadStoreInstInfo::IssuePriority::HIGHEST:
            os << "(highest)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::CACHE_RELOAD:
            os << "($_reload)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::CACHE_PENDING:
            os << "($_pending)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::MMU_RELOAD:
            os << "(mmu_reload)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::MMU_PENDING:
            os << "(mmu_pending)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::NEW_DISP:
            os << "(new_disp)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::LOWEST:
            os << "(lowest)";
            break;
        case core_example::LoadStoreInstInfo::IssuePriority::NUM_OF_PRIORITIES:
            throw sparta::SpartaException("NUM_OF_PRIORITIES cannot be a valid enum state.");
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os,
    const core_example::LoadStoreInstInfo::IssueState& state){
    // Print instruction issue state
    switch(state){
        case core_example::LoadStoreInstInfo::IssueState::READY:
            os << "(ready)";
            break;
        case core_example::LoadStoreInstInfo::IssueState::ISSUED:
            os << "(issued)";
            break;
        case core_example::LoadStoreInstInfo::IssueState::NOT_READY:
            os << "(not_ready)";
            break;
        case core_example::LoadStoreInstInfo::IssueState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
    }
    return os;
}

namespace simdb
{

template <>
inline void defineStructSchema<core_example::LoadStoreInstInfo>(StructSchema& schema)
{
    schema.setStructName("LSInstInfo");
    schema.addField<uint64_t>("DID");
    schema.addField<core_example::LoadStoreInstInfo::IssuePriority>("rank");
    schema.addField<core_example::LoadStoreInstInfo::IssueState>("state");
    schema.setAutoColorizeColumn("DID");
}

template <>
inline void writeStructFields<core_example::LoadStoreInstInfo>(
    const core_example::LoadStoreInstInfo* inst,
    StructFieldSerializer<core_example::LoadStoreInstInfo>* serializer)
{
    serializer->writeField<uint64_t>(inst->getInstUniqueID());
    serializer->writeField<core_example::LoadStoreInstInfo::IssuePriority>(inst->getPriority());
    serializer->writeField<core_example::LoadStoreInstInfo::IssueState>(inst->getState());
}

} // namespace simdb
