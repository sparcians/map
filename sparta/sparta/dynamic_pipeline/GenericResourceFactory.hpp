// <GenericResourceFactory.h> -*- C++ -*-


#pragma once

#include "sparta/dynamic_pipeline/GenericUnit.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

namespace sparta{
namespace dynamic_pipeline{
class GenericResourceFactory{
public:
    typedef sparta::dynamic_pipeline::GenericUnit GU;
    typedef GU::GenericUnitParameterSet GUPS;
    GenericResourceFactory() : generic_unit_rf_(new ResourceFactory<GU, GUPS>()){}
    ~GenericResourceFactory(){
        delete generic_unit_rf_;
    }
    auto getGUFactory() const -> sparta::ResourceFactory<GU, GUPS>*{
        return generic_unit_rf_;
    }
private:
    sparta::ResourceFactory<GU, GUPS>* generic_unit_rf_ {nullptr};
}; // struct GenericResourceFactory
}  // namespace dynamic_pipeline
}  // namespace sparta
