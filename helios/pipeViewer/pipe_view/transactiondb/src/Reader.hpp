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
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <sys/stat.h>

#include "PipelineDataCallback.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LexicalCast.hpp"
#include "sparta/pipeViewer/Outputter.hpp"

// #define READER_DBG 1
// #define READER_LOG 1

#define _LOG_MSG(strm, x) strm << "READER: " << x << std::endl

#if READER_DBG == 1
#define READER_LOG 1
#define READER_DBG_MSG(x) _LOG_MSG(std::cerr, x)
#else
#define READER_DBG_MSG(x)
#endif

#if READER_LOG == 1
#define READER_LOG_MSG(x) _LOG_MSG(std::cout, x)
#else
#define READER_LOG_MSG(x)
#endif

namespace sparta::pipeViewer {
    /*!
     * \brief Sanity checker for records. Used when doing a dump of the index
     * file to check that all transactions in the heartbeat actually belong
     * there
     */
    class RecordChecker : public PipelineDataCallback
    {
        private:
            uint64_t start;
            uint64_t end;

        public:
            RecordChecker(const uint64_t _start, const uint64_t _end) :
                start(_start),
                end(_end)
            {;}

            virtual void foundTransactionRecord(const transaction_t* r) override {
                if (r->time_Start < start ||
                    r->time_End   > end) {
                    std::cout << "Bounds on transactions were outside of heartbeat range " << start
                              << ", " << end << ". transaction:"
                              << " idx: "    << r->transaction_ID
                              << " disp: "   << r->display_ID
                              << " loc: "    << r->location_ID
                              << " start: "  << r->time_Start
                              << " end: "    << r->time_End
                              << " parent: " << r->parent_ID << std::endl;
                }
            }

            virtual void foundInstRecord(const instruction_t* r) override {
                foundTransactionRecord(r);
            }

            virtual void foundMemRecord(const memoryoperation_t* r) override {
                foundTransactionRecord(r);
            }

            virtual void foundAnnotationRecord(const annotation_t* r) override {
                foundTransactionRecord(r);
            }

            virtual void foundPairRecord(const pair_t* r) override {
                foundTransactionRecord(r);
            }
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
            class FileStream {
                private:
                    const std::string filename_;
                    const std::ios_base::openmode mode_;

                protected:
                    std::ifstream fstream_;

                public:
                    FileStream(std::string&& filename, const std::ios_base::openmode mode) :
                        filename_(std::move(filename)),
                        mode_(mode),
                        fstream_(filename_, mode)
                    {
                        //Make sure the file opened correctly!
                        sparta_assert(fstream_.is_open(),
                                      "Failed to open file " << filename_);
                        sparta_assert(fstream_.peek() != std::ifstream::traits_type::eof(),
                                      filename_ << " is empty. Did Argos database collection complete?");
                    }

                    FileStream(FileStream&&) = default;

                    ~FileStream() {
                        fstream_.close();
                    }

                    inline const auto& getFilename() const {
                        return filename_;
                    }

                    inline bool read(char* const buf, const size_t num_bytes) {
                        return !!fstream_.read(buf, num_bytes);
                    }

                    template<typename T>
                    inline bool read(T* const buf, const size_t num_bytes) {
                        return read(reinterpret_cast<char* const>(buf), num_bytes);
                    }

                    template<typename T>
                    inline bool read(T* const buf) {
                        return read(buf, sizeof(T));
                    }

                    template<typename T>
                    inline bool read(T& buf) {
                        return read(&buf);
                    }

                    inline bool seekg(const std::ifstream::pos_type pos) {
                        return !!fstream_.seekg(pos);
                    }

                    inline bool seekg(const std::ifstream::off_type pos, const std::ios::seekdir dir) {
                        return !!fstream_.seekg(pos, dir);
                    }

                    inline auto tellg() {
                        return fstream_.tellg();
                    }

                    inline auto gcount() const {
                        return fstream_.gcount();
                    }

                    inline bool good() const {
                        return fstream_.good();
                    }

                    inline void reopen() {
                        std::fstream::pos_type cur_pos = fstream_.tellg();
                        fstream_.close();
                        fstream_.clear();
                        fstream_.open(filename_, mode_);
                        fstream_.seekg(cur_pos);
                    }

                    inline void clear() {
                        fstream_.clear();
                    }

                    inline int64_t size() const {
                        struct stat stat_result;
                        stat(filename_.c_str(), &stat_result);
                        return stat_result.st_size;
                    }

                    inline operator bool() const {
                        return !!fstream_;
                    }
            };

