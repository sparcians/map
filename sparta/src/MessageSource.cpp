// <MessageSource> -*- C++ -*-


/*!
 * \file MessageSource.cpp
 * \brief Sparta MessageSource implementation.
 */

#include "sparta/log/MessageSource.hpp"

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/TimeManager.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/log/categories/CategoryManager.hpp"

namespace sparta {
    namespace log {

seq_num_type MessageSource::seq_num_ = 0;

// Implemented here to prevent circular dependency on scheduler
void MessageSource::emit_(const std::string& content) const {
    sparta_assert(getParent() != nullptr);
    const Clock* clk = getParent()->getClock();
    Scheduler::Tick ticks = 0;
    if(clk){
        ticks = clk->getScheduler()->getCurrentTick();
    }
    sparta::log::Message msg =
        {
            {
                *getParent(),                                      // Origin Node
                TimeManager::getTimeManager().getSecondsElapsed(), // Wall clock time
                ticks,                                             // Simulator ticks
                getCategoryID(),                                   // Category
                0,                                                 // Thread ID - simulator-defined (TBD)
                seq_num_                                           // Thread-local member
            },
            content
        };
    ++seq_num_;
    postNotification(msg);
}

MessageSource& MessageSource::getGlobalWarn()
{
    // Construct logger here because adding as a child of virtual global node
    // within the global virtual node confuses the observation logic
    static MessageSource warn(TreeNode::getVirtualGlobalNode(),
                              categories::WARN,
                              "Global warning messages");
    return warn;
}

MessageSource& MessageSource::getGlobalDebug()
{
    // Construct logger here because adding as a child of virtual global node
    // within the global virtual node confuses the observation logic
    static MessageSource debug(TreeNode::getVirtualGlobalNode(),
                               categories::DEBUG,
                               "Global debug messages");
    return debug;
}

MessageSource& MessageSource::getGlobalParameterTraceSource()
{
    // Construct logger here because adding as a child of virtual global node
    // within the global virtual node confuses the observation logic
    static MessageSource debug(TreeNode::getVirtualGlobalNode(),
                               categories::PARAMETERS_STR,
                               "Global parameter/configuration messages");
    return debug;
}
    } // namespace log
} // namespace sparta
