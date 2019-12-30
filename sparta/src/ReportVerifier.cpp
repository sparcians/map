// <ReportVerifier> -*- C++ -*-

#include "sparta/report/db/ReportVerifier.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <map>
#include <utility>
#include <iostream>
#include <iterator>

#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/utils/uuids.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/report/format/BaseFormatter.hpp"
#include "sparta/report/db/format/toCSV.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/schema/DatabaseTypedefs.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/report/db/ReportTimeseries.hpp"

namespace sparta {
namespace db {

//! Static initializations
std::string ReportVerifier::verif_results_dir_ = "AccuracyCheckedDBs";
bool ReportVerifier::verif_results_dir_is_changeable_ = true;

//! Ask for the verification results directory. It could
//! be the default "AccuracyCheckedDBs" or it could have
//! been changed via writeVerifResultsTo().
const std::string & ReportVerifier::getVerifResultsDir()
{
    sparta_assert(!ReportVerifier::verif_results_dir_.empty());
    return ReportVerifier::verif_results_dir_;
}

//! Redirect the verification artifacts to be written
//! somewhere other than the REPORT_VERIF_DEFAULT_DIR.
//! This throws an exception if the post-simulation
//! verification process has already begun.
void ReportVerifier::writeVerifResultsTo(const std::string & dir)
{
    if (dir.empty()) {
        std::cout << "  [simdb] ReportVerifier::writeVerifResultsTo() called "
                  << "with an empty directory string. This will be ignored."
                  << std::endl;
        return;
    }

    if (!ReportVerifier::verif_results_dir_is_changeable_ &&
        dir != ReportVerifier::getVerifResultsDir())
    {
        throw SpartaException("Redirecting the report verification ")
            << "artifacts directory is disallowed after the verification "
            << "process has already begun.";
    } else if (dir == ReportVerifier::getVerifResultsDir()) {
        return;
    }

    //For testing purposes, we may assign a verification
    //directory of "@", which means that we should create
    //a randomly-generated directory name.
    if (dir == "@") {
        std::ostringstream oss;
        oss << simdb::generateUUID();
        ReportVerifier::verif_results_dir_ = oss.str();
    } else {
        ReportVerifier::verif_results_dir_ = dir;
    }
}

//! Turn the provided timeseries report file (baseline) into
//! an equivalent report file from the appropriate database
//! records. Use a SpartaTester to check for file differences,
//! and give those differences back to the caller in the
//! "failure_differences" output argument.
bool verifyTimeseriesReport(
    const std::string & yaml_dest_file,
    const std::string & simdb_dest_file,
    const simdb::DatabaseID timeseries_id,
    const simdb::ObjectManager & sim_db,
    std::string & failure_differences)
{
    std::unique_ptr<simdb::ObjectRef> timeseries_ref =
        sim_db.findObject("Timeseries", timeseries_id);

    if (timeseries_ref == nullptr) {
        throw SpartaException("Unable to locate 'Timeseries' record with Id ")
            << timeseries_id;
    }

    ReportTimeseries timeseries(std::move(timeseries_ref));
    format::toCSV(&timeseries, simdb_dest_file);

    std::ostringstream cerr;
    std::unique_ptr<SpartaTester> tester =
        SpartaTester::makeTesterWithUserCError(cerr);

    tester->expectFilesEqual(
        yaml_dest_file, simdb_dest_file,
        true, __LINE__, __FILE__, false);

    failure_differences = cerr.str();
    if (!failure_differences.empty()) {
        std::ifstream fin_expected(yaml_dest_file);
        const std::string contents_expected{
            std::istreambuf_iterator<char>{fin_expected}, {}};

        std::ifstream fin_actual(simdb_dest_file);
        const std::string contents_actual{
            std::istreambuf_iterator<char>{fin_actual}, {}};

        //Until we can be 100% sure that our SimInfo table has enough information
        //in it to exactly reproduce verification failures, we'll just capture a
        //deep copy of the two files (baseline and SimDB-generated) and stash them
        //back in the database. This verification step is disabled in production
        //simulators.
        auto failed_deep_copy_tbl = sim_db.getTable("ReportVerificationDeepCopyFiles");

        failed_deep_copy_tbl->createObjectWithArgs(
            "DestFile", yaml_dest_file,
            "Expected", contents_expected,
            "Actual", contents_actual);
    }

    return failure_differences.empty();
}

//! Turn the provided non-timeseries report file e.g. json,
//! txt, html, etc. into an equivalent report file from the
//! appropriate database records. Use a SpartaTester to check
//! for file differences, and give those differences back to
//! the caller in the "failure_differences" output argument.
bool verifyNonTimeseriesReport(
    const std::string & yaml_dest_file,
    const std::string & simdb_dest_file,
    const std::string & format,
    const simdb::DatabaseID report_id,
    const simdb::ObjectManager & sim_db,
    std::string & failure_differences)
{
    if (!Report::createFormattedReportFromDatabase(
            sim_db, report_id, simdb_dest_file, format))
    {
        throw SpartaException("Unable to create report from SimDB: \n")
            << "\tdest_file: " << yaml_dest_file << "\n"
            << "\tformat:    " << format << "\n"
            << "\treport_id: " << report_id;
    }

    std::ostringstream cerr;
    std::unique_ptr<SpartaTester> tester =
        SpartaTester::makeTesterWithUserCError(cerr);

    tester->expectFilesEqual(
        yaml_dest_file, simdb_dest_file,
        true, __LINE__, __FILE__, false);

    failure_differences = cerr.str();
    if (!failure_differences.empty()) {
        std::ifstream fin_expected(yaml_dest_file);
        const std::string contents_expected{
            std::istreambuf_iterator<char>{fin_expected}, {}};

        std::ifstream fin_actual(simdb_dest_file);
        const std::string contents_actual{
            std::istreambuf_iterator<char>{fin_actual}, {}};

        //Until we can be 100% sure that our SimInfo table has enough information
        //in it to exactly reproduce verification failures, we'll just capture a
        //deep copy of the two files (baseline and SimDB-generated) and stash them
        //back in the database. This verification step is disabled in production
        //simulators.
        auto failed_deep_copy_tbl = sim_db.getTable("ReportVerificationDeepCopyFiles");

        failed_deep_copy_tbl->createObjectWithArgs(
            "DestFile", yaml_dest_file,
            "Expected", contents_expected,
            "Actual", contents_actual);
    }

    return failure_differences.empty();
}

/*!
 * \brief VerificationSummary implementation class
 */
class ReportVerifier::VerificationSummary::Impl
{
public:
    explicit Impl(const simdb::ObjectManager & sim_db) :
        sim_db_(sim_db)
    {}

