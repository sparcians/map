// <JSON_detail> -*- C++ -*-


/*!
 * \file JSON_detail.hpp
 * \brief JSON Report output formatter for stat's non value information
 */

#pragma once

#include <iostream>
#include <sstream>
#include <math.h>
#include <map>
#include <boost/lexical_cast.hpp>

#include "sparta/report/format/BaseOstreamFormatter.hpp"
#include "sparta/report/db/DatabaseContextCounter.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    namespace report
    {
        namespace format
        {

using StringPair = std::pair<std::string, std::string>;
/*!
 * \brief Struct to contain non value stat info
 */
struct info_data {
    std::string name;
    std::string desc;
    uint64_t vis;
    uint64_t n_class;
    std::vector<StringPair> metadata;
};

/*!
 * \brief Map to contain the stat information obtained from each report/subreport
 */
static std::map<std::string, std::list<info_data> > detail_json_map;


/*!
 * \brief Report formatter for JSON output
 * \note Non-Copyable
 */
class JSON_detail : public BaseOstreamFormatter
{
private:
    // JavaScript JSON format is considered as version 1.0.
    const std::string version_ = "2.1";

    // Clear the detail_json_map static variable before
    // report validation is performed
    virtual void doPostProcessingBeforeReportValidation() override final {
        detail_json_map.clear();
    }

public:
    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param output Osteram to write to when write() is called
     */
    JSON_detail(const Report* r, std::ostream& output) :
        BaseOstreamFormatter(r, output)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     * \param filename File which will be opened and appended to when write() is
     * called
     * \param mode. Optional open mode. Should be std::ios::out or
     * std::ios::app. Other values cause undefined behavior
     */
    JSON_detail(const Report* r,
        const std::string& filename,
        std::ios::openmode mode=std::ios::app) :
        BaseOstreamFormatter(r, filename, mode)
    { }

    /*!
     * \brief Constructor
     * \param r Report to provide output formatting for
     */
    JSON_detail(const Report* r) :
        BaseOstreamFormatter(r)
    { }

    /*!
     * \brief Return the JSON version used
     */
    std::string getVersion() const{
        return version_;
    }

    /*!
     * \brief Virtual Destructor
     */
    virtual ~JSON_detail()
    {
    }

protected:

    //! \name Output
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Writes a header to some output based on the report
     */
    virtual void writeHeaderToStream_(std::ostream& out) const override  {
        (void) out;
    }

    /*!
     * \brief Writes the content of this report to some output;
     */
    virtual void writeContentToStream_(std::ostream& out) const override
    {
        // collectDictContents_ will store the stat information into detail_json_map while going through report/subreports
        collectDictContents_(report_, 1, "");
        out << "{ \"_id\": \" \",\n";
        out << std::string(2, ' ') << "\"report_metadata\": ";
        if (metadata_kv_pairs_.empty()) {
            out << "{},\n";
        } else {
            out << "{\n";
            writeReportMetadataToStreamWithIndent_(out, 4, metadata_kv_pairs_);
            out << "\n" << std::string(2, ' ') << "},\n";
        }
        out << std::string(2, ' ') << "\"stat_info\": {\n";

        // Dump out the data from detail_json_map
        for (std::map<std::string, std::list<info_data> >::iterator it = detail_json_map.begin(); it != detail_json_map.end();) {
            out << std::string(4, ' ') << "\"" << it->first << "\": [\n";
            for (auto& lit : it->second) {
                out << std::string(6, ' ') << "{ \"name\": \"" << lit.name << "\",\n";
                out << std::string(8, ' ') << "\"desc\": \"" << lit.desc << "\",\n";
                out << std::string(8, ' ') << "\"vis\": \"" << lit.vis << "\",\n";
                out << std::string(8, ' ') << "\"class\": \"" << lit.n_class << "\""; // TODO: Need to set actual class level
                if (!lit.metadata.empty()) {
                    out << ",";
                }
                out << "\n";

                //Add any extra metadata that was given to the InstrumentationNode
                for (size_t md_index = 0; md_index < lit.metadata.size(); ++md_index) {
                    const auto & pair = lit.metadata[md_index];
                    try {
                        (void)boost::lexical_cast<double>(pair.second);
                        out << std::string(8, ' ') << "\"" << pair.first << "\": " << pair.second;
                    } catch (const boost::bad_lexical_cast &) {
                        out << std::string(8, ' ') << "\"" << pair.first << "\": \"" << pair.second << "\"";
                    }
                    if (md_index != lit.metadata.size() - 1) {
                        out << ",";
                    }
                    out << "\n";
                }
                out << std::string(6, ' ') << "}";
                if (&lit != &(it->second).back()) {
                    out << ",\n";
                } else {
                    out << "\n";
                }
            }
            out << std::string(4, ' ') << "]";
            if (++it != detail_json_map.end()){
                out << ",\n";
            } else {
                out << "\n";
            }
        }
        out << std::string(2, ' ') << "}\n";
        out << "}\n";
        out << std::endl;
    }


    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Helper function to flatten a hierarchical name
     * \note This removes everything before the last dot
     */
    std::string flattenReportName(std::string full_name) const{
        std::string local_name = full_name;
        std::size_t last_dot_idx = full_name.find_last_of(".");
        if (last_dot_idx != std::string::npos){
            local_name = full_name.substr(last_dot_idx+1);
        }
        return local_name;
    }

    /*!
     * \brief Collect the non value stat information and store into dtail_json_map
     */
    int collectDictContents_(const Report* r, int idx, std::string p_name) const
    {
        std::string local_name = "";
        if (p_name.compare("") == 0 || p_name.compare("@ on _SPARTA_global_node_") == 0) {
            local_name = flattenReportName(r->getName());
        } else {
            local_name = p_name + "." + flattenReportName(r->getName());
        }

        auto extract_stat = [&local_name](const Report::stat_pair_t & si) {
            std::string full_name = local_name + "." + si.first;
            std::string desc = si.second.getDesc(false);
            boost::replace_all(desc, "\"", "\\\"");
            struct info_data tmp;
            tmp.name = full_name;
            tmp.desc = desc;
            tmp.vis = si.second.getVisibility();
            tmp.n_class = si.second.getClass();
            const StatisticDef * stat_defn = si.second.getStatisticDef();
            if (stat_defn != nullptr) {
                tmp.metadata = stat_defn->getMetadata();
            } else {
                tmp.metadata = si.second.getMetadata();
            }
            return tmp;
        };

        const Report::SubStaticticInstances & sub_stats = r->getSubStatistics();
        const Report::DBSubStatisticInstances & db_sub_stats = r->getDBSubStatistics();

        std::set<const void*> dont_print_these;
        std::set<const void*> db_dont_print_these;
        for (const Report::stat_pair_t& si : r->getStatistics()) {
            if(si.first != ""){
                const StatisticInstance * stat_inst = &si.second;
                const StatisticDef * stat_defn = si.second.getStatisticDef();
                const CounterBase * ctr = si.second.getCounter();
                const ParameterBase * prm = si.second.getParameter();
                sparta_assert(static_cast<const void*>(this) != static_cast<const void*>(ctr));
                sparta_assert(static_cast<const void*>(this) != static_cast<const void*>(prm));

                auto sub_stat_iter = sub_stats.find(stat_defn);
                const bool valid_stat_def = (stat_defn != nullptr);
                const bool has_valid_sub_stats =
                    (valid_stat_def && sub_stat_iter != sub_stats.end());

                auto db_sub_stat_iter = db_sub_stats.find(stat_inst);
                const bool has_valid_db_sub_stats = (db_sub_stat_iter != db_sub_stats.end());

                if (has_valid_sub_stats && stat_defn->groupedPrintingDetail(sub_stat_iter->second,
                                                                            dont_print_these,
                                                                            nullptr,
                                                                            nullptr)) {
                    detail_json_map[si.first].push_back(extract_stat(si));
                    continue;
                }
                if (dont_print_these.count(ctr) > 0 || dont_print_these.count(prm) > 0) {
                    continue;
                }
                dont_print_these.clear();

                if (has_valid_db_sub_stats) {
                    const std::shared_ptr<db::DatabaseContextCounter> & db_ctx_ctr =
                        db_sub_stat_iter->second.first;

                    const std::vector<const StatisticInstance*> & db_sub_sis =
                        db_sub_stat_iter->second.second;

                    if (db_ctx_ctr->groupedPrintingDetail(db_sub_sis,
                                                          db_dont_print_these,
                                                          nullptr,
                                                          nullptr))
                    {
                        detail_json_map[si.first].push_back(extract_stat(si));
                        continue;
                    }
                }
                if (db_dont_print_these.count(stat_inst) > 0) {
                    continue;
                }
                db_dont_print_these.clear();

                detail_json_map[si.first].push_back(extract_stat(si));
            }
        }

        // Go through all subreports
        for (const Report& sr : r->getSubreports()) {
            collectDictContents_(&sr, idx+1, local_name);
        }

        return idx;
    }

    //! \brief Write out all report metadata key-value pairs to the ostream
    void writeReportMetadataToStreamWithIndent_(
        std::ostream& out,
        const size_t indent,
        const std::map<std::string, std::string> & metadata) const
    {
        if (metadata.empty()) {
            return;
        }
        const size_t num_metadata = metadata.size();
        size_t num_written_metadata = 0;
        for (const auto & md : metadata) {
            out << std::string(indent, ' ') << "\"" << md.first << "\": \"" << md.second << "\"";
            ++num_written_metadata;
            if (num_written_metadata <= num_metadata - 1) {
                out << ",\n";
            }
        }
    }

};

//! \brief JSON stream operator
inline std::ostream& operator<< (std::ostream& out, JSON_detail & f) {
    out << &f;
    return out;
}

        } // namespace format
    } // namespace report
} // namespace sparta
