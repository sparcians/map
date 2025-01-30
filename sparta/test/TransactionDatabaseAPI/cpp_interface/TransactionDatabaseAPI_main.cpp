
#include <inttypes.h>
#include <iostream>

// Required to enable std::this_thread::sleep_for
#ifndef _GLIBCXX_USE_NANOSLEEP
#define _GLIBCXX_USE_NANOSLEEP
#endif

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/Tree.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Utils.hpp"

/*!
 * \file TransactionDatabaseAPI_main.cpp
 * \brief Test for reading from pipeout transaction database
 */

TEST_INIT

#define QUERIES_PER_SEC(num, boost_timer) ((num)/(boost_timer.elapsed().user/1000000000.0))
#define SEC_PER_QUERY(num, boost_timer) ((boost_timer.elapsed().user/1000000000.0)/float(num))

using sparta::pipeViewer::TransactionDatabaseInterface;

/*!
 * \brief Helper for handling query responses
 */
class QueryResponse
{
public:
    uint64_t hits;
    uint64_t empty;
    uint64_t occupied;

    bool print;

    // TransactionDatabaseInterface::callback_fxn
    static void tickDataHandler(void* obj,
                                uint64_t tick,
                                //TransactionDatabaseInterface::Transaction const * const * content,
                                TransactionDatabaseInterface::const_interval_idx * content,
                                const TransactionDatabaseInterface::Transaction * transactions,
                                uint32_t content_len) {
        auto qr = static_cast<QueryResponse*>(obj);
        qr->gotTickData(tick, content, transactions, content_len);
    }

    QueryResponse() :
        hits(0),
        empty(0),
        occupied(0),
        print(false)
    {;}

    void reset() {
        hits = empty = occupied = 0;
    }

    // TransactionDatabaseInterface::callback_fxn without first argument
    void gotTickData(uint64_t tick,
                     //TransactionDatabaseInterface::Transaction const * const * content, // indexed by location
                     TransactionDatabaseInterface::const_interval_idx * content,
                     const TransactionDatabaseInterface::Transaction * transactions,
                     uint32_t content_len)
    {
        (void) tick;
        (void) content;
        (void) transactions;
        (void) content_len;
        if(!print){
            //for(auto i = 0ul; i < content_len; ++i){
            //    const TransactionDatabaseInterface::Transaction* ti = content[i];
            //    if(nullptr == ti){
            //        ++empty;
            //    }else{
            //        ++occupied;
            //    }
            //}
            uint32_t num_inspected = std::min<uint32_t>(200ul, content_len);
            for(auto i = 0ul; i < num_inspected; ++i){
                const TransactionDatabaseInterface::const_interval_idx ti = content[i];
                if(TransactionDatabaseInterface::NO_TRANSACTION == ti){
                    ++empty;
                }else{
                    ++occupied;
                }
            }
        }else{
            std::cout << std::setw(6) << tick << ": ";
            // Print all transaction ids in a row
            for(auto i = 0ul; i < content_len; ++i){
                const TransactionDatabaseInterface::const_interval_idx ti = content[i];
                if(TransactionDatabaseInterface::NO_TRANSACTION == ti){
                    std::cout << "---- ";
                    ++empty;
                }else{
                    //std::cout << std::setw(4) << ti->transaction_ID % 10000 << ' ';
                    std::cout << std::setw(4) << transactions[ti].transaction_ID % 10000 << ' ';
                    ++occupied;
                }
            }
            std::cout << std::endl;
        }

        ++hits;
    }
};

void query(TransactionDatabaseInterface& db, QueryResponse& qr,
           uint64_t start_inc, uint64_t end_inc, uint32_t num_queries){
    boost::timer::cpu_timer t;
    qr.reset();
    t.start();
    std::cout << "query [" << start_inc << "," << end_inc << "] x " << num_queries << std::endl;
    for(uint32_t n = 0; n < num_queries; ++n){
        db.query(start_inc, end_inc, QueryResponse::tickDataHandler, &qr);
    }
    std::cout << "  " << QUERIES_PER_SEC(num_queries, t) << " qps, " << SEC_PER_QUERY(num_queries, t) << std::endl;
    std::cout << "  " << qr.hits << " cycles, " << qr.empty << " empty, " << qr.occupied << " occupied\n";
    std::cout << "  " << db.stringize() << std::endl;
    db.writeNodeStates(std::cout) << std::endl;
}

int main(int argc, char** argv)
{
    std::string dbname = "../data_test_0";
    uint32_t num_locs = 1000;
    uint64_t print_start = 0;
    uint64_t print_stop = 0;

    if(argc > 1){
        dbname = argv[1];
        if(argc > 2){
            num_locs = atoi(argv[2]);
            if(argc > 3){
                if(argc > 4){
                    char* end;
                    print_start = strtoull(argv[3], &end, 10);
                    print_stop = strtoull(argv[4], &end, 10);
                }else{
                    std::cerr << "Arguments 3 and 4 (print start tick, print stop tick) are both "
                        "required if one is specified" << std::endl;
                    return 1;
                }
            }
        }
    }

    std::cout << "db: \"" << dbname << "\", num_locs: " << num_locs << std::endl;

    TransactionDatabaseInterface db(dbname, // db
                                    num_locs); // delibarately fewer slots than transactions

    std::cout << "File: [" << db.getFileStart() << ", " << db.getFileEnd() << ')' << std::endl;

    QueryResponse qr; // Query response handler
    boost::timer::cpu_timer t;
    //double dt;

    const uint32_t NUM_QUERIES = 5000;

    query(db, qr, 0,    100, 1);
    query(db, qr, 0,    100, NUM_QUERIES);
    query(db, qr, 500,  600, 1);
    query(db, qr, 500,  600, NUM_QUERIES);
    query(db, qr, 0,    700, 1);;
    query(db, qr, 0,    700, NUM_QUERIES/2);
    query(db, qr, 200,  3760, 1);
    query(db, qr, 200,  3760, NUM_QUERIES/4);
    query(db, qr, 2999, 4000, 1);
    query(db, qr, 2999, 4000, NUM_QUERIES/4);
    query(db, qr, 6000, 6300, 1);
    query(db, qr, 6000, 6300, NUM_QUERIES/4);
    //qr.print = true;
    //query(db, qr, 2999, 4000, 1);

    // print if enabled
    if(print_stop > 0){
        qr.print = true;
        query(db, qr, print_start, print_stop, 1);
    }

    REPORT_ERROR;

    return ERROR_CODE;
}
