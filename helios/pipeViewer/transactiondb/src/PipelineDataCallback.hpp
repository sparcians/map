// <PipelineDataCallback> -*- C++ -*-

#pragma once

#include <fstream>
#include <iostream>
#include "sparta/pipeViewer/transaction_structures.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta{
namespace pipeViewer{
    /**
     * \class PipelineDataCallback
     * \brief An abstract class that recieves transactions as they are
     * read from disk.
     */
    class PipelineDataCallback
    {
    public:
        virtual ~PipelineDataCallback() {}

        virtual void foundTransactionRecord(transaction_t*)
        {
            throw sparta::SpartaException("Read transaction with unknown transaction type");
        }
        virtual void foundInstRecord(instruction_t*) = 0;
        virtual void foundMemRecord(memoryoperation_t*) = 0;
        virtual void foundAnnotationRecord(annotation_t*) = 0;
        //! Add a virtual method for the case we find a Pair Transaction Record.
        // This method is called back in TransactionDatabaseInterval to build the
        // TransactionInterval to be used by pipeViewer.
        virtual void foundPairRecord(pair_t*) = 0;
    };
}//NAMESPACE:pipeViewer
}//NAMESPACE:sparta
