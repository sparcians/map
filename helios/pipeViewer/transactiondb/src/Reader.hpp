// <Reader> -*- C++ -*-
/**
 * \file Reader
 *
 * \brief Reads transctions using the record and index file
 *
*/

#pragma once

#include <functional>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <sys/stat.h>

#include "PipelineDataCallback.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LexicalCast.hpp"
#include "sparta/pipeViewer/Outputter.hpp"

// #define READER_DBG 1
// #define READER_LOG 1

#if READER_DBG==1
#define READER_LOG 1
#endif

namespace sparta{
namespace pipeViewer{

    /*!
     * \brief Sanity checker for records. Used when doing a dump of the index
     * file to check that all transactions in the heartbeat actually belong
     * there
     */
    class RecordChecker : public PipelineDataCallback
    {
        uint64_t start;
        uint64_t end;
    public:
        RecordChecker(uint64_t _start, uint64_t _end) :
            start(_start),
            end(_end)
        {;}

        virtual void foundTransactionRecord(transaction_t* r) override {
            if(r->time_Start < start){
                  std::cout << "Bounds on transactions were outside of heartbeat range " << start
                            << "," << end << ". transaction: idx: " << r->transaction_ID << " loc: "
                            << r->location_ID << " start: "
                            << r->time_Start << " end: " << r->time_End
                            << " parent: " << r->parent_ID << std::endl;
            }
            if(r->time_End > end){
              std::cout << "Bounds on transactions were outside of heartbeat range " << start
                        << "," << end << ". transaction: idx: " << r->transaction_ID << " loc: "
                        << r->location_ID << " start: "
                        << r->time_Start << " end: " << r->time_End
                        << " parent: " << r->parent_ID << std::endl;
            }
        }

        virtual void foundInstRecord(instruction_t* r) override {
            foundTransactionRecord(r);
        }

        virtual void foundMemRecord(memoryoperation_t* r) override {
            foundTransactionRecord(r);
        }

        virtual void foundAnnotationRecord(annotation_t* r) override {
            foundTransactionRecord(r);
        }

        virtual void foundPairRecord(pair_t* r) override {
            foundTransactionRecord(r);
        }
    };

    // Memory Layout of pair_struct. We build a vector of such structs before we start reading record back from the database.
    // This structs help us by telling us exactly how may pair values
    // there are in the current pair type, what their names are and how much bytes each of the values occupy.
    // We inquire such a vector of structs by giving it a unique id and this vector gives
    // us such a struct where the unique id matches.
    struct pair_struct{
        uint16_t UniqueID;
        uint16_t length;
        std::vector<uint16_t> types;
        std::vector<uint16_t> sizes;
        std::string formatGuide;
        std::vector<std::string> names;
    };

    /*!
     * \class Reader
     * @ brief A class that facilitates reading transactions from disk that
     * end in a given interval measured in cycles.
     *
     * The Reader will return the records found on disk by calling
     * methods in PipelineDataCallback, passing pointers to the read
     * transactions.
     */
    class Reader
    {
    private:
        /**
         * \brief return the correct position in the record file that corresponds
         * to the start cycle.
         * \param start The cycle number to start reading records from.
         */
        uint64_t findRecordReadPos_(uint64_t start)
        {
            //Figure out how far we need to seek into our index file.
            uint64_t step = first_index_ + (start/heartbeat_ * sizeof(uint64_t));

            //Now read the pointer from this location.
            if(!index_file_.seekg(step)) {
                sparta_assert(!"Could not seekg in for the given position.  Please report bug");
            }
            uint64_t pos = 0;

            //It might be the case that our index file is too small
            //to represent an end time that the user is requesting.
            //in the above algorythm either reached the end of the index file.

            //notice we look too see if the index file seeker has passed
            //size_of_index_file_ - 8 bytes b/c a special last index is written to the index file
            //to point to only the start of the last transaction.
            int64_t filepos = index_file_.tellg();
            if(filepos >= size_of_index_file_ - 8 || filepos == -1)
            {
                //We need to reset end of file reach flags for the index file.
                index_file_.clear();
                pos = size_of_record_file_;
            }
            else
            {
                index_file_.read((char*)&pos, sizeof(uint64_t));
            }
            return pos;
        }

        /**
         * \brief A helper method to round values up to the
         * interval values.
         * example 4600 rounds to 5000 when the interval is "1000"
         */
        uint64_t roundUp_(uint64_t num)
        {
            int remainder = num % heartbeat_;
            if(remainder == 0)
                return num;
            return num + heartbeat_ - remainder;
        }

        /**
         * \brief Return the start time in the file.
         */
        uint64_t findCycleFirst_()
        {
            //Make sure the user is not abusing our NON thread safe method.
            sparta_assert(!lock, "This reader class is not thread safe, and this method cannot be called"" from multiple threads.");
            lock = true;
            record_file_.seekg(0, std::ios::beg);
            transaction_t transaction;
            record_file_.read(reinterpret_cast<char*>(&transaction), sizeof(transaction_t));
            lock = false;
            return transaction.time_Start;
        }
        /**
         * \brief Return the last end time in the file.
         * Our output saved the last index to point to
         * the start of last record.
         */
        uint64_t findCycleLast_()
        {
            //Make sure the user is not abusing our NON thread safe method.
            sparta_assert(!lock, "This reader class is not thread safe, and this method cannot be called"" from multiple threads.");
            lock = true;
            //Notice that we reset the index file, b/c if
            //we had already read to the end of the file, we need to reset
            //end of file flags.
            index_file_.clear();
            //seek one entry back from the end of the index file
            index_file_.seekg(size_of_index_file_-sizeof(uint64_t));
            uint64_t pos = 0;
            index_file_.read((char*)&pos, sizeof(uint64_t));
            //read the transaction at the appropriate location.
            record_file_.seekg(pos, std::ios::beg);
            transaction_t transaction;
            record_file_.read(reinterpret_cast<char*>(&transaction), sizeof(transaction_t));
            lock = false;
            if(record_file_.gcount() != sizeof(transaction_t))
            {
                return highest_cycle_;
            }
            return transaction.time_End - 1;

        }


