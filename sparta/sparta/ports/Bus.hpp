// <Bus> -*- C++ -*-


/**
 * \file   Bus.hpp
 *
 * \brief  File that defines the Bus, BusSet and helper binding classes
 */

#ifndef __BUS_H__
#define __BUS_H__

#include <sstream>
#include <algorithm>

#include "sparta/utils/StringUtils.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/ports/PortSet.hpp"

namespace sparta
{
    /**
     * \class Bus
     * \brief Class that defines a Bus type
     *
     * A bus is conceptually a collection of In/DataOutPort types that can
     * be bound to a bus with the equivalent, but opposite series of
     * ports.  For example, a bus that contains one DataInPort and two
     * DataOutPorts can be bound to another bus that contains one DataOutPort
     * and two DataInPorts.  The binding occurs via name matching after
     * removing cerntain keywords, specifically "in" and "out".
     *
     * Bus objects can only be added to a BusSet, and BusSet objects
     * can only take Bus objects.  However, other objects such as
     * stats, regs, etc can be added to a Bus.
     *
     * During binding, the Bus will first segregate the In and Out
     * directional ports, strip the names for easy matching, and start
     * the process of individual port binding (bi-directional).  The
     * end result is a Bus that is either completely bound, or not
     * bound at all.  If there is an issue during binding, the
     * framework will throw a SpartaException listing the issue (such as
     * 'cannot discern the names' or 'no equivalence found for port
     * XYZ'
     */
    class Bus : public sparta::TreeNode
    {
        void populatePortMap_(PortSet::RegisteredPortMap & port_map,
                              const PortSet::RegisteredPortMap & src_ports,
                              const std::string & strip_str) const
        {
            for(auto & rpi : src_ports)
            {
                // Remove beginning/trailing in/out
                std::string stripped_port_name =
                    sparta::utils::strip_string_pattern(strip_str + "_", rpi.first);

                stripped_port_name =
                    sparta::utils::strip_string_pattern("_" + strip_str, stripped_port_name);

                // Remove underscores
                stripped_port_name.erase(std::remove(stripped_port_name.begin(),
                                                     stripped_port_name.end(), '_'),
                                         stripped_port_name.end());
                const auto & it = port_map.find(stripped_port_name);
                sparta_assert(it == port_map.end(),
                                  "Error: Cannot discern between port name " << rpi.first
                                  << " and " << it->second->getName());
                port_map.emplace(std::make_pair(stripped_port_name, rpi.second));
            }
        }

        void checkBinding_(const PortSet::RegisteredPortMap & port_map1,
                           const PortSet::RegisteredPortMap & port_map2,
                           std::string & unbound_ports) const
        {
            // First check to make sure that we can COMPLETELY bind
            // the buses
            for(auto & pm1_pt : port_map1) {
                const auto & pm2_pt = port_map2.find(pm1_pt.first);
                if(pm2_pt == port_map2.end()) {
                    unbound_ports += pm1_pt.second->getLocation() + ", ";
                }
            }
            for(auto & pm2_pt : port_map2) {
                const auto & pm1_pt = port_map1.find(pm2_pt.first);
                if(pm1_pt == port_map1.end()) {
                    unbound_ports += pm2_pt.second->getLocation() + ", ";
                }
            }
        }

        void bindPorts_(const PortSet::RegisteredPortMap & port_map1,
                        const PortSet::RegisteredPortMap & port_map2) const
        {
            // It will be a complete bind, go for it
            for(auto & pm1_pt : port_map1)
            {
                const auto & pm2_pt = port_map2.find(pm1_pt.first);
                sparta_assert(pm2_pt != port_map2.end());
                // Bi-directional
                sparta::bind(pm1_pt.second, pm2_pt->second);
            }
        }

        // Internal port set
        sparta::PortSet port_set_;
        bool precedence_set_ = false;

    public:
        /**
         * \brief Construct a bus given the parent BusSet
         * \param parent The parent of this Bus, MUST be a BusSet
         * \param name The name of this bus
         * \param desc The description of this bus
         */
        Bus(TreeNode * parent,
            const std::string & name,
            const std::string& group = GROUP_NAME_NONE,
            group_idx_type group_idx = GROUP_IDX_NONE,
            const std::string & desc = "");

