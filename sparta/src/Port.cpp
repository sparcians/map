// <Port.cpp> -*- C++ -*-


#include "sparta/ports/Port.hpp"

#include "sparta/ports/PortSet.hpp"

void sparta::Port::ensureParentIsPortSet_(TreeNode * parent)
{
    if(dynamic_cast<sparta::PortSet*>(parent) != nullptr) {
        return;
    }
    throw SpartaException("Port ") << getLocation()
                                 << " parent node is not a PortSet. Ports can only be added as "
                                 << "children of a sparta::PortSet ";
}
