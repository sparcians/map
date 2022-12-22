// <PipelineDataCallback> -*- C++ -*-

#pragma once

#include <fstream>
#include <iostream>
#include "sparta/pipeViewer/transaction_structures.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta::pipeViewer {
    /**
     * \class PipelineDataCallback
     * \brief An abstract class that recieves transactions as they are
     * read from disk.
     */
    class PipelineDataCallback
    {
    public:
        virtual ~PipelineDataCallback() {}

        virtual void foundTransactionRecord(const transaction_t*)
        {
            throw sparta::SpartaException("Read transaction with unknown transaction type");
        }
        virtual void foundInstRecord(const instruction_t*) = 0;
        virtual void foundMemRecord(const memoryoperation_t*) = 0;
        virtual void foundAnnotationRecord(const annotation_t*) = 0;
        //! Add a virtual method for the case we find a Pair Transaction Record.
        // This method is called back in TransactionDatabaseInterval to build the
        // TransactionInterval to be used by pipeViewer.
        virtual void foundPairRecord(const pair_t*) = 0;
    };
}//NAMESPACE:sparta::pipeViewer
