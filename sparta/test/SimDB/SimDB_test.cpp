/*!
 * \file SimDB_test.cpp
 *
 * \brief Tests functionality of SPARTA's statistics database
 */

#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/Schema.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <math.h>
#include <boost/filesystem.hpp>

TEST_INIT;

//! Put all the temporary .db files into one directory
//! right in the build folder where tests are run from.
//! We will delete them all at the end.
const std::string DB_DIR = "./temp_dbs";

//! RAII for creating and deleting the temp db directory
struct DirDeleter {
    DirDeleter() {
        boost::filesystem::create_directories(DB_DIR);
    }
    ~DirDeleter() {
        boost::filesystem::remove_all(DB_DIR);
    }
};

#define ScopedDatabaseDir \
    DirDeleter deleter; \
    (void) deleter;

//! ostream operator so we can do EXPECT_EQUAL(vec1, vec2)
template <class DataT>
inline std::ostream & operator<<(std::ostream & os,
                                 const std::vector<DataT> & data)
{
    if (data.empty()) {
        return os;
    }

    if (data.size() == 1) {
        os << "[" << data[0] << "]";
        return os;
    }

    //These vectors can be too long to really print to
    //stdout in a useful way... let's just truncate to
    //something like "[6.5, 3.4, 5.6, 7.8, 1.2, ...]"
    //
    //If the vector is less than or equal to 5 elements,
    //they will all get printed out.
    const size_t num_data_to_print = std::min(5ul, data.size());
    std::ostringstream oss;
    oss << "[";
    for (size_t idx = 0; idx < num_data_to_print-1; ++idx) {
        oss << data[idx] << ",";
    }
    oss << data[num_data_to_print-1];
    if (data.size() > num_data_to_print) {
        oss << ",...";
    }
    oss << "]";
    os << oss.str();
    return os;
}

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

