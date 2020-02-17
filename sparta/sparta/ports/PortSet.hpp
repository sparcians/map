// <Port> -*- C++ -*-


/**
 * \file   PortSet.hpp
 *
 * \brief File that defines the PortSet class
 */

#ifndef __PORT_SET_H__
#define __PORT_SET_H__
#include <set>
#include <list>
#include <unordered_map>

#include "sparta/ports/Port.hpp"

namespace sparta
{

    /**
     * \class PortSet
     * \brief A TreeNode that represents a set of ports used by a Resource
     *
     * Ports are TreeNodes in themselves, but to prevent clutter
     * within the ResourcTreeNode, use a PortSet to collect the ports
     * under a common structure.
     */
    class PortSet : public TreeNode
    {
    public:

        //! Convenience typedef
        typedef std::unordered_map<std::string, Port *> RegisteredPortMap;

        /**
         * \brief Construct a PortSet with a given parent.  The parent can be nullptr
         * \param parent The parent of this PortSet
         * \param desc Description of this PortSet
         */
        PortSet(TreeNode * parent, const std::string & desc = "Port Set") :
            TreeNode(parent, "ports", desc)
        {}

        /**
         * \brief Get a port by the given name
         * \param named_port The named port to retrieve
         * \return Pointer to the port or nullptr if not found
         */
        Port * getPort(const std::string & named_port) const {
            for(uint32_t i = 0; i < Port::N_DIRECTIONS; ++i) {
                auto found_port = registered_ports_[i].find(named_port);
                if(found_port != registered_ports_[i].end()) {
                    return found_port->second;
                }
            }
            //Throw an exception if we could not find the port.
            throw SpartaException("The port with the name : " + named_port + " could not be found");
            return nullptr;
        }

        /**
         * \brief Get a DataInPort by the given name (convenience function)
         * \param named_port The named DataInPort to retrieve
         * \return Pointer to the DataInPort or nullptr if not found.
         */
        // Port * getDataInPort(const std::string & named_port) {
        //     auto found_port = registered_ports_[Port::IN].find(named_port);
        //     if(found_port != registered_ports_[Port::IN].end()) {
        //         // Right name, wrong direction
        //         return found_port->second;
        //     }
        //     //Throw an exception if we could not find the port.
        //     throw SpartaException("The port with the name : " + named_port + " could not be found");
        //     return nullptr;
        // }

        // /**
        //  * \brief Get a DataOutPort by the given name (convenience function)
        //  * \param named_port The named DataOutPort to retrieve
        //  * \return Pointer to the port or nullptr if not found
        //  */
        // Port * getDataOutPort(const std::string & named_port) {
        //     auto found_port = registered_ports_[Port::OUT].find(named_port);
        //     if(found_port != registered_ports_[Port::OUT].end()) {
        //         // Right name, wrong direction
        //         return found_port->second;
        //     }
        //     //Throw an exception if we could not find the port.
        //     throw SpartaException("The port with the name : " + named_port + " could not be found");
        //     return nullptr;
        // }


        //! Cannot copy PortSets
        PortSet(const PortSet &) = delete;

        //! Cannot assign PortSets
        PortSet & operator=(const PortSet &) = delete;

        /**
         * \brief Get the ports in this PortSet for the given direction
         * \param direction The direction
         * \return Reference to the internal PortSet
         */
        RegisteredPortMap & getPorts(Port::Direction direction) {
            return registered_ports_[direction];
        }

    private:
        //! The registered ports within this PortSet
        RegisteredPortMap registered_ports_[Port::N_DIRECTIONS];

        /*!
         * \brief React to a child registration
         *
         * \param child TreeNode child that must be downcastable to a
         *        sparta::Port. This is a borrowed reference - child is
         *        *not* copied. Child lifetime must exceed that of
         *        this StatisticSet instance.
         * \pre Must not be finalized
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override {
            if(isFinalized()){
                throw SpartaException("Cannot add a child Port once a PortSet is finalized. "
                                    "Error with: ")
                    << getLocation();
            }

            Port* port = dynamic_cast<Port*>(child);
            if(nullptr != port){
                sparta_assert(registered_ports_[port->getDirection()].count(port->getName()) == 0,
                                  "ERROR: Port '" << port->getName() << "' already registered");
                registered_ports_[port->getDirection()][port->getName()] = port;
                return;
            }

            throw SpartaException("Cannot add TreeNode child ")
                << child->getName() << " to PortSet " << getLocation()
                << " because the child is not a Port or derivative";
        }

    };
}



// __PORT_SET_H__
#endif
