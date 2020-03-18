// <ExampleInst.cpp> -*- C++ -*-

#include "ExampleInst.hpp"

namespace core_example
{
    // This pipeline supports around 135 outstanding example instructions.
    sparta::SpartaSharedPointer<ExampleInst>::SpartaSharedPointerAllocator example_inst_allocator(300, 250);
}
