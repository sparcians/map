// <ExampleInst.h> -*- C++ -*-


#pragma once

#include "sparta/decode/DecoderBase.hpp"
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/resources/SharedData.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
#include "simdb/serialize/Serialize.hpp"

#include <cstdlib>
#include <ostream>
#include <map>

namespace core_example
{
    /*!
    * \class ExampleInst
    * \brief Example instruction that flows through the example/CoreModel
    */

    // Forward declaration of the Pair Definition class is must as we are friending it.
    class ExampleInstPairDef;

    class ExampleInst {
    public:

        // The modeler needs to alias a type called "SpartaPairDefinitionType" to the Pair Definition class  of itself
        using SpartaPairDefinitionType = ExampleInstPairDef;

        enum class Status : std::uint16_t{
            FETCHED = 0,
            __FIRST = FETCHED,
            DECODED,
            RENAMED,
            SCHEDULED,
            COMPLETED,
            RETIRED,
            __LAST
        };

        enum class TargetUnit : std::uint16_t{
            ALU0,
            ALU1,
            FPU,
            BR,
            LSU,
            ROB, // Instructions that go right to retire
            N_TARGET_UNITS
        };

        struct StaticInfo {
            sparta::decode::DecoderBase decode_base;
            TargetUnit unit;
            uint32_t execute_time;
            bool is_store_inst;
        };

        using InstStatus = sparta::SharedData<Status>;

        ExampleInst(const sparta::decode::DecoderBase & static_inst,
                    TargetUnit unit,
                    uint32_t execute_time,
                    bool isStore,
                    const sparta::Clock * clk,
                    Status state) :
            static_inst_(static_inst),
            unit_(unit),
            execute_time_(execute_time),
            isStoreInst_(isStore),
            status_("inst_status", clk, state),
            status_state_(state) {}

        ExampleInst(const StaticInfo & info,
                    const sparta::Clock * clk,
                    Status state = Status::FETCHED) :
            ExampleInst(info.decode_base,
                        info.unit,
                        info.execute_time,
                        info.is_store_inst,
                        clk,
                        state)
        {}

        const sparta::decode::DecoderBase & getStaticInst() const {
            return static_inst_;
        }

        const Status & getStatus() const {
            return status_state_.getEnumValue();
        }

        bool getCompletedStatus() const {
            return getStatus() == core_example::ExampleInst::Status::COMPLETED;
        }

        void setStatus(Status status) {
            status_state_.setValue(status);
            status_.write(status);
            if(getStatus() == core_example::ExampleInst::Status::COMPLETED) {
                if(ev_retire_ != 0) {
                    ev_retire_->schedule();
                }
            }
        }

        const TargetUnit& getUnit() const {
            return unit_;
        }

        void setLast(bool last, sparta::Scheduleable * rob_retire_event) {
            ev_retire_ = rob_retire_event;
            is_last_ = last;

            if(status_.isValidNS() && status_.readNS() == core_example::ExampleInst::Status::COMPLETED) {
                ev_retire_->schedule();
            }
        }

        void setVAdr(uint64_t vaddr) {
            vaddr_ = vaddr;
        }

        void setUniqueID(uint64_t uid) {
            unique_id_ = uid;
        }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getUniqueID() const {
            return unique_id_;
        }

        void setSpeculative(bool spec) {
            is_speculative_ = spec;
        }

        const char* getMnemonic() const { return static_inst_.mnemonic; }
        uint32_t getOpCode() const { return static_inst_.encoding; }
        uint64_t getVAdr() const { return vaddr_; }
        uint64_t getRAdr() const { return vaddr_ | 0x3000; } // faked
        uint64_t getParentId() const { return 0; }
        uint32_t getExecuteTime() const { return execute_time_; }
        bool isSpeculative() const { return is_speculative_; }
        bool isStoreInst() const { return isStoreInst_; }

    private:

        const sparta::decode::DecoderBase static_inst_;
        TargetUnit unit_;
        const uint32_t execute_time_ = 0;
        bool isStoreInst_ = false;
        sparta::memory::addr_t vaddr_ = 0;
        bool is_last_ = false;
        uint64_t unique_id_ = 0; // Supplied by Fetch
        bool is_speculative_ = false; // Is this instruction soon to be flushed?
        sparta::Scheduleable * ev_retire_ = nullptr;
        InstStatus status_;
        sparta::State<Status> status_state_;
    };

    extern sparta::SpartaSharedPointerAllocator<ExampleInst> example_inst_allocator;

    inline std::ostream & operator<<(std::ostream & os, const ExampleInst & inst) {
        os << inst.getMnemonic();
        return os;
    }

