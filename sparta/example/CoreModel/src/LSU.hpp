
#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"

#include "cache/TreePLRUReplacement.hpp"

#include "CoreTypes.hpp"
#include "FlushManager.hpp"
#include "SimpleTLB.hpp"
#include "SimpleDL1.hpp"

namespace core_example
{
    class LSU : public sparta::Unit
    {
    public:
        /*!
         * \class LSUParameterSet
         * \brief Parameters for LSU model
         */
        class LSUParameterSet : public sparta::ParameterSet
        {
        public:
            //! Constructor for LSUParameterSet
            LSUParameterSet(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            {
            }

            // Parameters for ldst_inst_queue
            PARAMETER(uint32_t, ldst_inst_queue_size, 8, "LSU ldst inst queue size")
            // Parameters for the TLB cache
            PARAMETER(bool, tlb_always_hit, false, "L1 TLB will always hit")
            // Parameters for the DL1 cache
            PARAMETER(uint64_t, dl1_line_size, 64, "DL1 line size (power of 2)")
            PARAMETER(uint64_t, dl1_size_kb, 32, "Size of DL1 in KB (power of 2)")
            PARAMETER(uint64_t, dl1_associativity, 8, "DL1 associativity (power of 2)")
            PARAMETER(bool, dl1_always_hit, false, "DL1 will always hit")
            // Parameters for event scheduling
            PARAMETER(uint32_t, issue_latency, 1, "Instruction issue latency")
            PARAMETER(uint32_t, mmu_latency, 1, "MMU/TLB access latency")
            PARAMETER(uint32_t, cache_latency, 1, "Cache access latency")
            PARAMETER(uint32_t, complete_latency, 1, "Instruction complete latency")
        };

        /*!
         * \brief Constructor for LSU
         * \note  node parameter is the node that represent the LSU and
         *        p is the LSU parameter set
         */
        LSU(sparta::TreeNode* node, const LSUParameterSet* p);

        ~LSU() {
            debug_logger_ << getContainer()->getLocation()
                          << ": "
                          << load_store_info_allocator.getNumAllocated()
                          << " LoadStoreInstInfo objects allocated/created"
                          << std::endl;
            debug_logger_ << getContainer()->getLocation()
                          << ": "
                          << memory_access_allocator.getNumAllocated()
                          << " MemoryAccessInfo objects allocated/created"
                          << std::endl;
        }

        //! name of this resource.
        static const char name[];


        ////////////////////////////////////////////////////////////////////////////////
        // Type Name/Alias Declaration
        ////////////////////////////////////////////////////////////////////////////////


        class LoadStoreInstInfo;
        class MemoryAccessInfo;

        using LoadStoreInstInfoPtr = sparta::SpartaSharedPointer<LoadStoreInstInfo>;
        using MemoryAccessInfoPtr = sparta::SpartaSharedPointer<MemoryAccessInfo>;
        using FlushCriteria = FlushManager::FlushingCriteria;

        enum class PipelineStage
        {
            MMU_LOOKUP = 0,     //1,
            CACHE_LOOKUP = 1,   //3,
            COMPLETE = 2,       //4
            NUM_STAGES
        };

        // Forward declaration of the Pair Definition class is must as we are friending it.
        class MemoryAccessInfoPairDef;
        // Keep record of memory access information in LSU
        class MemoryAccessInfo {
        public:

            // The modeler needs to alias a type called "SpartaPairDefinitionType" to the Pair Definition class of itself
            using SpartaPairDefinitionType = MemoryAccessInfoPairDef;

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

        // allocator for this object type
        sparta::SpartaSharedPointer<MemoryAccessInfo>::SpartaSharedPointerAllocator memory_access_allocator;

        /*!
        * \class MemoryAccessInfoPairDef
        * \brief Pair Definition class of the Memory Access Information that flows through the example/CoreModel
        */

        // This is the definition of the PairDefinition class of MemoryAccessInfo.
        // This PairDefinition class could be named anything but it needs to inherit
        // publicly from sparta::PairDefinition templatized on the actual class MemoryAcccessInfo.
        class MemoryAccessInfoPairDef : public sparta::PairDefinition<MemoryAccessInfo>{
        public:

            // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
            MemoryAccessInfoPairDef() : PairDefinition<MemoryAccessInfo>(){
                SPARTA_INVOKE_PAIRS(MemoryAccessInfo);
            }
            SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("valid", &MemoryAccessInfo::getPhyAddrIsReady),
                                SPARTA_ADDPAIR("mmu", &MemoryAccessInfo::getMMUState),
                                SPARTA_ADDPAIR("cache", &MemoryAccessInfo::getCacheState),
                                SPARTA_FLATTEN(&MemoryAccessInfo::getInstPtr));
        };

