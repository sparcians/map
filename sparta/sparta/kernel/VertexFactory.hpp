
// <VertexFactory> -*- C++ -*-


#pragma once

#include "sparta/kernel/Vertex.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>

namespace sparta
{

 /**
 * \class VertexFactory
 * \brief A class that allows you to create new Vertices
 *
 * A class used to keep track of all the new vertices create
 * and then allow you to 'delete' the vertices once the DAG
 * has been finalized. Used in the DAG when GOPs/Vertices are
 * created to form the DAG, as well as used in Scheduleables
 * to create the internal Vertex used by the Scheduleable to
 * link to the DAG.
 */
class VertexFactory
{

public:

    //Constructor
    VertexFactory() =default;

    /**
     * \brief Factory method to create new Vertices
     */
    template<typename ...ArgTypes>
    Vertex* newFactoryVertex(ArgTypes&&... args)
    {
        Vertex*   new_vertex = new Vertex(std::forward<ArgTypes>(args)...);
        vertices_.emplace_back(new_vertex);
        return new_vertex;
    }

    void dumpToCSV(std::ostream& os) const;

private:
    //! Unique ptr of DAG Vertices
    std::vector<std::unique_ptr<Vertex>> vertices_;
};


} // namespace sparta

