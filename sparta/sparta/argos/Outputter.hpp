// <Outputter.hpp> -*- C++ -*-

/**
 * \file  Outputter.hpp
 **  \brief Outputs Transactions to record file and builds index file while running *
 */

#ifndef __OUTPUTTER_H__
#define __OUTPUTTER_H__

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <iomanip>
#include <sstream>
#include <limits>

#include "sparta/argos/transaction_structures.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/TupleHashCompute.hpp"

namespace sparta{
    namespace argos{

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
        void writeData_(std::fstream& ss, const char * data, const std::size_t size)
        {
            ss.write(data, size);
        }

        public:

        /*!
        * \brief File format version written by this outputter.
        * This must be incremented on any change to the transaction type
        * \note If you are incrementing this, be sure argos::Reader is up to
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
            last_record_pos_ = record_file_.tellg();
            #ifdef PIPELINE_DBG
            std::cout << "writing transaction at: " << last_record_pos_ << " TMST: "
            << dat.time_Start << " TMEN: " << dat.time_End <<  std::endl;
            #endif
            writeData_(record_file_, reinterpret_cast<const char *>(&dat), sizeof(R_Type));
        }

        /**
        * \brief A method that marks a pointer to the record file's current location.
        * This method will likely be set to run on the schedular on a given interval.
        */
        void writeIndex();
        private:
        std::fstream record_file_; /*!< notice that fstream was used instead of ofstream bc we need to access tellg() */
        std::fstream index_file_; /*!< The file stream for index file being created */
        std::fstream map_file_; /*!< The file stream for map file which maps Location ID to Pair ID */
        std::fstream data_file_;/*! The file stream for data file which contains Name, Size, Pair number */
        std::fstream string_file_;/*! The file stream for string_map file which has the String representation */
        std::fstream display_format_file_;

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

            // Cant write a nullptr cstring!
            sparta_assert(dat.annt != nullptr);
            writeTransaction<transaction_t>((const transaction_t&)dat);
            writeData_(record_file_, reinterpret_cast<const char *>(&dat.length), sizeof(uint16_t));
            writeData_(record_file_, dat.annt, dat.length);
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

            typedef std::tuple<uint64_t, uint64_t, uint64_t> multiIndex;

            // A static Unordered Map with a Tuple of 3 Integers as Key and a String as Value.
            // This map is used to store the String mappings from the Intermediate Integer values.
            // We use integers as this makes the database smaller and also very fast to write to Binary File.
            // The first Integer in the May Key is the unique Pair they bleong to.
            // The Second Integer is the Field number they belong to.
            // The Third Integer is the actual Integral value which corresponds to the String value.
            static std::unordered_map<multiIndex, std::string, hashtuple::hash<multiIndex>> stringMap;

            // If we find a Location ID that we have not seen before, we store it in the Location ID set.
            if(locIDSet.find(dat.location_ID) == locIDSet.end()){
                locIDSet.insert(dat.location_ID);

                // We add the Location Id followed by the Pair Id of that record in the map file.
                map_file_ << dat.location_ID << ":" << dat.pairId << ":" << "\n";
            }

