// <SimulationInfo> -*- C++ -*-


#include "sparta/app/SimulationInfo.hpp"

#ifndef SPARTA_VERSION
#define SPARTA_VERSION "unknown"
#endif

namespace sparta{
    SimulationInfo SimulationInfo::sim_inst_; // Must be constructed after TimeManager
    std::stack<SimulationInfo*> SimulationInfo::sim_inst_stack_;
    const char SimulationInfo::sparta_version[] = SPARTA_VERSION;
}
