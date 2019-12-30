// <VertexFactory> -*- C++ -*-


#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "sparta/kernel/Vertex.hpp"
#include "sparta/kernel/VertexFactory.hpp"

namespace sparta
{
class Scheduler;

    Vertex* VertexFactory::newFactoryVertex(const std::string & label,
                                      sparta::Scheduler *scheduler,
                                      bool isgop)
    {
        factory_vertices.emplace_back(new Vertex(label,scheduler, isgop));
        return factory_vertices.back().get();
    }
}
