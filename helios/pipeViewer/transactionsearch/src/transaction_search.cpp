/*
 */

#include "transactiondb/src/Reader.hpp"
#include "transactiondb/src/PipelineDataCallback.hpp"
#include <iostream>
#include <string>
#include <set>
#include <boost/regex.hpp>


#define RESULT_TAG 'r'
#define PROGRESS_TAG 'p'
#define INFO_TAG 'i'

#define START_DELIMITER ':'

#define NUMBER_OF_PROGRESS_UPDATES 50

/*! Stores regular expression for comparison in callbacks */
boost::regex global_regular_expression;

/*! Stores string query for comparison in callbacks */
std::string global_string_query;

/*! Invert search */
bool global_invert_search;

/*! Location IDs to include in the search */
std::set<uint32_t> locations;

/*! Limit number of matches. 0 implies no limit*/
uint64_t march_limit;

/*! For update progress */
uint64_t last_step_number;
uint64_t search_start;
uint64_t search_end;
uint64_t search_width;
double search_update_stride;

/*! Incremented by one every regular expression match */
uint64_t global_hits;

uint64_t global_recs_viewed;
uint64_t global_recs_with_annot;
uint64_t global_recs_with_ins;
uint64_t global_recs_with_mem;
uint64_t global_recs_with_non_null_annot;
uint64_t global_recs_with_pair;

void handleProgressOutput(uint64_t current_time) {
    uint64_t step;
    if (search_update_stride != 0) {
        step = current_time / search_update_stride;
    } else {
        // Step is small that the search doesn't really need progress indicators
        step = 100000000;
    }
    if (step > last_step_number && current_time > search_start) {
        float fraction = (current_time-search_start)/(float)search_width;
        std::cout << PROGRESS_TAG << fraction << std::endl;
        last_step_number = step;
    }
}

/*!
 * \brief Writes a result to stdout in the form:
 * "<result tag><start time>,<end time>@<location ID><annotation delimiter><annotation>\n"
 */
void handleResultOutput(annotation_t* annotation) {
    std::cout << RESULT_TAG << annotation->time_Start << "," << annotation->time_End << "@" << annotation->location_ID;
    std::cout << START_DELIMITER;
    const char* pCur = annotation->annt;
    while(1){
        if(*pCur == 0){
            std::cout << '\0';
            break;
        }else if(*pCur == '\n' || *pCur == '\r'){
            std::cout << "\\n";
        }else{
            std::cout << *pCur;
        }
        pCur++;
    }
    std::cout << std::endl;
}

void handleResultOutput(pair_t* pairt) {
    std::cout << RESULT_TAG << pairt->time_Start << "," << pairt->time_End << "@" << pairt->location_ID << std::endl;
}

namespace sparta {
    namespace pipeViewer {
        /*! \brief Callback that compares annotations to regex using Boost library
         */
        class SearchStringCallback : public pipeViewer::PipelineDataCallback {
            virtual void foundAnnotationRecord(annotation_t* annotation) override {
                handleProgressOutput(annotation->time_Start);
                global_recs_viewed++;
                global_recs_with_annot++;
                if (locations.size() > 0 && locations.count(annotation->location_ID) == 0) {
                    return;
                }
                if (annotation->time_Start > search_end || annotation->time_End < search_start) {
                    return;
                }
                std::string annt_string(annotation->annt);
                if (annt_string.size()) {
                    global_recs_with_non_null_annot++;
                    if (global_invert_search) {
                        // Inverted search for string keys is a FULL-STRING match
                        if (annt_string != global_string_query) {
                            handleResultOutput(annotation);
                            global_hits++;
                        }
                    }
                    else {
                        size_t position = annt_string.find(global_string_query);
                        if (position != std::string::npos) {
                            handleResultOutput(annotation);
                            global_hits++;
                        }
                    }
                }
            }

            virtual void foundInstRecord(instruction_t*) override {
                global_recs_viewed++;
                global_recs_with_ins++;
            }

            virtual void foundMemRecord(memoryoperation_t*) override {
                global_recs_viewed++;
                global_recs_with_mem++;
            }

            virtual void foundPairRecord(pair_t*) override {
                global_recs_viewed++;
                global_recs_with_pair++;
            }
        };

