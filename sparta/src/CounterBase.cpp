// <Counter> -*- C++ -*-


/*!
 * \file Counter.cpp
 * \brief Implements Counter, CycleCounter, and ReadOnlyCounter, StatisticSet,
 * and StatisticDef
 */

#include <string>

#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/simulation/TreeNode.hpp"

constexpr char sparta::StatisticSet::NODE_NAME[];

// Statistic Set
void sparta::StatisticDef::ensureParentIsStatisticSet_(sparta::TreeNode* parent){
    if(dynamic_cast<sparta::StatisticSet*>(parent) == nullptr){
        throw SpartaException("StatisticDef ") << getLocation()
            << " parent node is not a StatisticSet. StatisticDefs can only be "
               "added as children of a StatisticSet";
    }
}

void sparta::CounterBase::ensureParentIsValid_(TreeNode* parent){
    if(dynamic_cast<sparta::StatisticSet*>(parent) != nullptr){
        return;
    }

    // Add support for context counters (nested) This will require the
    // ContextCounter to be subtreed with more counters
    if(dynamic_cast<sparta::InstrumentationNode*>(parent) != nullptr){
        return;
    }
    throw SpartaException("Counter ") << getLocation()
        << " parent node is not a StatisticSet. Counters can only be added as "
        "children of a StatisticSet";
}