void testReportHeaders()
{
    PRINT_ENTER_TEST

    namespace db = sparta::db;

    simdb::ObjectManager obj_mgr(DB_DIR);

    //Before opening the database, verify that getDatabaseFile()
    //just returns the DB_DIR path.
    EXPECT_EQUAL(obj_mgr.getDatabaseFile(), DB_DIR);

    simdb::Schema si_schema;
    db::buildSimulationDatabaseSchema(si_schema);

    std::unique_ptr<simdb::DbConnProxy> db_proxy(new simdb::SQLiteConnProxy);
    obj_mgr.createDatabaseFromSchema(si_schema, std::move(db_proxy));

    //The database file should now be set, and not just DB_DIR
    EXPECT_NOTEQUAL(obj_mgr.getDatabaseFile(), DB_DIR);
    EXPECT_FALSE(obj_mgr.getDatabaseFile().empty());

    const std::string report_name = "simple_stats.yaml on _SPARTA_global_node_";
    const uint64_t start_time = 1200;

    //Use an end time that is large enough to overflow int64_t (signed).
    //SQLite does not support uint64_t out of the box, so we have to cast
    //to and from int64_t to overcome this.
    const uint64_t end_time = std::numeric_limits<uint64_t>::max();

    const std::string start_trigger_expr =
        "top.core0.rob.stats.total_number_retired >= 1200";

    const std::string update_trigger_expr =
        "notif.stats_profiler == 10";

    const std::string stop_trigger_expr =
        "notif.shutdown == 100";

    const std::string dest_file = "foo.csv";

    const std::string si_locations = "foo,bar,biz,baz";

    //Ensure the connection from the ReportHeader table
    //to the StringMetadata table is working
    const std::string user_metadata_name = "UserMetaFoo";
    const std::string user_metadata_value = "OrigValue";
    const std::string user_metadata_overwritten_value = "NewValue";

    //Make sure hidden metadata can be written/read
    const std::string user_metadata_name2 = "MyHiddenFoo";
    const std::string hidden_metadata_name = "__" + user_metadata_name2;
    const std::string hidden_metadata_value = "you_cannot_see_me";

    sparta::utils::ValidValue<simdb::DatabaseID> header_db_id;

    {
        //Put the header object in its own scope
        db::ReportHeader header(obj_mgr);

        //Save this object's database ID for later...
        header_db_id = header.getId();

        //Populate the values. Let's do this inside a single
        //transaction for better performance.
        obj_mgr.safeTransaction([&]() {
            header.setReportName(report_name);
            header.setReportStartTime(start_time);
            header.setReportEndTime(end_time);
            header.setSourceReportDescDestFile(dest_file);
            header.setSourceReportNumStatInsts(4);
            header.setCommaSeparatedSILocations(si_locations);
            header.setStringMetadata(user_metadata_name, user_metadata_value);
            header.setStringMetadata(hidden_metadata_name, hidden_metadata_value);
        });

        //Let's check the string metadata value... and then overwrite it.
        EXPECT_EQUAL(header.getStringMetadata(user_metadata_name),
                     user_metadata_value);

        header.setStringMetadata(
            user_metadata_name, user_metadata_overwritten_value);
    }

    {
        //The previous header object is destroyed, but the
        //database is still open. We should be able to connect
        //to it again using the database ID that we got earlier.

        //Start by getting a wrapper around the row in the
        //header table.
        std::unique_ptr<simdb::ObjectRef> obj_ref =
            obj_mgr.findObject("ReportHeader", header_db_id);

        //Now give that object reference to a ReportHeader
        //object, who will give us more friendly read API's
        //around the record's property values.
        db::ReportHeader header(std::move(obj_ref));

        //Verify the values are all correct
        EXPECT_EQUAL(header.getReportName(), report_name);
        EXPECT_EQUAL(header.getReportStartTime(), start_time);
        EXPECT_EQUAL(header.getReportEndTime(), end_time);
        EXPECT_EQUAL(header.getSourceReportDescDestFile(), dest_file);
        EXPECT_EQUAL(header.getCommaSeparatedSILocations(), si_locations);
        EXPECT_EQUAL(header.getStringMetadata(user_metadata_name),
                     user_metadata_overwritten_value);
        EXPECT_TRUE(header.getStringMetadata("nonexistent").empty());

        auto all_metadata = header.getAllStringMetadata();
        EXPECT_TRUE(all_metadata.find(user_metadata_name2) == all_metadata.end());

        auto all_hidden_metadata = header.getAllHiddenStringMetadata();
        auto hidden_iter = all_hidden_metadata.find(user_metadata_name2);
        EXPECT_TRUE(hidden_iter != all_hidden_metadata.end());
        EXPECT_EQUAL(hidden_iter->second, hidden_metadata_value);

        //Ensure that the hidden metadata does not contain anything *but*
        //the one hidden value we added to this report header
        all_hidden_metadata.erase(hidden_iter);
        EXPECT_TRUE(all_hidden_metadata.empty());
    }
}

