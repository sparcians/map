// <TableSummaries> -*- C++ -*-

#pragma once

#include "simdb/schema/Schema.hpp"
#include "simdb/schema/TableTypedefs.hpp"

#include <string>

namespace simdb {

/*!
 * \class TableSummaries
 * \brief This utility lets SimDB users define named aggregation
 * methods to summarize table columns. TableRef and ObjectManager
 * have APIs that let you trigger a table summary, which invokes
 * all registered aggregation methods one by one. Things like min,
 * max, avg, etc. can be captured easily this way.
 */
class TableSummaries
{
public:
    TableSummaries & define(const std::string & algo_name,
                            const SummaryFunction & algo_impl)
    {
        named_summary_fcns_[algo_name] = algo_impl;
        return *this;
    }

    const NamedSummaryFunctions & getSummaryAlgos() const {
        return named_summary_fcns_;
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
    TableSummaries &>::type
    excludeTables(const T & table_name) {
        excluded_tables_.insert(table_name);
        return *this;
    }

    template <typename T>
    typename std::enable_if<
        std::is_same<typename std::decay<T>::type, const char*>::value,
    TableSummaries &>::type
    excludeTables(T table_name) {
        excluded_tables_.insert(table_name);
        return *this;
    }

    template <typename T, typename... Args>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
    TableSummaries &>::type
    excludeTables(const T & table_name,
                  Args &&... args) {
        excludeTables(table_name);
        excludeTables(std::forward<Args>(args)...);
        return *this;
    }

    template <typename T, typename... Args>
    typename std::enable_if<
        std::is_same<typename std::decay<T>::type, const char*>::value,
    TableSummaries &>::type
    excludeTables(T table_name,
                  Args &&... args) {
        excludeTables(table_name);
        excludeTables(std::forward<Args>(args)...);
        return *this;
    }

private:
    NamedSummaryFunctions named_summary_fcns_;
    std::unordered_set<std::string> excluded_tables_;

    friend class DbConnProxy;
    friend class Schema;
};

}

