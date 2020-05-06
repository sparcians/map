

#pragma once

#include <vector>

#include "sparta/resources/Queue.hpp"
#include "ExampleInst.hpp"

namespace core_example
{

    //! Instruction Queue
    typedef sparta::Queue<ExampleInstPtr> InstQueue;

    //! \typedef InstGroup
    //! \brief Typedef to define the instruction group
    typedef std::vector<InstQueue::value_type> InstGroup;

    namespace message_categories {
        const std::string INFO = "info";
        // More can be added here, with any identifier...
    }

}

