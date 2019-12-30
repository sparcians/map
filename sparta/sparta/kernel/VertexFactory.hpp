
// <VertexFactory> -*- C++ -*-


#ifndef __VERTEXFACTORY__H__
#define __VERTEXFACTORY__H__

#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>

namespace sparta
{

class Vertex;

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
    Vertex* newFactoryVertex(const std::string & label,
                                                sparta::Scheduler *scheduler,
                                                bool isgop = false );

private:
    //! Unique ptr of DAG Vertices
    std::vector<std::unique_ptr<Vertex>> factory_vertices;
};


} // namespace sparta

#endif