        /*!
         * \brief Read a record of any format. Older formats are upconverted to new format.
         */
         void readRecord_(int64_t& pos, uint64_t start, uint64_t end) {
            if(version_ == 1){
                readRecord_v1_(pos, start, end);
            }else if(version_ == 2){
                readRecord_v2_(pos, start, end);
            }else{
                throw SpartaException("This pipeViewer reader library does not know how to read a record "
                                    " for version ") << version_ << " file " << filepath_;
            }
        }

        /*!
         * \brief Implementation of readRecord_ function which accepts the version 1 format)
         */
       void readRecord_v1_(int64_t& pos, uint64_t start, uint64_t end) {
            const uint32_t MAX_ANNT_LEN = 16384;
            const uint32_t ANNT_BUF_SIZE = MAX_ANNT_LEN + 1;
            char annt_buf[ANNT_BUF_SIZE];
            annt_buf[0] = '\0';

            version1::transaction_t transaction;
            record_file_.read(reinterpret_cast<char*>(&transaction), sizeof(version1::transaction_t));
            pos += sizeof(version1::transaction_t);

            if((transaction.flags & TYPE_MASK) == is_Annotation){
                version1::annotation_t annot;
                memcpy(&annot, &transaction, sizeof(version1::transaction_t));
                record_file_.read(reinterpret_cast<char*>(&annot.length), sizeof(uint16_t));
                pos += sizeof(uint16_t);
                annot.annt = annt_buf;
                if(annot.length > MAX_ANNT_LEN){
                    // Truncate because this giant annotation exceeds buffer.
                    // An alternative would be replace annt_buf with a heap variable and
                    // enlarge it each time a larger annotation was encountered.
                    record_file_.read(annt_buf, MAX_ANNT_LEN); // Read max portion of annotaiton
                    pos += MAX_ANNT_LEN;
                    std::cerr << "Had to truncate annotation " << annot.transaction_ID
                              << " starting at " << annot.time_Start << " of length " << annot.length
                              << " to " << MAX_ANNT_LEN << " because it exceeded buffer size." << std::endl
                              << " ANNOTATION:\n" << std::string(annt_buf) << std::endl;
                    record_file_.seekg(annot.length - MAX_ANNT_LEN, std::ios::cur); // Skip rest
                    pos += annot.length - MAX_ANNT_LEN;
                    annot.length = MAX_ANNT_LEN;
                }else{
                    record_file_.read(annt_buf, annot.length);
                    pos += annot.length;
                }

                // Only send along transaction in the query range
                if(transaction.time_End < start || transaction.time_Start > end){
#if READER_DBG
                    // Skip transactions by not sending them along to the callback.
                    // read is faster than seekg aparently. This DOES help performance
                    std::cerr << "READER: skipped transaction outside of window [" << start << ", "
                              << end << "). start: " << transaction.time_Start << " end: "
                              << transaction.time_End
                              << " parent: " << transaction.parent_ID << std::endl;
#endif
                }else{
#ifdef READER_DBG
                    std::cout << "READER: found annt. " << "loc: " << annot.location_ID << " start: "
                              << annot.time_Start << " end: " << annot.time_End
                              << " parent: " << annot.parent_ID << std::endl;
#endif
                    annotation_t new_anno(std::move(annot));
                    data_callback_->foundAnnotationRecord(&new_anno);
                }
                return;
            }else if((transaction.flags & TYPE_MASK) == is_Instruction){
                record_file_.seekg(-sizeof(version1::transaction_t), std::ios::cur);
                pos -= sizeof(version1::transaction_t);
                version1::instruction_t inst;
                record_file_.read(reinterpret_cast<char*>(&inst), sizeof(version1::instruction_t));
                pos += sizeof(version1::instruction_t);
#ifdef READER_DBG
                std::cout << "READER: found inst. start: " << inst.time_Start << " end: " << inst.time_End << std::endl;
#endif
                instruction_t new_inst(std::move(inst));
                data_callback_->foundInstRecord(&new_inst);
                return;
            }else if((transaction.flags & TYPE_MASK) == is_MemoryOperation){
                record_file_.seekg(-sizeof(version1::transaction_t), std::ios::cur);
                pos -= sizeof(version1::transaction_t);
                version1::memoryoperation_t memop;
                record_file_.read(reinterpret_cast<char*>(&memop), sizeof(version1::memoryoperation_t));
                pos += sizeof(version1::memoryoperation_t);
#ifdef READER_DBG
                std::cout << "READER: found inst. start: " << memop.time_Start << " end: " << memop.time_End << std::endl;
#endif
                memoryoperation_t new_memop(std::move(memop));
                data_callback_->foundMemRecord(&new_memop);
                return;
            }else{
                throw sparta::SpartaException("An unidentifiable transaction type found in this record."
                                          " It is possible the data may be corrupt.");
            }
        }

