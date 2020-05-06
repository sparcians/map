// <CPUFactories.h> -*- C++ -*-


#pragma once

#include "sparta/simulation/ResourceFactory.hpp"
#include "Core.hpp"
#include "Fetch.hpp"
#include "Decode.hpp"
#include "Rename.hpp"
#include "Dispatch.hpp"
#include "Execute.hpp"
#include "LSU.hpp"
#include "SimpleTLB.hpp"
#include "BIU.hpp"
#include "MSS.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"
#include "Preloader.hpp"

namespace core_example{

/**
 * @file  CPUFactories.h
 * @brief CPUFactories will act as the place which contains all the
 *        required factories to build sub-units of the CPU.
 *
 * CPUFactories unit will
 * 1. Contain resource factories to build each core of the CPU
 * 2. Contain resource factories to build microarchitectural units in each core
 */
struct CPUFactories{

    //! \brief Resouce Factory to build a Core Unit
    sparta::ResourceFactory<core_example::Core,
                          core_example::Core::CoreParameterSet> core_rf;

    //! \brief Resouce Factory to build a Fetch Unit
    sparta::ResourceFactory<core_example::Fetch,
                          core_example::Fetch::FetchParameterSet> fetch_rf;

    //! \brief Resouce Factory to build a Decode Unit
    sparta::ResourceFactory<core_example::Decode,
                          core_example::Decode::DecodeParameterSet> decode_rf;

    //! \brief Resouce Factory to build a Rename Unit
    sparta::ResourceFactory<core_example::Rename,
                          core_example::Rename::RenameParameterSet> rename_rf;

    //! \brief Resouce Factory to build a Dispatch Unit
    sparta::ResourceFactory<core_example::Dispatch,
                          core_example::Dispatch::DispatchParameterSet> dispatch_rf;

    //! \brief Resouce Factory to build a Execute Unit
    sparta::ResourceFactory<core_example::Execute,
                          core_example::Execute::ExecuteParameterSet> execute_rf;

    //! \brief Resouce Factory to build a LSU Unit
    sparta::ResourceFactory<core_example::LSU,
                          core_example::LSU::LSUParameterSet> lsu_rf;

    //! \brief Resouce Factory to build a TLB Unit
    sparta::ResourceFactory<core_example::SimpleTLB,
                          core_example::SimpleTLB::TLBParameterSet> tlb_rf;

    //! \brief Resouce Factory to build a BIU Unit
    sparta::ResourceFactory<core_example::BIU,
                          core_example::BIU::BIUParameterSet> biu_rf;

    //! \brief Resouce Factory to build a MSS Unit
    sparta::ResourceFactory<core_example::MSS,
                          core_example::MSS::MSSParameterSet> mss_rf;

    //! \brief Resouce Factory to build a ROB Unit
    sparta::ResourceFactory<core_example::ROB,
                          core_example::ROB::ROBParameterSet> rob_rf;

    //! \brief Resouce Factory to build a Flush Unit
    sparta::ResourceFactory<core_example::FlushManager,
                          core_example::FlushManager::FlushManagerParameters> flushmanager_rf;

    //! \brief Resouce Factory to build a Preloader Unit
    sparta::ResourceFactory<core_example::Preloader,
                          core_example::Preloader::PreloaderParameterSet> preloader_rf;
}; // struct CPUFactories
}  // namespace core_example
