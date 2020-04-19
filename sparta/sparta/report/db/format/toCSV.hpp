// <toCSV> -*- C++ -*-

#pragma once

#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta {
namespace db {
namespace format {

/*!
 * \brief Utility which writes out an entire timeseries
 * object found in a database file to the provided filename.
 */
inline void toCSV(ReportTimeseries * ts, const std::string & filename)
{
    std::ostream * out_ptr = nullptr;
    std::ofstream fout;

    if (filename == "1") {
        //SPARTA uses the "filename" of "1" to indicate
        //writing to stdout.
        out_ptr = &std::cout;
    } else {
        fout.open(filename);
        if (!fout) {
            throw SpartaException("Unable to open file for write: '")
                << filename << "'";
        }
        out_ptr = &fout;
    }
    std::ostream & out = *out_ptr;

    const auto & header = ts->getHeader();

    // Write all header comments. These are written in this order (as
    // opposed to alphabetical, or any other order) so that
    // database-regenerated CSV report files match exactly what you
    // would get using legacy CSV formatters during simulation
    // (BaseFormatter).
    //
    // We're on the first line of metadata:
    //
    //   # report="stats.yaml on top.core0",start=4436,end=SIMULATION_END,report_format=csv
    //   # enabled=none,period=5,type=nanoseconds,counter=NS,terminate=none,warmup=1202
    //
    const std::string raw_header = header.getStringMetadata("RawHeader");
    if (!raw_header.empty() && raw_header != "unset") {
        out << raw_header;
    } else {
        out << "# report=\"" << header.getReportName() << "\",";
        const auto sim_start = header.getReportStartTime();
        out << "start=" << sim_start << ",";
        out << "end=";
        const auto sim_end = header.getReportEndTime();
        if (sim_end == std::numeric_limits<uint64_t>::max()) {
            out << "SIMULATION_END";
        } else {
            out << sim_end;
        }

        //Add any additional metadata to the CSV header
        std::map<std::string, std::string> string_metadata =
            header.getAllStringMetadata();

        // There is one piece of metadata that we *always* know is
        // going to be in the database, because SimDB is writing it
        // regardless. Is is the "report_format" string. We write this
        // at the end of the first line of metadata because that's
        // where the BaseFormatter always puts it.
        auto format_iter = string_metadata.find("report_format");
        sparta_assert(format_iter != string_metadata.end());
        out << ",report_format=" << format_iter->second;
        string_metadata.erase(format_iter);
        out << "\n";

        //The "Elapsed" metadata is only used in certain other
        //non-timeseries report formats. But legacy CSV does not
        //use it, so we'll discard it here.
        format_iter = string_metadata.find("Elapsed");
        if (format_iter != string_metadata.end()) {
            string_metadata.erase(format_iter);
        }

        //All other metadata name-value pairs get written on
        //their own line near the top of the CSV file. They
        //all go in alphabetical order. We're on the second
        //line of metadata:
        //
        //   # report="stats.yaml on top.core0",start=4436,end=SIMULATION_END,report_format=csv
        //   # enabled=none,period=5,type=nanoseconds,counter=NS,terminate=none,warmup=1202
        //
        const size_t num_string_metadata = string_metadata.size();
        if (num_string_metadata > 0) {
            out << "# ";
            if (num_string_metadata == 1) {
                out << string_metadata.begin()->first << "="
                    << string_metadata.begin()->second << "\n";
            } else {
                auto md_iter = string_metadata.begin();
                for (size_t md_idx = 0; md_idx < num_string_metadata-1; ++md_idx) {
                    out << md_iter->first << "=" << md_iter->second << ",";
                    ++md_iter;
                }
                out << md_iter->first << "=" << md_iter->second << "\n";
            }
        }
    }

    //Write the SI locations ("scheduler.ticks,scheduler.seconds,...")
    out << header.getCommaSeparatedSILocations() << "\n";

    //Read the SI data from the database blob by blob. The RangeIterator
    //class will handle the internals for maximum performance, whether
    //the SI values were compressed or not, saved in row-major or column-
    //major format, etc.
    ReportTimeseries::RangeIterator iterator(*ts);
    static const auto start = std::numeric_limits<uint64_t>::min();
    static const auto end = std::numeric_limits<uint64_t>::max();
    iterator.positionRangeAroundSimulatedPicoseconds(start, end);

    while (iterator.getNext()) {
        const double * si_values_ptr = iterator.getCurrentSliceDataValuesPtr();
        const size_t num_si_values = iterator.getCurrentSliceNumDataValues();

        sparta_assert(num_si_values > 0 && si_values_ptr != nullptr);
        if (num_si_values == 1) {
            out << Report::formatNumber(*si_values_ptr) << "\n";
        } else {
            for (size_t si_idx = 0; si_idx < num_si_values-1; ++si_idx) {
                out << Report::formatNumber(*(si_values_ptr+si_idx)) << ",";
            }
            out << Report::formatNumber(*(si_values_ptr+num_si_values-1)) << "\n";
        }
    }
}

} // namespace format
} // namespace db
} // namespace sparta