        /**
         * \brief Read a single record at \a pos and increment pos
         */
        void readRecord_v2_(int64_t& pos, uint64_t start, uint64_t end) {
            const uint32_t MAX_ANNT_LEN = 16384;
            const uint32_t ANNT_BUF_SIZE = MAX_ANNT_LEN + 1;
            char annt_buf[ANNT_BUF_SIZE];
            annt_buf[0] = '\0';

            transaction_t transaction;
            record_file_.read(reinterpret_cast<char*>(&transaction), sizeof(transaction_t));
            pos += sizeof(transaction_t);

            // some struct to populate the record too.
            annotation_t annot;
            instruction_t inst;
            memoryoperation_t memop;
            pair_t pairt;

            switch (transaction.flags & TYPE_MASK)
            {
            case is_Annotation :
                {

                    memcpy(&annot, &transaction, sizeof(transaction_t));
                    record_file_.read(reinterpret_cast<char*>(&annot.length), sizeof(uint16_t));
                    pos += sizeof(uint16_t);
                    annot.annt = annt_buf;
                    if(annot.length > MAX_ANNT_LEN){
                        // Truncate because this giant annotation exceeds buffer.
                        // An alternative would be replace annt_buf with a heap variable and
                        // enlarge it each time a larger annotation was encountered.
                        record_file_.read(annt_buf, MAX_ANNT_LEN); // Read max portion of annotaiton
                        pos += MAX_ANNT_LEN;
                        std::cerr << "Had to truncate annotation " << annot.transaction_ID
                                  << " starting at " << annot.time_Start << " of length " << annot.length
                                  << " to " << MAX_ANNT_LEN << " because it exceeded buffer size." << std::endl
                                  << " ANNOTATION:\n" << std::string(annt_buf) << std::endl;
                        record_file_.seekg(annot.length - MAX_ANNT_LEN, std::ios::cur); // Skip rest
                        pos += annot.length - MAX_ANNT_LEN;
                        annot.length = MAX_ANNT_LEN;
                    }else{
                        record_file_.read(annt_buf, annot.length);
                        pos += annot.length;
                    }

                    (void) start;
                    //// Sanity check the transactions coming out
                    //if(start % heartbeat_ == 0 // Only try this sanity checking if start is a multiple of heartbeat_
                    //   && transaction.time_Start < start // Got a transaction starting before the first heartbeat containing this query
                    //   && transaction.time_End > transaction.time_Start){ // ignore 0-length transactions
                    //    std::cout << "Found a transaction where start < query-start && end <= query-start: ("
                    //              << transaction.time_Start <<  " < " << start << ")"
                    //              << " && " << transaction.time_End <<  "<= " << start << ") : \""
                    //              << annot.annt << "\""
                    //              << std::endl;
                    //}

                    (void) end;
                    // Only send along transaction in the query range
                    if(transaction.time_End < start || transaction.time_Start > end){
                        #if READER_DBG
                            // Skip transactions by not sending them along to the callback.
                            // read is faster than seekg aparently. This DOES help performance
                            std::cerr << "READER: skipped transaction outside of window [" << start << ", "
                                  << end << "). start: " << transaction.time_Start << " end: "
                                  << transaction.time_End
                                  << " parent: " << transaction.parent_ID << std::endl;
                        #endif
                    }else{
                        #ifdef READER_DBG
                            std::cout << "READER: found annt. " << "loc: " << annot.location_ID << " start: "
                                  << annot.time_Start << " end: " << annot.time_End
                                  << " parent: " << annot.parent_ID << std::endl;
                        #endif
                        data_callback_->foundAnnotationRecord(&annot);
                    }
                } break;

            case is_Instruction:
                {
                    record_file_.seekg(-sizeof(transaction_t), std::ios::cur);
                    pos -= sizeof(transaction_t);

                    record_file_.read(reinterpret_cast<char*>(&inst), sizeof(instruction_t));
                    pos += sizeof(instruction_t);
                    #ifdef READER_DBG
                        std::cout << "READER: found inst. start: " << inst.time_Start << " end: " << inst.time_End << std::endl;
                    #endif
                    data_callback_->foundInstRecord(&inst);
                } break;

            case is_MemoryOperation:
                {
                    record_file_.seekg(-sizeof(transaction_t), std::ios::cur);
                    pos -= sizeof(transaction_t);

                    record_file_.read(reinterpret_cast<char*>(&memop), sizeof(memoryoperation_t));
                    pos += sizeof(memoryoperation_t);
                    #ifdef READER_DBG
                        std::cout << "READER: found inst. start: " << memop.time_Start << " end: " << memop.time_End << std::endl;
                    #endif
                    data_callback_->foundMemRecord(&memop);
                } break;

            // If we have found a record which is of Pair Type,
            // then we enter into this switch case
            // which contains the logic to read back records of
            // pair type using record file, map file
            // and In-memory data structures and rebuild the pair
            // record one by one.
            case is_Pair : {

                // First, we do a simple memcpy of the generic transaction structure
                // we read into our pair structure
                memcpy(&pairt, &transaction, sizeof(transaction_t));

                // We grab the location Id of this record we are about to read from
                // the generic transaction structure
                unsigned long long location_ID = transaction.location_ID;
                std::string delim = ":";

                // The loc_map is an In-memory Map which contains a mapping
                // of Location ID to Pair ID.
                // We are going to use this map to lookup the Location ID we
                // just read from this current record
                // and find out the pair id of such a record
                std::string token = loc_map_[std::to_string(location_ID)];

                // We are now going to use the In-memory data structure we built
                //during the Reader construction.
                // This Data Structure contains information about the name strings
                // and their sizeof data
                // for every different type of pair we have collected.
                // So, we retrieve records from this structure one by one,
                // till we find that the pair id of the record we just read
                // from the record file
                // matches the pair id of the current record we retrieved from
                // the In-memory data Structure.
                for(auto & st : map_){
                    if(st.UniqueID == std::stoull(token)){

                        // We lookup the length, the name strings and the sizeofs of
                        // every anme string from the retrieved
                        // record of the Data Struture and copy the values into out
                        // live Pair Transaction record
                        pairt.length = st.length;
                        std::string token = "#";
                        std::string guideString = st.formatGuide;
                        while(guideString.size()){
                            if(guideString.find(token) != std::string::npos){
                                uint16_t index = guideString.find(token);
                                pairt.delimVector.emplace_back(guideString.substr(0, index));
                                guideString = guideString.substr(index + token.size());
                                if(guideString.empty()){
                                    pairt.delimVector.emplace_back(guideString);
                                }
                            }
                            else{
                                pairt.delimVector.emplace_back(guideString);
                                guideString = "";
                            }
                        }

                        pairt.nameVector.reserve(pairt.length);
                        pairt.valueVector.reserve(pairt.length);
                        pairt.sizeOfVector.reserve(pairt.length);
                        pairt.stringVector.reserve(pairt.length);
                        pairt.nameVector.emplace_back("pairid");
                        pairt.valueVector.emplace_back(std::make_pair(st.UniqueID, false));
                        pairt.sizeOfVector.emplace_back(sizeof(uint16_t));
                        pairt.stringVector.emplace_back(" ");

                        for(std::size_t i = 1; i != st.length; ++i){
                            pairt.nameVector.emplace_back(st.names[i]);
                            pairt.sizeOfVector.emplace_back(st.sizes[i]);
                            if(st.types[i] == 0){
                                switch(pairt.sizeOfVector[i]){
                                    case sizeof(uint8_t) : {
                                        uint8_t tmp;

                                        // We read exactly the number of bytes
                                        // this value occupies from the database.
                                        // This is a crucial step else if we read
                                        // wrong number of bytes, our read procedure
                                        // will crash in near future.
                                        record_file_.read(reinterpret_cast<char*>(&tmp), sizeof(uint8_t));
                                        pos += sizeof(uint8_t);
                                        pairt.valueVector.emplace_back(std::make_pair(tmp, true));
                                    }break;
                                    case sizeof(uint16_t) : {
                                        uint16_t tmp;

                                        // We read exactly the number of bytes this
                                        // value occupies from the database.
                                        // This is a crucial step else if we read wrong
                                        // number of bytes, our read procedure will crash in near future.
                                        record_file_.read(reinterpret_cast<char*>(&tmp), sizeof(uint16_t));
                                        pos += sizeof(uint16_t);
                                        pairt.valueVector.emplace_back(std::make_pair(tmp, true));
                                    }break;
                                    case sizeof(uint32_t) : {
                                        uint32_t tmp;

                                        // We read exactly the number of bytes
                                        // this value occupies from the database.
                                        // This is a crucial step else if we read
                                        // wrong number of bytes, our read procedure
                                        // will crash in near future.
                                        record_file_.read(reinterpret_cast<char*>(&tmp), sizeof(uint32_t));
                                        pos += sizeof(uint32_t);
                                        pairt.valueVector.emplace_back(std::make_pair(tmp, true));
                                    }break;
                                    case sizeof(uint64_t) : {
                                        uint64_t tmp;

                                        // We read exactly the number of bytes
                                        // this value occupies from the database.
                                        // This is a crucial step else if we read
                                        // wrong number of bytes, our read procedure
                                        // will crash in near future.
                                        record_file_.read(reinterpret_cast<char*>(&tmp), sizeof(uint64_t));
                                        pos += sizeof(uint64_t);
                                        pairt.valueVector.emplace_back(std::make_pair(tmp, true));
                                    }break;
                                    default : {
                                    throw sparta::SpartaException(
                                        "Data Type not supported for reading/writing.");
                                    }
                                }

                                // Finally for a certain field "i", we check if there is a string
                                // representation for the integral value, by checking,
                                // if a key with current pair id, current field id, current integral value
                                // exists in the In-memory String Map. If yes, we grab the value
                                // and place it in the enum vector.
                                if(stringMap_.find(std::make_tuple(pairt.valueVector[0].first,
                                                                   i - 1,
                                                                   pairt.valueVector[i].first)) !=
                                                                   stringMap_.end()){
                                    pairt.stringVector.emplace_back(stringMap_[std::make_tuple(
                                                                               pairt.valueVector[0].first,
                                                                               i - 1,
                                                                               pairt.valueVector[i].first)]);
                                    pairt.valueVector[i].second = false;
                                }

                                // Else, we push empty string in the enum vector at this index.
                                else{
                                    pairt.stringVector.emplace_back(std::to_string(pairt.valueVector[i].first));
                                }
                            }
                            else if(st.types[i] == 1){
                                uint16_t annotationLength;
                                record_file_.read(reinterpret_cast<char*>(&annotationLength), sizeof(uint16_t));
                                pos += sizeof(uint16_t);
                                std::unique_ptr<char[] , std::function<void(char * ptr)>>
                                    annot_ptr(new char[annotationLength + 1],
                                        [&](char * ptr) { delete [] ptr; });
                                record_file_.read(annot_ptr.get(), annotationLength);
                                pos += annotationLength;
                                std::string annot_str(annot_ptr.get(), annotationLength);
                                pairt.stringVector.emplace_back(annot_str);
                                pairt.valueVector.emplace_back(std::make_pair(std::numeric_limits<uint64_t>::max(), false));
                                // This bool value describes if this field has a string-only value.
                                // String only values are those values which are stored in database as
                                // strings and has no integral representation of itself.
                                const bool string_only_field = true;
                                pairt.valueVector.emplace_back(std::make_pair(
                                    std::numeric_limits<
                                        typename decltype(pairt.valueVector)::value_type::first_type>::max(),
                                    string_only_field));
                            }
                        }
                        break;
                    }
                }
                #ifdef READER_DBG
                    std::cout << "READER: found pair. start: " << pairt.time_Start << " end: " << pairt.time_End << std::endl;
                #endif
                data_callback_->foundPairRecord(&pairt);
            }
            break;

            default:
                {
                    throw sparta::SpartaException("Unknown Transaction Found. Data might be corrupt.");
                }
            }
        }

