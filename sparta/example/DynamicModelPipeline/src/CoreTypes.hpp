

#pragma once

// #define BOOST_SP_NO_ATOMIC_ACCESS 1
// #include <boost/shared_ptr.hpp>

#include <vector>
#include "sparta/resources/Queue.hpp"
//#include "sparta/resources/Buffer.hpp"
//#include "sparta/resources/RefPointer.hpp"

#include "ExampleInst.hpp"

namespace core_example
{

    //! Instruction Queue
    typedef sparta::Queue<ExampleInstPtr> InstQueue;
    /// Instruction Buffer
    //typedef sparta::Buffer<ExampleInstPtr> InstBuffer;

    //! \typedef InstGroup Typedef to define the instruction group
    typedef std::vector<InstQueue::value_type> InstGroup;

    namespace message_categories {
        const std::string INFO = "info";
        // More can be added here, with any identifier...
    }

}

