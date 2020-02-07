// <EdgeFactory> -*- C++ -*-


#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "sparta/kernel/Vertex.hpp"
#include "sparta/kernel/EdgeFactory.hpp"

namespace sparta
{

EdgeFactory::~EdgeFactory() {
    for (auto e : edges_) {
        delete(e);
    }
}

void EdgeFactory::removeEdge(const sparta::Edge* e) {
    auto ei = edges_.find(e);
    if (ei != edges_.end()) {
        edges_.erase(ei);
    }
    delete(*ei);
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
