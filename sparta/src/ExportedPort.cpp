
// <ExportedPort.cpp> -*- C++ -*-

#include "sparta/ports/ExportedPort.hpp"
#include "sparta/utils/TreeUtils.hpp"

/**
 * \file   ExportedPort.cpp
 * \brief  File that defines the ExportedPort class
 */
namespace sparta
{
    void ExportedPort::bind(Port * port) {
        if(nullptr == exported_port_) {
            sparta_assert(exported_port_search_path_ != nullptr,
                          "For ExportedPort, if the exported port is not explicitly given, "
                          "need a search path to find it");
            std::vector<sparta::TreeNode *> port_matches;
            if(0 == utils::recursiveTreeSearch(exported_port_search_path_, internal_port_name_, port_matches)) {
                sparta::SpartaException e;
                e << "Could not find ports matching the name "
                  << internal_port_name_ << " with the starting location: "
                  << exported_port_search_path_->getLocation();
                throw e;
            }
            sparta_assert(1 == port_matches.size(),
                          "Found multiple matches for port name " << internal_port_name_
                          << " with the starting location: " << exported_port_search_path_->getLocation());
            try {
                exported_port_ = port_matches[0]->getAs<sparta::Port>();
            }
            catch(SpartaException & ex) {
                sparta::SpartaException e;
                e << "ExportedPort:: Have a TreeNode name match for "
                  << internal_port_name_ << " but it is not a Port class type: "
                  << port_matches[0]->getLocation();
                throw e;
            }
            catch(...) {
                throw;
            }
        }
        // Have port, will travel
        exported_port_->bind(port);
    }
}
