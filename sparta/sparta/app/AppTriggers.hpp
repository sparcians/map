// <AppTriggers.hpp> -*- C++ -*-


/*!
 * \file AppTriggers.hpp
 * \brief Application-infrastructure triggers.
 */

#pragma once


#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/pevents/PeventController.hpp"
#include "sparta/log/Tap.hpp"

namespace sparta {
    namespace app {

/*!
 * \brief Trigger for strating logging given a number of tap descriptors
 * \note Attaches all taps on go, reports warning on stop
 */
class LoggingTrigger : public trigger::Triggerable
{
    Simulation& sim_;
    log::TapDescVec taps_;

public:

    LoggingTrigger(Simulation& sim,
                   const log::TapDescVec& taps) :
        Triggerable(),
        sim_(sim),
        taps_(taps)
    {;}

public:

    virtual void go() override {
        sim_.installTaps(taps_);
    }
    virtual void stop() override {
        std::cerr << "Warning: no support for STOPPING a LoggingTrigger" << std::endl;
    }
};



    } // namespace app
} // namespace sparta
