
// <ExportedPort.cpp> -*- C++ -*-

#include "sparta/ports/ExportedPort.hpp"
#include "sparta/utils/TreeUtils.hpp"

/**
 * \file   ExportedPort.cpp
 * \brief  File that defines the ExportedPort class
 */
namespace sparta
{
    void ExportedPort::resolvePort_()
    {
        if(nullptr != internal_port_) { return; }

        sparta_assert(internal_port_search_path_ != nullptr,
                      "For ExportedPort, if the exported port is not explicitly given, "
                      "need a search path to find it");
        std::vector<sparta::TreeNode *> port_matches;
        if(0 == utils::recursiveTreeSearch(internal_port_search_path_, internal_port_name_, port_matches)) {
            sparta::SpartaException e;
            e << "Could not find ports matching the name "
              << internal_port_name_ << " with the starting location: "
              << internal_port_search_path_->getLocation();
            throw e;
        }

        // Found at least one port that matches the name
        if(port_matches.size() == 1)
        {
            try {
                internal_port_ = port_matches[0]->getAs<sparta::Port>();
                // This can happen if the user names the exported port
                // the same name as the internal port, but there is no
                // internal port with that name!
                if(internal_port_ == this) {
                    internal_port_ = nullptr;
                    // Throw to the catch handler below -- same error
                    throw sparta::SpartaException();
                }
            }
            catch(...) {
                sparta::SpartaException e;
                e << "ExportedPort:: Have a TreeNode name match for '"
                  << internal_port_name_ << "' but it is not a Port class type: "
                  << port_matches[0]->getLocation();
                throw e;
            }
        }
        else {
            // This can happen if the user names the exported port
            // the same name as the internal port, but there are
            // more matches than the expected two:
            //
            // 1. Exported Port
            // 2. Internal port
            sparta_assert(port_matches.size() == 2,
                          "Found multiple matches for port name " << internal_port_name_
                          << " with the starting location: " << internal_port_search_path_->getLocation());

            // One of the found ports is the exported port,
            // the other is the intended internal port
            for (auto port : port_matches)
            {
                try {
                    (void)port->getAs<sparta::ExportedPort>();    // See if this is the exported port
                }
                catch(...) {
                    // Not the exported port
                    internal_port_ = port->getAs<sparta::Port>();
                }
            }
            sparta_assert(internal_port_ != nullptr,
                          "Did not find an internal port named '" << internal_port_name_
                          << "' with the starting location: " << internal_port_search_path_->getLocation());
        }
    }

    void ExportedPort::bind(Port * port)
    {
        resolvePort_();

        // See if the given port is also an ExportedPort
        auto exported_port = dynamic_cast<sparta::ExportedPort*>(port);
        if(exported_port != nullptr)
        {
            exported_port->resolvePort_();
            port = exported_port->getInternalPort();
        }

        // Have port, will travel
        internal_port_->bind(port);
    }
}
