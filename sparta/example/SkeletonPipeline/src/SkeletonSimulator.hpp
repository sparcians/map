// <SkeletonSimulation.hpp> -*- C++ -*-

#pragma once

#include <cinttypes>

#include "sparta/app/Simulation.hpp"

/*!
 * \brief SkeletonSimulator which builds the model and configures it
 */
class SkeletonSimulator : public sparta::app::Simulation
{
public:

    /*!
     * \brief Construct SkeletonSimulator
     * \param be_noisy Be verbose -- not necessary, just an skeleton
     */
    SkeletonSimulator(sparta::Scheduler & scheduler, bool be_noisy);

    // Tear it down
    virtual ~SkeletonSimulator();

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
};