        void reopenFileStream_(std::fstream & fs, const std::string & filename, std::ios::openmode mode = std::ios::in)
        {
            size_t cur_pos = fs.tellg();
            fs.close();
            fs.clear();
            fs.open(filename, mode);
            fs.seekg(cur_pos);
        }

        void checkIndexUpdates_()
        {
            struct stat index_stat_result, record_stat_result;

            stat(index_file_path_.c_str(), &index_stat_result);

            stat(record_file_path_.c_str(), &record_stat_result);

            if(index_stat_result.st_size != size_of_index_file_ && record_stat_result.st_size != size_of_record_file_)
            {
                int64_t record_remainder = record_stat_result.st_size % heartbeat_;

                if(record_stat_result.st_size - record_remainder == size_of_record_file_)
                {
                    return;
                }

                reopenFileStream_(record_file_, record_file_path_, std::ios::in | std::ios::binary);
                reopenFileStream_(index_file_, index_file_path_, std::ios::in | std::ios::binary);
                reopenFileStream_(map_file_, map_file_path_, std::ios::in);
                reopenFileStream_(data_file_, data_file_path_, std::ios::in);

                size_of_index_file_ = index_stat_result.st_size;

                if(record_remainder != 0)
                {
                    size_of_record_file_ = record_stat_result.st_size - record_remainder;
                }
                else
                {
                    size_of_record_file_ = record_stat_result.st_size;
                }

                highest_cycle_ = findCycleLast_();

                file_updated_ = true;
            }
        }
    public:
        /**
         * \brief Construct a Reader
         * \param filename the name of the record file
         * \param cd a pointer to for the PipelineDataCallback to use.
         */
        Reader(std::string filepath, PipelineDataCallback* cb) :
            filepath_(filepath),
            record_file_path_(std::string(filepath + "record.bin")),
            index_file_path_(std::string(filepath + "index.bin")),
            map_file_path_(std::string(filepath + "map.dat")),
            data_file_path_(std::string(filepath + "data.dat")),
            string_file_path_(std::string(filepath + "string_map.dat")),
            display_file_path_(std::string(filepath + "display_format.dat")),
            record_file_(record_file_path_, std::fstream::in | std::fstream::binary ),
            index_file_(index_file_path_, std::fstream::in | std::fstream::binary),
            map_file_(map_file_path_, std::fstream::in),
            data_file_(data_file_path_, std::fstream::in),
            string_file_(string_file_path_, std::fstream::in),
            display_file_(display_file_path_, std::fstream::in),
            data_callback_(cb),
            size_of_index_file_(0),
            size_of_record_file_(0),
            lowest_cycle_(0),
            highest_cycle_(0),
            lock(false),
            file_updated_(false),
            loc_map_(),
            map_(),
            stringMap_()
        {
            sparta_assert(data_callback_);

            //Make sure the file opened correctly!
            if(!record_file_.is_open())
            {
                throw sparta::SpartaException("Failed to open file, "+filepath+"record.bin");
            }
            if(!index_file_.is_open())
            {
                throw sparta::SpartaException("Failed to open file, "+filepath+"index.bin");
            }

            #ifdef READER_LOG
            std::cout << "READER: pipeViewer reader opened: " << filepath << "record.bin" << std::endl;
            #endif

            // Read header from index file
            char header_buf[64];
            // Note that this is not shared with pipeViewer::Outputter since it must
            // remain even if the Outputter is changed
            const std::string EXPECTED_HEADER_PREFIX = "sparta_pipeout_version:";
            const std::size_t HEADER_SIZE = EXPECTED_HEADER_PREFIX.size() + 4 + 1; // prefix + number + newline
            index_file_.read(header_buf, HEADER_SIZE);
            // Assuming older version until header proves otherwise
            version_ = 1;
            if(index_file_.gcount() != (int)HEADER_SIZE){
                // Assume old version because the file is too small to have a header
                index_file_.clear(); // Clear error flags after failing to read enough data
                index_file_.seekg(0, std::ios::beg); // Restore
            }if(strncmp(header_buf, EXPECTED_HEADER_PREFIX.c_str(), EXPECTED_HEADER_PREFIX.size())){
                // Header prefix did not match. Assume old version
                index_file_.seekg(0, std::ios::beg); // Restore
            }else{
                // Header prefix matched. Read version
                *(header_buf + HEADER_SIZE - 1) = '\0'; // Insert null
                version_ = sparta::lexicalCast<decltype(version_)>(header_buf+EXPECTED_HEADER_PREFIX.size());
            }
            sparta_assert(version_ > 0 && version_ <= Outputter::FILE_VERSION,
                        "pipeout file " << filepath << " determined to be format "
                        << version_ << " which is not known by this version of SPARTA. Version "
                        "expected to be in range [1, " << Outputter::FILE_VERSION << "]");
            sparta_assert(index_file_.good(),
                        "Finished reading index file header for " << filepath << " but "
                        "ended up with non-good file handle somehow. This is a bug in the "
                        "header-reading logic");

            // Read the heartbeat size from our index file.
            // This will be the first integer in the file except for the header if there is one
            // Warning: this must be called while the index_file_ is already seeked to the
            // start of file, which it is after opening.
            index_file_.read((char*)&heartbeat_, sizeof(uint64_t));

            // Save the first index entry position
            first_index_ = index_file_.tellg();

            #ifdef READER_LOG
            std::cout << "READER: Heartbeat is: " << heartbeat_ << std::endl;
            #endif
            sparta_assert(heartbeat_ != 0,
                        "Pipeout database \"" << filepath << "\" had a heartbeat of 0. This "
                        "would be too slow to actually load");

            //Determine the size of our index file
            index_file_.seekg(0, std::fstream::end);
            size_of_index_file_ = index_file_.tellg();
            //Determine the size of our record file.
            record_file_.seekg(0, std::ios::end);
            size_of_record_file_ = record_file_.tellg();

            //cache the earliest start and stop of the record file
            lowest_cycle_ = findCycleFirst_();
            highest_cycle_ = findCycleLast_();

            std::string delim = ":";

            // Building the In-Memory LocationID -> PairID Lookup structure
            // from the map_file_ which contains the same.
            // We read this map_file_ just once during the construction
            // of the Reader Class and store all the relationships
            // in an unordered_map called loc_map. When we read each Pair Record,
            // we quickly do a lookup with the Location ID
            // of the record in this map, and retrieve the Pair ID of that record,
            // so that we can go ahead and get all the information
            // about its name strings and sizeof and pair length from other data structures.
            //The fields in the file are separated by ":"

            // Read throught the whole map File
            while(map_file_){
                std::string Data_String;

                // Read a line from the file, and if we cannot find a line,
                // that means we have read the whole file
                if(!getline(map_file_, Data_String)){
                    break;
                }

                //Parse the string, one delimiter at a time.
                size_t last = 0;
                size_t next = 0;
                next = Data_String.find(delim, last);

                // We know the format of the map file is always like,
                // A:B: so we know we can iterate twice over the
                // delimiters in every single line and retrieve the individual values as strings
                std::string key_ = Data_String.substr(last, next - last);
                last = next + 1;
                next = Data_String.find(delim, last);
                std::string val_ = Data_String.substr(last, next - last);

                // Once we have got the kep string and value string,
                // we immediately put the pair in the Map.
                // We don't even have to check for duplicate pairs as
                // there never will be a duplicate strings,
                // because, in the Outputter.h we build the Map file
                // with only unique Location IDs.
                loc_map_.insert(std::make_pair(key_, val_));
                last = next + 1;
            }

            // Building the In-memory Pair Lookup structure, such that,
            // in future when reading back record from transaction file,
            // we can use this structure to know about the length, name strings
            // and sizeof the values
            // for that paritcular pair, instead of using a file on disk for this.
            // We read through the Data file just once, and populate this structure.
            //The fields in the file are separated by ":"

            //Read through the whole Data File
            while(data_file_){
                pair_struct pStruct;
                std::string Data_String;

                // Read a line from the file, and if we cannot find a line,
                // that means we have read the whole file
                if(!getline(data_file_, Data_String)){
                    break;
                }

                //Parse the string, one delimiter at a time.
                size_t last = 0;
                size_t next = 0;
                next = Data_String.find(delim, last);

                //We know the first value in such a string is always the Pair ID
                pStruct.UniqueID = std::stoull(Data_String.substr(last, next - last));
                last = next + 1;
                next = Data_String.find(delim, last);

                // We know the second value in such a string
                // is always the Length of this particular Pair.
                pStruct.length = std::stoull(Data_String.substr(last, next - last)) + 1;
                last = next + 1;

                // The first pair value is the pair-id. We can make up any name
                // for the name string for this pair.
                pStruct.names.push_back("pairid");
                pStruct.sizes.push_back(sizeof(uint16_t));
                pStruct.types.emplace_back(0);

                // Iterate through the rest of the delimiters in the stringline
                // and build up the Name Strings and the Sizeof values for every field.
                while((next = Data_String.find(delim, last)) != std::string::npos){
                    pStruct.names.emplace_back(Data_String.substr(last, next - last));
                    last = next + 1;
                    next = Data_String.find(delim, last);
                    pStruct.sizes.emplace_back(std::stoull(Data_String.substr(last, next - last)));
                    last = next + 1;
                    next = Data_String.find(delim, last);
                    pStruct.types.emplace_back(std::stoull(Data_String.substr(last, next - last)));
                    last = next + 1;
                }

                //Finally, when we have completely parsed one line of this file,
                // it means we have complete knowledge of one pair type.
                // We then insert this pair into out Lookup structure.
                map_.emplace_back(pStruct);
            }

            while(display_file_){
                std::string dataString;
                if(!getline(display_file_, dataString)){
                    break;
                }
                size_t last = 0, next = 0;
                next = dataString.find(delim, last);
                uint16_t pairId = std::stoull(dataString.substr(last, next - last));
                last = next + 1;
                next = dataString.find("\n", last);
                for(auto& st : map_){
                    if(st.UniqueID == pairId){
                        st.formatGuide = dataString.substr(last, next - last);
                    }
                }
            }

            // Now, we are the In-memory Structure that will help us perform
            // a lookup to find out the Actual String Representation from an integer.
            // When a modeler has methods which actually return strings like
            // instruction opcodes like "and", "str", "adddl" or mmu state like "ready", "not ready" etc.,
            // we need to display exactly this kind of string back in the pipeViewer Viewer.
            // But storing such strings in Database
            // takes up a lot of space and also, writing strings to a binary file is time intensive.
            // So, during writing records to the Database,
            // we created and used a Map which stores as Key a Tuple of 3 integers where the
            // first integer represented the Pair Id of the record, for example,
            // which pair does this record belong to?
            // Is it ExampleInst or MemoryAccessInfo or LoadStoreInstInfo.
            // The second integer represented which field of that
            // pair record this value belonged to, for example, the third field or
            // the fourth field and finally, the third and the last field,
            // was the actual unique value for that string. For example, a tuple key of 1,2,3
            // -> "not ready" in the map means that the value 3 in the 2nd field of the
            // 1st type of Pair actually is represented by the string "not ready".
            // By using this structure, we will populate the enum vector of our pair
            // records and send the record back to the pipeViewer side, so that we can
            // represent such strings in the pipeViewer Viewer.
            // Read every line of the String Map file
            while(string_file_){
                std::string Data_String;

                // If we do not find a string line, that means we have read all
                // the lines and have reached the end of the file
                if(!getline(string_file_, Data_String)){
                    break;
                }

                // We do not need to iterate throuogh this because we know exactly
                // how many delimiters there would be in every line of this file
                size_t last = 0;
                size_t next = 0;
                next = Data_String.find(delim, last);

                //The first value before the delimiter would be our pair ID
                uint64_t pid = std::stoull(Data_String.substr(last, next - last));
                last = next + 1;
                next = Data_String.find(delim, last);

                // The first value before the second delimiter would be our Field ID
                uint64_t fid = std::stoull(Data_String.substr(last, next - last));
                last = next + 1;
                next = Data_String.find(delim, last);

                // The first value before the third delimiter would be our actual mapped value
                uint64_t fval = std::stoull(Data_String.substr(last, next - last));
                last = next + 1;
                next = Data_String.find(delim, last);

                // the value before the final delimiter would be a string value and we parse this string value,
                // create a tuple key with the previous index values and put this string as the value for key.
                std::string strval = Data_String.substr(last, next - last);
                stringMap_[std::make_tuple(pid, fid, fval)] = strval;
            }
        }
        //!Destructor.
        virtual ~Reader()
        {
            index_file_.close();
            record_file_.close();
            map_file_.close();
            data_file_.close();
            string_file_.close();
            display_file_.close();
        }

