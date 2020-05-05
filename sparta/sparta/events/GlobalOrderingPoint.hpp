// <GlobalOrderingPoint.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/Utils.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include <string>

namespace sparta
{
    /*!
     * \class GlobalOrderingPoint
     * \brief Used to set precedence between Scheduleable types across simulation
     *
     * In cases where two events in different blocks need
     * synchronization, this class can allow a modeler to set a
     * precedence between those entities, even across blocks that do
     * not know about each other.
     *
     * Example: a core's load/store unit needs to send "ready" to the
     * middle machine for operand readiness.  The middle machine will
     * pick the instruction that corresponds to that ready signal _on
     * the same cycle_ the load/store sends ready.  The modeler
     * requires the load/store unit to be scheduled first before the
     * middle machine's picker:
     *
     * \code
     * class LSU {
     *    EventT ev_send_ready_;
     * };
     *
     * class MidMachine {
     *    EventT pick_instruction_;
     * };
     * \endcode
     *
     * In the above case, the event `ev_send_ready_` _must be scheduled before_ `pick_instruction_`.
     *
     * To do this, set up a sparta::GlobalOrderingPoint in both unit's constructors:
     *
     * \code
     * LSU::LSU(sparta::TreeNode * container, const LSUParameters*)
     * {
     *     // Sent ready signal MUST COME BEFORE the GlobalOrderingPoint
     *     ev_send_ready_ >> sparta::GlobalOrderingPoint(container, "lsu_midmachine_order");
     * }
     *
     * MidMachine::MidMachine(sparta::TreeNode * container, const MidMachineParameters*)
     * {
     *     // The GlobalOrderingPoint must come before picking
     *     sparta::GlobalOrderingPoint(container, "lsu_midmachine_order") >> pick_instruction_;
     * }
     * \endcode
     *
     * The modeler must ensure the name of the
     * sparta::GlobalOrderingPoint is the *same* on both calls.
     */
    class GlobalOrderingPoint
    {
    public:
        /**
         * \brief Construction a GlobalOrderingPoint
         *
         * \param node The node this GOP is associated with
         * \param name The name that's used to register with the internal DAG
         *
         * See descition for use
         */
        GlobalOrderingPoint(sparta::TreeNode * node,
                            const std::string & name) :
            name_(name)
        {
            auto scheduler   = notNull(node)->getScheduler();
            dag_             = notNull(scheduler)->getDAG();
            gordering_point_ = notNull(dag_)->getGOPoint(name);
            sparta_assert(nullptr != gordering_point_, "Issues trying to get GOPoint " << name);
        }

        /**
         * \brief Handy method for debug
         * \return The name of this GOP
         */
        const std::string & getName() const {
            return name_;
        }

        /**
         * \brief Used by the precedence rules
         * \return The internal GOP from the internal DAG
         */
        DAG::GOPoint * getGOPoint() const {
            return gordering_point_;
        }

    private:
        DAG          * dag_ = nullptr;
        DAG::GOPoint * gordering_point_ = nullptr;
        const std::string name_;
    };
}
