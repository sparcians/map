// <EdgeFactory> -*- C++ -*-


#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "sparta/kernel/Vertex.hpp"
#include "sparta/kernel/EdgeFactory.hpp"

namespace sparta
{

EdgeFactory::~EdgeFactory(){}

void EdgeFactory::removeEdge(const sparta::Edge* edge) {
    auto ei = std::find_if(edges_.begin(), edges_.end(),
                           [edge] (const auto & ptr) -> bool
                           {
                               return ptr.get() == edge;
                           });

    if (ei != edges_.end()) {
        edges_.erase(ei);
    }
}

void EdgeFactory::dumpToCSV(std::ostream &os) const {
    std::ios_base::fmtflags os_state(os.flags());

    bool first = true;
    for (const auto& e : edges_) {
        e->dumpToCSV(os, first);
        first = false;
    }

    os.flags(os_state);
}

}