            /**
             * \class ColonDelimitedFile
             * \brief Class that knows how to read ':'-delimited files used by the Argos pair format
             */
            class ColonDelimitedFile : public FileStream {
                    /**
                     * \struct CustomSpaceCType
                     * \brief Subclass of std::ctype<char> that allows overriding which characters should be
                     * considered whitespace
                     */
                    template<char... SpaceChars>
                    struct CustomSpaceCType : std::ctype<char> {
                        private:
                            using CTypeArray = std::array<std::ctype_base::mask, table_size>;

                            /**
                             * \brief Generates a constexpr character table with the characters in SpaceChars set as whitespace
                             */
                            static inline constexpr CTypeArray genSpaceTable_() {
                                CTypeArray space_table{};
                                // First set every character to std::ctype_base::mask - treating them as non-whitespace
                                for(size_t i = 0; i < table_size; ++i) {
                                    space_table[i] = std::ctype_base::mask();
                                }

                                // Lambda that sets a character to whitespace
                                auto add_space_char = [&](const char c) { space_table[c] = std::ctype_base::space; };
                                // Apply the lambda over all of the characters in SpaceChars
                                (add_space_char(SpaceChars), ...);
                                return space_table;
                            }

                        public:
                            CustomSpaceCType() :
                                std::ctype<char>(getTable())
                            {
                            }

                            static inline std::ctype_base::mask const* getTable() {
                                // Init the table
                                static constexpr CTypeArray space_table = genSpaceTable_();
                                // Return a pointer to the table
                                return space_table.data();
                            }
                    };

                    // The FileLineCType only treats newlines as whitespace, causing the >> operator to grab an entire line at a time
                    using FileLineCType = CustomSpaceCType<'\n'>;

                public:
                    /**
                     * \class LineStream
                     * \brief Subclass of std::istringstream with some useful specializations for the >> operator. This class is used to
                     * automatically tokenize each line in a ':'-delimited file using the >> operator.
                     */
                    class LineStream : public std::istringstream {
                        private:
                            // The PairFormatCType treats ':' as whitespace, causing the >> operator to tokenize on
                            // any ':' in its internal string buffer
                            using PairFormatCType = CustomSpaceCType<':'>;

                            /**
                             * \brief Reads an aribitrary tuple from the stream
                             */
                            template<size_t idx, typename ... Args>
                            inline LineStream& readTuple_(std::tuple<Args...>& val) {
                                // We reached the end of the tuple, so no more work to be done
                                if constexpr(idx >= sizeof...(Args)) {
                                    return *this;
                                }
                                else {
                                    // Grab the current index and then recursively grab the next one
                                    // (this will get unrolled by the compiler)
                                    *this >> std::get<idx>(val);
                                    return readTuple_<idx+1>(val);
                                }
                            }

                            /**
                             * \brief Sets the locale to a PairFormatCType
                             */
                            inline void setLocale_() {
                                imbue(std::locale(std::locale(), new PairFormatCType()));
                            }

                        public:
                            LineStream() :
                                std::istringstream()
                            {
                                setLocale_();
                            }

                            explicit LineStream(const std::string& str) :
                                std::istringstream(str)
                            {
                                setLocale_();
                            }

                            using std::istringstream::operator>>;

                            /**
                             * \brief >> operator specialization for enums
                             */
                            template<typename T>
                            inline std::enable_if_t<std::is_enum_v<T>, LineStream&> operator>>(T& val) {
                                // Read into a temporary variable of the underlying type of the enum
                                std::underlying_type_t<T> new_val;
                                *this >> new_val;
                                // Then cast it to the enum type
                                val = static_cast<T>(new_val);
                                return *this;
                            }

                            /**
                             * \brief >> operator specialization for std::pair
                             */
                            template<typename T, typename U>
                            inline LineStream& operator>>(std::pair<T, U>& val) {
                                *this >> val.first >> val.second;
                                return *this;
                            }

                            /**
                             * \brief >> operator specialization for std::tuple
                             */
                            template<typename ... Args>
                            inline LineStream& operator>>(std::tuple<Args...>& val) {
                                return readTuple_<0>(val);
                            }

                            /**
                             * \brief >> operator specialization for std::unordered_map
                             * Reads in the key and value as an std::pair and then does a move-emplace into the map
                             * Note that this only adds 1 element at a time rather than attempting to read an entire map at once
                             */
                            template<typename Key,
                                     typename T,
                                     typename Hash = std::hash<Key>,
                                     typename KeyEqual = std::equal_to<Key>,
                                     typename Allocator = std::allocator<std::pair<const Key, T>>>
                            inline LineStream& operator>>(std::unordered_map<Key, T, Hash, KeyEqual, Allocator>& map) {
                                std::pair<Key, T> new_pair;
                                *this >> new_pair;
                                map.emplace(std::move(new_pair));
                                return *this;
                            }

