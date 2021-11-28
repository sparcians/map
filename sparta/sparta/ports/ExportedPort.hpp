// <ExportedPort.hpp> -*- C++ -*-

/**
 * \file   ExportedPort.hpp
 *
 * \brief  File that defines the ExportedPort class
 */
#pragma once

#include <string>
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/utils/Utils.hpp"

namespace sparta
{
    /*!
     * \class ExportedPort
     * \brief Class that "exports" a port that's contained in the same
     *        ResourceTreeNode structure
     *
     * Built to address the request in GitHub issue #172, ExportedPort
     * allows a modeler to "represent" a Port deep in a component
     * hierarchy at a higher level.  For example, if the modeler is
     * building a CPU model with the following hierarchy:
     *
     * \code
     * top.cpu
     *      +-> lsu
     *           +-> biu_interface
     *                     +-> ports
     *                           +-> out_cpu_request <sparta::DataOutPort>
     * top.mss
     *      +-> coherency_module
     *                +-> cpu_interface
     *                         +-> ports
     *                               +-> in_cpu_request <sparta::DataInPort>
     * \endcode
     *
     * Binding of the `out_cpu_request` to `in_cpu_request` would be
     * very verbose and very hard-coded:
     *
     * \code
     * sparta::bind(top->getChildAs("top.cpu.lsu.biu_interface.ports.out_cpu_request"),
     *              top->getChildAs("top.mss.coherency_module.cpu_interface.ports.in_cpu_request"));
     *
     * \endcode
     *
     * With ExportedPort, the modeler can represent these ports at a higher level:
     *
     * \code
     *
     * top.cpu
     *      +-> ports.out_cpu_request <sparta::ExportedPort -> lsu.biu_interface.ports.out_cpu_request>
     *      +-> lsu
     *           +-> biu_interface
     *                     +-> ports
     *                           +-> out_cpu_request <sparta::DataOutPort>
     * top.mss
     *      +-> ports
     *           +-> in_cpu_request <sparta::ExportedPort -> coherency_module.cpu_interface.ports.in_cpu_request>
     *      +-> coherency_module
     *                +-> cpu_interface
     *                         +-> ports
     *                               +-> in_cpu_request <sparta::DataInPort>
     *
     * // Usage with ExportedPort
     * sparta::bind(top->getChildAs("top.cpu.ports.out_cpu_request"),
     *              top->getChildAs("top.mss.ports.in_cpu_request"));
     * \endcode
     *
     */
    class ExportedPort : public Port
    {
    public:

        ExportedPort(sparta::TreeNode *    portset,
                     const std::string &   exported_port_name,
                     sparta::TreeNode    * exported_port_search_path,
                     const std::string &   internal_port_name) :
            Port(portset, Port::Direction::UNKNOWN, exported_port_name),
            exported_port_search_path_(exported_port_search_path),
            internal_port_name_(internal_port_name)
        {}

        ExportedPort(sparta::TreeNode  * portset,
                     const std::string & exported_port_name,
                     sparta::Port      * exported_port) :
            Port(portset, sparta::notNull(exported_port)->getDirection(), exported_port_name),
            exported_port_(exported_port),
            internal_port_name_(exported_port->getName())
        {}

        //! \brief Override Port::bind
        //! \param port The port to bind the internal exported port
        void bind(Port * port) override final;

    private:
        // The exported port -- to either be found or provided
        sparta::Port * exported_port_ = nullptr;

        // Non-const as the Port TreeNode contained in the path will be modified
        sparta::TreeNode * exported_port_search_path_ = nullptr;
        const std::string  internal_port_name_;
    };
}
