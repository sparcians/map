// <Outputter> -*- C++ -*-

/**
 * \file  Outputter
 * \brief Outputs Transactions to record file and builds index file while running
 *
 */
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "sparta/pipeViewer/Outputter.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/pipeViewer/transaction_structures.hpp"

// #define PIPELINE_DBG 1
namespace sparta::pipeViewer
{
    Outputter::Outputter(const std::string& filepath, const uint64_t interval) :
        record_file_(filepath + "record.bin", std::fstream::out | std::fstream::binary),
        index_file_(filepath + "index.bin", std::fstream::out | std::fstream::binary),
        map_file_(filepath + "map.dat", std::ios::out),
        data_file_(filepath + "data.dat", std::ios::out),
        string_file_(filepath + "string_map.dat", std::ios::out),
        display_format_file_(filepath + "display_format.dat", std::ios::out),
        last_record_pos_(0)
    {
        // Make sure the files opened correctly!
        sparta_assert(index_file_.is_open() && record_file_.is_open(),
                      "Failed to open the path to write pipeline collection files."
                      " It may be possible that the directory does not exist."
                      " at filepath: FILEPATH+PREFIX=" << filepath);
        // Throw on write failure
        record_file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        index_file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        // Write the index file version first. The index file should naturally skip this
        // Do not 0-pad the version number - it will be cast from a string to an int
        std::ostringstream t;
        t << HEADER_PREFIX << std::setw(VERSION_LENGTH) << FILE_VERSION << '\n';
        const std::string header = t.str();
        sparta_assert(header.size() == HEADER_SIZE); // Do not change the header format
        static_assert(sizeof(transaction_t) == 48,
                      "size of a transaction changed. May want to increase file format version. "
                      "If no changes were made to SPARTA, compiler is generating a structure "
                      "layout that is incompatible with pipeViewer");
        writeData_(index_file_, header.data(), header.size());
        // Notice that we write the interval offset first.
        writeData_(index_file_, interval);
        index_file_.flush();
    }
    Outputter::~Outputter(){
        //Write an index for the end of the record file, so that the last record
        //is always easily accessable reguardless of indexing.
        writeData_(index_file_, last_record_pos_);
        //std::cout << "Writing Last Record Position " << last_record_pos_ << std::endl;
        //std::cout << "The Pipeline Collection is closing the output stream"
        // <<" this may take some time." << std::endl;
        index_file_.close();
        record_file_.close();
        map_file_.close();
        data_file_.close();
        string_file_.close();
        display_format_file_.close();
        std::cout << "The outputter is done destructing." << std::endl;
    }
    void Outputter::writeIndex(){
        const uint64_t current_record_pos = record_file_.tellp();
        writeData_(index_file_, current_record_pos);
        index_file_.flush();
    }
}//namespace sparta::pipeViewer