    void setManagledDescriptorDefFilesToSimDbDefFiles(
        std::map<std::string, std::string> && mangled_fnames)
    {
        desc_dest_files_to_simdb_dest_files_ = std::move(mangled_fnames);
    }

    void setMangledDescriptorDefFilesToYamlDestFiles(
        std::map<std::string, std::string> && mangled_fnames)
    {
        desc_dest_files_to_yaml_dest_files_ = std::move(mangled_fnames);
    }

    const std::map<std::string, std::string> &
        getMangledDescriptorDefFilesToYamlDestFiles() const
    {
        return desc_dest_files_to_yaml_dest_files_;
    }

    bool hasSummary() const
    {
        return !pass_fail_by_filename_.empty();
    }

    std::set<std::string> getPassingReportFilenames() const
    {
        std::set<std::string> passing;
        for (const auto & pf : pass_fail_by_filename_) {
            if (pf.second) {
                passing.insert(pf.first);
            }
        }
        return passing;
    }

    std::set<std::string> getFailingReportFilenames() const
    {
        std::set<std::string> failing;
        for (const auto & pf : pass_fail_by_filename_) {
            if (!pf.second) {
                failing.insert(pf.first);
            }
        }
        return failing;
    }

    bool reportIsTimeseries(const std::string & filename) const
    {
        if (tested_timeseries_report_filenames_.count(filename) > 0) {
            return true;
        }
        if (tested_non_timeseries_report_filenames_.count(filename) == 0) {
            throw SpartaException("Unrecognized filename given to a VerificationSummary ('")
                << filename << "')";
        }
        return false;
    }

