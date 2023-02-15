// <Counter> -*- C++ -*-


/*!
 * \file EventNode.cpp
 * \brief Implements methods for EventNode class
 */

#include "sparta/events/EventNode.hpp"

#include <memory>

#include "sparta/events/EventSet.hpp"
#include "sparta/utils/SpartaException.hpp"

// Statistic Set
void sparta::EventNode::ensureParentIsEventSet_(sparta::TreeNode* parent){
    if(dynamic_cast<sparta::EventSet*>(parent) == nullptr){
        throw SpartaException("EventNode ") << getLocation()
                                          << " parent node is not a EventSet. Events can only be "
                                          << "added as children of a EventSet";
    }
}
