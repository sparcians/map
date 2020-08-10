/*
 */

#include "transactiondb/src/Reader.hpp"
#include "transactiondb/src/PipelineDataCallback.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <set>
#include <regex>


namespace sparta {
    namespace pipeViewer {

        /*! \brief Base class for search callbacks that holds configuration parameters and common helper methods
         */
        class BaseSearchCallback : public PipelineDataCallback {
            protected:
                static constexpr char RESULT_TAG_ = 'r';
                static constexpr char PROGRESS_TAG_ = 'p';
                static constexpr char INFO_TAG_ = 'i';
                static constexpr char START_DELIMITER_ = ':';
                static constexpr uint64_t NUMBER_OF_PROGRESS_UPDATES_ = 50;

                /*! Invert search */
                const bool invert_search_;

                /*! Location IDs to include in the search */
                std::set<uint32_t> locations_;

                /*! For update progress */
                uint64_t last_step_number_ = 0;
                uint64_t search_start_ = 0;
                uint64_t search_end_ = 0;
                uint64_t search_width_ = 0;
                double search_update_stride_ = 0;

                /*! Incremented by one every regular expression match */
                uint64_t hits_ = 0;

                uint64_t recs_viewed_ = 0;
                uint64_t recs_with_annot_ = 0;
                uint64_t recs_with_ins_ = 0;
                uint64_t recs_with_mem_ = 0;
                uint64_t recs_with_non_null_annot_ = 0;
                uint64_t recs_with_pair_ = 0;

                inline void handleProgressOutput_(const uint64_t current_time) {
                    uint64_t step;
                    if (search_update_stride_ != 0) {
                        step = current_time / search_update_stride_;
                    } else {
                        // Step is small that the search doesn't really need progress indicators
                        step = 100000000;
                    }
                    if (step > last_step_number_ && current_time > search_start_) {
                        float fraction = (current_time - search_start_) / static_cast<float>(search_width_);
                        std::cout << PROGRESS_TAG_ << fraction << std::endl;
                        last_step_number_ = step;
                    }
                }

                /*!
                 * \brief Writes a result to stdout in the form:
                 * "<result tag><start time>,<end time>@<location ID><annotation delimiter><annotation>\n"
                 */
                inline void handleResultOutput_(const uint64_t start_time,
                                                const uint64_t end_time,
                                                const uint64_t location_id,
                                                const char* str) {
                    std::cout << RESULT_TAG_
                              << start_time << "," << end_time << "@" << location_id
                              << START_DELIMITER_;

                    if(str) {
                        do {
                            if(*str == '\n' || *str == '\r') {
                                std::cout << "\\n";
                            }
                            else {
                                std::cout << *str;
                            }
                            ++str;
                        }
                        while(*str != 0);
                    }

                    std::cout << std::endl;
                }

                /*!
                 * \brief Writes a result to stdout in the form:
                 * "<result tag><start time>,<end time>@<location ID><annotation delimiter><annotation>\n"
                 */
                inline void handleResultOutput_(const annotation_t* annotation) {
                    handleResultOutput_(annotation->time_Start,
                                        annotation->time_End,
                                        annotation->location_ID,
                                        annotation->annt);
                }

                inline void handleResultOutput_(const pair_t* pairt, const std::string& formatted_pair) {
                    handleResultOutput_(pairt->time_Start,
                                        pairt->time_End,
                                        pairt->location_ID,
                                        formatted_pair.c_str());
                }

            public:
                BaseSearchCallback(const char* invert_search_str, const char* location_str) :
                    invert_search_(!!strtoull(invert_search_str, nullptr, 10))
                {
                    std::stringstream ss(location_str);
                    uint32_t id;
                    while (ss >> id) {
                        locations_.insert(id);
                        if (ss.peek() == ',') {
                            ss.ignore();
                        }
                    }
                }

                void setSearchParams(const uint64_t search_start, const uint64_t search_end) {
                    search_start_ = search_start;
                    search_end_ = search_end;
                    search_width_ = search_end - search_start;
                    search_update_stride_ = double(search_width_) / NUMBER_OF_PROGRESS_UPDATES_;
                }

                void startProgress() const {
                    std::cout << INFO_TAG_ << "search start:  " << search_start_ << std::endl
                              << INFO_TAG_ << "search locs:   (" <<  locations_.size() << ") [";

                    for (auto & lid : locations_) {
                        std::cout << lid << " ";
                    }

                    std::cout << "]" << std::endl
                              << INFO_TAG_ << "search invert: " << invert_search_ << std::endl;
                }