        /**
         * \brief Get the port set this bus uses to maintain the ports
         * \return Reference to the PortSet
         */
        PortSet & getPortSet() {
            return port_set_;
        }

        /**
         * \brief Register the given port with the bus
         * \param port The port to add to the Bus
         * \throw Will throw a sparta::SpartaException is the port already exists
         */
        void registerPort(Port * port) {
            sparta_assert(precedence_set_ == false,
                        "Cannot add ports after any call to set precedence, e.g. inportsPrecede()");

            port_set_.addChild(port);
        }

        //! Enable pipeline collection on the bus.  This method walks
        //! through the ports register with this bus and enables
        //! collection on them
        void enableCollection() {
            for(auto & p : port_set_.getPorts(Port::IN)) {
                p.second->enableCollection(this);
            }
            for(auto & p : port_set_.getPorts(Port::OUT)) {
                p.second->enableCollection(this);
            }
        }

        /**
         * \brief Get the ports in this PortSet for the given direction
         * \param direction The direction
         * \return Reference to the internal PortSet
         */
        PortSet::RegisteredPortMap & getPorts(Port::Direction direction) {
            return port_set_.getPorts(direction);
        }

        /**
         * \brief Set the port delay to the given value for all IN ports
         * \param delay_cycles Number of cycles to set the delay for
         */
        void setInPortDelay(uint32_t delay_cycles) {
            PortSet::RegisteredPortMap & in_ports = getPorts(sparta::Port::IN);
            for(auto & pi : in_ports) {
                sparta::Port * port = pi.second;
                sparta_assert (port != nullptr);
                port->setPortDelay (static_cast<sparta::Clock::Cycle>(delay_cycles));
            }
        }

        /**
         * \brief Set the port delay to the given value for all IN ports
         * \param delay_cycles Number of cycles to set the delay for
         */
        void setInPortDelay(double delay_cycles) {
            PortSet::RegisteredPortMap & in_ports = getPorts(sparta::Port::IN);
            for(auto & pi : in_ports) {
                sparta::Port * port = pi.second;
                sparta_assert (port != nullptr);
                port->setPortDelay (delay_cycles);
            }
        }

        /**
         * \brief Make all inports precede the given event
         * \param event The event that should fire AFTER all inports fire
         */
        template<typename EventT>
        void inportsPrecede(EventT & event) {
            PortSet::RegisteredPortMap & in_ports = getPorts(sparta::Port::IN);
            for(auto & pi : in_ports) {
                InPort * port = dynamic_cast<InPort*>(pi.second);
                sparta_assert (port != nullptr);
                port->registerConsumerEvent(event);
            }
            precedence_set_ = true;
        }

        /**
         * \brief Make all outports succeed the given event
         * \param event The event that should fire BEFORE any outports are processed
         */
        template<typename EventT>
        void outportsSucceed(EventT & event) {
            PortSet::RegisteredPortMap & out_ports = getPorts(sparta::Port::OUT);
            for(auto & pi : out_ports) {
                OutPort * port = dynamic_cast<OutPort*>(pi.second);
                sparta_assert (port != nullptr);
                port->registerProducingEvent(event);
            }
            precedence_set_ = true;
        }

        /**
         * \brief Bind bus1 to bus2
         * \param bus1 Bus1
         */
        void bind(Bus * other_bus)
        {
            // Step 1, separate the bus' in and out ports
            PortSet::RegisteredPortMap this_bus_in_ports;
            PortSet::RegisteredPortMap this_bus_out_ports;
            PortSet::RegisteredPortMap other_bus_in_ports;
            PortSet::RegisteredPortMap other_bus_out_ports;
            populatePortMap_(this_bus_in_ports,   getPorts(Port::IN), "in");
            populatePortMap_(this_bus_out_ports,  getPorts(Port::OUT), "out");
            populatePortMap_(other_bus_in_ports,  other_bus->getPorts(Port::IN), "in");
            populatePortMap_(other_bus_out_ports, other_bus->getPorts(Port::OUT), "out");

            // Step 2, throw an exception for those ports that cannot
            // be not bound -- not equivalent buses
            //
            // I could make sure that the number of entries in
            // this_bus_in_ports == other_bus_out_ports, but that
            // doesn't help the developer debug the issue.  Instead,
            // we try to find the likely unbound ports
            std::string unbound_ports;
            checkBinding_(this_bus_in_ports, other_bus_out_ports, unbound_ports);
            checkBinding_(this_bus_out_ports, other_bus_in_ports, unbound_ports);
            sparta_assert(unbound_ports.empty(),
                              "When binding bus '" << getName()
                              << "' to bus '" << other_bus->getName()
                              << "', the following ports will NOT get bound (no equivalence): "
                              << unbound_ports);

            // Step 3, go through this bus's in ports and bind to the
            // out port in other_bus.
            bindPorts_(this_bus_in_ports, other_bus_out_ports);
            bindPorts_(this_bus_out_ports, other_bus_in_ports);
        }
    protected:
        void onSettingParent_(const TreeNode* parent) const override;
    };

