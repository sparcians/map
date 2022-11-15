
#include "transactiondb/src/Reader.hpp"
#include "transactiondb/src/PipelineDataCallback.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include <iomanip>
#include <unordered_map>
#include <functional>
#include <unistd.h>

/**
 * \file Database_dumper.cpp
 * \brief dump an pipeViewer database to a human readable format.
 * Instructions, run ./ArgosDumper <path+database prefix>
 * the database prefix should be the same prefix passed to the simulator when creating the database
 * example
 * ./ArgosDumper ../../sim/data_ > out.csv
 *
 * I recommend dumping the output to an *.csv file. Then in open office you can set the file to
 * recognize spaces as new columns. If you do this, then you will have a nice output formatted table
 * that should be a little easier to read/manipulate using sort functionality and what not in office.
 */
namespace sparta
{
namespace pipeViewer
{
    class DumpCallback : public PipelineDataCallback
    {
        private:
            bool merge_transactions = false;
            bool sort_by_end_time = false;

            std::unordered_map<uint16_t, std::shared_ptr<transaction_t> > continued_transactions;
            std::stringstream& output_buffer;

            // Returns whether the given transaction is split across a heartbeat
            bool isContinued(transaction_t *t) const
            {
                return (t->flags & CONTINUE_FLAG) != 0;
            }

            // Prints a transaction to output_buffer
            // This is used for the default sort mode (by transaction ID) when merging transactions
            template<typename T>
            void printToBuf(T* t, void(*print_func)(T*, std::ostream &)) const
            {
                uint64_t trans_id = t->transaction_ID;

                print_func(t, output_buffer);
            }

            template<typename T>
            void genericTransactionHandler(T* t, void(*print_func)(T*, std::ostream &))
            {
                // If there's no merging, then we can just print the transaction and be done
                if(!merge_transactions)
                {
                    print_func(t, std::cout);
                    return;
                }

                uint16_t loc_id = t->location_ID;

                std::stringstream sstr;

                // This transaction has already been encountered and is split across a heartbeat boundary
                if(continued_transactions.count(loc_id))
                {
                    // Update the saved transaction with the latest end time
                    std::shared_ptr<T> cont_trans = std::static_pointer_cast<T>(continued_transactions.at(loc_id));
                    cont_trans->time_End = t->time_End;

                    // If this transaction isn't continued, then it's the last one in the chain. So, we can print it and delete the entry.
                    if(!isContinued(t))
                    {
                        if(!sort_by_end_time)
                        {
                            // This will be out of order with respect to transaction ID, so print it to the buffer instead of stdout
                            printToBuf<T>(cont_trans.get(), print_func);
                        }
                        else
                        {
                            // We're sorting by end time, so we can just print directly to stdout
                            print_func(cont_trans.get(), std::cout);
                        }

                        continued_transactions.erase(loc_id);
                    }
                }
                else
                {
                    // This is the first part of a transaction that has been split across a heartbeat boundary
                    if(isContinued(t))
                    {
                        continued_transactions[loc_id] = std::make_shared<T>(*t);
                    }
                    // This transaction isn't split at all
                    else
                    {
                        if(!sort_by_end_time)
                        {
                            // This will be out of order with respect to transaction ID, so print it to the buffer instead of stdout
                            printToBuf<T>(t, print_func);
                        }
                        else
                        {
                            // We're sorting by end time, so we can just print directly to stdout
                            print_func(t, std::cout);
                        }
                    }
                }


            }

            static void printInst(instruction_t *t, std::ostream & os)
            {
                os << std::setbase(10);
                os << "*instruction* " << t->transaction_ID << " @ " << t->location_ID << " start: " << t->time_Start << " end: "<<t->time_End;
                os << " opcode: " << std::setbase(16) << std::showbase << t->operation_Code << " vaddr: " << t->virtual_ADR;
                os << " real_addr: " << t->real_ADR << std::endl;
            }

            virtual void foundInstRecord(instruction_t*t)
            {
                genericTransactionHandler<instruction_t>(t, &printInst);
            }

            static void printMemOp(memoryoperation_t *t, std::ostream & os)
            {
                os << std::setbase(10);
                os << "*memop* " << t->transaction_ID << " @ " << t->location_ID << " start: " << t->time_Start << " end: "<<t->time_End;
                os << std::setbase(16) << std::showbase << " vaddr: " << t->virtual_ADR;
                os << " real_addr: " << t->real_ADR << std::endl;
            }

            virtual void foundMemRecord(memoryoperation_t*t)
            {
                genericTransactionHandler<memoryoperation_t>(t, printMemOp);
            }

            static void printPairOp(pair_t * p, std::ostream & os) {
                os << "*pair* @ " << p->location_ID << " ";
                for (uint32_t i = 0; i < p->nameVector.size(); ++i) {
                    os << p->nameVector[i] << "(" << p->stringVector[i] << ") ";
                }
                os << "start: " << p->time_Start << " end: " << p->time_End;
                os << std::endl;
            }

            virtual void foundPairRecord(pair_t * t){
                genericTransactionHandler<pair_t>(t, printPairOp);
            }

            static void printAnnotation(annotation_t * t, std::ostream & os)
            {
                os << "*annotation* " << t->transaction_ID << " @ " << t->location_ID << " start: " << t->time_Start << " end: "<<t->time_End;
                os << t->annt << std::endl;
            }

            virtual void foundAnnotationRecord(annotation_t*t)
            {
                genericTransactionHandler<annotation_t>(t, printAnnotation);
            }

        public:
            DumpCallback(const bool merge, const bool sort, std::stringstream& buffer) :
                merge_transactions(merge),
                sort_by_end_time(sort),
                output_buffer(buffer)
            {
            }

    };

}//namespace sparta
}//namespace pipeViewer

void usage()
{
    std::cerr << "Usage: ArgosDumper [-h] [-m] [-s] argos_db_prefix" << std::endl
              << "Options:" << std::endl
              << "\t-h\t\tPrint usage info" << std::endl
              << "\t-m\t\tMerge transactions that were split by a heartbeat interval" << std::endl
              << "\t-s\t\tSort output by transaction end time" << std:: endl;
}

int main(int argc, char ** argv)
{
    std::stringstream output_buffer;
    std::string db_path = "db_pipeout/pipeout";
    if (argc > 1) {
        db_path = argv[1];
    }

    const bool merge_transactions = false;
    const bool sort_by_end_time = true;

    auto reader = sparta::pipeViewer::Reader::construct<sparta::pipeViewer::DumpCallback>(db_path,
                                                                                          merge_transactions,
                                                                                          sort_by_end_time,
                                                                                          output_buffer);

    // Get data
    reader.getWindow(reader.getCycleFirst(), reader.getCycleLast());

    // If we're sorting by transaction ID, then we need to flush the buffer
    // In non-merging mode, sorting by transaction ID and end time should be identical
    if(merge_transactions && !sort_by_end_time)
    {
        std::cout << output_buffer.str();
    }

    std::cout << "range: [" << reader.getCycleFirst() << ", " << reader.getCycleLast() << "]" << std::endl;

    // Check indices
    std::cout << "Checking indices:" << std::endl;
    reader.dumpIndexTransactions();

}