                            /**
                             * \brief >> operator specialization for std::vector
                             * Reads in the value then does a move-emplace into the vector
                             * Note that this only adds 1 element at a time rather than attempting to read an entire vector at once
                             */
                            template<typename T, typename Allocator = std::allocator<T>>
                            inline LineStream& operator>>(std::vector<T, Allocator>& val) {
                                T new_val;
                                *this >> new_val;
                                val.emplace_back(std::move(new_val));
                                return *this;
                            }
                    };

                    explicit ColonDelimitedFile(std::string&& filename, std::ios_base::openmode mode = std::ios_base::in) :
                        FileStream(std::move(filename), mode)
                    {
                        fstream_.imbue(std::locale(std::locale(), new FileLineCType));
                    }

                    /**
                     * \brief Iterates over the entire file line by line, tokenizing the line with a LineStream and then
                     * calling a callback function for further processing
                     */
                    template<typename CallbackFunc>
                    inline void processWith(CallbackFunc&& func) {
                        LineStream strm;
                        for(auto it = std::istream_iterator<std::string>(fstream_); it != std::istream_iterator<std::string>(); ++it) {
                            strm.str(*it);
                            strm.clear();
                            func(strm);
                        }
                    }

                    /**
                     * \brief Simplified form of processWith that reads an entire file into a single object
                     */
                    template<typename T>
                    inline void processInto(T& obj) {
                        processWith([&](LineStream& strm) { strm >> obj; });
                    }
            };

            // Memory Layout of pair_struct. We build a map of such structs before we start reading record back from the database.
            // This structs help us by telling us exactly how may pair values
            // there are in the current pair type, what their names are and how much bytes each of the values occupy.
            struct pair_struct{
                uint16_t length;
                std::vector<std::string> names;
                std::vector<uint16_t> sizes;
                std::vector<uint16_t> types;
                PairFormatterVector formats;

                explicit pair_struct(ColonDelimitedFile::LineStream& strm) :
                    length(0),
                    names(1, "pairid"),
                    sizes(1, sizeof(uint16_t)),
                    types(1, 0),
                    formats(1, PairFormatter::DECIMAL)
                {
                    strm >> length;
                    ++length; // Increment length to account for the default pairid field

                    names.reserve(length);
                    sizes.reserve(length);
                    types.reserve(length);

                    // Iterate through the rest of the delimiters in the stringline
                    // and build up the Name Strings and the Sizeof values for every field.
                    // Formats are read later from a different file
                    while(!strm.eof()) {
                        strm >> names >> sizes >> types;
                    }

                    formats.reserve(length);
                }
            };

            /**
             * \brief return the correct position in the record file that corresponds
             * to the start cycle.
             * \param start The cycle number to start reading records from.
             */
            inline uint64_t findRecordReadPos_(const uint64_t start)
            {
                //Figure out how far we need to seek into our index file.
                const uint64_t step = first_index_ + (start/heartbeat_ * sizeof(uint64_t));

                //Now read the pointer from this location.
                if(!index_file_.seekg(step)) {
                    sparta_assert(!"Could not seekg in for the given position.  Please report bug");
                }
                uint64_t pos = size_of_record_file_;

                //It might be the case that our index file is too small
                //to represent an end time that the user is requesting.
                //in the above algorythm either reached the end of the index file.

                //notice we look too see if the index file seeker has passed
                //size_of_index_file_ - 8 bytes b/c a special last index is written to the index file
                //to point to only the start of the last transaction.
                const int64_t filepos = index_file_.tellg();
                if(filepos >= size_of_index_file_ - 8 || filepos == -1)
                {
                    //We need to reset end of file reach flags for the index file.
                    index_file_.clear();
                }
                else
                {
                    index_file_.read(pos);
                }
                return pos;
            }

            /**
             * \brief A helper method to round values up to the
             * interval values.
             * example 4600 rounds to 5000 when the interval is "1000"
             */
            inline uint64_t roundUp_(const uint64_t num)
            {
                const auto sub_sum = num + heartbeat_ - 1;
                return sub_sum - (sub_sum % heartbeat_);
            }

            inline std::string readAnnotation_(const uint16_t length) {
                std::string annt_buf(length, '\0');
                record_file_.read(annt_buf.data(), length);
                return annt_buf;
            }

            inline void acquireLock_() {
                sparta_assert(!lock_,
                              "This reader class is not thread safe, "
                              "and this method cannot be called from multiple threads.");
                lock_ = true;
            }

            /**
             * \brief Return the start time in the file.
             */
            inline uint64_t findCycleFirst_()
            {
                //Make sure the user is not abusing our NON thread safe method.
                acquireLock_();
                record_file_.seekg(0, std::ios::beg);
                transaction_t transaction;
                record_file_.read(transaction);
                clearLock();
                return transaction.time_Start;
            }

