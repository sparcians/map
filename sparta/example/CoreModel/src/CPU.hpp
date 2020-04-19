// <CPU.h> -*- C++ -*-


#pragma once

#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace core_example{

/**
 * @file  CPU.h
 * @brief CPU Unit acts as a logical unit containing multiple cores
 *
 * CPU unit will
 * 1. Attach itself to the root Simulation Device node
 * 2. Use its factory to create multiple cores
 * 3. Use sub-factories within its factory to create microarchitecture units
 */
class CPU : public sparta::Unit{
public:

    //! \brief Parameters for CPU model
    class CPUParameterSet : public sparta::ParameterSet{
    public:
        CPUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n){}

        // Dummy configuration parameters and environment variables that affect CPU utilization
        PARAMETER(bool, fastpoll, true, "FASTPOLL configuration enable")
        PARAMETER(bool, out_of_order_execution, false, "Execution Order")
        PARAMETER(uint32_t, superscalar_degree, 4, "Degree of ILP")
        PARAMETER(uint32_t, nettype, 3, "NETTYPE configuration parameter")
        PARAMETER(uint32_t, ds_max_query, 2, "Max number of parallel decision support queries")
        PARAMETER(uint32_t, max_pdq_priority, 4, "Percentage of parallel database query resources")
        PARAMETER(uint32_t, ds_max_scans, 8, "Number of PDQ scan threads running concurrently")
        PARAMETER(double, frequency_ghz, 1.2, "CPU Clock frequency")
        PARAMETER(std::string, vpclass, "4 Virtual processors of AIO VPclass", "Virtual Processor")
    };

    //! \brief Name of this resource. Required by sparta::UnitFactory
    static constexpr char name[] = "cpu";

    /**
     * @brief Constructor for CPU
     *
     * @param node The node that represents (has a pointer to) the CPU
     * @param p The CPU's parameter set
     */
    CPU(sparta::TreeNode* node, const CPUParameterSet* params);

    //! \brief Destructor of the CPU Unit
    ~CPU();
private:

    //! \brief Internal configuration units of this processor
    bool fastpoll_;
    bool out_of_order_execution_;
    uint32_t superscalar_degree_;
    uint32_t nettype_;
    uint32_t ds_max_query_;
    uint32_t max_pdq_priority_;
    uint32_t ds_max_scans_;
    double frequency_ghz_;
    std::string vpclass_;
}; // class CPU
}  // namespace core_example