    std::string getFailureDifferences(const std::string & filename) const
    {
        auto iter = failure_diffs_by_filename_.find(filename);
        return (iter != failure_diffs_by_filename_.end() ? iter->second : "");
    }

    void serializeSummary(
        const simdb::ObjectManager & sim_db) const
    {
        sim_db.safeTransaction([&]() {
            simdb::DatabaseID sim_info_id = 0;
            simdb::ObjectQuery sim_info_query(sim_db, "SimInfo");
            sim_info_query.writeResultIterationsTo("Id", &sim_info_id);

            //Until there is a stronger link between simulations
            //and the contents of the SimInfo table, we'll just
            //see if this table has exactly one record in it.
            //Until multiple simulations all feed the same SimDB
            //file, SimInfo will only have one record (or none).
            auto result_iter = sim_info_query.executeQuery();
            if (!result_iter->getNext() || result_iter->getNext()) {
                sim_info_id = 0;
            }

            auto verif_tbl = sim_db.getTable("ReportVerificationResults");
            const auto passing_files = getPassingReportFilenames();
            for (const auto & passing : passing_files) {
                const std::string & dest_file = passing;
                const bool is_timeseries = reportIsTimeseries(dest_file);

                verif_tbl->createObjectWithArgs(
                    "DestFile", dest_file,
                    "SimInfoID", sim_info_id,
                    "Passed", 1,
                    "IsTimeseries", (int)is_timeseries);
            }

            auto failure_summary_tbl = sim_db.getTable("ReportVerificationFailureSummaries");
            const auto failing_files = getFailingReportFilenames();
            for (const auto & failing : failing_files) {
                const std::string & dest_file = failing;
                const bool is_timeseries = reportIsTimeseries(dest_file);
                const std::string failure_diffs = getFailureDifferences(dest_file);

                auto failure_obj = verif_tbl->createObjectWithArgs(
                    "DestFile", dest_file,
                    "SimInfoID", sim_info_id,
                    "Passed", 0,
                    "IsTimeseries", (int)is_timeseries);

                failure_summary_tbl->createObjectWithArgs(
                    "ReportVerificationResultID", failure_obj->getId(),
                    "FailureSummary", failure_diffs);
            }
        });
    }

    std::map<std::string, std::string> getFinalDestFiles() const
    {
        std::map<std::string, std::string> simdb_to_yaml_dest_files;
        for (const auto & mangled : desc_dest_files_to_simdb_dest_files_) {
            const std::string & mangled_desc_dest_file = mangled.first;
            const std::string & simdb_dest_file = mangled.second;

            auto mangled_iter = desc_dest_files_to_yaml_dest_files_.find(
                mangled_desc_dest_file);
            sparta_assert(mangled_iter != desc_dest_files_to_yaml_dest_files_.end());
            const std::string & yaml_dest_file = mangled_iter->second;

            simdb_to_yaml_dest_files[simdb_dest_file] = yaml_dest_file;
        }

        //Trim away any leading '/' characters. They trip up
        //the boost::filesystem::copy_file() calls.
        std::map<std::string, std::string> trimmed_fnames;
        for (auto & simdb_to_yaml : simdb_to_yaml_dest_files) {
            namespace bfs = boost::filesystem;
            bfs::path p(simdb_to_yaml.second);
            std::string to;

            auto parent_path = p.parent_path();
            if (!bfs::is_directory(parent_path)) {
                auto not_slash = simdb_to_yaml.second.find_first_not_of("/");
                if (not_slash != std::string::npos) {
                    to = simdb_to_yaml.second.substr(not_slash);
                } else {
                    //We could warn or throw, but the calling code
                    //is not going to make the copy_file() call
                    //without some adjustments to the dest_file
                    //name.
                    to = simdb_to_yaml.second;
                }
            } else {
                to = simdb_to_yaml.second;
            }
            trimmed_fnames[simdb_to_yaml.first] = to;
        }

        return trimmed_fnames;
    }

