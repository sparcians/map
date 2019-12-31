// <State.tpp> -*- C++ -*-

#include "sparta/simulation/State.hpp"
#include "sparta/app/Simulation.hpp"

namespace sparta{

    //! Forward Declaration of classes.
    namespace app{
        class Simulation;
        class SimulationConfiguration;
    }

    //! Constructor of the Specialized sparta::State stores a pointer
    // to the Simulation from which it queries whether State Tracking
    // is enabled or not and the name of the file, if it is.
    template<typename T>
    State<T,
        typename std::enable_if<
            std::is_same<T, sparta::PhasedObject::TreePhase>::value>::type>
    ::State(sparta::app::Simulation * sim) :
        sim_(sim)
    {}

    //! This method does the actual work of querying the Simulation for its
    // Simulation Configuration. It then asks the Configuration whether the
    // user has enabled state tracking or not. If yes, then it turns on
    // State Tracking flag in Pool Manager and also passes the name of the
    // State Tracking file to the Manager.
    template<typename T>
    void State<T,
        typename std::enable_if<
            std::is_same<T, sparta::PhasedObject::TreePhase>::value>::type>
    ::configure() {
        if(!(sim_->getSimulationConfiguration())->getStateTrackingFilename().empty()) {
            tracker::StatePoolManager::getInstance().setTrackingFilename(
                (sim_->getSimulationConfiguration())->getStateTrackingFilename());
        }
        tracker::StatePoolManager::getInstance().setScheduler(sim_->getScheduler());
    }

    //! Destruction of this Specialized sparta::State signals that all the
    // Histogram data should be collected and compiled from individual pools
    // which reside in a map inside the Pool Manager.
    template<typename T>
    State<T,
        typename std::enable_if<
            std::is_same<T, sparta::PhasedObject::TreePhase>::value>::type>
    ::~State(){
        tracker::StatePoolManager::getInstance().flushPool();
    }

}
