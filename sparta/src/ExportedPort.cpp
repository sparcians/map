
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
        if(nullptr == interal_port_) {
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
            sparta_assert(1 == port_matches.size(),
                          "Found multiple matches for port name " << internal_port_name_
                          << " with the starting location: " << internal_port_search_path_->getLocation());
            try {
                interal_port_ = port_matches[0]->getAs<sparta::Port>();
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
        interal_port_->bind(port);
    }
}
