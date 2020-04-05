
// <EdgeFactory> -*- C++ -*-


#pragma once

#include "sparta/kernel/Vertex.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <set>

namespace sparta
{

/**
* \class EdgeFactory
* \brief A class that allows you to create new Vertices
*
* A class used to keep track of all the new vertices create
* and then allow you to 'delete' the vertices once the DAG
* has been finalized. Used in the DAG when GOPs/Vertices are
* created to form the DAG, as well as used in Scheduleables
* to create the internal Vertex used by the Scheduleable to
* link to the DAG.
*/
class EdgeFactory
{

public:

    //Constructor
    EdgeFactory() =default;
    ~EdgeFactory();

    /**
     * \brief Factory method to create new Vertices
     */
    template<typename ...ArgTypes>
    Edge* newFactoryEdge(ArgTypes&&... args)
    {
        Edge*   new_edge = new Edge(std::forward<ArgTypes>(args)...);
        edges_.emplace(new_edge);
        return new_edge;
    }

    void removeEdge(const Edge* e);
    void dumpToCSV(std::ostream& os) const;

private:
    // I use bare Edge* here instead of std::unique_ptr<> since I need
    // to be able to do a set::find() on the Edge address. The
    // class destructor zaps the Edges
    std::set<const Edge*> edges_;
};


} // namespace sparta


