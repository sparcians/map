// <GenericUnit.cpp> -*- C++ -*-


#include <string>

#include "sparta/dynamic_pipeline/GenericUnit.hpp"
#include "sparta/simulation/Unit.hpp"

namespace sparta {
class TreeNode;
}  // namespace sparta

namespace rdp = sparta::dynamic_pipeline;

rdp::GenericUnit::GenericUnit(sparta::TreeNode* node,
                              const rdp::GenericUnit::GenericUnitParameterSet*) :
    sparta::Unit{node}{}

rdp::GenericUnit::GenericUnit(const std::string& name,
                              sparta::TreeNode* node,
                              const rdp::GenericUnit::GenericUnitParameterSet* params) :
    rdp::GenericUnit::GenericUnit(node, params){
        name_ = name;
    }

auto rdp::GenericUnit::getName() const -> const std::string&{
    return name_;
}

rdp::GenericUnit::~GenericUnit() = default;