                void finishedProgress() const {
                    std::cout << PROGRESS_TAG_ << 1 << std::endl //finish off so progress bar doesn't hang
                              << INFO_TAG_ << "Number of records: " << recs_viewed_ << std::endl
                              << INFO_TAG_ << "Number of records with annotation: " << recs_with_annot_ << std::endl
                              << INFO_TAG_ << "Number of records with instruction: " << recs_with_ins_ << std::endl
                              << INFO_TAG_ << "Number of records with memory: " << recs_with_mem_ << std::endl
                              << INFO_TAG_ << "Number of records  with pair: " << recs_with_pair_ << std::endl
                              << INFO_TAG_ << "Number of non-null annotations (searched): " << recs_with_non_null_annot_ << std::endl
                              << INFO_TAG_ << "Number of hits: " << hits_ << std::endl;
                }
        };

        /*! \brief Callback that compares annotations to a string
         */
        class SearchStringCallback : public BaseSearchCallback {
            private:
                /*! Stores string query for comparison in callbacks */
                std::string string_query_;

            public:
                SearchStringCallback(std::string query, const char* invert_search_str, const char* location_str) :
                    BaseSearchCallback(invert_search_str, location_str),
                    string_query_(std::move(query))
                {
                }

                virtual void foundAnnotationRecord(annotation_t* annotation) override {
                    handleProgressOutput_(annotation->time_Start);
                    ++recs_viewed_;
                    ++recs_with_annot_;

                    if (annotation->time_Start > search_end_ || annotation->time_End < search_start_) {
                        return;
                    }

                    if (!locations_.empty() && locations_.count(annotation->location_ID) == 0) {
                        return;
                    }

                    std::string_view annt_string(annotation->annt);
                    if (!annt_string.empty()) {
                        ++recs_with_non_null_annot_;
                        if (invert_search_) {
                            // Inverted search for string keys is a FULL-STRING match
                            if (annt_string != string_query_) {
                                handleResultOutput_(annotation);
                                ++hits_;
                            }
                        }
                        else {
                            size_t position = annt_string.find(string_query_);
                            if (position != std::string::npos) {
                                handleResultOutput_(annotation);
                                ++hits_;
                            }
                        }
                    }
                }

                virtual void foundInstRecord(instruction_t*) override {
                    ++recs_viewed_;
                    ++recs_with_ins_;
                }

                virtual void foundMemRecord(memoryoperation_t*) override {
                    ++recs_viewed_;
                    ++recs_with_mem_;
                }

                virtual void foundPairRecord(pair_t* pair) override {
                    handleProgressOutput_(pair->time_Start);
                    ++recs_viewed_;
                    ++recs_with_pair_;

                    if (pair->time_Start > search_end_ || pair->time_End < search_start_) {
                        return;
                    }

                    if (!locations_.empty() && locations_.count(pair->location_ID) == 0) {
                        return;
                    }

                    const auto annt_string = formatPairAsAnnotation(pair);
                    if (!annt_string.empty()) {
                        ++recs_with_non_null_annot_;
                        if (invert_search_) {
                            // Inverted search for string keys is a FULL-STRING match
                            if (annt_string != string_query_) {
                                handleResultOutput_(pair, annt_string);
                                ++hits_;
                            }
                        }
                        else {
                            size_t position = annt_string.find(string_query_);
                            if (position != std::string::npos) {
                                handleResultOutput_(pair, annt_string);
                                ++hits_;
                            }
                        }
                    }
                }
        };

        /*! \brief Callback that compares annotations to regex
         */
        class SearchRegexCallback : public BaseSearchCallback {
            private:
                /*! Stores regular expression for comparison in callbacks */
                std::regex regular_expression_;

            public:
                SearchRegexCallback(const char* regex, const char* invert_search_str, const char* location_str) :
                    BaseSearchCallback(invert_search_str, location_str),
                    regular_expression_(regex)
                {
                }

                virtual void foundAnnotationRecord(annotation_t* annotation) override {
                    handleProgressOutput_(annotation->time_Start);
                    ++recs_viewed_;
                    ++recs_with_annot_;
                    if (!locations_.empty() && locations_.count(annotation->location_ID) == 0) {
                        return;
                    }
                    if (annotation->time_Start > search_end_ || annotation->time_End < search_start_) {
                        return;
                    }
                    if (strlen(annotation->annt)) {
                        ++recs_with_non_null_annot_;
                        if ((!invert_search_) == std::regex_search(annotation->annt, std::regex(regular_expression_))) {
                            handleResultOutput_(annotation);
                            ++hits_;
                        }
                    }
                }