            /**
             * \brief Return the last end time in the file.
             * Our output saved the last index to point to
             * the start of last record.
             */
            inline uint64_t findCycleLast_()
            {
                //Make sure the user is not abusing our NON thread safe method.
                acquireLock_();
                //Notice that we reset the index file, b/c if
                //we had already read to the end of the file, we need to reset
                //end of file flags.
                index_file_.clear();
                //seek one entry back from the end of the index file
                index_file_.seekg(size_of_index_file_-sizeof(uint64_t));
                uint64_t pos = 0;
                index_file_.read(pos);
                //read the transaction at the appropriate location.
                record_file_.seekg(pos, std::ios::beg);
                transaction_t transaction;
                record_file_.read(transaction);
                clearLock();
                if(record_file_.gcount() != sizeof(transaction_t))
                {
                    return highest_cycle_;
                }
                return transaction.time_End - 1;

            }

            template<bool CountRecords>
            inline size_t readRecordVersion_(const std::fstream::pos_type end_pos,
                                             const uint64_t start,
                                             const uint64_t end)
            {
                size_t recsread = 0;
                for(auto pos = record_file_.tellg(); pos < end_pos && pos != -1; pos = record_file_.tellg())
                {
                    // Read, checking for chunk_end
                    readRecord_(start, end);
                    if constexpr(CountRecords) {
                        ++recsread;
                    }
                }
                return recsread;
            }

            /*!
             * \brief Read a record of any format. Older formats are upconverted to new format.
             */
            template<bool CountRecords = false>
            inline size_t readRecords_(const std::fstream::pos_type end_pos, const uint64_t start, const uint64_t end) {
                sparta_assert(version_ == 2, "Only version 2 is currenly supported");
                return readRecordVersion_<CountRecords>(end_pos, start, end);
            }

