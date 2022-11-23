/*!
 * \file ReportVerifier.cpp
 * \brief Test for ReportVerifier functionality
 */

#include "sparta/report/db/ReportVerifier.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "sparta/report/db/Schema.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <fstream>

TEST_INIT

using sparta::SpartaTester;
namespace db = sparta::db;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

std::string makeLoremText()
{
    std::ostringstream lorem;
    lorem << "Lorem ipsum dolor sit amet, consectetur adipiscing elit,    " << std::endl;
    lorem << "sed do eiusmod tempor incididunt ut labore et dolore magna  " << std::endl;
    lorem << "aliqua. Ut enim ad minim veniam, quis nostrud exercitation  " << std::endl;
    lorem << "ullamco laboris nisi ut aliquip ex ea commodo consequat.    " << std::endl;
    lorem << "Duis aute irure dolor in reprehenderit in voluptate velit   " << std::endl;
    lorem << "esse cillum dolore eu fugiat nulla pariatur. Excepteur sint " << std::endl;
    lorem << "occaecat cupidatat non proident, sunt in culpa qui officia  " << std::endl;
    lorem << "deserunt mollit anim id est laborum.                        " << std::endl;
    return lorem.str();
}

void spartaTesterEquivalentFiles()
{
    PRINT_ENTER_TEST

    std::ostringstream cerr;
    std::unique_ptr<SpartaTester> tester =
        SpartaTester::makeTesterWithUserCError(cerr);

    const std::string fname1 = "lorem1.txt";
    const std::string fname2 = "lorem2.txt";
    const std::string header = "# foo=5,bar=asdf";

    {
        std::ofstream fout(fname1);
        fout << header << "\n";
        fout << makeLoremText();
    }

    {
        std::ofstream fout(fname2);
        fout << header << "\n";
        fout << makeLoremText();
    }

    tester->expectFilesEqual(fname1, fname2, true, __LINE__, __FILE__, false);
    EXPECT_TRUE(cerr.str().empty());
    EXPECT_EQUAL(SpartaTester::getErrorCode(tester.get()), 0);
}

void spartaTesterDifferentFiles()
{
    PRINT_ENTER_TEST

    std::ostringstream cerr;
    std::unique_ptr<SpartaTester> tester =
        SpartaTester::makeTesterWithUserCError(cerr);

    const std::string fname1 = "lorem1.txt";
    const std::string fname2 = "lorem2.txt";
    const std::string header = "# foo=5,bar=asdf";

    {
        std::ofstream fout(fname1);
        fout << header << "\n";
        fout << makeLoremText();
    }

    {
        std::ofstream fout(fname2);
        std::string lorem = makeLoremText();
        boost::replace_all(lorem, "aliqua", "aliquip");
        boost::replace_all(lorem, "consequat", "consectetur");
        fout << header << "\n";
        fout << lorem;
    }

    tester->expectFilesEqual(fname1, fname2, true, __LINE__, __FILE__, false);
    EXPECT_FALSE(cerr.str().empty());
    EXPECT_NOTEQUAL(SpartaTester::getErrorCode(tester.get()), 0);
}

void testVerificationTables()
{
    PRINT_ENTER_TEST

    simdb::ObjectManager obj_mgr(".");

    simdb::Schema schema;
    db::buildSimulationDatabaseSchema(schema);

    std::unique_ptr<simdb::DbConnProxy> db_proxy(new simdb::SQLiteConnProxy);
    obj_mgr.createDatabaseFromSchema(schema, std::move(db_proxy));

    const std::string dest_file = "AccuracyCheckedDBs/abcd-1234/out2.csv";
    const simdb::DatabaseID sim_info_id = 14;
    const bool passed = false;
    const bool is_timeseries = true;

    auto verif_tbl = obj_mgr.getTable("ReportVerificationResults");

    verif_tbl->createObjectWithArgs("DestFile", dest_file,
                                    "SimInfoID", sim_info_id,
                                    "Passed", (int)passed,
                                    "IsTimeseries", (int)is_timeseries);

    simdb::ObjectQuery query(obj_mgr, "ReportVerificationResults");
    query.addConstraints("Passed", simdb::constraints::equal, 0);
    EXPECT_EQUAL(query.countMatches(), 1);

    std::string record_dest_file;
    simdb::DatabaseID record_sim_info_id;
    int record_passed;
    int record_is_timeseries;

    query.writeResultIterationsTo("DestFile", &record_dest_file,
                                  "SimInfoID", &record_sim_info_id,
                                  "Passed", &record_passed,
                                  "IsTimeseries", &record_is_timeseries);

    EXPECT_EQUAL(query.countMatches(), 1);
    query.executeQuery()->getNext();

    EXPECT_EQUAL(record_dest_file, dest_file);
    EXPECT_EQUAL(record_sim_info_id, sim_info_id);
    EXPECT_EQUAL(record_passed, (int)passed);
    EXPECT_EQUAL(record_is_timeseries, (int)is_timeseries);
}

int main()
{
    spartaTesterEquivalentFiles();
    spartaTesterDifferentFiles();
    testVerificationTables();

    REPORT_ERROR;
    return ERROR_CODE;
}