            // If we find a Pair ID we have not seen before, we store it in the Pair ID set.
            if(pairIDSet.find(dat.pairId) == pairIDSet.end()){
                pairIDSet.insert(dat.pairId);

                // We write the Pair ID to the data file followed by the Number of pairs
                // this kind of pair collectable contains.
                // The first pair of every pair record is its PairID, so we do not add that to the database.
                data_file_ << dat.pairId << ":" << dat.length << ":";

                // We write the generic transaction structure to the record file.
                writeTransaction<transaction_t>((const transaction_t &)dat);

                // We iterate over all the name value pairs of the current pair record.
                for(std::size_t i = 0; i < dat.length; ++i){

                    if(dat.valueVector[i].second){

                        // We add the Name String followed by the Sizeof in the data file.
                        data_file_ << dat.nameVector[i] << ":" << dat.sizeOfVector[i] << ":0:";

                        // We write the Value for field "i" and only write as much Bytes
                        // as it needs to by checking Sizes[i].
                        writeData_(record_file_,
                            reinterpret_cast<const char*>(&dat.valueVector[i].first), dat.sizeOfVector[i]);

                        // We check if the value at field "i" has any String Representation.
                        // If it has, then its corresponding string vector field will not be empty.
                        if(!dat.stringVector[i].empty()){

                            // We check if we have seen this exact pair, field and value before or not.
                            auto temp_tuple = std::make_tuple(dat.pairId, i, dat.valueVector[i].first);
                            if(stringMap.find(std::move(temp_tuple)) == stringMap.end()){

                                // If we have not seen this exact pair, field and value before, we store it
                                //by making the appropriate key and putting the String as its value.
                                temp_tuple = std::make_tuple(dat.pairId, i, dat.valueVector[i].first);
                                stringMap[std::move(temp_tuple)] = dat.stringVector[i];

                                // We add this mapping into out String Map file which we will
                                // use when reading back from the database.
                                string_file_ << dat.pairId << ":" << i << ":"
                                             << dat.valueVector[i].first << ":" << dat.stringVector[i]
                                             << ":" << "\n";
                            }
                        }
                    }
                    else{
                        // We add the Name String followed by the Sizeof in the data file.
                        data_file_ << dat.nameVector[i] << ":0:1:";

                        // We write the Value for field "i" and only write as much Bytes
                        // as it needs to by checking Sizes[i].
                        uint16_t length = dat.stringVector[i].size();
                        writeData_(record_file_, reinterpret_cast<const char*>(&length), sizeof(uint16_t));
                        const char * annotation = dat.stringVector[i].c_str();
                        writeData_(record_file_, annotation, length);
                    }
                }
                data_file_ << "\n";
                display_format_file_ << dat.pairId << ":" << dat.delimVector[0] << "\n";
            }

            // If we find a Pair ID we have seen before,
            // we do not store it in the Pair ID set and move forward with just writing it to file.
            else{

                // We write the generic transaction structure to the record file.
                writeTransaction<transaction_t>((const transaction_t &)dat);

                // We iterate over all the name value pairs of the current pair record.
                for(std::size_t i = 0; i < dat.length; ++i){
                    if(dat.valueVector[i].second){
                        // We write the Value for field "i" and only write as much Bytes
                        // as it needs to by checking Sizes[i].
                        writeData_(record_file_,
                            reinterpret_cast<const char*>(&dat.valueVector[i].first), dat.sizeOfVector[i]);

                        // We check if the value at field "i" has any String Representation.
                        // If it has, then its corresponding string vector field will not be empty.
                        if(!dat.stringVector[i].empty()){

                            // We check if we have seen this exact pair, field and value before or not.
                            auto temp_tuple = std::make_tuple(dat.pairId, i, dat.valueVector[i].first);
                            if(stringMap.find(std::move(temp_tuple)) == stringMap.end()){

                                // If we have not seen this exact pair, field and value before, we store it
                                //by making the appropriate key and putting the String as its value.
                                temp_tuple = std::make_tuple(dat.pairId, i, dat.valueVector[i].first);
                                stringMap[std::move(temp_tuple)] = dat.stringVector[i];

                                // We add this mapping into out String Map file which we will
                                // use when reading back from the database.
                                string_file_ << dat.pairId << ":" << i
                                             << ":" << dat.valueVector[i].first << ":"
                                             << dat.stringVector[i] << ":" << "\n";
                            }
                        }
                    }
                    else{

                        // We write the Value for field "i" and only write as much Bytes
                        // as it needs to by checking Sizes[i].
                        uint16_t length = dat.stringVector[i].size() + 1;
                        writeData_(record_file_, reinterpret_cast<const char*>(&length), sizeof(uint16_t));
                        const char * annotation = dat.stringVector[i].c_str();
                        writeData_(record_file_, annotation, length);
                    }
                }
            }
        }
    }//namespace argos
}//namespace sparta
//__OUTPUTTER_H__
#endif