            /**
             * \brief Read a single record at \a pos and increment pos
             */
            inline void readRecord_(const uint64_t start, const uint64_t end) {
                transaction_t transaction;
                record_file_.read(transaction);
                sparta_assert(record_file_.good(), "Previous read of the argos DB failed");

                switch (transaction.flags & TYPE_MASK)
                {
                    case is_Annotation :
                    {
                        annotation_t annot(std::move(transaction));
                        record_file_.read(annot.length);
                        annot.annt = readAnnotation_(annot.length);

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

                        // Only send along transaction in the query range
                        if(transaction.time_End < start || transaction.time_Start > end){
                            // Skip transactions by not sending them along to the callback.
                            // read is faster than seekg aparently. This DOES help performance
                            READER_DBG_MSG("skipped transaction outside of window [" << start << ", "
                                           << end << "). start: " << transaction.time_Start << " end: "
                                           << transaction.time_End
                                           << " parent: " << transaction.parent_ID);
                        }else{
                            READER_DBG_MSG("found annt. " << "loc: " << annot.location_ID << " start: "
                                           << annot.time_Start << " end: " << annot.time_End
                                           << " parent: " << annot.parent_ID);
                            data_callback_->foundAnnotationRecord(&annot);
                        }
                    } break;

                    case is_Instruction:
                    {
                        instruction_t inst;
                        record_file_.seekg(-sizeof(transaction_t), std::ios::cur);
                        record_file_.read(inst);

                        READER_DBG_MSG("found inst. start: " << inst.time_Start << " end: " << inst.time_End);

                        data_callback_->foundInstRecord(&inst);
                    } break;

                    case is_MemoryOperation:
                    {
                        memoryoperation_t memop;
                        record_file_.seekg(-sizeof(transaction_t), std::ios::cur);
                        record_file_.read(memop);

                        READER_DBG_MSG("found inst. start: " << memop.time_Start << " end: " << memop.time_End);

                        data_callback_->foundMemRecord(&memop);
                    } break;

                    // If we have found a record which is of Pair Type,
                    // then we enter into this switch case
                    // which contains the logic to read back records of
                    // pair type using record file, map file
                    // and In-memory data structures and rebuild the pair
                    // record one by one.
                    case is_Pair : {
                        pair_t pairt(std::move(transaction));

                        // The loc_map is an In-memory Map which contains a mapping
                        // of Location ID to Pair ID.
                        // We are going to use this map to lookup the Location ID we
                        // just read from this current record
                        // and find out the pair id of such a record
                        const auto unique_id = loc_map_.at(transaction.location_ID);

                        // We are now going to use the In-memory data structure we built
                        // during the Reader construction.
                        // This Data Structure contains information about the name strings
                        // and their sizeof data
                        // for every different type of pair we have collected.
                        // So, we retrieve records from this structure one by one,
                        // till we find that the pair id of the record we just read
                        // from the record file
                        // matches the pair id of the current record we retrieved from
                        // the In-memory data Structure.
                        const auto& st = map_.at(unique_id);

                        // We lookup the length, the name strings and the sizeofs of
                        // every anme string from the retrieved
                        // record of the Data Struture and copy the values into out
                        // live Pair Transaction record
                        pairt.length = st.length;
                        pairt.nameVector = st.names;
                        pairt.sizeOfVector = st.sizes;
                        pairt.delimVector = st.formats;

                        pairt.valueVector.reserve(pairt.length);
                        pairt.valueVector.emplace_back(std::make_pair(unique_id, false));

                        pairt.stringVector.reserve(pairt.length);
                        pairt.stringVector.emplace_back(std::to_string(unique_id));

                        for(std::size_t i = 1; i != st.length; ++i){
                            if(st.types[i] == 0) {
                                // Type 0 = integer
                                const auto item_size = pairt.sizeOfVector[i];
                                sparta_assert(item_size <= sizeof(pair_t::IntT),
                                              "Data Type not supported for reading/writing.");
                                pair_t::IntT tmp = 0;
                                record_file_.read(&tmp, item_size);
                                pairt.valueVector.emplace_back(std::make_pair(tmp, true));

                                // Finally for a certain field "i", we check if there is a string
                                // representation for the integral value, by checking,
                                // if a key with current pair id, current field id, current integral value
                                // exists in the In-memory String Map. If yes, we grab the value
                                // and place it in the enum vector.
                                if(const auto it = stringMap_.find(std::make_tuple(pairt.valueVector[0].first,
                                                                                   i-1, // string map doesn't include the UID field, so index 0 == field index 1
                                                                                   pairt.valueVector[i].first));
                                   it != stringMap_.end()) {
                                    pairt.stringVector.emplace_back(it->second);
                                    pairt.valueVector[i].second = false;
                                }

                                // Else, we convert integer value to string
                                else {
                                    const auto int_value = pairt.valueVector[i].first;

                                    if (int_value == std::numeric_limits<pair_t::IntT>::max()) {
                                        // Max value, so probably bad...push empty string
                                        pairt.stringVector.emplace_back("");
                                    } else {
                                        const auto& format_str = pairt.delimVector[i];

                                        std::ios_base::fmtflags format_flags = std::ios::dec;
                                        std::string_view fmt_prefix = "";

                                        if(format_str == PairFormatter::HEX) {
                                            format_flags = std::ios::hex;
                                            fmt_prefix = "0x";
                                        }
                                        else if(format_str == PairFormatter::OCTAL) {
                                            format_flags = std::ios::oct;
                                            fmt_prefix = "0";
                                        }

                                        std::ostringstream int_str;
                                        int_str << fmt_prefix;
                                        int_str.setf(format_flags, std::ios::basefield);
                                        int_str << int_value;
                                        pairt.stringVector.emplace_back(int_str.str());
                                    }
                                }
                            }
                            else if(st.types[i] == 1){
                                // Type 1 = string
                                uint16_t annotationLength;
                                record_file_.read(annotationLength);

                                std::string annot_str(annotationLength, '\0');
                                record_file_.read(annot_str.data(), annotationLength);
                                pairt.stringVector.emplace_back(std::move(annot_str));

                                // This bool value describes if this field has a string-only value.
                                // String only values are those values which are stored in database as
                                // strings and has no integral representation of itself.
                                const bool string_only_field = true;
                                pairt.valueVector.emplace_back(
                                    std::make_pair(
                                        std::numeric_limits<decltype(pairt.valueVector)::value_type::first_type>::max(),
                                        string_only_field
                                    )
                                );
                            } else {
                                pairt.stringVector.emplace_back("none");
                                pairt.valueVector.emplace_back(std::make_pair(0, false));
                            }
                        }

                        READER_DBG_MSG("found pair. start: " << pairt.time_Start << " end: " << pairt.time_End);

                        data_callback_->foundPairRecord(&pairt);
                    } break;

                    default:
                    {
                        throw sparta::SpartaException("Unknown Transaction Found. Data might be corrupt.");
                    }
                }
            }

