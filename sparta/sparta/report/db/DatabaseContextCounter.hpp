// <DatabaseContextCounter> -*- C++ -*-

#ifndef __SPARTA_SIMDB_CONTEXT_COUNTER_H__
#define __SPARTA_SIMDB_CONTEXT_COUNTER_H__

#include <unordered_set>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"

namespace sparta {

class Report;
class StatisticInstance;

namespace db {

/*!
 * \brief This class mimics sparta::ContextCounter<T> but is
 * only used outside of a running simulation when producing
 * report files from database records.
 *
 * Objects like Report and StatisticInstance are recreated
 * during SimDB-driven report generation, *without* needing
 * to actually build TreeNode's of any kind. Unlike simple
 * StatisticInstance's which wrap Counters or Parameters,
 * this class is more involved due to the fact that reports
 * have very specific rules for how they write out their
 * ContextCounter's to the formatted report file.
 *
 * In other words, DatabaseContextCounter is only relatively
 * complex since its counterpart sparta::ContextCounter<T>
 * has relatively complex formatting rules, and SimDB-driven
 * reports need to exactly match simulation-driven reports.
 */
class DatabaseContextCounter
{
public:
    //! This class will take care of writing out the contents
    //! of the sub-statistic SI's. The caller who is using
    //! one of these objects needs to be able to ask if a
    //! particular SI has already been serialized.
    using UnprintableSIs = std::unordered_set<const StatisticInstance*>;

    //! Construction with the ContextCounter root SI and its
    //! sub-SI's that live under the root. Here is what the
    //! original simulation may have had:
    //!
    //!    SI (StatisticDef)
    //!        SI (CounterBase)
    //!        SI (CounterBase)
    //!
    //! The corresponding DatabaseContextCounter would then be:
    //!
    //!    SI (cc_node)
    //!        SI (unprintable_sis[*])
    //!        SI (unprintable_sis[*])
    //!
    //! Since we don't have a tree outside of a simulation, we
    //! cannot create StatisticDef's or CounterBase's. Or at
    //! least we *should not* have to recreate parts of the
    //! original tree, since everything we need to know about
    //! report/SI hierarchy, along with all metadata and SI
    //! values, is right there in the database. Reconstructing
    //! a tree using "real" components like StatisticDef /
    //! StatisticSet / InstrumentationNode / ContextCounter
    //! is very unnecessary overhead, and very cumbersome code
    //! to maintain.
    DatabaseContextCounter(
        const StatisticInstance * cc_node,
        std::shared_ptr<UnprintableSIs> & unprintable_sis);

    //! Analogous to TreeNode::getName()
    const std::string & getName() const;

    //! Analogous to InstrumentationNode::groupedPrinting()
    bool groupedPrinting(
        const std::vector<const StatisticInstance*> & sub_stats,
        std::set<const void*> & dont_print_these,
        void * grouped_json,
        void * doc) const;

    //! Analogous to InstrumentationNode::groupedPrintingReduced()
    bool groupedPrintingReduced(
        const std::vector<const StatisticInstance*> & sub_stats,
        std::set<const void*> & dont_print_these,
        void * grouped_json,
        void * doc) const;

    //! Analogous to InstrumentationNode::groupedPrintingDetail()
    bool groupedPrintingDetail(
        const std::vector<const StatisticInstance*> & sub_stats,
        std::set<const void*> & dont_print_these,
        void * grouped_json,
        void * doc) const;

private:
    //! Analogous to ContextCounter<T>::ContextCounterInfo
    struct ContextCounterInfo {
        std::string name_;
        std::string desc_;
        InstrumentationNode::visibility_t vis_ = 0;
        double val_ = 0;
        const void * ctx_addr_ = nullptr;
    };
    mutable std::vector<ContextCounterInfo> ctx_info_;

    //! Analogous to ContextCounter<T> (StatisticDef) 'this' pointer
    const StatisticInstance * cc_node_ = nullptr;

    //! Analogous to ContextCounter<T>::internal_counters_
    std::shared_ptr<UnprintableSIs> unprintable_sis_;

    //! Analogous to TreeNode::getDesc()
    std::string cc_desc_;

    //! Analogous to TreeNode::getName()
    std::string cc_name_;

    //! Mimics ContextCounter<T>::extractCtxInfo_
    void extractCtxInfo_(
        const std::vector<const StatisticInstance*> & sub_stats) const;

    //! Mimics __groupedPrinting() free function in sparta/ContextCounter.h
    bool groupedPrinting_(
        std::set<const void*> & dont_print_these,
        void * grouped_json_, void * doc_,
        const std::vector<ContextCounterInfo> & ctx_info,
        const std::string & aggregate_desc,
        const InstrumentationNode::visibility_t aggregate_vis) const;

    //! Mimics __groupedPrintingReduced() free function in sparta/ContextCounter.h
    bool groupedPrintingReduced_(
        std::set<const void*> & dont_print_these,
        void * grouped_json_, void * doc_,
        const std::vector<ContextCounterInfo> & ctx_info) const;

    //! Mimics __groupedPrintingDetail() free function in sparta/ContextCounter.h
    bool groupedPrintingDetail_(
        std::set<const void*> & dont_print_these,
        void * grouped_json_, void * doc_,
        const std::vector<ContextCounterInfo> & ctx_info) const;

    //! At the end of the various "grouped printing" methods, tack on
    //! any "unprintable SI(s)" into the "dont_print_these" sets.
    void appendUnprintablesToSet_(
        std::set<const void*> & dont_print_these) const;
};

} // namespace db
} // namespace sparta

#endif
