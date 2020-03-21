
#include "sparta/utils/SpartaAssert.hpp"
#include "LSU.hpp"

namespace core_example
{
    const char LSU::name[] = "lsu";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    LSU::LSU(sparta::TreeNode *node, const LSUParameterSet *p) :
        sparta::Unit(node),
        memory_access_allocator(50, 30),   // 50 and 30 are arbitrary numbers here.  It can be tuned to an exact value.
        load_store_info_allocator(50, 30),
        ldst_inst_queue_("lsu_inst_queue", p->ldst_inst_queue_size, getClock()),
        ldst_inst_queue_size_(p->ldst_inst_queue_size),

        tlb_always_hit_(p->tlb_always_hit),
        dl1_always_hit_(p->dl1_always_hit),

        issue_latency_(p->issue_latency),
        mmu_latency_(p->mmu_latency),
        cache_latency_(p->cache_latency),
        complete_latency_(p->complete_latency)
    {
        // Pipeline collection config
        ldst_pipeline_.enableCollection(node);
        ldst_inst_queue_.enableCollection(node);


        // Startup handler for sending initiatl credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(LSU, sendInitialCredits_));


        // Port config
        in_lsu_insts_.registerConsumerHandler<LSU, ExampleInstPtr, &LSU::getInstsFromDispatch_>(this);
        in_biu_ack_.registerConsumerHandler<LSU, ExampleInstPtr, &LSU::getAckFromBIU_>(this);
        in_rob_retire_ack_.registerConsumerHandler<LSU, ExampleInstPtr, &LSU::getAckFromROB_>(this);
        in_reorder_flush_.registerConsumerHandler<LSU, FlushManager::FlushingCriteria, &LSU::handleFlush_>(this);


        // Pipeline events config
        ldst_pipeline_.performOwnUpdates();
        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP),
                                                CREATE_SPARTA_HANDLER(LSU, handleMMULookupReq_));

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP),
                                                CREATE_SPARTA_HANDLER(LSU, handleCacheLookupReq_));

        ldst_pipeline_.registerHandlerAtStage(static_cast<uint32_t>(PipelineStage::COMPLETE),
                                                CREATE_SPARTA_HANDLER(LSU, completeInst_));


        // Event precedence setup
        uev_cache_drive_biu_port_ >> uev_mmu_drive_biu_port_;

        // NOTE:
        // To resolve the race condition when:
        // Both cache and MMU try to drive the single BIU port at the same cycle
        // Here we give cache the higher priority

        // DL1 cache config
        const uint64_t dl1_line_size = p->dl1_line_size;
        const uint64_t dl1_size_kb = p->dl1_size_kb;
        const uint64_t dl1_associativity = p->dl1_associativity;
        std::unique_ptr<sparta::cache::ReplacementIF> repl(new sparta::cache::TreePLRUReplacement
                                                         (dl1_associativity));
        dl1_cache_.reset(new SimpleDL1( getContainer(), dl1_size_kb, dl1_line_size, *repl ));

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "LSU construct: #" << node->getGroupIdx();
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Send initial credits (ldst_inst_queue_size_) to Dispatch Unit
    void LSU::sendInitialCredits_()
    {
        out_lsu_credits_.send(ldst_inst_queue_size_);


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "LSU initial credits for Dispatch Unit: " << ldst_inst_queue_size_;
        }
    }

    // Receive new load/store instruction from Dispatch Unit
    void LSU::getInstsFromDispatch_(const ExampleInstPtr & inst_ptr)
    {
        // Create load/store memory access info
        MemoryAccessInfoPtr mem_info_ptr = sparta::allocate_sparta_shared_pointer<MemoryAccessInfo>(memory_access_allocator,
                                                                                                    inst_ptr);

        // Create load/store instruction issue info
        LoadStoreInstInfoPtr inst_info_ptr = sparta::allocate_sparta_shared_pointer<LoadStoreInstInfo>(load_store_info_allocator,
                                                                                                       mem_info_ptr);

        // Append to instruction issue queue
        appendIssueQueue_(inst_info_ptr);

        // Update issue priority & Schedule an instruction issue event
        updateIssuePriorityAfterNewDispatch_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));

        // NOTE:
        // IssuePriority should always be updated before a new issue event is scheduled.
        // This guarantees that whenever a new instruction issue event is scheduled:
        // (1)Instruction issue queue already has "something READY";
        // (2)Instruction issue arbitration is guaranteed to be sucessful.


        // Update instruction status
        inst_ptr->setStatus(ExampleInst::Status::SCHEDULED);

        // NOTE:
        // It is a bug if instruction status is updated as SCHEDULED in the issueInst_()
        // The reason is: when issueInst_() is called, it could be scheduled for
        // either a new issue event, or a re-issue event
        // however, we can ONLY update instruction status as SCHEDULED for a new issue event


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Another issue event scheduled";
        }
    }

    // Receive MSS access acknowledge from Bus Interface Unit
    void LSU::getAckFromBIU_(const ExampleInstPtr & inst_ptr)
    {
        if (inst_ptr == mmu_pending_inst_ptr_) {
            rehandleMMULookupReq_(inst_ptr);
        }
        else if (inst_ptr == cache_pending_inst_ptr_) {
            rehandleCacheLookupReq_(inst_ptr);
        }
        else {
            sparta_assert(false, "Unexpected BIU Ack event occurs!");
        }
    }

    // Receive update from ROB whenever store instructions retire
    void LSU::getAckFromROB_(const ExampleInstPtr & inst_ptr)
    {
        sparta_assert(inst_ptr->getStatus() == ExampleInst::Status::RETIRED,
                        "Get ROB Ack, but the store inst hasn't retired yet!");

        updateIssuePriorityAfterStoreInstRetire_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Get Ack from ROB! Retired store instruction: " << inst_ptr;
        }
    }

    // Issue/Re-issue ready instructions in the issue queue
    void LSU::issueInst_()
    {
        // Instruction issue arbitration
        const LoadStoreInstInfoPtr & win_ptr = arbitrateInstIssue_();
        // NOTE:
        // win_ptr should always point to an instruction ready to be issued
        // Otherwise assertion error should already be fired in arbitrateInstIssue_()

        // Append load/store pipe
        ldst_pipeline_.append(win_ptr->getMemoryAccessInfoPtr());

        // Update instruction issue info
        win_ptr->setState(LoadStoreInstInfo::IssueState::ISSUED);
        win_ptr->setPriority(LoadStoreInstInfo::IssuePriority::LOWEST);

        // Schedule another instruction issue event if possible
        if (isReadyToIssueInsts_()) {
            uev_issue_inst_.schedule(sparta::Clock::Cycle(1));
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Issue/Re-issue Instruction: " << win_ptr->getInstPtr();
        }
    }

    // Handle MMU access request
    void LSU::handleMMULookupReq_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::MMU_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        bool isAlreadyHIT = (mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT);
        bool MMUBypass = isAlreadyHIT;

        if (MMUBypass) {

            if (SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "MMU Lookup is skipped (TLB is already hit)!";
            }

            return;
        }

        // Access TLB, and check TLB hit or miss
        bool TLB_HIT = MMULookup_(mem_access_info_ptr);

        if (TLB_HIT) {
            // Update memory access info
            mem_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::HIT);
            // Update physical address status
            mem_access_info_ptr->setPhyAddrStatus(true);
        }
        else {
            // Update memory access info
            mem_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::MISS);

            if (mmu_busy_ == false) {
                // MMU is busy, no more TLB MISS can be handled, RESET is required on finish
                mmu_busy_ = true;
                // Keep record of the current TLB MISS instruction
                mmu_pending_inst_ptr_ = mem_access_info_ptr->getInstPtr();

                // NOTE:
                // mmu_busy_ RESET could be done:
                // as early as port-driven event for this miss finish, and
                // as late as TLB reload event for this miss finish.

                // Schedule port-driven event in BIU
                uev_mmu_drive_biu_port_.schedule(sparta::Clock::Cycle(0));

                // NOTE:
                // The race between simultaneous MMU and cache requests is resolved by
                // specifying precedence between these two competing events


                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_ << "MMU is trying to drive BIU request port!";
                }
            }
            else {

                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_
                        << "MMU miss cannot be served right now due to another outstanding one!";
                }
            }

            // NEW: Invalidate pipeline stage
            ldst_pipeline_.invalidateStage(static_cast<uint32_t>(PipelineStage::MMU_LOOKUP));
        }
    }

    // Drive BIU request port from MMU
    void LSU::driveBIUPortFromMMU_()
    {
        bool succeed = false;

        // Check if DataOutPort is available
        if (!out_biu_req_.isDriven()) {
            sparta_assert(mmu_pending_inst_ptr_ != nullptr,
                "Attempt to drive BIU port when no outstanding TLB miss exists!");

            // Port is available, drive port immediately, and send request out
            out_biu_req_.send(mmu_pending_inst_ptr_);

            succeed = true;
        }
        else {
            // Port is being driven by another source, wait for one cycle and check again
            uev_mmu_drive_biu_port_.schedule(sparta::Clock::Cycle(1));
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            if (succeed) {
                info_logger_ << "MMU is driving the BIU request port!";
            }
            else {
                info_logger_ << "MMU is waiting to drive the BIU request port!";
            }
        }
    }

    // Handle cache access request
    void LSU::handleCacheLookupReq_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        const ExampleInstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();

        const bool phyAddrIsReady =
            mem_access_info_ptr->getPhyAddrStatus();
        const bool isAlreadyHIT =
            (mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT);
        const bool isUnretiredStore =
            inst_ptr->isStoreInst() && (inst_ptr->getStatus() != ExampleInst::Status::RETIRED);
        const bool cacheBypass = isAlreadyHIT || !phyAddrIsReady || isUnretiredStore;

        if (cacheBypass) {

            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                if (isAlreadyHIT) {
                    info_logger_ << "Cache Lookup is skipped (Cache already hit)!";
                }
                else if (!phyAddrIsReady) {
                    info_logger_ << "Cache Lookup is skipped (Physical address not ready)!";
                }
                else if (isUnretiredStore) {
                    info_logger_ << "Cache Lookup is skipped (Un-retired store instruction)!";
                }
                else {
                    sparta_assert(false, "Cache access is bypassed without a valid reason!");
                }
            }

            return;
        }

        // Access cache, and check cache hit or miss
        const bool CACHE_HIT = cacheLookup_(mem_access_info_ptr);

        if (CACHE_HIT) {
            // Update memory access info
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::HIT);
        }
        else {
            // Update memory access info
            mem_access_info_ptr->setCacheState(MemoryAccessInfo::CacheState::MISS);

            if (cache_busy_ == false) {
                // Cache is now busy, no more CACHE MISS can be handled, RESET required on finish
                cache_busy_ = true;
                // Keep record of the current CACHE MISS instruction
                cache_pending_inst_ptr_ = mem_access_info_ptr->getInstPtr();

                // NOTE:
                // cache_busy_ RESET could be done:
                // as early as port-driven event for this miss finish, and
                // as late as cache reload event for this miss finish.

                // Schedule port-driven event in BIU
                uev_cache_drive_biu_port_.schedule(sparta::Clock::Cycle(0));

                // NOTE:
                // The race between simultaneous MMU and cache requests is resolved by
                // specifying precedence between these two competing events


                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_ << "Cache is trying to drive BIU request port!";
                }
            }
            else {

                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_ << "Cache miss cannot be served right now due to another outstanding one!";
                }
            }

            // NEW: Invalidate pipeline stage
            ldst_pipeline_.invalidateStage(static_cast<uint32_t>(PipelineStage::CACHE_LOOKUP));
        }
    }

    // Drive BIU request port from cache
    void LSU::driveBIUPortFromCache_()
    {
        bool succeed = false;

        // Check if DataOutPort is available
        if (!out_biu_req_.isDriven()) {
            sparta_assert(cache_pending_inst_ptr_ != nullptr,
                "Attempt to drive BIU port when no outstanding cache miss exists!");

            // Port is available, drive the port immediately, and send request out
            out_biu_req_.send(cache_pending_inst_ptr_);

            succeed = true;
        }
        else {
            // Port is being driven by another source, wait for one cycle and check again
            uev_cache_drive_biu_port_.schedule(sparta::Clock::Cycle(1));
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            if (succeed) {
                info_logger_ << "Cache is driving the BIU request port!";
            }
            else {
                info_logger_ << "Cache is waiting to drive the BIU request port!";
            }
        }
    }

    // Retire load/store instruction
    void LSU::completeInst_()
    {
        const auto stage_id = static_cast<uint32_t>(PipelineStage::COMPLETE);

        // Check if flushing event occurred just now
        if (!ldst_pipeline_.isValid(stage_id)) {
            return;
        }


        const MemoryAccessInfoPtr & mem_access_info_ptr = ldst_pipeline_[stage_id];
        const ExampleInstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        bool isStoreInst = inst_ptr->isStoreInst();

        // Complete load instruction
        if (!isStoreInst) {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Load instruction cannot complete when cache is still a miss!");

            // Update instruction status
            inst_ptr->setStatus(ExampleInst::Status::COMPLETED);

            // Remove completed instruction from issue queue
            popIssueQueue_(inst_ptr);

            // Update instruction issue queue credits to Dispatch Unit
            out_lsu_credits_.send(1, 0);


            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "Complete Load Instruction: "
                             << inst_ptr->getMnemonic()
                             << " uid(" << inst_ptr->getUniqueID() << ")";
            }

            return;
        }


        // Complete store instruction
        if (inst_ptr->getStatus() != ExampleInst::Status::RETIRED) {

            sparta_assert(mem_access_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::HIT,
                        "Store instruction cannot complete when TLB is still a miss!");

            // Update instruction status
            inst_ptr->setStatus(ExampleInst::Status::COMPLETED);


            if (SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "Complete Store Instruction: "
                             << inst_ptr->getMnemonic()
                             << " uid(" << inst_ptr->getUniqueID() << ")";
            }
        }
        // Finish store operation
        else {
            sparta_assert(mem_access_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::HIT,
                        "Store inst cannot finish when cache is still a miss!");

            // Remove store instruction from issue queue
            popIssueQueue_(inst_ptr);

            // Update instruction issue queue credits to Dispatch Unit
            out_lsu_credits_.send(1, 0);


            if (SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "Store operation is done!";
            }
        }


        // NOTE:
        // Checking whether an instruction is ready to complete could be non-trivial
        // Right now we simply assume:
        // (1)Load inst is ready to complete as long as both MMU and cache access finish
        // (2)Store inst is ready to complete as long as MMU (address translation) is done
    }

    // Handle instruction flush in LSU
    void LSU::handleFlush_(const FlushCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Start Flushing!";
        }

        // Flush criteria setup
        auto flush = [criteria] (const uint64_t & id) -> bool {
            return id >= static_cast<uint64_t>(criteria);
        };

        // Flush load/store pipeline entry
        flushLSPipeline_(flush);

        // Mark flushed flag for unfinished speculative MMU access
        if (mmu_busy_ && flush(mmu_pending_inst_ptr_->getUniqueID())) {
            mmu_pending_inst_flushed = true;
        }

        // Mark flushed flag for unfinished speculative cache access
        if (cache_busy_ && flush(cache_pending_inst_ptr_->getUniqueID())) {
            cache_pending_inst_flushed_ = true;
        }

        // Flush instruction issue queue
        flushIssueQueue_(flush);

        // Cancel issue event already scheduled if no ready-to-issue inst left after flush
        if (!isReadyToIssueInsts_()) {
            uev_issue_inst_.cancel();
        }

        // NOTE:
        // Flush is handled at Flush phase (inbetween PortUpdate phase and Tick phase).
        // This also guarantees that whenever an instruction issue event happens,
        // instruction issue arbitration should always succeed, even when flush happens.
        // Otherwise, assertion error is fired inside arbitrateInstIssue_()
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append new load/store instruction into issue queue
    void LSU::appendIssueQueue_(const LoadStoreInstInfoPtr & inst_info_ptr)
    {
        sparta_assert(ldst_inst_queue_.size() <= ldst_inst_queue_size_,
                        "Appending issue queue causes overflows!");

        // Always append newly dispatched instructions to the back of issue queue
        ldst_inst_queue_.push_back(inst_info_ptr);


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Append new load/store instruction to issue queue!";
        }
    }

    // Pop completed load/store instruction out of issue queue
    void LSU::popIssueQueue_(const ExampleInstPtr & inst_ptr)
    {
        // Look for the instruction to be completed, and remove it from issue queue
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            if ((*iter)->getInstPtr() == inst_ptr) {
                ldst_inst_queue_.erase(iter);

                return;
            }
        }

        sparta_assert(false, "Attempt to complete instruction no longer exiting in issue queue!");
    }

    // Arbitrate instruction issue from ldst_inst_queue
    const LSU::LoadStoreInstInfoPtr & LSU::arbitrateInstIssue_()
    {
        sparta_assert(ldst_inst_queue_.size() > 0, "Arbitration fails: issue is empty!");

        // Initialization of winner
        auto win_ptr_iter = ldst_inst_queue_.begin();

        // Select the ready instruction with highest issue priority
        for (auto iter = ldst_inst_queue_.begin(); iter != ldst_inst_queue_.end(); iter++) {
            // Skip not ready-to-issue instruction
            if (!(*iter)->isReady()) {
                continue;
            }

            // Pick winner
            if (!(*win_ptr_iter)->isReady() || (*iter)->winArb(*win_ptr_iter)) {
                win_ptr_iter = iter;
            }
            // NOTE:
            // If the inst pointed to by (*win_ptr_iter) is not ready (possible @initialization),
            // Re-assign it pointing to the ready-to-issue instruction pointed by (*iter).
            // Otherwise, both (*win_ptr_iter) and (*iter) point to ready-to-issue instructions,
            // Pick the one with higher issue priority.
        }

        sparta_assert((*win_ptr_iter)->isReady(), "Arbitration fails: no instruction is ready!");

        return *win_ptr_iter;
    }

    // Check for ready to issue instructions
    bool LSU::isReadyToIssueInsts_() const
    {
        bool isReady = false;

        // Check if there is at least one ready-to-issue instruction in issue queue
        for (auto const &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->isReady()) {
                isReady = true;

                break;
            }
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            if (isReady) {
                info_logger_ << "At least one more instruction is ready to be issued!";
            }
            else {
                info_logger_ << "No more instruction is ready to be issued!";
            }
        }

        return isReady;
    }


    // Access MMU/TLB
    bool LSU::MMULookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const ExampleInstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t vaddr = inst_ptr->getVAdr();

        bool tlb_hit = false;

        // C++ comma operator: assign tlb_hit first, then evaluate it. Just For Fun
        if (tlb_hit = tlb_always_hit_, tlb_hit) {
        }
        else {
            auto tlb_entry = tlb_cache_->peekLine(vaddr);
            tlb_hit = (tlb_entry != nullptr) && tlb_entry->isValid();

            // Update MRU replacement state if TLB HIT
            if (tlb_hit) {
                tlb_cache_->touch(*tlb_entry);
            }
        }


        if (SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            if (tlb_always_hit_) {
                info_logger_ << "TLB HIT all the time: vaddr=0x" << std::hex << vaddr;
            }
            else if (tlb_hit) {
                info_logger_ << "TLB HIT: vaddr=0x" << std::hex << vaddr;
            }
            else {
                info_logger_ << "TLB MISS: vaddr=0x" << std::hex << vaddr;
            }
        }

        return tlb_hit;
    }

    // Re-handle outstanding MMU access request
    void LSU::rehandleMMULookupReq_(const ExampleInstPtr & inst_ptr)
    {
        // MMU is no longer busy any more
        mmu_busy_ = false;
        mmu_pending_inst_ptr_.reset();

        // NOTE:
        // MMU may not have to wait until MSS Ack comes back
        // MMU could be ready to service new TLB MISS once previous request has been sent
        // However, that means MMU has to keep record of a list of pending instructions

        // Check if this MMU miss Ack is for an already flushed instruction
        if (mmu_pending_inst_flushed) {
            mmu_pending_inst_flushed = false;


            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "BIU Ack for a flushed MMU miss is received!";
            }

            // Schedule an instruction (re-)issue event
            // Note: some younger load/store instruction(s) might have been blocked by
            // this outstanding miss
            updateIssuePriorityAfterTLBReload_(inst_ptr, true);
            if (isReadyToIssueInsts_()) {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }
            return;
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "BIU Ack for an outstanding MMU miss is received!";
        }

        // Reload TLB entry
        reloadTLB_(inst_ptr->getVAdr());

        // Update issue priority & Schedule an instruction (re-)issue event
        updateIssuePriorityAfterTLBReload_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "MMU rehandling event is scheduled!";
        }
    }

    // Reload TLB entry
    void LSU::reloadTLB_(uint64_t vaddr)
    {
        auto tlb_entry = &tlb_cache_->getLineForReplacementWithInvalidCheck(vaddr);
        tlb_cache_->allocateWithMRUUpdate(*tlb_entry, vaddr);


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "TLB reload complete!";
        }
    }

    // Access Cache
    bool LSU::cacheLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const ExampleInstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t phyAddr = inst_ptr->getRAdr();

        bool cache_hit = false;

        if (dl1_always_hit_) {
            cache_hit = true;
        }
        else {
            auto cache_line = dl1_cache_->peekLine(phyAddr);
            cache_hit = (cache_line != nullptr) && cache_line->isValid();

            // Update MRU replacement state if Cache HIT
            if (cache_hit) {
                dl1_cache_->touchMRU(*cache_line);
            }
        }


        if (SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            if (dl1_always_hit_) {
                info_logger_ << "DL1 Cache HIT all the time: phyAddr=0x" << std::hex << phyAddr;
            }
            else if (cache_hit) {
                info_logger_ << "DL1 Cache HIT: phyAddr=0x" << std::hex << phyAddr;
            }
            else {
                info_logger_ << "DL1 Cache MISS: phyAddr=0x" << std::hex << phyAddr;
            }
        }

        return cache_hit;
    }

    // Re-handle outstanding cache access request
    void LSU::rehandleCacheLookupReq_(const ExampleInstPtr & inst_ptr)
    {
        // Cache is no longer busy any more
        cache_busy_ = false;
        cache_pending_inst_ptr_.reset();

        // NOTE:
        // Depending on cache is blocking or not,
        // It may not have to wait until MMS Ack returns.
        // However, that means cache has to keep record of a list of pending instructions

        // Check if this cache miss Ack is for an already flushed instruction
        if (cache_pending_inst_flushed_) {
            cache_pending_inst_flushed_ = false;

            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "BIU Ack for a flushed cache miss is received!";
            }

            // Schedule an instruction (re-)issue event
            // Note: some younger load/store instruction(s) might have been blocked by
            // this outstanding miss
            updateIssuePriorityAfterCacheReload_(inst_ptr, true);
            if (isReadyToIssueInsts_()) {
                uev_issue_inst_.schedule(sparta::Clock::Cycle(0));
            }

            return;
        }


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "BIU Ack for an outstanding cache miss is received!";
        }

        // Reload cache line
        reloadCache_(inst_ptr->getRAdr());

        // Update issue priority & Schedule an instruction (re-)issue event
        updateIssuePriorityAfterCacheReload_(inst_ptr);
        uev_issue_inst_.schedule(sparta::Clock::Cycle(0));


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Cache rehandling event is scheduled!";
        }
    }

    // Reload cache line
    void LSU::reloadCache_(uint64_t phyAddr)
    {
        auto dl1_cache_line = &dl1_cache_->getLineForReplacementWithInvalidCheck(phyAddr);
        dl1_cache_->allocateWithMRUUpdate(*dl1_cache_line, phyAddr);


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Cache reload complete!";
        }
    }

    // Update issue priority when newly dispatched instruction comes in
    void LSU::updateIssuePriorityAfterNewDispatch_(const ExampleInstPtr & inst_ptr)
    {
        for (auto &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->getInstPtr() == inst_ptr) {

                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::NEW_DISP);

                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");
    }

    // Update issue priority after cache reload
    void LSU::updateIssuePriorityAfterTLBReload_(const ExampleInstPtr & inst_ptr,
                                                 const bool is_flushed_inst)
    {
        bool is_found = false;

        for (auto &inst_info_ptr : ldst_inst_queue_) {
            const MemoryAccessInfoPtr & mem_info_ptr = inst_info_ptr->getMemoryAccessInfoPtr();

            if (mem_info_ptr->getMMUState() == MemoryAccessInfo::MMUState::MISS) {
                // Re-activate all TLB-miss-pending instructions in the issue queue
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending MMU miss instruction here
                // However, re-activation must be scheduled somewhere else

                if (inst_info_ptr->getInstPtr() == inst_ptr) {
                    // Update issue priority for this outstanding TLB miss
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::MMU_RELOAD);

                    // NOTE:
                    // The priority should be set in such a way that
                    // the outstanding miss is always re-issued earlier than other pending miss
                    // Here we have MMU_RELOAD > MMU_PENDING

                    is_found = true;
                }
            }
        }

        sparta_assert(is_flushed_inst || is_found,
            "Attempt to rehandle TLB lookup for instruction not yet in the issue queue!");
    }

    // Update issue priority after cache reload
    void LSU::updateIssuePriorityAfterCacheReload_(const ExampleInstPtr & inst_ptr,
                                                   const bool is_flushed_inst)
    {
        bool is_found = false;

        for (auto &inst_info_ptr : ldst_inst_queue_) {
            const MemoryAccessInfoPtr & mem_info_ptr = inst_info_ptr->getMemoryAccessInfoPtr();

            if (mem_info_ptr->getCacheState() == MemoryAccessInfo::CacheState::MISS) {
                // Re-activate all cache-miss-pending instructions in the issue queue
                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                // NOTE:
                // We may not have to re-activate all of the pending cache miss instruction here
                // However, re-activation must be scheduled somewhere else

                if (inst_info_ptr->getInstPtr() == inst_ptr) {
                    // Update issue priority for this outstanding cache miss
                    inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                    inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_RELOAD);

                    // NOTE:
                    // The priority should be set in such a way that
                    // the outstanding miss is always re-issued earlier than other pending miss
                    // Here we have CACHE_RELOAD > CACHE_PENDING > MMU_RELOAD

                    is_found = true;
                }
            }
        }

        sparta_assert(is_flushed_inst || is_found,
                    "Attempt to rehandle cache lookup for instruction not yet in the issue queue!");
    }

    // Update issue priority after store instruction retires
    void LSU::updateIssuePriorityAfterStoreInstRetire_(const ExampleInstPtr & inst_ptr)
    {
        for (auto &inst_info_ptr : ldst_inst_queue_) {
            if (inst_info_ptr->getInstPtr() == inst_ptr) {

                inst_info_ptr->setState(LoadStoreInstInfo::IssueState::READY);
                inst_info_ptr->setPriority(LoadStoreInstInfo::IssuePriority::CACHE_PENDING);

                return;
            }
        }

        sparta_assert(false,
            "Attempt to update issue priority for instruction not yet in the issue queue!");

    }

    // Flush instruction issue queue
    template<typename Comp>
    void LSU::flushIssueQueue_(const Comp & flush)
    {
        uint32_t credits_to_send = 0;

        auto iter = ldst_inst_queue_.begin();
        while (iter != ldst_inst_queue_.end()) {
            auto inst_id = (*iter)->getInstPtr()->getUniqueID();

            auto delete_iter = iter++;

            if (flush(inst_id)) {
                ldst_inst_queue_.erase(delete_iter);

                // NOTE:
                // We cannot increment iter after erase because it's already invalidated by then

                ++credits_to_send;


                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_ << "Flush Instruction ID: " << inst_id;
                }
            }
        }

        if (credits_to_send > 0) {
            out_lsu_credits_.send(credits_to_send);


            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "Flush " << credits_to_send << " instructions in issue queue!";
            }
        }
    }

    // Flush load/store pipe
    template<typename Comp>
    void LSU::flushLSPipeline_(const Comp & flush)
    {
        uint32_t stage_id = 0;
        for (auto iter = ldst_pipeline_.begin(); iter != ldst_pipeline_.end(); iter++, stage_id++) {
            // If the pipe stage is already invalid, no need to flush
            if (!iter.isValid()) {
                continue;
            }

            auto inst_id = (*iter)->getInstPtr()->getUniqueID();
            if (flush(inst_id)) {
                ldst_pipeline_.flushStage(iter);


                if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                    info_logger_ << "Flush Pipeline Stage[" << stage_id
                                 << "], Instruction ID: " << inst_id;
                }
            }
        }
    }

} // namespace core_example
