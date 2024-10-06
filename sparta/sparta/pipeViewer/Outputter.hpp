// <Outputter.hpp> -*- C++ -*-

/**
 * \file  Outputter.hpp
 **  \brief Outputs Transactions to record file and builds index file while running *
 */

#pragma once

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <iomanip>
#include <sstream>
#include <limits>

#include "sparta/pipeViewer/transaction_structures.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/TupleHashCompute.hpp"

namespace sparta::pipeViewer
{


    /**
     *  \class Outputter
     * @ brief A class that facilitates taking in Record objects and writing
     * them to the record file, and building the index as it goes.
     *
     * The index file is a list of uint64_t pointers into the record
     * file for the first transaction that ended at a multiple of a
     * standard offset, such as there is an index for every "interval"
     * of cycles.
     *
     * (Note) the first entry in the index file will always be the interval amount
     * (Note) the last entry in the index file will always point to the last record
     * written to file.
     *
     */
    class Outputter
    {
        /**
         * \brief Safely write data to a file with error checking.
         */
        void writeData_(std::ofstream& ss, const char* const data, const std::size_t size)
        {
            ss.write(data, size);
        }

        template<typename T>
        void writeData_(std::ofstream& ss, const T* const data, const std::size_t size = sizeof(T))
        {
            writeData_(ss, reinterpret_cast<const char* const>(data), size);
        }

        template<typename T>
        void writeData_(std::ofstream& ss, const T& data)
        {
            writeData_(ss, &data, sizeof(T));
        }

    public:

        /*!
         * \brief File format version written by this outputter.
         * This must be incremented on any change to the transaction type
         * \note If you are incrementing this, be sure pipeViewer::Reader is up to
         * date and backward compatible
         */
        static constexpr uint32_t FILE_VERSION = 2;

        /**
         * \brief Construct an Outputter
         * \param file_path the path to the folder to store output files.
         * \param interval The number of cycles between indexes
         */
        Outputter(const std::string& filepath, const uint64_t interval);

        /**
         * \brief Close the Record file and the index file.
         * also writes an index pointing to the end of the record file.
         */
        virtual ~Outputter();

        /**
         * \brief Write a generic transaction to the record file, and update the index file.
         * \param dat The transaction to be written.
         */
        template<class R_Type>
        void writeTransaction(const R_Type& dat)
        {
            last_record_pos_ = record_file_.tellp();
#ifdef PIPELINE_DBG
            std::cout << "writing transaction at: " << last_record_pos_ << " TMST: "
                      << dat.time_Start << " TMEN: " << dat.time_End <<  std::endl;
#endif
            writeData_(record_file_, dat);
        }

        /**
         * \brief A method that marks a pointer to the record file's current location.
         * This method will likely be set to run on the schedular on a given interval.
         */
        void writeIndex();
    private:
        std::ofstream record_file_; /*!< The record file contains the actual transaction data */
        std::ofstream index_file_; /*!< The file stream for index file being created */
        std::ofstream map_file_; /*!< The file stream for map file which maps Location ID to Pair ID */
        std::ofstream data_file_;/*! The file stream for data file which contains Name, Size, Pair number */
        std::ofstream string_file_;/*! The file stream for string_map file which has the String representation */
        std::ofstream display_format_file_;

        uint64_t last_record_pos_; /*!< A pointer to the last record written */
    };

    /*!
     * \brief Write an annotation transaction to the record file.
     * notice that an annotation needs special attention in order
     * to take care of outputting ANNT data itself properly
     * \warning this deletes the memory for ANNT in the dat passed
     */
    template<>
    inline void Outputter::writeTransaction(const annotation_t& dat)
    {

        writeTransaction<transaction_t>(static_cast<transaction_t>(dat));
        writeData_(record_file_, dat.length);
        writeData_(record_file_, dat.annt.data(), dat.length);
    }