void testReportTimeseries()
{
    PRINT_ENTER_TEST

    namespace db = sparta::db;

    simdb::ObjectManager obj_mgr(DB_DIR);

    simdb::Schema si_schema;
    db::buildSimulationDatabaseSchema(si_schema);

    std::unique_ptr<simdb::DbConnProxy> db_proxy(new simdb::SQLiteConnProxy);
    obj_mgr.createDatabaseFromSchema(si_schema, std::move(db_proxy));

    sparta::utils::ValidValue<simdb::DatabaseID> timeseries1_id;
    sparta::utils::ValidValue<simdb::DatabaseID> timeseries2_id;

    //Create a few random SI values vectors and time values
    //to go with them. These exist outside the scope of the
    //writer code below so we can use them to verify the
    //data values were written to & retrieved from the
    //database correctly.
    const uint32_t num_stat_insts_in_timeseries1 = rand() % 2000 + 50;
    const uint32_t num_stat_insts_in_timeseries2 = rand() % 2000 + 50;

    auto random_si_values = [=](const uint32_t num_pts) {
        std::vector<double> si_values(num_pts);
        for (auto & val : si_values) {
            val = rand() * M_PI;
        }
        return si_values;
    };

    //Create some header metadata
    const std::string report1_name = "MyFirstTimeseriesReport";
    const std::string report2_name = "MySecondTimeseriesReport";

    //Pick random SI values and some time values to go with them...

    //...Timeseries 1
    const std::vector<double> ts1_si1_values =
        random_si_values(num_stat_insts_in_timeseries1);

    const std::vector<double> ts1_si2_values =
        random_si_values(num_stat_insts_in_timeseries1);

    const std::vector<double> ts1_si3_values =
        random_si_values(num_stat_insts_in_timeseries1);

    //...Timeseries 2
    const std::vector<double> ts2_si1_values =
        random_si_values(num_stat_insts_in_timeseries2);

    const std::vector<double> ts2_si2_values =
        random_si_values(num_stat_insts_in_timeseries2);

    const std::vector<double> ts2_si3_values =
        random_si_values(num_stat_insts_in_timeseries2);

    //Typically, a timeseries report will have evenly spaced
    //"time values" since report updates are usually captured
    //on a counter trigger, cycle trigger, or time trigger.
    //These fire at regular intervals.
    //
    //However, timeseries reports can also be generated using
    //sparta::NotificationSource's as the update trigger, and for
    //those types of reports the update rate is essentially
    //random.
    //
    //In order to address all use cases, we store the simulated
    //picoseconds (from the Scheduler) as well as the current
    //cycle (from the root Clock) for each SI blob we write to
    //the database.
    //
    //These four values are all part of the TimeseriesChunk
    //index for fast retrieval later on.
    const uint64_t sim_picoseconds_time1 = 130;
    const uint64_t sim_picoseconds_time2 = 920;
    const uint64_t sim_picoseconds_time3 = 1835;

    const uint64_t root_clk_cur_cycles_time1 = 3450;
    const uint64_t root_clk_cur_cycles_time2 = 9004;
    const uint64_t root_clk_cur_cycles_time3 = 12408;

    {
        //Create two timeseries objects
        db::ReportTimeseries ts1(obj_mgr);
        db::ReportTimeseries ts2(obj_mgr);

        //Save these objects' database IDs for later...
        timeseries1_id = ts1.getId();
        timeseries2_id = ts2.getId();

        //Populate the values. Note that we are NOT doing all three
        //commands in one SQL statement (ObjectManager::safeTransaction)
        //because real simulations will be feeding data into the database
        //periodically or even asynchronously. Building up all of that
        //pending data inside a single transaction / commit in the hopes
        //of faster runtime performance would cause memory problems or
        //even exhaust memory entirely in the worst case.

        //Timeseries 1, SI vector 1
        ts1.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time1, root_clk_cur_cycles_time1, ts1_si1_values,
            db::MajorOrdering::ROW_MAJOR);

        //Timeseries 2, SI vector 1
        ts2.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time1, root_clk_cur_cycles_time1, ts2_si1_values,
            db::MajorOrdering::ROW_MAJOR);

        //Timeseries 1, SI vector 2
        ts1.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time2, root_clk_cur_cycles_time2, ts1_si2_values,
            db::MajorOrdering::ROW_MAJOR);

        //Timeseries 1, SI vector 3
        ts1.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time3, root_clk_cur_cycles_time3, ts1_si3_values,
            db::MajorOrdering::ROW_MAJOR);

        //Timeseries 2, SI vector 2
        ts2.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time2, root_clk_cur_cycles_time2, ts2_si2_values,
            db::MajorOrdering::ROW_MAJOR);

        //Timeseries 2, SI vector 3
        ts2.writeStatisticInstValuesAtTimeT(
            sim_picoseconds_time3, root_clk_cur_cycles_time3, ts2_si3_values,
            db::MajorOrdering::ROW_MAJOR);

        //Verify an exception is thrown if we attempt to write
        //SI values at time "t" that is larger than int64 max.
        EXPECT_THROW(ts2.writeStatisticInstValuesAtTimeT(
            std::numeric_limits<uint64_t>::max(),
            std::numeric_limits<uint64_t>::max(),
            ts2_si3_values,
            db::MajorOrdering::ROW_MAJOR));

        //**********************************************************
        //There is a unit test dedicated to the ReportHeader object.
        //But let's add a little bit of header data through the
        //timeseries object anyway. This will test that the connection
        //between the header object (table) and timeseries object
        //(another table) is working.
        ts1.getHeader().setReportName(report1_name);
        ts2.getHeader().setReportName(report2_name);

        //TODO: This piece of metadata is needed to decompress SI data.
        //Find another way to decompress blobs without requiring this.
        //We aren't even compressing blobs in this unit test, so this
        //at least should not be required if compression is not even
        //enabled.
        ts1.getHeader().setSourceReportNumStatInsts(
            num_stat_insts_in_timeseries1);
        ts2.getHeader().setSourceReportNumStatInsts(
            num_stat_insts_in_timeseries2);
    }

    {
        //The previous timeseries objects are destroyed, but the
        //database is still open. We should be able to connect
        //to these timeseries objects again using the database
        //ID's that we got earlier.

        //Start by getting wrappers around the rows in the
        //timeseries table.
        std::unique_ptr<simdb::ObjectRef> obj_ref1 =
            obj_mgr.findObject("Timeseries", timeseries1_id);

        std::unique_ptr<simdb::ObjectRef> obj_ref2 =
            obj_mgr.findObject("Timeseries", timeseries2_id);

        //Now give those object references to ReportTimeseries
        //objects, who will give us more friendly read API's
        //around the SI values.
        db::ReportTimeseries disk_ts1(std::move(obj_ref1));
        db::ReportTimeseries disk_ts2(std::move(obj_ref2));

        std::vector<std::vector<double>> timeseries1_si_chunks;
        std::vector<std::vector<double>> timeseries2_si_chunks;

        //Get all data from [time1,time3] (** simulated picoseconds **)
        disk_ts1.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time1, sim_picoseconds_time3, timeseries1_si_chunks);

        disk_ts2.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time1, sim_picoseconds_time3, timeseries2_si_chunks);

        EXPECT_EQUAL(timeseries1_si_chunks.size(), 3);
        EXPECT_EQUAL(timeseries1_si_chunks[0], ts1_si1_values);
        EXPECT_EQUAL(timeseries1_si_chunks[1], ts1_si2_values);
        EXPECT_EQUAL(timeseries1_si_chunks[2], ts1_si3_values);

        EXPECT_EQUAL(timeseries2_si_chunks.size(), 3);
        EXPECT_EQUAL(timeseries2_si_chunks[0], ts2_si1_values);
        EXPECT_EQUAL(timeseries2_si_chunks[1], ts2_si2_values);
        EXPECT_EQUAL(timeseries2_si_chunks[2], ts2_si3_values);

        //Get all data from [time2,time3] (** simulated picoseconds **)
        timeseries1_si_chunks.clear();
        timeseries1_si_chunks.shrink_to_fit();

        timeseries2_si_chunks.clear();
        timeseries2_si_chunks.shrink_to_fit();

        disk_ts1.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time2, sim_picoseconds_time3, timeseries1_si_chunks);

        disk_ts2.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time2, sim_picoseconds_time3, timeseries2_si_chunks);

        EXPECT_EQUAL(timeseries1_si_chunks.size(), 2);
        EXPECT_EQUAL(timeseries1_si_chunks[0], ts1_si2_values);
        EXPECT_EQUAL(timeseries1_si_chunks[1], ts1_si3_values);

        EXPECT_EQUAL(timeseries2_si_chunks.size(), 2);
        EXPECT_EQUAL(timeseries2_si_chunks[0], ts2_si2_values);
        EXPECT_EQUAL(timeseries2_si_chunks[1], ts2_si3_values);

        //Get all data from [time2] (** simulated picoseconds **)
        timeseries1_si_chunks.clear();
        timeseries1_si_chunks.shrink_to_fit();

        timeseries2_si_chunks.clear();
        timeseries2_si_chunks.shrink_to_fit();

        disk_ts1.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time2, sim_picoseconds_time2, timeseries1_si_chunks);

        disk_ts2.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time2, sim_picoseconds_time2, timeseries2_si_chunks);

        EXPECT_EQUAL(timeseries1_si_chunks.size(), 1);
        EXPECT_EQUAL(timeseries1_si_chunks[0], ts1_si2_values);

        EXPECT_EQUAL(timeseries2_si_chunks.size(), 1);
        EXPECT_EQUAL(timeseries2_si_chunks[0], ts2_si2_values);

        //Try to get any data from **outside the timeseries range entirely**
        timeseries1_si_chunks.clear();
        timeseries1_si_chunks.shrink_to_fit();

        timeseries2_si_chunks.clear();
        timeseries2_si_chunks.shrink_to_fit();

        disk_ts1.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time3 + 5000,
            sim_picoseconds_time3 + 10000,
            timeseries1_si_chunks);

        disk_ts2.getStatisticInstValuesBetweenSimulatedPicoseconds(
            sim_picoseconds_time3 + 5000,
            sim_picoseconds_time3 + 10000,
            timeseries2_si_chunks);

        EXPECT_TRUE(timeseries1_si_chunks.empty());
        EXPECT_TRUE(timeseries2_si_chunks.empty());

        //Run a query to get some SI data between two root clock cycles,
        //instead of between two simulated picoseconds.
        timeseries1_si_chunks.clear();
        timeseries1_si_chunks.shrink_to_fit();

        timeseries2_si_chunks.clear();
        timeseries2_si_chunks.shrink_to_fit();

        //root clock current cycles @ time1 -> @ time2
        //                         [------- ,  -------]
        disk_ts1.getStatisticInstValuesBetweenRootClockCycles(
            root_clk_cur_cycles_time1, root_clk_cur_cycles_time2, timeseries1_si_chunks);

        EXPECT_EQUAL(timeseries1_si_chunks.size(), 2);
        EXPECT_EQUAL(timeseries1_si_chunks[0], ts1_si1_values);
        EXPECT_EQUAL(timeseries1_si_chunks[1], ts1_si2_values);

        //root clock current cycles @ time2 -> @ time3
        //                         [------- ,  -------]   ** notice this range
        //                                                  is different **
        disk_ts2.getStatisticInstValuesBetweenRootClockCycles(
            root_clk_cur_cycles_time2, root_clk_cur_cycles_time3, timeseries2_si_chunks);

        EXPECT_EQUAL(timeseries2_si_chunks.size(), 2);
        EXPECT_EQUAL(timeseries2_si_chunks[0], ts2_si2_values);
        EXPECT_EQUAL(timeseries2_si_chunks[1], ts2_si3_values);

        //Verify that no exception is thrown if we attempt to read
        //SI values at time "t" that is larger than int64 max. This
        //should return *empty* SI vectors, but it should not throw.
        EXPECT_NOTHROW(disk_ts2.getStatisticInstValuesBetweenSimulatedPicoseconds(
            std::numeric_limits<uint64_t>::max(),
            std::numeric_limits<uint64_t>::max(),
            timeseries2_si_chunks));
        EXPECT_TRUE(timeseries2_si_chunks.empty());

        EXPECT_NOTHROW(disk_ts2.getStatisticInstValuesBetweenRootClockCycles(
            std::numeric_limits<uint64_t>::max(),
            std::numeric_limits<uint64_t>::max(),
            timeseries2_si_chunks));
        EXPECT_TRUE(timeseries2_si_chunks.empty());

        //**********************************************************
        //Verify the header data is correct. This is mostly testing
        //that the connection between the timeseries table and the
        //header table is working. All the individual metadata tests
        //are in a different unit test.
        EXPECT_EQUAL(disk_ts1.getHeader().getReportName(), report1_name);
        EXPECT_EQUAL(disk_ts2.getHeader().getReportName(), report2_name);
    }
}