    typedef sparta::SpartaSharedPointer<ExampleInst> ExampleInstPtr;
    inline std::ostream & operator<<(std::ostream & os, const ExampleInstPtr & inst) {
        os << *inst;
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const ExampleInst::TargetUnit & unit) {
        switch(unit) {
            case ExampleInst::TargetUnit::ALU0:
                os << "ALU0";
                break;
            case ExampleInst::TargetUnit::ALU1:
                os << "ALU1";
                break;
            case ExampleInst::TargetUnit::FPU:
                os << "FPU";
                break;
            case ExampleInst::TargetUnit::BR:
                os << "BR";
                break;
            case ExampleInst::TargetUnit::LSU:
                os << "LSU";
                break;
            case ExampleInst::TargetUnit::ROB:
                os << "ROB";
                break;
            case ExampleInst::TargetUnit::N_TARGET_UNITS:
                throw sparta::SpartaException("N_TARGET_UNITS cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const ExampleInst::Status & status) {
        switch(status) {
            case ExampleInst::Status::FETCHED:
                os << "FETCHED";
                break;
            case ExampleInst::Status::DECODED:
                os << "DECODED";
                break;
            case ExampleInst::Status::RENAMED:
                os << "RENAMED";
                break;
            case ExampleInst::Status::SCHEDULED:
                os << "SCHEDULED";
                break;
            case ExampleInst::Status::COMPLETED:
                os << "COMPLETED";
                break;
            case ExampleInst::Status::RETIRED:
                os << "RETIRED";
                break;
            case ExampleInst::Status::__LAST:
                throw sparta::SpartaException("__LAST cannot be a valid enum state.");
        }
        return os;
    }

    /*!
    * \class ExampleInstPairDef
    * \brief Pair Definition class of the Example instruction that flows through the example/CoreModel
    */
    // This is the definition of the PairDefinition class of ExampleInst.
    // This PairDefinition class could be named anything but it needs to
    // inherit publicly from sparta::PairDefinition templatized on the actual class ExampleInst.
    class ExampleInstPairDef : public sparta::PairDefinition<ExampleInst>{
        public:
            // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
        ExampleInstPairDef() : PairDefinition<ExampleInst>(){
            SPARTA_INVOKE_PAIRS(ExampleInst);
        }
        SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",      &ExampleInst::getUniqueID),
                              SPARTA_ADDPAIR("uid",      &ExampleInst::getUniqueID),
                              SPARTA_ADDPAIR("mnemonic", &ExampleInst::getMnemonic),
                              SPARTA_ADDPAIR("complete", &ExampleInst::getCompletedStatus),
                              SPARTA_ADDPAIR("unit",     &ExampleInst::getUnit),
                              SPARTA_ADDPAIR("latency",  &ExampleInst::getExecuteTime),
                              SPARTA_ADDPAIR("raddr",    &ExampleInst::getRAdr, std::ios::hex),
                              SPARTA_ADDPAIR("vaddr",    &ExampleInst::getVAdr, std::ios::hex))
    };
}

namespace simdb
{

template <>
inline void defineEnumMap<core_example::ExampleInst::TargetUnit>(std::string& enum_name, std::map<std::string, uint16_t>& map)
{
    using TargetUnit = core_example::ExampleInst::TargetUnit;

    enum_name = "TargetUnit";
    map["ALU0"] = static_cast<int32_t>(TargetUnit::ALU0);
    map["ALU1"] = static_cast<int32_t>(TargetUnit::ALU1);
    map["FPU"]  = static_cast<int32_t>(TargetUnit::FPU);
    map["BR"]   = static_cast<int32_t>(TargetUnit::BR);
    map["LSU"]  = static_cast<int32_t>(TargetUnit::LSU);
    map["ROB"]  = static_cast<int32_t>(TargetUnit::ROB);
}

template <>
inline void defineStructSchema<core_example::ExampleInst>(StructSchema<core_example::ExampleInst>& schema)
{
    using TargetUnit = core_example::ExampleInst::TargetUnit;

    schema.addField<uint64_t>("DID");
    schema.addField<uint64_t>("uid");
    schema.addField<std::string>("mnemonic");
    schema.addBoolField("complete");
    schema.addField<TargetUnit>("unit");
    schema.addField<uint32_t>("latency");
    schema.addHexField<uint64_t>("raddr");
    schema.addHexField<uint64_t>("vaddr");
    schema.setAutoColorizeColumn("DID");
}

template <>
inline void writeStructFields<core_example::ExampleInst>(const core_example::ExampleInst* inst, StructFieldSerializer<core_example::ExampleInst>* serializer)
{
    serializer->writeField<uint64_t>(inst->getUniqueID());
    serializer->writeField<uint64_t>(inst->getUniqueID());
    serializer->writeField(inst->getMnemonic());
    serializer->writeField<int32_t>(inst->getCompletedStatus());
    serializer->writeField<core_example::ExampleInst::TargetUnit>(inst->getUnit());
    serializer->writeField<uint32_t>(inst->getExecuteTime());
    serializer->writeField<uint64_t>(inst->getRAdr());
    serializer->writeField<uint64_t>(inst->getVAdr());
}

} // namespace simdb