            inline void checkIndexUpdates_()
            {
                const auto index_size = index_file_.size();
                const auto record_size = record_file_.size();

                if(index_size != size_of_index_file_ && record_size != size_of_record_file_)
                {
                    const int64_t record_remainder = record_size % heartbeat_;

                    if(record_size - record_remainder == size_of_record_file_)
                    {
                        return;
                    }

                    record_file_.reopen();
                    index_file_.reopen();
                    map_file_.reopen();
                    data_file_.reopen();

                    size_of_index_file_ = index_size;

                    if(record_remainder != 0)
                    {
                        size_of_record_file_ = record_size - record_remainder;
                    }
                    else
                    {
                        size_of_record_file_ = record_size;
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
            Reader(std::string filepath, std::unique_ptr<PipelineDataCallback>&& data_callback) :
                filepath_(std::move(filepath)),
                record_file_(filepath_ + "record.bin", std::fstream::in | std::fstream::binary ),
                index_file_(filepath_ + "index.bin", std::fstream::in | std::fstream::binary),
                map_file_(filepath_ + "map.dat", std::fstream::in),
                data_file_(filepath_ + "data.dat", std::fstream::in),
                string_file_(filepath_ + "string_map.dat", std::fstream::in),
                display_file_(filepath_ + "display_format.dat", std::fstream::in),
                data_callback_(std::move(data_callback)),
                size_of_index_file_(0),
                size_of_record_file_(0),
                lowest_cycle_(0),
                highest_cycle_(0),
                lock_(false),
                file_updated_(false),
                loc_map_(),
                map_(),
                stringMap_()
            {
                sparta_assert(data_callback_);

                READER_LOG_MSG("pipeViewer reader opened: " << record_file_.getFilename());

                // Read header from index file
                char header_buf[HEADER_SIZE];
                index_file_.read(header_buf, HEADER_SIZE);
                // Assuming older version until header proves otherwise
                version_ = 1;
                if(static_cast<size_t>(index_file_.gcount()) != HEADER_SIZE) {
                    // Assume old version because the file is too small to have a header
                    index_file_.clear(); // Clear error flags after failing to read enough data
                    index_file_.seekg(0, std::ios::beg); // Restore
                }
                else if(HEADER_PREFIX.compare(0,
                                              HEADER_PREFIX.size(),
                                              header_buf,
                                              HEADER_PREFIX.size())) {
                    // Header prefix did not match. Assume old version
                    index_file_.seekg(0, std::ios::beg); // Restore
                }
                else {
                    // Header prefix matched. Read version
                    header_buf[HEADER_SIZE - 1] = '\0'; // Insert null
                    version_ = sparta::lexicalCast<decltype(version_)>(header_buf + HEADER_PREFIX.size());
                }
                sparta_assert(version_ > 0 && version_ <= Outputter::FILE_VERSION,
                              "pipeout file " << filepath_ << " determined to be format "
                              << version_ << " which is not known by this version of SPARTA. Version "
                              "expected to be in range [1, " << Outputter::FILE_VERSION << "]");
                sparta_assert(index_file_.good(),
                              "Finished reading index file header for " << filepath_ << " but "
                              "ended up with non-good file handle somehow. This is a bug in the "
                              "header-reading logic");

                // Read the heartbeat size from our index file.
                // This will be the first integer in the file except for the header if there is one
                // Warning: this must be called while the index_file_ is already seeked to the
                // start of file, which it is after opening.
                index_file_.read(heartbeat_);

                // Save the first index entry position
                first_index_ = index_file_.tellg();

                READER_LOG_MSG("Heartbeat is: " << heartbeat_);

                sparta_assert(heartbeat_ != 0,
                              "Pipeout database \"" << filepath_ << "\" had a heartbeat of 0. This "
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

                // Read through the whole map File
                map_file_.processInto(loc_map_);

                // Building the In-memory Pair Lookup structure, such that,
                // in future when reading back record from transaction file,
                // we can use this structure to know about the length, name strings
                // and sizeof the values
                // for that paritcular pair, instead of using a file on disk for this.
                // We read through the Data file just once, and populate this structure.
                //The fields in the file are separated by ":"

                data_file_.processWith([&](auto& strm) {
                    uint16_t unique_id;
                    strm >> unique_id;

                    pair_struct pStruct(strm);

                    //Finally, when we have completely parsed one line of this file,
                    // it means we have complete knowledge of one pair type.
                    // We then insert this pair into out Lookup structure.
                    map_.emplace(unique_id, pStruct);
                });

                display_file_.processWith([&](auto& strm) {
                    uint16_t pairId;
                    strm >> pairId;

                    auto& fmt_vec = map_.at(pairId).formats;

                    while(!strm.eof()) {
                        strm >> fmt_vec;
                    }
                });

                // Read every line of the String Map file
                string_file_.processInto(stringMap_);
            }

            Reader(Reader&& rhs) = default;

            //!Destructor.
            virtual ~Reader() = default;

            template<typename CallbackType, typename ... CallbackArgs>
            inline static Reader construct(const std::string& filepath, CallbackArgs&&... cb_args) {
                return Reader(filepath, std::make_unique<CallbackType>(std::forward<CallbackArgs>(cb_args)...));
            }

            /**
             * \brief Clears the internal lock. This should be used ONLY when an
             * exception occurs during loading
             */
            inline void clearLock(){
                lock_ = false;
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
            inline void getWindow(const uint64_t start, const uint64_t end)
            {
                READER_LOG_MSG("returning window. START: " << start << " END: " << end);
                // sparta_assert(//start < end, "Cannot return a window where the start value is greater than the end value");

                //Make sure the user is not abusing our NON thread safe method.
                acquireLock_();
                //round the end up to the nearest interval.
                const uint64_t chunk_end = roundUp_(end);

                READER_LOG_MSG("end rounded to: " << chunk_end);

                //First we will want to make sure we are ready to read at the correct
                //position in the record file.
                record_file_.seekg(findRecordReadPos_(start));

                const auto read_pos = record_file_.tellg();
                //what space does this interval span in the record file.
                const std::fstream::pos_type full_data_size = findRecordReadPos_(chunk_end) - read_pos;
                //Now start processing the chunk.
                const std::fstream::pos_type end_pos = read_pos + full_data_size;

                READER_LOG_MSG("start_pos: " << read_pos << " end_pos: " << end_pos);

                //Make sure we have not passed the end position, also make sure we
                //are not at -1 bc that means we reached the end of the file!
                //As we read records. Read each as a transaction.
                //check the flags, seek back and then read it as the proper type,
                //then pass the a pointer to the struct to the appropriate callback.


#if READER_LOG == 1
                READER_LOG_MSG("read " << std::dec << readRecords_<true>(end_pos, start, end) << " records");
#else
                readRecords_<false>(end_pos, start, end);
#endif
                //unlock our assertion test.
                sparta_assert(lock_);
                clearLock();
            }

            /**
             * \brief Reads transactions afer each index in the entire file
             */
            inline void dumpIndexTransactions() {
                auto prev_cb = std::move(data_callback_);
                try{
                    uint64_t tick = 0;
                    index_file_.seekg(0, std::ios::beg);
                    while(tick <= getCycleLast() + (heartbeat_-1)){
                        int64_t pos;

                        // Set up a record checker to ensure all transactions fall
                        // within the range being queried
                        data_callback_ = std::make_unique<RecordChecker>(tick, tick + heartbeat_);

                        pos = findRecordReadPos_(tick);

                        std::cout << "Heartbeat at t=" << std::setw(10) <<  tick << " @ filepos " << std::setw(9)
                                  << pos << " first transaction:" << std::endl;


                        const uint64_t chunk_end = roundUp_(tick + heartbeat_);
                        std::cout << "chunk end rounded to: " << chunk_end << std::endl
                                  << "record file pos before: " << record_file_.tellg() << std::endl;
                        record_file_.seekg(pos, std::ios::beg);
                        const auto read_pos = record_file_.tellg();
                        std::cout << "record file pos after:  " << read_pos << std::endl;
                        if(read_pos == EOF) {
                            std::cerr << "TellG says EOF!" << std::endl;
                        }
                        else {

                            //what space does this interval span in the record file.
                            const uint64_t full_data_size = findRecordReadPos_(chunk_end) - read_pos;

                            //Now start processing the chunk.
                            const int64_t end_pos = pos + full_data_size;
                            std::cout << "pos = " << pos << ", end_pos = " << end_pos << std::endl;


                            const auto recsread = readRecords_<true>(end_pos, tick, chunk_end);
                            std::cout << "Records: " << recsread << std::endl;
                        }
                        std::cout << "record file pos after read: " << record_file_.tellg() << std::endl;
                        std::cout << "pos variable after read:    " << read_pos << std::endl;
                        tick += heartbeat_;
                        std::cout << "\n";
                    }
                }catch(...){
                    // Restore data callback before propogating exception
                    data_callback_ = std::move(prev_cb);
                    throw;
                }

                // Restore callback
                data_callback_ = std::move(prev_cb);

                if(uint64_t tmp; index_file_.read(tmp)) {
                    std::cout << "Read junk at the end of the index file:\n  " << tmp;
                    while(index_file_.read(tmp)) {
                        std::cout << "  " << tmp;
                    }
                }
            }

            /**
             * \brief Gets the size of a data chunk. This is a minimum granularity
             * of file reads when looking for any range. Chunks are in terms of
             * ticks and chunks always begin at ticks which are chunk-size-aligned
             */
            inline uint64_t getChunkSize() const
            {
                return heartbeat_;
            }

            /**
             * \brief Return the start time in the file.
             */
            inline uint64_t getCycleFirst() const
            {
                READER_DBG_MSG("Returning first cycle: " << lowest_cycle_);
                //This needs to be changed.
                //BUG When this returns 0, we miss many transactions in the viewer.
                return lowest_cycle_;
            }

            /**
             * \brief Return the last end time in the file.
             * Our output saved the last index to point to
             * the start of last record.
             */
            inline uint64_t getCycleLast() const
            {
                READER_DBG_MSG("Returning last cycle: " << highest_cycle_);
                return highest_cycle_;
            }

            /**
             * \brief Gets the version of the pipeout files loaded
             */
            inline uint32_t getVersion() const
            {
                return version_;
            }

            inline bool isUpdated()
            {
                checkIndexUpdates_();
                return file_updated_;
            }

            inline void ackUpdated()
            {
                file_updated_ = false;
            }

            template<typename CallbackType>
            inline CallbackType& getCallbackAs() {
                return *static_cast<CallbackType*>(data_callback_.get());
            }

            template<typename CallbackType>
            inline const CallbackType& getCallbackAs() const {
                return *static_cast<const CallbackType*>(data_callback_.get());
            }

        private:
            const std::string filepath_; /*!< Path to this file */
            FileStream record_file_; /*!< The record file stream */
            FileStream index_file_;  /*!< The index file stream */
            ColonDelimitedFile map_file_;    /*!< The map file stream */
            ColonDelimitedFile data_file_;   /*!< The data file stream */
            ColonDelimitedFile string_file_; /*!< The string map file stream */
            ColonDelimitedFile display_file_;
            std::unique_ptr<PipelineDataCallback> data_callback_; /*<! A pointer to a callback to pass records too */
            uint64_t heartbeat_; /*!< The heartbeast-size in cycles of our indexes in the index file */
            uint64_t first_index_; /*!< Position in file of first index entry */
            uint32_t version_; /*!< Version of the file being read */
            int64_t size_of_index_file_; /*!< The total byte size of the index file */
            int64_t size_of_record_file_; /*!< The total byte size of the record file */
            uint64_t lowest_cycle_; /*!< The lowest cycle in the file. */
            uint64_t highest_cycle_; /*!< The highest cycle in the file. */
            bool lock_; /*!< A tool used too assert that this file Reader is not thread safe.*/
            bool file_updated_; /*!< Set to true when the open database has changed */
            // In-memory data structure to hold the mapping of Location ID
            // of generic transaction structures
            // and map them to Pair IDs of pair transaction structs.
            std::unordered_map<uint32_t, uint16_t> loc_map_;
            // In-memory data structure to hold the unique pairId and
            // subsequent information about its name strings and
            // sizeofs for every different pair type
            std::unordered_map<uint16_t, pair_struct> map_;
            // In-memory data structure to hold the string map structure
            // which maps a tuple of integers reflecting the pair type,
            // field number and field value to the actual String
            // we want to display in pipeViewer
            std::unordered_map<std::tuple<uint64_t, uint64_t, uint64_t>,
                               std::string,
                               hashtuple::hash<std::tuple<uint64_t, uint64_t, uint64_t>>>
            stringMap_;
    };

    // Formats a pair into an annotation-like string - used by transactionsearch and Python interface
    // This version accepts the individual pair_t members as parameters so that it can be used with the
    // transactionInterval type
    inline static std::string formatPairAsAnnotation(const uint64_t transaction_ID,
                                                     const uint64_t display_ID,
                                                     const uint16_t length,
                                                     const std::vector<std::string>& nameVector,
                                                     const std::vector<std::string>& stringVector) {
        std::ostringstream annt_preamble;
        std::ostringstream annt_body;

        const uint16_t display_id = static_cast<uint16_t>((display_ID < 0x1000 ? display_ID : transaction_ID) & 0xfff);

        std::ios flags(nullptr);
        flags.copyfmt(annt_preamble);
        annt_preamble << std::setw(3) << std::setfill('0') << std::hex << display_id;
        annt_preamble.copyfmt(flags);
        annt_preamble << ' ';

        for(size_t i = 1; i < length; ++i) {
            const auto& name = nameVector[i];
            const auto& value = stringVector[i];

            if(name != "DID") {
                annt_body << name << '(' << value << ") ";
            }

            if(name == "uid") {
                annt_preamble << 'u' << (std::stoull(value) % 10000) << ' ';
            }
            else if(name == "pc") {
                flags.copyfmt(annt_preamble);
                annt_preamble << "0x" << std::setw(4) << std::setfill('0') << std::hex << (std::stoull(value, 0, 16) & 0xffff) << ' ';
                annt_preamble.copyfmt(flags);
            }
            else if(name == "mnemonic") {
                const std::string_view value_view(value);
                annt_preamble << value_view.substr(0, 7) << ' ';
            }
        }

        return annt_preamble.str() + annt_body.str();
    }

    // Formats a pair into an annotation-like string - used by transactionsearch and Python interface
    inline static std::string formatPairAsAnnotation(const pair_t* pair) {
        return formatPairAsAnnotation(pair->transaction_ID,
                                      pair->display_ID,
                                      pair->length,
                                      pair->nameVector,
                                      pair->stringVector);
    }
}//NAMESPACE:sparta::pipeViewer
