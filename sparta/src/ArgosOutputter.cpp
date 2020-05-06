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
namespace sparta{
    namespace pipeViewer{
        constexpr uint32_t Outputter::FILE_VERSION;
        Outputter::Outputter(const std::string& filepath, const uint64_t interval) :
            record_file_(),
            index_file_(),
            map_file_(),
            data_file_(),
            string_file_(),
            display_format_file_(),
            last_record_pos_(0){
            record_file_.open(std::string(filepath + "record.bin"), std::fstream::out | std::fstream::binary);
            index_file_.open( std::string(filepath + "index.bin"), std::fstream::out | std::fstream::binary);
            map_file_.open(std::string(filepath + "map.dat"), std::ios::out);
            data_file_.open(std::string(filepath + "data.dat"), std::ios::out);
            string_file_.open(std::string(filepath + "string_map.dat"), std::ios::out);
            display_format_file_.open(std::string(filepath + "display_format.dat"), std::ios::out);
            // Make sure the files opened correctly!
            if(!index_file_.is_open() || !record_file_.is_open()){
                throw sparta::SpartaException("Failed to open the path to write pipeline collection files."
                                          " It may be possible that the directory does not exist."
                                          " at filepath: FILEPATH+PREFIX=" + filepath);
            }
            // Throw on write failure
            record_file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
            index_file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
            // Write the index file version first. The index file should naturally skip this
            // Do not 0-pad the version number - it will be cast from a string to an int
            std::stringstream t;
            t << "sparta_pipeout_version:" << std::setw(4) << FILE_VERSION << "\n";
            const std::string header = t.str();
            sparta_assert(header.size() == 28); // Do not change the header format
            static_assert(sizeof(transaction_t) == 40,
                          "size of a transaction changed. May want to increase file format version. "
                          "If no changes were made to SPARTA, compiler is generating a structure "
                          "layout that is incompatible with pipeViewer");
            writeData_(index_file_, header.c_str(), header.size());
            // Notice that we write the interval offset first.
            writeData_(index_file_,reinterpret_cast<const char *>(&interval), sizeof(uint64_t));
            index_file_.flush();
        }
        Outputter::~Outputter(){
            //Write an index for the end of the record file, so that the last record
            //is always easily accessable reguardless of indexing.
            writeData_(index_file_, reinterpret_cast<const char *>(&last_record_pos_), sizeof(uint64_t));
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
            uint64_t current_record_pos = record_file_.tellg();
            writeData_(index_file_, reinterpret_cast<const char *>(&current_record_pos), sizeof(uint64_t));
            index_file_.flush();
        }
    }//namespace pipeViewer
}//namespace sparta
