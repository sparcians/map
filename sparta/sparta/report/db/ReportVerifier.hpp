// <ReportVerifier> -*- C++ -*-

#ifndef __SPARTA_SIMDB_REPORT_VERIFIER_H__
#define __SPARTA_SIMDB_REPORT_VERIFIER_H__

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simdb_fwd.hpp"
#include "simdb/ObjectManager.hpp"

namespace sparta { namespace report { namespace format {
    class BaseFormatter;
}}}

namespace sparta { namespace app {
    class ReportDescriptor;
}}

namespace sparta {
    class Scheduler;

namespace db {

/*!
 * \brief Verification utility which compares a formatted
 * report file generated during a SPARTA simulation, against
 * a SimDB-generated report file that was made after the
 * simulation was already over.
 */
class ReportVerifier
{
public:
    //! Ask for the verification results directory. This is
    //! a subdirectory relative to the current directory where
    //! verification artifacts live. Failed report files may
    //! be copied here for debugging purposes after simulation.
    static const std::string & getVerifResultsDir();

    //! Redirect the verification artifacts to be written
    //! to a different directory. This is not required;
    //! a default subdirectory will be used unless told
    //! otherwise. Call getVerifResultsDir() to see the
    //! current artifacts directory.
    //!
    //! This throws an exception if the post-simulation
    //! verification process has already begun.
    static void writeVerifResultsTo(const std::string & dir);

    //! SPARTA will add reports one by one for verification
    //! at the end of simulation.
    void addReportToVerify(const app::ReportDescriptor & rd);

    //! Optionally add one or more BaseFormatter's to this
    //! verifier. The doPostProcessingBeforeReportValidation()
    //! method will get called in between each report validation.
    void addBaseFormatterForPreVerificationReset(
        const std::string & filename,
        report::format::BaseFormatter * formatter);

    //! Verification summary to be returned to the caller
    //! after all SimDB-generated reports have been checked
    //! for equivalence against the physical report file
    //! left in the pwd during simulation.
    class VerificationSummary
    {
    public:
        ~VerificationSummary() = default;

        //! This method returns false only when no report files
        //! were added via ReportVerifier::addReportToVerify()
        bool hasSummary() const;

        //! Return a list of report files that *passed* verification.
        std::set<std::string> getPassingReportFilenames() const;

        //! Return a list of report files that *failed* verification.
        std::set<std::string> getFailingReportFilenames() const;

        //! For the given report file, was it found in the database
        //! as a timeseries or not?
        bool reportIsTimeseries(const std::string & filename) const;

        //! For the given report file, return a failure summary
        //! that highlights differences between the physical
        //! report produced by the simulation, and the report
        //! produced from the database after simulation.
        //!
        //! Returns an empty string if this report passed the
        //! verification, or if there was no report by this
        //! name found in the VerificationSummary.
        std::string getFailureDifferences(const std::string & filename) const;

        //! Write all contents of this report verification summary
        //! to the provided database.
        void serializeSummary(
            const simdb::ObjectManager & sim_db) const;

        //! Get a 1-to-1 mapping of the filenames that were passed
        //! into the verifier, and the names of the files that the
        //! verifier used in order to run the verification.
        //!
        //! For instance, say your original yaml file had dest_file's
        //! 'foo.csv' and 'bar.json', and the database had file location
        //! '/tmp/abcd-1234.db'
        //!
        //! The SimDB-generated files may end up in a subfolder like this:
        //!
        //!    <pwd>/abcd-1234
        //!            /foo.csv_i34l2kj
        //!            /bar.json_wqr90w4
        //!
        //! The file suffixes may have been added to help ensure
        //! that file-to-file comparisons do not fail due to many
        //! simulations running in parallel (make regress) and
        //! producing files with potentially the same dest_file
        //! name.
        //!
        //! This mapping would then be:
        //!
        //!      { { "bar.json", "abcd-1234/bar.json_wqr90w4" },
        //!        { "foo.csv",  "abcd-1234/foo.csv_i34l2kj"  } }
        std::map<std::string, std::string> getFinalDestFiles() const;

    private:
        //! Let the ReportVerifier give us report files one
        //! by one to verify. Returns true if the verification
        //! succeeded, false otherwise. More details can be
        //! obtained via the public APIs.
        bool verifyReport_(const std::string & filename,
                           const Scheduler * scheduler);

        //! ReportVerifier is the only one who can create one
        //! of these objects.
        VerificationSummary(const simdb::ObjectManager & sim_db);
        friend class ReportVerifier;

        //! Hide implementation details from sight.
        class Impl;

        std::shared_ptr<Impl> impl_;
    };

    //! Verify each report report file that was added via
    //! addReportToVerify(), and return a summary object
    //! for inspection and error reporting.
    std::unique_ptr<VerificationSummary> verifyAll(
        const simdb::ObjectManager & sim_db,
        const Scheduler * scheduler);

private:
    std::map<std::string, std::string> to_verify_;
    std::map<std::string, report::format::BaseFormatter*> formatters_;
    static std::string verif_results_dir_;
    static bool verif_results_dir_is_changeable_;
};

} // namespace db
} // namespace sparta

#endif