        /*! \brief Callback that compares annotations to regex using Boost library
         */
        class SearchBoostRegexCallback : public pipeViewer::PipelineDataCallback {
            virtual void foundAnnotationRecord(annotation_t* annotation) override {
                handleProgressOutput(annotation->time_Start);
                global_recs_viewed++;
                global_recs_with_annot++;
                std::string annt_string(annotation->annt);
                if (locations.size() > 0 && locations.count(annotation->location_ID) == 0) {
                    return;
                }
                if (annotation->time_Start > search_end || annotation->time_End < search_start) {
                    return;
                }
                if (annt_string.size()) {
                    global_recs_with_non_null_annot++;
                    if ((!global_invert_search) == boost::regex_search(annt_string, boost::regex(global_regular_expression))) {
                        handleResultOutput(annotation);
                        global_hits++;
                    }
                }
            }

            virtual void foundInstRecord(instruction_t*) override {
                global_recs_viewed++;
                global_recs_with_ins++;
            }

            virtual void foundMemRecord(memoryoperation_t*) override {
                global_recs_viewed++;
                global_recs_with_mem++;
            }

            virtual void foundPairRecord(pair_t*) override {
                global_recs_viewed++;
                global_recs_with_pair++;
            }
        };
    } //namespace pipeViewer
} //namespace sparta

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
    sparta::pipeViewer::Reader* reader;
    sparta::pipeViewer::SearchStringCallback srcallback;
    sparta::pipeViewer::SearchBoostRegexCallback srcallback_boost;
    if (std::string(argv[2]) == std::string("string"))  {
        global_string_query = std::string(argv[3]);
        reader = new sparta::pipeViewer::Reader(argv[1], &srcallback);
    }
    else if (std::string(argv[2]) == std::string("regex"))  {
        global_regular_expression = boost::regex(argv[3]);
        reader = new sparta::pipeViewer::Reader(argv[1], &srcallback_boost);
    }
    else {
        std::cerr << "unknown search type " << argv[2] << std::endl;
        return 1;
    }
    global_invert_search = !!strtoull(argv[4], nullptr, 10);
    int64_t tmp = std::strtoll(argv[5], nullptr, 10);
    if (tmp < 0) {
        search_start = reader->getCycleFirst();
    }
    else {
        search_start = std::strtoull(argv[5], nullptr, 10);
    }

    tmp = std::strtoll(argv[6], nullptr, 10);
    if (tmp < 0) {
        search_end = reader->getCycleLast();
    }
    else {
        search_end = std::strtoull(argv[6], nullptr, 10);
    }

    std::string loc_id_string = argv[7];
    std::stringstream ss(loc_id_string);
    uint32_t id;
    while (ss >> id) {
        locations.insert(id);
        if (ss.peek() == ',') {
            ss.ignore();
        }
    }

    // Reject negative-length searches.
    // Allow 0-length search or negative search if start is past eof-cycle
    // because it is common for eof-cycle+1 to be used as search_start and
    // end_cycle to be automatically computed as eof-cycle
    if (search_start < reader->getCycleLast() && search_end < search_start) {
        std::cerr << "negative search range [" << search_start << ", " << search_end << ")" << std::endl;
        return 1;
    }

    search_width = search_end - search_start;
    search_update_stride = double(search_width) / NUMBER_OF_PROGRESS_UPDATES;

    std::cout << INFO_TAG << "search start:  " << search_start << std::endl;
    std::cout << INFO_TAG << "search locs:   (" <<  locations.size() << ") [";
    for (auto & lid : locations) {
        std::cout << lid << " ";
    }
    std::cout << "]" << std::endl;
    std::cout << INFO_TAG << "search invert: " << global_invert_search << std::endl;

    last_step_number = 0;
    reader->getWindow(search_start, search_end);

    std::cout << PROGRESS_TAG << 1 << std::endl; //finish off so progress bar doesn't hang
    //std::cout << "Expression: " << global_expression << " test_type: " << test_type_str << std::endl;
    std::cout << INFO_TAG << "Number of records: " << global_recs_viewed << std::endl;
    std::cout << INFO_TAG << "Number of records with annotation: " << global_recs_with_annot << std::endl;
    std::cout << INFO_TAG << "Number of records with instruction: " << global_recs_with_ins << std::endl;
    std::cout << INFO_TAG << "Number of records with memory: " << global_recs_with_mem << std::endl;
    std::cout << INFO_TAG << "Number of records  with pair: " << global_recs_with_pair << std::endl;
    std::cout << INFO_TAG << "Number of non-null annotations (searched): " << global_recs_with_non_null_annot << std::endl;
    std::cout << INFO_TAG << "Number of hits: " << global_hits << std::endl;
    return 0;
}