                virtual void foundInstRecord(instruction_t*) override {
                    ++recs_viewed_;
                    ++recs_with_ins_;
                }

                virtual void foundMemRecord(memoryoperation_t*) override {
                    ++recs_viewed_;
                    ++recs_with_mem_;
                }

                virtual void foundPairRecord(pair_t* pair) override {
                    handleProgressOutput_(pair->time_Start);
                    ++recs_viewed_;
                    ++recs_with_pair_;

                    if (pair->time_Start > search_end_ || pair->time_End < search_start_) {
                        return;
                    }

                    if (!locations_.empty() && locations_.count(pair->location_ID) == 0) {
                        return;
                    }

                    const auto annt_string = formatPairAsAnnotation(pair);
                    if (!annt_string.empty()) {
                        ++recs_with_non_null_annot_;
                        if ((!invert_search_) == std::regex_search(annt_string, std::regex(regular_expression_))) {
                            handleResultOutput_(pair, annt_string);
                            ++hits_;
                        }
                    }
                }
        };
    } //namespace pipeViewer
} //namespace sparta

class ConstructReaderException : public std::exception {
    private:
        std::string type_;

    public:
        ConstructReaderException(const char* type) :
            type_("unknown search type ")
        {
            type_ += type;
        }

        const char* what() const noexcept final {
            return type_.c_str();
        }
};

static sparta::pipeViewer::Reader constructReader(char** argv) {
    if (strcmp(argv[2], "string") == 0) {
        return sparta::pipeViewer::Reader::construct<sparta::pipeViewer::SearchStringCallback>(argv[1],
                                                                                               argv[3],
                                                                                               argv[4],
                                                                                               argv[7]);
    }

    if (strcmp(argv[2], "regex") == 0) {
        return sparta::pipeViewer::Reader::construct<sparta::pipeViewer::SearchRegexCallback>(argv[1],
                                                                                              argv[3],
                                                                                              argv[4],
                                                                                              argv[7]);
    }

    throw ConstructReaderException(argv[2]);
}

/*!
 * \brief Location search main.
 *
 * argv expected to contain
 * \li 1: Database prefix
 * \li 2: Search model ("regex" or "string")
 * \li 3: Search expression
 * \li 4: Invert Search (1=yes, 0=no). On regex, only mismatches will be in the result set. On
 *        string-match searches where this is 1, a FULL-STRING comparison will be performed and
 *        annotations which differ from the full query are matched. When 0 for string-match searches
 *        matches are found when the query string is within an annotation
 * \li 5: Search Start tick. -1 implies start of file
 * \li 6: Search End tick. -1 implies end of file
 * \li 7: Location filter. Comma-delimited list of location IDs. If empty, no filtering is done
 */
int main(int argc, char** argv) {
    if (argc != 8) {
        std::cout << "Usage: transactionsearch <transaction db> <string|regex> <query> <invert> <start tick> <end tick> <locations> " << std::endl;
        return 1;
    }

    try {
        sparta::pipeViewer::Reader reader = constructReader(argv);

        uint64_t search_start;
        uint64_t search_end;

        int64_t tmp = std::strtoll(argv[5], nullptr, 10);
        if (tmp < 0) {
            search_start = reader.getCycleFirst();
        }
        else {
            search_start = static_cast<uint64_t>(tmp);
        }

        tmp = std::strtoll(argv[6], nullptr, 10);
        if (tmp < 0) {
            search_end = reader.getCycleLast();
        }
        else {
            search_end = static_cast<uint64_t>(tmp);
        }

        // Reject negative-length searches.
        // Allow 0-length search or negative search if start is past eof-cycle
        // because it is common for eof-cycle+1 to be used as search_start and
        // end_cycle to be automatically computed as eof-cycle
        if (search_start < reader.getCycleLast() && search_end < search_start) {
            std::cerr << "negative search range [" << search_start << ", " << search_end << ")" << std::endl;
            return 1;
        }

        auto& cb = reader.getCallbackAs<sparta::pipeViewer::BaseSearchCallback>();
        cb.setSearchParams(search_start, search_end);

        cb.startProgress();
        reader.getWindow(search_start, search_end);
        cb.finishedProgress();
    }
    catch(const ConstructReaderException& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
