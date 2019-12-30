// <GenericUnit.h> -*- C++ -*-


#ifndef __GENERIC_UNIT_H__
#define __GENERIC_UNIT_H__

#include <string>
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace sparta{
namespace dynamic_pipeline{
class GenericUnit : public sparta::Unit{
public:
    class GenericUnitParameterSet : public sparta::ParameterSet{
    public:
        GenericUnitParameterSet(sparta::TreeNode* node) : sparta::ParameterSet(node){}
    };

    GenericUnit(sparta::TreeNode* node,
                const GenericUnitParameterSet* params);

    GenericUnit(const std::string& name,
                sparta::TreeNode* node,
                const GenericUnitParameterSet* params);

    ~GenericUnit();

    auto getName() const -> const std::string&;
    static constexpr char name[] = "generic_unit";
    private:
    std::string name_;
}; // class GenericUnit
}  // namespace dynamic_pipeline
}  // namespace sparta
#endif
