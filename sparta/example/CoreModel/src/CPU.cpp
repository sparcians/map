// <CPU.cpp> -*- C++ -*-


#include "CPU.hpp"

//! \brief Name of this resource. Required by sparta::UnitFactory
constexpr char core_example::CPU::name[];

//! \brief Constructor of this CPU Unit
core_example::CPU::CPU(sparta::TreeNode* node, const core_example::CPU::CPUParameterSet* params) :
    sparta::Unit{node},
    fastpoll_{params->fastpoll},
    out_of_order_execution_{params->out_of_order_execution},
    superscalar_degree_{params->superscalar_degree},
    nettype_{params->nettype},
    ds_max_query_{params->ds_max_query},
    max_pdq_priority_{params->max_pdq_priority},
    ds_max_scans_{params->ds_max_scans},
    frequency_ghz_{params->frequency_ghz},
    vpclass_{params->vpclass}{}

//! \brief Destructor of this CPU Unit
core_example::CPU::~CPU() = default;
