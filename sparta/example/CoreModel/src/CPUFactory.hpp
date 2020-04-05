// <CPUFactory.h> -*- C++ -*-


#pragma once

#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "CPU.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "CPUTopology.hpp"

namespace core_example{

/**
 * @file  CPUFactory.h
 * @brief CPUFactory will act as the place where a user-defined topology
 *        will be translated into actual Resource Treenodes. It is responsible
 *        for binding ports between units.
 *
 * CPUFactory unit will
 * 1. Set recipe for topology and number of cores in processor
 * 2. Build the actual tree nodes
 * 3. Bind the ports of different logical units together
 */
class CPUFactory : public sparta::ResourceFactory<CPU, CPU::CPUParameterSet>{
public:

    /**
     * @brief Constructor for CPUFactory
     */
    CPUFactory();

    /**
     * @brief Destructor for CPUFactory
     */
    ~CPUFactory();

    /**
     * @brief Set the user-defined topology for this microarchitecture
     */
    auto setTopology(const std::string&, const uint32_t) -> void;

    /**
     * @brief Build the device tree by instantiating resource nodes
     */
    auto buildTree(sparta::RootTreeNode*) -> void;

    /**
     * @brief Bind all the ports between different units and set TLBs and preload
     */
    auto bindTree(sparta::RootTreeNode*) -> void;

    /**
     * @brief Get the list of resources instantiated in this topology
     */
    auto getResourceNames() const -> const std::vector<std::string>&;
private:

    /**
     * @brief Implementation : Build the device tree by instantiating resource nodes
     */
    auto buildTree_(sparta::RootTreeNode*,
                    const std::vector<CPUTopology::UnitInfo>&) -> void;
    /**
     * @brief Implementation : Bind all the ports between different units and set TLBs and preload
     */
    auto bindTree_(sparta::RootTreeNode*,
                   const std::vector<CPUTopology::PortConnectionInfo>&) -> void;

    /**
     * @brief Wildcard to be replaced by the multicore idx
     */
    const char to_replace_ {'*'};

    /**
     * @brief The user-defined topology unit
     */
    std::unique_ptr<CPUTopology> topology_;

    /**
     * @brief Vector of instantiated resource names
     */
    std::vector<std::string> resource_names_;

    /**
     * @brief Vector of private tree nodes
     */
    std::vector<sparta::TreeNode*> private_nodes_;

    /**
     * @brief RAII way of deleting nodes that this class created
     */
    std::vector<std::unique_ptr<sparta::ResourceTreeNode>> to_delete_;
}; // class CPUFactory
}  // namespace core_example