    /*!
     * \brief Write a pair transaction to the record file.
     */
    template<>
    inline void Outputter::writeTransaction(const pair_t & dat){

        // A static Unordered Set which contains unique location id of the records.
        static std::unordered_set<uint32_t> locIDSet;

        // A static Unordered Set which contains unique pair id of the records.
        static std::unordered_set<uint16_t> pairIDSet;

        using multiIndex = std::tuple<uint64_t, uint64_t, uint64_t>;

        // A static Unordered Map with a Tuple of 3 Integers as Key and a String as Value.
        // This map is used to store the String mappings from the Intermediate Integer values.
        // We use integers as this makes the database smaller and also very fast to write to Binary File.
        // The first Integer in the May Key is the unique Pair they bleong to.
        // The Second Integer is the Field number they belong to.
        // The Third Integer is the actual Integral value which corresponds to the String value.
        static std::unordered_map<multiIndex, std::string, hashtuple::hash<multiIndex>> stringMap;

        // If we find a Location ID that we have not seen before, we store it in the Location ID set.
        if(locIDSet.emplace(dat.location_ID).second) {
            // We add the Location Id followed by the Pair Id of that record in the map file.
            map_file_ << dat.location_ID << ':' << dat.pairId << '\n';
        }

        // If we find a Pair ID we have not seen before, we store it in the Pair ID set.
        if(pairIDSet.emplace(dat.pairId).second) {
            // We write the Pair ID to the data file followed by the Number of pairs
            // this kind of pair collectable contains.
            // The first pair of every pair record is its PairID, so we do not add that to the database.
            data_file_ << dat.pairId << ':' << dat.length;

            // We write the generic transaction structure to the record file.
            writeTransaction<transaction_t>(static_cast<transaction_t>(dat));

            // We iterate over all the name value pairs of the current pair record.
            for(std::size_t i = 0; i < dat.length; ++i) {
                data_file_ << ':';
                if(dat.valueVector[i].second){
                    // We add the Name String followed by the Sizeof in the data file.
                    data_file_ << dat.nameVector[i] << ':' << dat.sizeOfVector[i] << ":0";

                    // We write the Value for field "i" and only write as much Bytes
                    // as it needs to by checking Sizes[i].
                    writeData_(record_file_,
                               &dat.valueVector[i].first,
                               dat.sizeOfVector[i]);

                    // We check if the value at field "i" has any String Representation.
                    // If it has, then its corresponding string vector field will not be empty.
                    if(!dat.stringVector[i].empty()){
                        // We check if we have seen this exact pair, field and value before or not.
                        if(const auto& [val, str] = std::tie(dat.valueVector[i].first, dat.stringVector[i]);
                           stringMap.emplace(std::piecewise_construct, std::forward_as_tuple(dat.pairId, i, val), std::forward_as_tuple(str)).second) {
                            // We add this mapping into out String Map file which we will
                            // use when reading back from the database.
                            string_file_ << dat.pairId
                                         << ':' << i
                                         << ':' << val
                                         << ':' << str << '\n';
                        }
                    }
                }
                else{
                    // We add the Name String followed by the Sizeof in the data file.
                    data_file_ << dat.nameVector[i] << ":0:1";

                    // We write the Value for field "i" and only write as much Bytes
                    // as it needs to by checking Sizes[i].
                    const auto& str = dat.stringVector[i];
                    const uint16_t length = str.size();
                    writeData_(record_file_, length);
                    writeData_(record_file_, str.data(), length);
                }
            }
            data_file_ << '\n';
            display_format_file_ << dat.pairId;
            for(const auto& fmt: dat.delimVector) {
                display_format_file_ << ':' << static_cast<PairFormatterInt>(fmt);
            }
            display_format_file_ << '\n';
        }

        // If we find a Pair ID we have seen before,
        // we do not store it in the Pair ID set and move forward with just writing it to file.
        else{

            // We write the generic transaction structure to the record file.
            writeTransaction<transaction_t>(static_cast<transaction_t>(dat));

            // We iterate over all the name value pairs of the current pair record.
            for(std::size_t i = 0; i < dat.length; ++i){
                if(dat.valueVector[i].second){
                    // We write the Value for field "i" and only write as much Bytes
                    // as it needs to by checking Sizes[i].
                    writeData_(record_file_,
                               &dat.valueVector[i].first,
                               dat.sizeOfVector[i]);

                    // We check if the value at field "i" has any String Representation.
                    // If it has, then its corresponding string vector field will not be empty.
                    if(!dat.stringVector[i].empty()){
                        // We check if we have seen this exact pair, field and value before or not.
                        if(const auto& [val, str] = std::tie(dat.valueVector[i].first, dat.stringVector[i]);
                           stringMap.emplace(std::piecewise_construct, std::forward_as_tuple(dat.pairId, i, val), std::forward_as_tuple(str)).second) {
                            // We add this mapping into out String Map file which we will
                            // use when reading back from the database.
                            string_file_ << dat.pairId
                                         << ':' << i
                                         << ':' << val
                                         << ':' << str << '\n';
                        }
                    }
                }
                else{
                    // We write the Value for field "i" and only write as much Bytes
                    // as it needs to by checking Sizes[i].
                    const auto& str = dat.stringVector[i];
                    const uint16_t length = str.size();
                    writeData_(record_file_, length);
                    writeData_(record_file_, str.data(), length);
                }
            }
        }
    }
}//namespace sparta::pipeViewer