        // Forward declaration of the Pair Definition class is must as we are friending it.
        class LoadStoreInstInfoPairDef;
        // Keep record of instruction issue information
        class LoadStoreInstInfo
        {
        public:
            // The modeler needs to alias a type called "SpartaPairDefinitionType" to the Pair Definition class  of itself
            using SpartaPairDefinitionType = LoadStoreInstInfoPairDef;
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

        sparta::SpartaSharedPointer<LoadStoreInstInfo>::SpartaSharedPointerAllocator load_store_info_allocator;

        /*!
        * \class LoadStoreInstInfoPairDef
        * \brief Pair Definition class of the load store instruction that flows through the example/CoreModel
        */
        // This is the definition of the PairDefinition class of LoadStoreInstInfo.
        // This PairDefinition class could be named anything but it needs to inherit
        // publicly from sparta::PairDefinition templatized on the actual class LoadStoreInstInfo.
        class LoadStoreInstInfoPairDef : public sparta::PairDefinition<LoadStoreInstInfo>{
        public:

            // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
            LoadStoreInstInfoPairDef() : PairDefinition<LoadStoreInstInfo>(){
                SPARTA_INVOKE_PAIRS(LoadStoreInstInfo);
            }
            SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("rank", &LoadStoreInstInfo::getPriority),
                                SPARTA_ADDPAIR("state", &LoadStoreInstInfo::getState),
                                SPARTA_FLATTEN(&LoadStoreInstInfo::getMemoryAccessInfoPtr));
        };

        void setTLB(SimpleTLB& tlb)
        {
            tlb_cache_ = &tlb;
        }
    private:
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<InstQueue::value_type> in_lsu_insts_
            {&unit_port_set_, "in_lsu_insts", 1};

        sparta::DataInPort<ExampleInstPtr> in_biu_ack_
            {&unit_port_set_, "in_biu_ack", 1};

        sparta::DataInPort<ExampleInstPtr> in_rob_retire_ack_
            {&unit_port_set_, "in_rob_retire_ack", 1};

        sparta::DataInPort<FlushCriteria> in_reorder_flush_
            {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};


        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataOutPort<uint32_t> out_lsu_credits_
            {&unit_port_set_, "out_lsu_credits"};

        sparta::DataOutPort<ExampleInstPtr> out_biu_req_
            {&unit_port_set_, "out_biu_req"};


        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        // Issue Queue
        using LoadStoreIssueQueue = sparta::Buffer<LoadStoreInstInfoPtr>;
        LoadStoreIssueQueue ldst_inst_queue_;
        const uint32_t ldst_inst_queue_size_;

        // TLB Cache
        SimpleTLB* tlb_cache_ = nullptr;
        const bool tlb_always_hit_;
        bool mmu_busy_ = false;
        bool mmu_pending_inst_flushed = false;
        // Keep track of the instruction that causes current outstanding TLB miss
        ExampleInstPtr mmu_pending_inst_ptr_ = nullptr;

        // NOTE:
        // Depending on how many outstanding TLB misses the MMU could handle at the same time
        // This single slot could potentially be extended to a mmu pending miss queue


        // L1 Data Cache
        using DL1Handle = SimpleDL1::Handle;
        DL1Handle dl1_cache_;
        const bool dl1_always_hit_;
        bool cache_busy_ = false;
        bool cache_pending_inst_flushed_ = false;
        // Keep track of the instruction that causes current outstanding cache miss
        ExampleInstPtr cache_pending_inst_ptr_ = nullptr;

        sparta::collection::Collectable<bool> cache_busy_collectable_{
            getContainer(), "dcache_busy", &cache_busy_};

        // NOTE:
        // Depending on which kind of cache (e.g. blocking vs. non-blocking) is being used
        // This single slot could potentially be extended to a cache pending miss queue


        // Load/Store Pipeline
        using LoadStorePipeline = sparta::Pipeline<MemoryAccessInfoPtr>;
        LoadStorePipeline ldst_pipeline_
            {"LoadStorePipeline", static_cast<uint32_t>(PipelineStage::NUM_STAGES), getClock()};

        // Event Scheduling Parameters
        const uint32_t issue_latency_;
        const uint32_t mmu_latency_;
        const uint32_t cache_latency_;
        const uint32_t complete_latency_;


        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to issue instruction
        sparta::UniqueEvent<> uev_issue_inst_{&unit_event_set_, "issue_inst",
                CREATE_SPARTA_HANDLER(LSU, issueInst_)};

        // Event to drive BIU request port from MMU
        sparta::UniqueEvent<> uev_mmu_drive_biu_port_ {&unit_event_set_, "mmu_drive_biu_port",
                CREATE_SPARTA_HANDLER(LSU, driveBIUPortFromMMU_)};

        // Event to drive BIU request port from Cache
        sparta::UniqueEvent<> uev_cache_drive_biu_port_ {&unit_event_set_, "cache_drive_biu_port",
                CREATE_SPARTA_HANDLER(LSU, driveBIUPortFromCache_)};


        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Send initial credits (ldst_inst_queue_size_) to Dispatch Unit
        void sendInitialCredits_();