    bool verifyReport(const std::string & filename)
    {
        //Until the timeseries and non-timeseries backend is
        //more streamlined, we need to run separate queries to infer
        //which type of report this is. The code path for database-
        //regenerated report files is currently quite different for
        //a timeseries report and every other type of report.
        simdb::ObjectQuery timeseries_query(sim_db_, "ReportHeader");
        simdb::DatabaseID timeseries_id = 0;
        std::string desc_dest_file;

        timeseries_query.addConstraints(
            "DestFile", simdb::constraints::equal, filename);

        timeseries_query.writeResultIterationsTo(
            "TimeseriesID", &timeseries_id,
            "DestFile", &desc_dest_file);

        auto result_iter = timeseries_query.executeQuery();
        if (result_iter->getNext()) {
            //This dest_file is a timeseries report.
            auto mangled_iter = desc_dest_files_to_simdb_dest_files_.find(
                desc_dest_file);
            sparta_assert(mangled_iter != desc_dest_files_to_simdb_dest_files_.end());
            const std::string & simdb_dest_file = mangled_iter->second;

            std::string failure_differences;
            const bool passed = verifyTimeseriesReport(
                desc_dest_file, simdb_dest_file,
                timeseries_id, sim_db_,
                failure_differences);

            //Store some pass/fail metadata for later and return.
            pass_fail_by_filename_[desc_dest_file] = passed;
            tested_timeseries_report_filenames_.insert(desc_dest_file);
            if (!passed) {
                failure_diffs_by_filename_[filename] = failure_differences;
            }
            return passed;
        }

        simdb::ObjectQuery non_timeseries_query(
            sim_db_, "ReportVerificationMetadata");

        simdb::DatabaseID report_id = 0;
        std::string format;

        non_timeseries_query.addConstraints(
            "DestFile", simdb::constraints::equal, filename);

        non_timeseries_query.writeResultIterationsTo(
            "RootReportNodeID", &report_id,
            "DestFile", &desc_dest_file,
            "Format", &format);

        result_iter = non_timeseries_query.executeQuery();
        if (result_iter->getNext()) {
            //This dest_file is a non-timeseries report.
            auto mangled_iter = desc_dest_files_to_simdb_dest_files_.find(
                desc_dest_file);
            sparta_assert(mangled_iter != desc_dest_files_to_simdb_dest_files_.end());
            const std::string & simdb_dest_file = mangled_iter->second;

            std::string failure_differences;
            const bool passed = verifyNonTimeseriesReport(
                desc_dest_file, simdb_dest_file,
                format, report_id, sim_db_,
                failure_differences);

            //Store some pass/fail metadata for later and return.
            pass_fail_by_filename_[desc_dest_file] = passed;
            tested_non_timeseries_report_filenames_.insert(desc_dest_file);
            if (!passed) {
                failure_diffs_by_filename_[filename] = failure_differences;
            }
            return passed;
        }

        //If the dest_file was not found to be a timeseries
        //or a non-timeseries in this database, that means
        //the report is not even in the database (or it got
        //"lost" due to something like invalid foreign keys).
        throw SpartaException("Unable to run report verification for file '")
            << desc_dest_file << "'. File not found in the database ("
            << sim_db_.getDatabaseFile() << ").";
    }

private:
    const simdb::ObjectManager & sim_db_;
    std::map<std::string, bool> pass_fail_by_filename_;
    std::set<std::string> tested_timeseries_report_filenames_;
    std::set<std::string> tested_non_timeseries_report_filenames_;
    std::map<std::string, std::string> failure_diffs_by_filename_;
    std::map<std::string, std::string> desc_dest_files_to_simdb_dest_files_;
    std::map<std::string, std::string> desc_dest_files_to_yaml_dest_files_;
};

ReportVerifier::VerificationSummary::VerificationSummary(
    const simdb::ObjectManager & sim_db) :
  impl_(new ReportVerifier::VerificationSummary::Impl(sim_db))
{
}

bool ReportVerifier::VerificationSummary::hasSummary() const
{
    return impl_->hasSummary();
}

std::set<std::string> ReportVerifier::VerificationSummary::getPassingReportFilenames() const
{
    return impl_->getPassingReportFilenames();
}

std::set<std::string> ReportVerifier::VerificationSummary::getFailingReportFilenames() const
{
    return impl_->getFailingReportFilenames();
}

bool ReportVerifier::VerificationSummary::reportIsTimeseries(
    const std::string & filename) const
{
    return impl_->reportIsTimeseries(filename);
}

std::string ReportVerifier::VerificationSummary::getFailureDifferences(
    const std::string & filename) const
{
    return impl_->getFailureDifferences(filename);
}

void ReportVerifier::VerificationSummary::serializeSummary(
    const simdb::ObjectManager & sim_db) const
{
    impl_->serializeSummary(sim_db);
}

std::map<std::string, std::string>
    ReportVerifier::VerificationSummary::getFinalDestFiles() const
{
    return impl_->getFinalDestFiles();
}

bool ReportVerifier::VerificationSummary::verifyReport_(const std::string & filename)
{
    return impl_->verifyReport(filename);
}

void ReportVerifier::addReportToVerify(const app::ReportDescriptor & rd)
{
    to_verify_[rd.dest_file] = rd.getDescriptorOrigDestFile();
}

void ReportVerifier::addBaseFormatterForPreVerificationReset(
    const std::string & filename,
    report::format::BaseFormatter * formatter)
{
    formatters_[filename] = formatter;
}

std::unique_ptr<ReportVerifier::VerificationSummary> ReportVerifier::verifyAll(
    const simdb::ObjectManager & sim_db)
{
    //Immediately lock down the verification artifacts directory
    //from changes for the rest of the program.
    sparta_assert(!ReportVerifier::getVerifResultsDir().empty());
    ReportVerifier::verif_results_dir_is_changeable_ = false;

    //Since we typically use report verification in regression
    //testing, we'll have many simulations concurrently writing
    //to the filesystem. They could have identical dest_file's
    //which could trip up the verification code (use the wrong
    //dest_file as the baseline and get sporadic failures).
    //
    //We'll add a random uuid onto each dest_file to help prevent
    //this from happening, or at least make it extremely unlikely.
    std::map<std::string, std::string> mangled_filenames;
    for (const auto & file : to_verify_) {
        std::ostringstream random_file_suffix;
        random_file_suffix << simdb::generateUUID();
        mangled_filenames[file.first] =
            file.first + "_" + random_file_suffix.str();
    }

    std::unique_ptr<VerificationSummary> summary(
        new VerificationSummary(sim_db));

    if (to_verify_.empty()) {
        return summary;
    }

    summary->impl_->setManagledDescriptorDefFilesToSimDbDefFiles(
        std::move(mangled_filenames));

    summary->impl_->setMangledDescriptorDefFilesToYamlDestFiles(
        std::move(to_verify_));

    sim_db.safeTransaction([&]() {
        const auto & desc_dest_files_to_yaml_dest_files =
            summary->impl_->getMangledDescriptorDefFilesToYamlDestFiles();

        for (const auto & file : desc_dest_files_to_yaml_dest_files) {
            if (file.first == "1") {
                std::cout << "  [simdb]  Skipping report validation check for "
                          << "std::cout report (dest_file: \"1\")\n";
                continue;
            }
            auto formatter_iter = formatters_.find(file.first);
            if (formatter_iter != formatters_.end()) {
                formatter_iter->second->doPostProcessingBeforeReportValidation();
            }
            summary->verifyReport_(file.first);
        }
    });

    return summary;
}

} // namespace db
} // namespace sparta
