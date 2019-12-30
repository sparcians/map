// <Simulation.h> -*- C++ -*-


#ifndef __EXAMPLE_SIMULATOR_H__
#define __EXAMPLE_SIMULATOR_H__

#include "sparta/app/Simulation.hpp"

/*!
 * \brief ExampleSimulator which builds the model and configures it
 */
class ExampleSimulator : public sparta::app::Simulation
{
public:

    /*!
     * \brief Construct ExampleSimulator
     * \param num_producers The number of producers to make
     * \param be_noisy Be verbose -- not necessary, just an example
     */
    ExampleSimulator(sparta::Scheduler & scheduler, uint32_t num_producers, bool be_noisy);

    // Tear it down
    virtual ~ExampleSimulator();

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

    const uint32_t num_producers_ = 1;

    //! Verbosity
    const bool be_noisy_ = false;
};

// __EXAMPLE_SIMULATOR_H__
#endif