void testReportCreationFromSimDB()
{
    PRINT_ENTER_TEST

    //Connect to 'sample.db' and create all non-timeseries reports
    //from a root-level report node we find in this database. This
    //is for smoke testing only, and does not validate the contents
    //of the resulting report files.
    simdb::ObjectManager obj_mgr(DB_DIR);
    EXPECT_TRUE(obj_mgr.connectToExistingDatabase("sample.db"));

    simdb::ObjectQuery query(obj_mgr, "ReportNodeHierarchy");
    query.addConstraints("ParentNodeID", simdb::constraints::equal, 0);

    int report_db_id = -1;
    query.writeResultIterationsTo("Id", &report_db_id);

    auto result_iter = query.executeQuery();
    EXPECT_TRUE(result_iter != nullptr);
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_TRUE(report_db_id > 0);

    sparta::Scheduler sched;

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.json", "json", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.reduced.json", "json_reduced", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.detail.json", "json_detail", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.js.json", "js_json", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.html", "html", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.txt", "txt", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.py", "python", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.gnu", "gnuplot", &sched));

    EXPECT_TRUE(sparta::Report::createFormattedReportFromDatabase(
                obj_mgr, report_db_id,
                "test.stats.mapping.json", "stats_mapping", &sched));
}

void testClockHierarchies()
{
    using sparta::Clock;

    sparta::Scheduler sched;
    sparta::ClockManager m(&sched);
    Clock::Handle root_clk = m.makeRoot();
    Clock::Handle clk_12 = m.makeClock("C12", root_clk, 1, 2);
    Clock::Handle clk_23 = m.makeClock("C23", root_clk, 2, 3);
    m.normalize();

    simdb::ObjectManager obj_mgr(DB_DIR);
    simdb::Schema si_schema;
    sparta::db::buildSimulationDatabaseSchema(si_schema);

    std::unique_ptr<simdb::DbConnProxy> db_proxy(new simdb::SQLiteConnProxy);
    obj_mgr.createDatabaseFromSchema(si_schema, std::move(db_proxy));

    struct ClkData {
        std::string name;
        double period = 0;
        double ratio = 0;
        uint32_t freq = 0;
    };

    auto verify_clk_data = [&](const ClkData & actual,
                               Clock::Handle expected)
    {
        EXPECT_EQUAL(actual.name,
                     expected->getName());

        EXPECT_EQUAL(actual.period,
                     expected->getPeriod());

        EXPECT_EQUAL(actual.ratio,
                     static_cast<double>(expected->getRatio()));

        EXPECT_EQUAL(actual.freq,
                     static_cast<uint32_t>(expected->getFrequencyMhz()));
    };

    auto verify_obj_ref = [&](std::unique_ptr<simdb::ObjectRef> & actual,
                              Clock::Handle expected)
    {
        ClkData data;
        data.name = actual->getPropertyString("Name");
        data.period = actual->getPropertyDouble("Period");
        data.ratio = actual->getPropertyDouble("RatioToParent");
        data.freq = actual->getPropertyUInt32("FreqMHz");
        verify_clk_data(data, expected);
    };

    auto root_clk_id = root_clk->serializeTo(obj_mgr);
    auto root_obj_ref = obj_mgr.findObject("ClockHierarchy", root_clk_id);
    verify_obj_ref(root_obj_ref, root_clk);

    simdb::ObjectQuery query(obj_mgr, "ClockHierarchy");
    ClkData data;

    query.addConstraints(
        "ParentClockID",
        simdb::constraints::equal,
        root_clk_id);

    query.writeResultIterationsTo(
        "Name", &data.name,
        "Period", &data.period,
        "RatioToParent", &data.ratio,
        "FreqMHz", &data.freq);

    auto result_iter = query.executeQuery();

    EXPECT_TRUE(result_iter->getNext());
    verify_clk_data(data, clk_12);

    EXPECT_TRUE(result_iter->getNext());
    verify_clk_data(data, clk_23);

    EXPECT_FALSE(result_iter->getNext());
}

int main()
{
    ScopedDatabaseDir
    srand(time(0));

    testReportHeaders();
    testReportTimeseries();
    testReportCreationFromSimDB();
    testClockHierarchies();

    REPORT_ERROR;
    return ERROR_CODE;
}