        /**
         * \brief Clears the internal lock. This should be used ONLY when an
         * exception occurs during loading
         */
        void clearLock(){
            lock = false;
        }

        /*!
         * \brief Set the data callback, returning the previous callback
         */
        PipelineDataCallback* setDataCallback(PipelineDataCallback* cb)
        {
            auto prev = data_callback_;
            data_callback_ = cb;
            sparta_assert(data_callback_, "Data callback must not be nullptr");
            return prev;
        }

        /**
         * \brief using our PipelineDataCallback pointer
         * pass all of the transactions found in a given interval of cycles.
         * \param start the intervals start cycle. Transactions with
         * TMEN of "start" WILL be included in this window.
         * "start" will also round down to the lower index closest to "start"
         * \param end the intervals stop cycle. transactions
         * with TMEN of "end" will NOT be included in the window.
         *
         * [start, end) where start rounded down and end rounded up
         *
         * so if our interval = 1000
         * getWindowls(3500, 4700) will essentially return all transactions
         * ending between
         * [3000, 5000)
         *
         * \warning start must be GREATER than end.
         * \warning This method IS NOT thread safe.
         */
        void getWindow(uint64_t start, uint64_t end)
        {
            #ifdef READER_LOG
            std::cout << "\nREADER: returning window. START: " << start << " END: " << end << std::endl;
            #endif
            // sparta_assert(//start < end, "Cannot return a window where the start value is greater than the end value");

            //Make sure the user is not abusing our NON thread safe method.
            sparta_assert(!lock, "This reader class is not thread safe, and this method cannot be called"" from multiple threads.");
            lock = true;
            //round the end up to the nearest interval.
            uint64_t chunk_end = roundUp_(end);
            #ifdef READER_LOG
            std::cout << "READER: end rounded to: " << chunk_end << std::endl;
            #endif
            //First we will want to make sure we are ready to read at the correct
            //position in the record file.
            int64_t pos = findRecordReadPos_(start);
            record_file_.seekg(pos);

            //what space does this interval span in the record file.
            uint64_t full_data_size = findRecordReadPos_(chunk_end) - record_file_.tellg();
            //Now start processing the chunk.
            int64_t end_pos = pos + full_data_size;

            #ifdef READER_LOG
            std::cout << "READER: start_pos: " << pos << " end_pos: " << end_pos << std::endl;
            #endif

            //Make sure we have not passed the end position, also make sure we
            //are not at -1 bc that means we reached the end of the file!
            //As we read records. Read each as a transaction.
            //check the flags, seek back and then read it as the proper type,
            //then pass the a pointer to the struct to the appropriate callback.


            uint32_t recsread = 0;
            if(version_ == 1){
            while(pos < end_pos && pos != -1)
            {
                // Read, checking for chunk_end
                readRecord_v1_(pos, start, chunk_end);
                recsread++;
            }
            }else if(version_ == 2){
                while(pos < end_pos && pos != -1)
                {
                    // Read, checking for chunk_end
                    readRecord_v2_(pos, start, chunk_end);
                    recsread++;
                }
            }else{
                throw SpartaException("This pipeViewer reader library does not know how to read a window "
                                    " for version ") << version_ << " file: " << filepath_;
            }

            #ifdef READER_LOG
            std::cout << "READER: read " << std::dec << recsread << " records" << std::endl;
            #endif
            //unlock our assertion test.
            sparta_assert(lock);
            lock = false;
        }

