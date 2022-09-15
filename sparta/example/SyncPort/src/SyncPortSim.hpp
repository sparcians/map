// <SyncPortSimulation.hpp> -*- C++ -*-

#pragma once

#include <cinttypes>

#include "sparta/app/Simulation.hpp"

/*!
 * \brief SyncPortSimulator which builds the model and configures it
 */
class SyncPortSimulator : public sparta::app::Simulation
{
public:

    /*!
     * \brief Construct SyncPortSimulator
     * \param be_noisy Be verbose -- not necessary, just an skeleton
     */
    SyncPortSimulator(sparta::Scheduler & scheduler, bool be_noisy);

    // Tear it down
    virtual ~SyncPortSimulator();

private:

    //////////////////////////////////////////////////////////////////////
    // Setup

    //! Build the tree with tree nodes, but does not instantiate the
    //! unit yet
    void buildTree_() override;

    //! Configure the tree and apply any last minute parameter changes
    void configureTree_() override;

    //! The tree is now configured, built, and instantiated.  We need
    //! to bind things together.
    void bindTree_() override;

    //! Verbosity
    const bool be_noisy_ = false;

    //////////////////////////////////////////////////////////////////////
    sparta::ClockManager  clock_manager_;
    sparta::Clock::Handle root_clk_;
    sparta::Clock::Handle producer_clk_;
    sparta::Clock::Handle consumer_clk_;
};