    /**
     * \class BusSet
     * \brief A TreeNode that represents a set of Buses
     *
     * Buses are TreeNodes in themselves, but to prevent clutter
     * within the ResourcTreeNode, use a BusSet to collect the buses
     * under a common structure.
     */
    class BusSet : public TreeNode
    {
    public:
        /**
         * \brief Construct a BusSet with a given parent.  The parent can be nullptr
         * \param parent The parent of this BusSet
         * \param desc Description of this BusSet
         */
        BusSet(TreeNode * parent, const std::string & desc) :
            TreeNode(parent, "buses", desc)
        {}

        //! Cannot copy BusSets
        BusSet(const BusSet &) = delete;

        //! Cannot assign BusSets
        BusSet & operator=(const BusSet &) = delete;

    protected:

        void onAddingChild_(TreeNode* child) override {
            Bus * bus = dynamic_cast<Bus*>(child);
            sparta_assert(bus != nullptr,
                              "ERROR: Attempting to add object '" << child->getName()
                              << "' which is not a Bus type to '" << getLocation() << "'");

        }

    private:
        //! The registered buses within this BusSet
        std::unordered_map<std::string, Bus*> registered_buses_;
    };

    //! Constructor
    inline Bus::Bus(TreeNode * parent,
                    const std::string & name,
                    const std::string& group,
                    group_idx_type group_idx,
                    const std::string & desc)  :
        TreeNode(name, group, group_idx, desc),
        port_set_(this, desc + " PortSet")
    {
        setExpectedParent_(parent);
        if(parent != nullptr) {
            parent->addChild(this); // Do not inherit parent state
        }
    }

    //! Sanity checking...
    inline void Bus::onSettingParent_(const TreeNode* parent) const
    {
        // Buses can only be added to a BusSet
        const BusSet * bus_set = dynamic_cast<const BusSet*>(parent);
        sparta_assert(bus_set != nullptr,
                          "ERROR: Attempting to add Bus '" << getName()
                          << "' to something that is not a BusSet");
    }

    /**
     * \brief Bind two buses together
     * \param p1 First bus to bind to
     * \param p2 Second bus to bind to
     *
     * This method will bi-directionally bind two buses together
     */
    inline void bind(Bus * p1, Bus * p2) {
        sparta_assert(p1 != nullptr);
        sparta_assert(p2 != nullptr);
        p1->bind(p2);
    }

    /**
     * \brief Bind two buses together
     * \param p1 First bus to bind to
     * \param p2 Second bus to bind to
     *
     * This method will bi-directionally bind two buses together
     */
    inline void bind(Bus & p1, Bus & p2) {
        bind(&p1, &p2);
    }

    /**
     * \brief Bind two buses together
     * \param p1 First bus to bind to
     * \param p2 Second bus to bind to
     *
     * This method will bi-directionally bind two buses together
     */
    inline void bind(Bus * p1, Bus & p2) {
        sparta_assert(p1 != nullptr);
        bind(p1, &p2);
    }

    /**
     * \brief Bind two buses together
     * \param p1 First bus to bind to
     * \param p2 Second bus to bind to
     *
     * This method will bi-directionally bind two buses together
     */
    inline void bind(Bus & p1, Bus * p2) {
        sparta_assert(p2 != nullptr);
        bind(&p1, p2);
    }
}

// __BUS_H__
#endif
