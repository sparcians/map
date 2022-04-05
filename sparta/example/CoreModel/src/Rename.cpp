/**
 * @file   Rename.cpp
 * @brief
 *
 *
 */

#include <algorithm>
#include "Rename.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/report/DatabaseInterface.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"

namespace core_example
{
    const char Rename::name[] = "rename";

    Rename::Rename(sparta::TreeNode * node,
                   const RenameParameterSet * p) :
        sparta::Unit(node),
        uop_queue_("rename_uop_queue", p->rename_queue_depth,
                   node->getClock(), getStatisticSet()),
        num_to_rename_per_cycle_(p->num_to_rename)
    {
        uop_queue_.enableCollection(node);

        // The path into the Rename block
        // - Instructions are received on the Uop Queue Append port
        // - Credits arrive on the dispatch queue credits port
        in_uop_queue_append_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, decodedInstructions_, InstGroup));
        in_dispatch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, creditsDispatchQueue_, uint32_t));
        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, handleFlush_, FlushManager::FlushingCriteria));
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Rename, sendInitialCredits_));
    }

    // Send the initial credit count
    void Rename::sendInitialCredits_()
    {
        out_uop_queue_credits_.send(uop_queue_.capacity());
    }

    void Rename::creditsDispatchQueue_(const uint32_t & credits)
    {
        sparta_assert(in_dispatch_queue_credits_.dataReceived());

        credits_dispatch_ += credits;
        if (uop_queue_.size() > 0) {
            ev_rename_insts_.schedule();
        }
    }

    // Handle incoming flush
    void Rename::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got a flush call for " << criteria;
        }
        out_uop_queue_credits_.send(uop_queue_.size());
        uop_queue_.clear();

        if (!stop_checking_db_access_) {
            if (auto container = getContainer()) {
                if (auto sim = container->getSimulation()) {
                    if (sparta::IsFeatureValueEnabled(sim->getFeatureConfiguration(),
                                                    "wildcard-components"))
                    {
                        if (auto dbconn = GET_DB_FOR_COMPONENT(Stats, container)) {
                            //Run a simple query against the database just to verify
                            //the connection is open and accepting requests
                            (void) dbconn->findObject("ObjectManagersInDatabase", 1);
                        }
                    }
                }
            }
            stop_checking_db_access_ = true;
        }
    }

    void Rename::decodedInstructions_(const InstGroup & insts)
    {
        sparta_assert(in_uop_queue_append_.dataReceived());

        for(auto & i : insts) {
            uop_queue_.push(i);
        }

        // If we have credits from dispatch, schedule a rename session
        // this cycle
        if (credits_dispatch_ > 0) {
            ev_rename_insts_.schedule();
        }
    }

    void Rename::renameInstructions_()
    {

        uint32_t num_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_rename = std::min(credits_dispatch_, num_rename);

        if(num_rename > 0)
        {

            InstGroup insts;
            for(uint32_t i = 0; i < num_rename; ++i) {
                insts.emplace_back(uop_queue_.read(0));
                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << ": sending inst to dispatch: " << uop_queue_.read(0);
                }
                uop_queue_.pop();
            }
            out_dispatch_queue_write_.send(insts);
            credits_dispatch_ -= num_rename;

            // Replenish credits in the Decode unit
            out_uop_queue_credits_.send(num_rename);
        }

        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0)) {
            ev_rename_insts_.schedule(1);
        }
    }

}
