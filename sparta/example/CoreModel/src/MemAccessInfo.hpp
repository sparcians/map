#pragma once

#include "ExampleInst.hpp"
#include "simdb/serialize/Serialize.hpp"

namespace core_example
{
    class MemoryAccessInfo;
    using MemoryAccessInfoPtr = sparta::SpartaSharedPointer<MemoryAccessInfo>;

    // Keep record of memory access information in LSU
    class MemoryAccessInfo {
    public:

        enum class MMUState : std::uint32_t {
            NO_ACCESS = 0,
            __FIRST = NO_ACCESS,
            MISS,
            HIT,
            NUM_STATES,
            __LAST = NUM_STATES
        };

        enum class CacheState : std::uint64_t {
            NO_ACCESS = 0,
            __FIRST = NO_ACCESS,
            MISS,
            HIT,
            NUM_STATES,
            __LAST = NUM_STATES
        };

        MemoryAccessInfo() = delete;

        MemoryAccessInfo(const ExampleInstPtr & inst_ptr) :
            ldst_inst_ptr_(inst_ptr),
            phyAddrIsReady_(false),
            mmu_access_state_(MMUState::NO_ACCESS),

            // Construct the State object here
            cache_access_state_(CacheState::NO_ACCESS) {}

        virtual ~MemoryAccessInfo() {}

        // This ExampleInst pointer will act as our portal to the ExampleInst class
        // and we will use this pointer to query values from functions of ExampleInst class
        const ExampleInstPtr & getInstPtr() const { return ldst_inst_ptr_; }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getInstUniqueID() const {
            const ExampleInstPtr &inst_ptr = getInstPtr();

            return inst_ptr == nullptr ? 0 : inst_ptr->getUniqueID();
        }

        void setPhyAddrStatus(bool isReady) { phyAddrIsReady_ = isReady; }
        bool getPhyAddrStatus() const { return phyAddrIsReady_; }

        const MMUState & getMMUState() const {
            return mmu_access_state_.getEnumValue();
        }

        void setMMUState(const MMUState & state) {
            mmu_access_state_.setValue(state);
        }

        const CacheState & getCacheState() const {
            return cache_access_state_.getEnumValue();
        }
        void setCacheState(const CacheState & state) {
            cache_access_state_.setValue(state);
        }

        // This is a function which will be added in the addArgs API.
        bool getPhyAddrIsReady() const{
            return phyAddrIsReady_;
        }

    private:
        // load/store instruction pointer
        ExampleInstPtr ldst_inst_ptr_;

        // Indicate MMU address translation status
        bool phyAddrIsReady_;

        // MMU access status
        sparta::State<MMUState> mmu_access_state_;

        // Cache access status
        sparta::State<CacheState> cache_access_state_;

    };  // class MemoryAccessInfo

    inline std::ostream & operator<<(std::ostream & os,
        const core_example::MemoryAccessInfo::MMUState & mmu_access_state){
        switch(mmu_access_state){
            case MemoryAccessInfo::MMUState::NO_ACCESS:
                os << "no_access";
                break;
            case MemoryAccessInfo::MMUState::MISS:
                os << "miss";
                break;
            case MemoryAccessInfo::MMUState::HIT:
                os << "hit";
                break;
            case MemoryAccessInfo::MMUState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
        const core_example::MemoryAccessInfo::CacheState & cache_access_state){
        switch(cache_access_state){
            case MemoryAccessInfo::CacheState::NO_ACCESS:
                os << "no_access";
                break;
            case MemoryAccessInfo::CacheState::MISS:
                os << "miss";
                break;
            case MemoryAccessInfo::CacheState::HIT:
                os << "hit";
                break;
            case MemoryAccessInfo::CacheState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

} // namespace core_example

namespace simdb
{

template <>
inline void defineEnumMap<core_example::MemoryAccessInfo::MMUState>(std::string& enum_name, std::map<std::string, uint32_t>& map)
{
    using MMUState = core_example::MemoryAccessInfo::MMUState;

    enum_name = "MMUState";
    map["NoAccess"] = static_cast<int32_t>(MMUState::NO_ACCESS);
    map["Miss"] = static_cast<int32_t>(MMUState::MISS);
    map["Hit"]  = static_cast<int32_t>(MMUState::HIT);
}

template <>
inline void defineEnumMap<core_example::MemoryAccessInfo::CacheState>(std::string& enum_name, std::map<std::string, uint64_t>& map)
{
    using CacheState = core_example::MemoryAccessInfo::CacheState;

    enum_name = "CacheState";
    map["NoAccess"] = static_cast<int32_t>(CacheState::NO_ACCESS);
    map["Miss"] = static_cast<int32_t>(CacheState::MISS);
    map["Hit"]  = static_cast<int32_t>(CacheState::HIT);
}

template <>
inline void defineStructSchema<core_example::MemoryAccessInfo>(StructSchema<core_example::MemoryAccessInfo>& schema)
{
    schema.addField<uint64_t>("DID");
    schema.addBoolField("valid");
    schema.addField<core_example::MemoryAccessInfo::MMUState>("mmu");
    schema.addField<core_example::MemoryAccessInfo::CacheState>("cache");
    schema.setAutoColorizeColumn("DID");
}

template <>
inline void writeStructFields<core_example::MemoryAccessInfo>(
    const core_example::MemoryAccessInfo* inst,
    StructFieldSerializer<core_example::MemoryAccessInfo>* serializer)
{
    serializer->writeField<uint64_t>(inst->getInstUniqueID());
    serializer->writeField<int32_t>(inst->getPhyAddrIsReady());
    serializer->writeField<core_example::MemoryAccessInfo::MMUState>(inst->getMMUState());
    serializer->writeField<core_example::MemoryAccessInfo::CacheState>(inst->getCacheState());
}

} // namespace simdb
