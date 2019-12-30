// <Flush.h> -*- C++ -*-


/**
 * \file   FlushManager.h
 * \brief  File that defines support for event flushing in SPARTA
 */

#ifndef __FLUSH_MANAGER_H__
#define __FLUSH_MANAGER_H__

#include <cinttypes>
#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"

namespace core_example
{
    /**
     * \class FlushManager
     *
     * \brief Class used by performance models for signaling a
     *        flushing event across blocks.
     *
     * The usage is pretty simple.  Create a FlushManager within
     * the topology and have individual units bind their
     * DataInPorts to the appropriate flush ports (based on type
     * [reflected in the name]).
     *
     * When a Flush is instigated on the Tick phase, on the phase
     * sparta::SchedulingPhase::Flush the signal will be delivered
     * to the unit (+1 cycle or more later).  The unit will be
     * given a criteria for flushing that it can use to determine
     * what components it needs to remove from its internal data
     * structures.
     */
    class FlushManager : public sparta::Unit
    {
    public:
        typedef uint64_t FlushingCriteria;
        static constexpr char name[] = "flushmanager";

        class FlushManagerParameters : public sparta::ParameterSet
        {
        public:
            FlushManagerParameters(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

        };

        /*!
         * \brief Create a FlushManager in the tree
         * \param rc     The parent resource tree node
         * \param params Pointer to the flush manager parameters
         */
        FlushManager(sparta::TreeNode *rc, const FlushManagerParameters * params) :
            Unit(rc, name),
            out_retire_flush_(getPortSet(), "out_retire_flush", false),
            in_retire_flush_(getPortSet(), "in_retire_flush", 0),
            out_fetch_flush_redirect_(getPortSet(), "out_fetch_flush_redirect", false),
            in_fetch_flush_redirect_(getPortSet(), "in_fetch_flush_redirect", 0)
        {
            (void)params;
            in_retire_flush_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(FlushManager,
                                                                      forwardRetireFlush_,
                                                                      FlushingCriteria));
            in_fetch_flush_redirect_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(FlushManager,
                                                                      forwardFetchRedirectFlush_,
                                                                      uint64_t));
        }

    private:

        // Flushing criteria
        sparta::DataOutPort<FlushingCriteria> out_retire_flush_;
        sparta::DataInPort <FlushingCriteria> in_retire_flush_;

        // Flush redirect for Fetch
        sparta::DataOutPort<uint64_t> out_fetch_flush_redirect_;
        sparta::DataInPort <uint64_t> in_fetch_flush_redirect_;

        // Internal method used to forward the flush to the attached
        // listeners
        void forwardRetireFlush_(const FlushingCriteria & flush_data) {
            out_retire_flush_.send(flush_data);
        }

        // Internal method used to forward the fetch redirect
        void forwardFetchRedirectFlush_(const uint64_t & flush_data) {
            out_fetch_flush_redirect_.send(flush_data);
        }
    };
}

// __FLUSH_MANAGER_H__
#endif
