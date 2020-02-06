// <VertexFactory> -*- C++ -*-

#include "sparta/kernel/VertexFactory.hpp"
#include <iostream>

namespace sparta {

void VertexFactory::dumpToCSV(std::ostream& os) const {
    std::ios_base::fmtflags os_state(os.flags());

    bool first = true;
    for (const auto& v : vertices_) {
        v->dumpToCSV(os, first);
        first = false;
    }

    os.flags(os_state);
}

} // namespace sparta