        /**
         * \brief Reads transactions afer each index in the entire file
         */
        void dumpIndexTransactions() {
            auto prev_cb = data_callback_;
            try{
                uint64_t tick = 0;
                index_file_.seekg(0, std::ios::beg);
                while(tick <= getCycleLast() + (heartbeat_-1)){
                    int64_t pos;
                    //index_file_.read((char*)&pos, sizeof(uint64_t));
                    //if(index_file_.eof()){
                    //    break;
                    //}

                    // Set up a record checker to ensure all transactions fall
                    // within the range being queried
                    RecordChecker rec(tick, tick + heartbeat_);
                    data_callback_ = &rec;

                    pos = findRecordReadPos_(tick);

                    std::cout << "Heartbeat at t=" << std::setw(10) <<  tick << " @ filepos " << std::setw(9)
                              << pos << " first transaction:" << std::endl;


                    uint64_t chunk_end = roundUp_(tick + heartbeat_);
                    std::cout << "chunk end rounded to: " << chunk_end << std::endl;
                    std::cout << "record file pos before: " << record_file_.tellg() << std::endl;
                    record_file_.seekg(pos, std::ios::beg);
                    std::cout << "record file pos after:  " << record_file_.tellg() << std::endl;
                    if(record_file_.tellg() == EOF) {
                        std::cerr << "TellG says EOF!" << std::endl;
                    }
                    else {

                        //what space does this interval span in the record file.
                        uint64_t full_data_size = findRecordReadPos_(chunk_end) - record_file_.tellg();

                        //Now start processing the chunk.
                        int64_t end_pos = pos + full_data_size;
                        std::cout << "pos = " << pos << ", end_pos = " << end_pos << std::endl;


                        uint32_t recsread = 0;
                        while(pos < end_pos && pos != -1)
                        {
                            // Read, checking for chunk_end
                            readRecord_(pos, tick, chunk_end);
                            recsread++;
                        }

                        //readRecord_(pos, tick, tick + heartbeat_);
                        std::cout << "Records: " << recsread << std::endl;
                    }
                    std::cout << "record file pos after read: " << record_file_.tellg() << std::endl;
                    std::cout << "pos variable after read:    " << pos << std::endl;
                    tick += heartbeat_;
                    std::cout << "\n";
                }
            }catch(...){
                // Restore data callback before propogating exception
                data_callback_ = prev_cb;
                throw;
            }

            uint64_t tmp;
            if(index_file_.read((char*)&tmp, sizeof(uint64_t))){
                std::cout << "Read junk at the end of the index file:\n  " << tmp;
                while(index_file_.read((char*)&tmp, sizeof(uint64_t))){
                    std::cout << "  " << tmp;
                }
            }
        }

