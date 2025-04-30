// <BaseFormatter> -*- C++ -*-


/*!
 * \file BaseFormatter.cpp
 * \brief Static data for base report formatter
 */

#include "sparta/report/format/BaseFormatter.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/detail/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/type_index/type_index_facade.hpp>
#include <set>

// Formatters
#include "sparta/report/format/CSV.hpp"
#include "sparta/report/format/BasicHTML.hpp"
#include "sparta/report/format/Text.hpp"
#include "sparta/report/format/Gnuplot.hpp"
#include "sparta/report/format/PythonDict.hpp"
#include "sparta/report/format/JavascriptObject.hpp"
#include "sparta/report/format/JSON.hpp"
#include "sparta/report/format/JSON_reduced.hpp"
#include "sparta/report/format/JSON_detail.hpp"
#include "sparta/report/format/StatsMapping.hpp"
#include "sparta/report/format/BaseOstreamFormatter.hpp"

#if SIMDB_ENABLED
#include "simdb/sqlite/DatabaseManager.hpp"
#endif

namespace sparta {
    namespace report {
        namespace format {

const std::vector<FormatterFactory> BaseFormatter::FACTORIES = {
    // Plaintext. Default because it is first
    { {"txt", "text"},
      "Plaintext report output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new Text(r,fn); }
    },

    { {"html", "htm"},
      "Basic HTML Report Output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new BasicHTML(r,fn); }
    },

    { {"csv", "csv_cumulative"},
      "CSV Report Output (supports updating)",
      [](const Report* r, const std::string& fn) -> BaseFormatter* {
          return new CSV(r,fn, std::ios::trunc | std::ios::out); }
    },

    { {"js_json", "jsjson"},
      "JavaScript Object Notation Report Output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new JavascriptObject(r,fn); }
    },

    { {"python", "py"},
      "Python dictionary output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new PythonDict(r,fn); }
    },

    { {"json", "JSON"},
      "JavaScript Object Notation Report Output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new JSON(r,fn); }
    },

    { {"json_reduced", "JSON_reduced"},
      "JavaScript Object Notation Report Output with only stat values reported",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new JSON_reduced(r,fn); }
    },

    { {"json_detail", "JSON_detail"},
      "JavaScript Object Notation Report Output with only non-stat value information reported (desc, vis, etc...)",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new JSON_detail(r,fn); }
    },

    { {"gnuplot", "gplt"},
      "Gnuplot report output",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new Gnuplot(r,fn); }
    },

    { {"stats_mapping"},
      "Mapping from CSV column headers to Statistic Instance locations",
      [](const Report* r, const std::string& fn) -> BaseFormatter* { return new StatsMapping(r,fn); }
    },
    // End of list
    { {},
      "",
      [](const Report* r, const std::string& fn) -> BaseFormatter* {
          (void) r;(void) fn; return nullptr;
        }
    }
};


const FormatterFactory*
    BaseFormatter::determineFactory(const std::string& lower_filename,
                                    const std::string& format)
{
    std::string no_whitespace(format);
    boost::erase_all(no_whitespace, " ");

    std::vector<std::string> formats;
    boost::split(formats, no_whitespace, boost::is_any_of(","));

    if (formats.size() > 1) {
        std::set<const FormatterFactory*> formatters;
        for (auto & fmt : formats) {
            auto cumulative_idx = fmt.find("_cumulative");
            if (cumulative_idx != std::string::npos) {
                fmt.erase(cumulative_idx);
            }
            const FormatterFactory * formatter =
                BaseFormatter::determineFactory(lower_filename, fmt);
            formatters.insert(formatter);
        }
        if (formatters.size() != 1) {
            throw SpartaException(
                "Comma-separated lists of formats must only be of one "
                "format type, such as 'csv, csv_cumulative'. Format "
                "given: '") << format << "'";
        }
        return *formatters.begin();
    }

    auto& facts = sparta::report::format::BaseFormatter::FACTORIES;

    for(auto& factitr : facts){
        for(const std::string& ext : factitr.exts){
            // Note: ext is assumed lower case
            // rd.format is guaranteed lowercase from ReportDescriptor
            if(format != ""){
                if(format == ext){
                    return &factitr;
                }
            }else{
                // Compare '.' + extension against end of filename
                if(lower_filename.rfind(std::string(".") + ext) \
                   == lower_filename.size() - 1 - ext.size()){
                    return &factitr;
                }
            }
        }
    }

    // Ensure sane factory count
    sparta_assert(facts.size() > 0, "Report formatter FACTORIES list contains no factories");

    // Get default factory (index 0)
    return &facts[0];
}

bool BaseFormatter::isValidFormatName(const std::string& format)
{
    std::string no_whitespace(format);
    boost::erase_all(no_whitespace, " ");

    std::vector<std::string> formats;
    boost::split(formats, no_whitespace, boost::is_any_of(","));

    if (formats.size() > 1) {
        bool is_valid = true;
        for (const auto & fmt : formats) {
            is_valid &= BaseFormatter::isValidFormatName(fmt);
        }
        return is_valid;
    }

    const auto& facts = sparta::report::format::BaseFormatter::FACTORIES;

    for(auto& factitr : facts){
        for(const std::string& ext : factitr.exts){
            // Note: ext is assumed lower case
            // rd.format is guaranteed lowercase from ReportDescriptor
            if(format != "" && format == ext){
                    return true;
            }
        }
    }
    return false;
}

void BaseFormatter::configSimDbReport(
    simdb::DatabaseManager* db_mgr,
    const int report_desc_id,
    const int report_id)
{
#if SIMDB_ENABLED
    // Note that in order to keep the schema data types uniform,
    // we write every bit of metadata as a string. The python
    // export module will convert the data types to the correct
    // types when it writes formatted reports after simulation.
    auto meta_kv_pairs = metadata_kv_pairs_;

    const std::string pretty_print = pretty_print_enabled_ ? "true" : "false";
    meta_kv_pairs["PrettyPrint"] = pretty_print;

    const std::string omit_zero = zero_si_values_omitted_ ? "true" : "false";
    meta_kv_pairs["OmitZeros"] = omit_zero;

    // Add any formatter-specific metadata
    auto extra_meta_kv_pairs = getExtraSimDbMetadata_();
    for (const auto& pair : extra_meta_kv_pairs) {
        meta_kv_pairs[pair.first] = pair.second;
    }

    for (const auto& pair : meta_kv_pairs) {
        db_mgr->INSERT(
            SQL_TABLE("ReportMetadata"),
            SQL_COLUMNS("ReportDescID", "ReportID", "MetaName", "MetaValue"),
            SQL_VALUES(report_desc_id, report_id, pair.first, pair.second));
    }
#else
    (void) db_mgr;
    (void) report_desc_id;
    (void) report_id;
#endif
}

} // namespace format
} // namespace report
} // namespace sparta
