// <PipelineDataCallback> -*- C++ -*-

#ifndef __PIPELINE_DATA_CALLBACK__
#define __PIPELINE_DATA_CALLBACK__

#include <fstream>
#include <iostream>
#include "transaction_structures.h"
#include "sparta/utils/SpartaException.hpp"
namespace sparta{
namespace argos{
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
        // TransactionInterval to be used by Argos.
        virtual void foundPairRecord(pair_t*) = 0;
    };
}//NAMESPACE:argos
}//NAMESPACE:sparta
#endif
