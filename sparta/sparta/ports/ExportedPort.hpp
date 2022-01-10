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
     * hierarchy at a higher level for the sole purpose of _binding
     * only_.  This is not intended to be a fully functioning Port.
     *
     * The internal port can be either provided directly or searched
     * for during binding.
     *
     * For example, if the modeler is building a CPU model with the
     * following hierarchy:
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

        /**
         * \brief Create an ExportedPort that exposes an internal port by name
         *
         * \param portset The sparta::PortSet this ExportedPort belongs to
         * \param exported_port_name The exported port name; can be different from the internal port
         * \param internal_port_search_path The TreeNode to search for the internal port
         * \param internal_port_name The name of the internal port to represent
         *
         */
        ExportedPort(sparta::TreeNode  * portset,
                     const std::string & exported_port_name,
                     sparta::TreeNode  * internal_port_search_path,
                     const std::string & internal_port_name) :
            Port(portset, Port::Direction::UNKNOWN, exported_port_name),
            internal_port_search_path_(internal_port_search_path),
            internal_port_name_(internal_port_name)
        {}

        /**
         *
         * \brief Create an ExportedPort for an explicit internal port
         *
         * \param portset The sparta::PortSet this ExportedPort belongs to
         * \param exported_port_name The exported port name; can be different from the internal port
         * \param internal_port The internal port to be exported
         *
         */
        ExportedPort(sparta::TreeNode  * portset,
                     const std::string & exported_port_name,
                     sparta::Port      * internal_port) :
            Port(portset, sparta::notNull(internal_port)->getDirection(), exported_port_name),
            internal_port_(internal_port),
            internal_port_name_(internal_port->getName())
        {}

        //! \brief Override Port::bind
        //! \param port The port to bind the internal exported port
        void bind(Port * port) override final;

        //! \brief Override Port::isBound
        //! \return true if internal port is bound; false if not bound
        bool isBound() const override final
        {
            // If the internal_port is nullptr, it has not been bound
            // yet (searched for)
            if(nullptr != internal_port_) {
                return internal_port_->isBound();
            }
            return false;
        }

        //! \brief Get the direction of the port
        //! \return The Port direction of the internal port; UNKNOWN if not bound
        //!
        //! If the port is to be found during binding, this function
        //! will return UNKNOWN until binding is complete.
        Direction getDirection() const override {
            if(nullptr == internal_port_) {
                return Port::Direction::UNKNOWN;
            }
             return internal_port_->getDirection();
        }

        //! \brief Function that cannot be used in ExportedPort.
        //! The user must set auto precedence directly in the internal port
        void participateInAutoPrecedence(bool) override final {
            sparta_assert(false, "You cannot set auto precedence on an ExportedPort");
        }

        //! \brief Does the internal Port participate in auto-precedence
        //! \return true if so, false otherwise or the port is not available
        bool doesParticipateInAutoPrecedence() const override final {
            if(internal_port_) {
                return internal_port_->doesParticipateInAutoPrecedence();
            }
            return false;
        }

        //! \brief Return the intenal representative port (non-const)
        //! \return The internal port this ExportedPort represents
        //!
        //! This method _might_ return nullptr if the port is to be
        //! found during binding (and was not initially provided)
        sparta::Port * getInternalPort() { return internal_port_; }

        //! \brief Return the intenal representative port (const)
        //! \return The internal port this ExportedPort represents
        //!
        //! This method _might_ return nullptr if the port is to be
        //! found during binding (and was not initially provided)
        const sparta::Port * getInternalPort() const { return internal_port_; }

        //! \brief Print the exported port
        //! \param pretty Make it a pretty print (ignored)
        std::string stringize(bool pretty=false) const override {
            sparta_assert(internal_port_ != this);
            std::stringstream ss;
            ss << "[exported port <" << getLocation() << "> ";
            if(internal_port_) {
                ss << internal_port_->stringize(pretty);
            }
            else {
                ss << "undefined";
            }
            ss << "]";
            return ss.str();
        }


    private:
        // The interal port -- to either be found or provided
        sparta::Port * internal_port_ = nullptr;

        // Non-const as the Port TreeNode contained in the path will
        // be modified during binding.  If this variable were const,
        // the code could not find the to-be-modified internal_port_
        sparta::TreeNode * internal_port_search_path_ = nullptr;
        const std::string  internal_port_name_;

        // Resolve the internal port
        void resolvePort_();
    };
}
