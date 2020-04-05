// <TableTypedefs> -*- C++ -*-

#pragma once

#include "simdb_fwd.hpp"

#include <functional>
#include <vector>
#include <map>
#include <string>

namespace simdb {

//! Table summary functions are applied to the table's columns
//! that support summarization. The input arguments to this
//! function are as follows:
//!
//!   std::vector<double> vals       ->  All of the column's record
//!                                      values that have not yet
//!                                      been summarized.
//!
//!   ObjectProxy * current_summary  ->  Existing record holding all
//!                                      of the summarized table values
//!                                      for this column.
//!
//! To illustrate the ObjectProxy input argument, say we wanted
//! to capture the average value of all records in a table for
//! each column in that table. One way to do this would be be
//! to gather all records in the table since the start of the
//! program, and give all of them to the registered averaging
//! function. But this gets slower each time the TableRef's
//! captureSummary() method is called, since the table itself
//! is presumably growing over time.
//!
//! Here is another way which lets the averaging function make
//! use of previously-summarized records:
//!
//!    double calcAverage(const std::vector<double> & new_vals,
//!                       const ObjectProxy * existing_summary)
//!    {
//!        double avg = NAN;
//!        if (new_vals.empty()) {
//!            if (existing_summary == nullptr) {
//!                return avg;
//!            }
//!            avg = existing_summary->getPropertyDouble("avg");
//!        } else {
//!            double total_avg_numerator = std::accumulate(
//!                new_vals.begin(), new_vals.end(), 0, std::plus<double>());
//!            size_t total_avg_denominator = new_vals.size();
//!            if (existing_summary) {
//!                total_avg_numerator += existing_summary->getPropertyDouble("avg");
//!                total_avg_denominator += existing_summary->getPropertyDouble("count");
//!            }
//!            avg = (total_avg_numerator / total_avg_denominator);
//!        }
//!        return avg;
//!    }
//!
//!
//! This could be registered with the schema's TableSummaries
//! object like so:
//!
//!    simdb::TableSummaries summary;
//!    summary.define("avg", &calcAverage)
//!           .define("count", [](const std::vector<double> & vals,
//!                               const ObjectProxy * cur_summary) {
//!                                   double count = vals.size();
//!                                   if (cur_summary) {
//!                                       count += cur_summary->getPropertyDouble("count");
//!                                   }
//!                                   return count;
//!                            });
//!
using SummaryFunction = std::function<double(const std::vector<double> & vals)>;
  
//! Map of table summary calculation functions by summary
//! function name. For instance:
//!
//!    {{ "min", [](const std::vector<double> & vals) {
//!                  return vals.empty() ? NAN :
//!                    *std::min_element(vals.begin(), vals.end());
//!                },
//!     { "max", [](const std::vector<double> & vals) {
//!                  return vals.empty() ? NAN :
//!                    *std::max_element(vals.begin(), vals.end());
//!                }}
//!
using NamedSummaryFunctions = std::map<std::string, SummaryFunction>;

}