        // Receive new load/store Instruction from Dispatch Unit
        void getInstsFromDispatch_(const ExampleInstPtr &);

        // Receive MSS access acknowledge from Bus Interface Unit
        void getAckFromBIU_(const ExampleInstPtr &);

        // Receive update from ROB whenever store instructions retire
        void getAckFromROB_(const ExampleInstPtr &);

        // Issue/Re-issue ready instructions in the issue queue
        void issueInst_();

        // Handle MMU access request
        void handleMMULookupReq_();

        // Drive BIU request port from MMU
        void driveBIUPortFromMMU_();

        // Handle cache access request
        void handleCacheLookupReq_();

        // Drive BIU request port from cache
        void driveBIUPortFromCache_();

        // Retire load/store instruction
        void completeInst_();

        // Handle instruction flush in LSU
        void handleFlush_(const FlushCriteria &);


        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        // Append new load/store instruction into issue queue
        void appendIssueQueue_(const LoadStoreInstInfoPtr &);

        // Pop completed load/store instruction out of issue queue
        void popIssueQueue_(const ExampleInstPtr &);

        // Arbitrate instruction issue from ldst_inst_queue
        const LoadStoreInstInfoPtr & arbitrateInstIssue_();

        // Check for ready to issue instructions
        bool isReadyToIssueInsts_() const;

        // Access MMU/TLB
        bool MMULookup_(const MemoryAccessInfoPtr &);

        // Re-handle outstanding MMU access request
        void rehandleMMULookupReq_(const ExampleInstPtr &);

        // Reload TLB entry
        void reloadTLB_(uint64_t);

        // Access Cache
        bool cacheLookup_(const MemoryAccessInfoPtr &);

        // Re-handle outstanding cache access request
        void rehandleCacheLookupReq_(const ExampleInstPtr &);

        // Reload cache line
        void reloadCache_(uint64_t);

        // Update issue priority after dispatch
        void updateIssuePriorityAfterNewDispatch_(const ExampleInstPtr &);

        // Update issue priority after TLB reload
        void updateIssuePriorityAfterTLBReload_(const ExampleInstPtr &,
                                                const bool = false);

        // Update issue priority after cache reload
        void updateIssuePriorityAfterCacheReload_(const ExampleInstPtr &,
                                                  const bool = false);

        // Update issue priority after store instruction retires
        void updateIssuePriorityAfterStoreInstRetire_(const ExampleInstPtr &);

        // Flush instruction issue queue
        template<typename Comp>
        void flushIssueQueue_(const Comp &);

        // Flush load/store pipeline
        template<typename Comp>
        void flushLSPipeline_(const Comp &);
    };

    inline std::ostream & operator<<(std::ostream & os,
        const core_example::LSU::MemoryAccessInfo::MMUState & mmu_access_state){
        switch(mmu_access_state){
            case LSU::MemoryAccessInfo::MMUState::NO_ACCESS:
                os << "no_access";
                break;
            case LSU::MemoryAccessInfo::MMUState::MISS:
                os << "miss";
                break;
            case LSU::MemoryAccessInfo::MMUState::HIT:
                os << "hit";
                break;
            case LSU::MemoryAccessInfo::MMUState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
        const core_example::LSU::MemoryAccessInfo::CacheState & cache_access_state){
        switch(cache_access_state){
            case LSU::MemoryAccessInfo::CacheState::NO_ACCESS:
                os << "no_access";
                break;
            case LSU::MemoryAccessInfo::CacheState::MISS:
                os << "miss";
                break;
            case LSU::MemoryAccessInfo::CacheState::HIT:
                os << "hit";
                break;
            case LSU::MemoryAccessInfo::CacheState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os,
        const core_example::LSU::LoadStoreInstInfo::IssuePriority& rank){
        switch(rank){
            case LSU::LoadStoreInstInfo::IssuePriority::HIGHEST:
                os << "(highest)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::CACHE_RELOAD:
                os << "($_reload)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::CACHE_PENDING:
                os << "($_pending)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::MMU_RELOAD:
                os << "(mmu_reload)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::MMU_PENDING:
                os << "(mmu_pending)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::NEW_DISP:
                os << "(new_disp)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::LOWEST:
                os << "(lowest)";
                break;
            case LSU::LoadStoreInstInfo::IssuePriority::NUM_OF_PRIORITIES:
                throw sparta::SpartaException("NUM_OF_PRIORITIES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os,
        const core_example::LSU::LoadStoreInstInfo::IssueState& state){
        // Print instruction issue state
        switch(state){
            case LSU::LoadStoreInstInfo::IssueState::READY:
                os << "(ready)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::ISSUED:
                os << "(issued)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::NOT_READY:
                os << "(not_ready)";
                break;
            case LSU::LoadStoreInstInfo::IssueState::NUM_STATES:
                throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }
} // namespace core_example