        /**
         * \brief Gets the size of a data chunk. This is a minimum granularity
         * of file reads when looking for any range. Chunks are in terms of
         * ticks and chunks always begin at ticks which are chunk-size-aligned
         */
        uint64_t getChunkSize() const
        {
            return heartbeat_;
        }

        /**
         * \brief Return the start time in the file.
         */
        uint64_t getCycleFirst() const
        {
            #ifdef READER_DBG
            std::cout << "READER: Returning first cycle: " << lowest_cycle_ << std::endl;
            #endif
            //This needs to be changed.
            //BUG When this returns 0, we miss many transactions in the viewer.
            return lowest_cycle_;
        }
        /**
         * \brief Return the last end time in the file.
         * Our output saved the last index to point to
         * the start of last record.
         */
        uint64_t getCycleLast() const
        {
            #ifdef READER_DBG
            std::cout << "READER: Returning last cycle: " << highest_cycle_ << std::endl;
            #endif
            return highest_cycle_;

        }

        /**
         * \brief Gets the version of the pipeout files loaded
         */
        uint32_t getVersion() const
        {
            return version_;
        }

        bool isUpdated()
        {
            checkIndexUpdates_();
            return file_updated_;
        }

        void ackUpdated()
        {
            file_updated_ = false;
        }

    private:
        const std::string filepath_; /*!< Path to this file */
        const std::string record_file_path_; /*!< Path to the record file */
        const std::string index_file_path_; /*!< Path to the index file */
        const std::string map_file_path_; /*!< Path to the map file which maps LocationID to Pair number */
        // Path to the data file which contains information about the name strings and the size in Bytes
        // their values hold for every different pair
        const std::string data_file_path_;
        // Path to the string_map file which contains String Representations of the actual values,
        // mapped from the intermediate Tuple used to write in Database
        const std::string string_file_path_;
        const std::string display_file_path_;
        std::fstream record_file_; /*!< The record file stream */
        std::fstream index_file_;  /*!< The index file stream */
        std::fstream map_file_;    /*!< The map file stream */
        std::fstream data_file_;   /*!< The data file stream */
        std::fstream string_file_; /*!< The string map file stream */
        std::fstream display_file_;
        PipelineDataCallback* data_callback_; /*<! A pointer to a callback to pass records too */
        uint64_t heartbeat_; /*!< The heartbeast-size in cycles of our indexes in the index file */
        uint64_t first_index_; /*!< Position in file of first index entry */
        uint32_t version_; /*!< Version of the file being read */
        int64_t size_of_index_file_; /*!< The total byte size of the index file */
        int64_t size_of_record_file_; /*!< The total byte size of the record file */
        uint64_t lowest_cycle_; /*!< The lowest cycle in the file. */
        uint64_t highest_cycle_; /*!< The highest cycle in the file. */
        bool lock; /*!< A tool used too assert that this file Reader is not thread safe.*/
        bool file_updated_; /*!< Set to true when the open database has changed */
        // In-memory data structure to hold the mapping of Location ID
        // of generic transaction structures
        // and map them to Pair IDs of pair transaction structs.
        std::unordered_map<std::string, std::string> loc_map_;
        // In-memory data structure to hold the unique pairId and
        // subsequent information about its name strings and
        // sizeofs for every different pair type
        std::vector<pair_struct> map_;
        // In-memory data structure to hold the string map structure
        // which maps a tuple of integers reflecting the pair type,
        // field number and field value to the actual String
        // we want to display in pipeViewer
        std::unordered_map<std::tuple<
            uint64_t, uint64_t, uint64_t>,
            std::string, hashtuple::hash<
            std::tuple<uint64_t, uint64_t, uint64_t>>>
            stringMap_;
    };
}//NAMESPACE:pipeViewer
}//NAMESPACE:sparta
