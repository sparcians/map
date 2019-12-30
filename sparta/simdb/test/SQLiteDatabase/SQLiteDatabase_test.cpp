/*!
 * \file SQLiteDatabase_test.cpp
 *
 * \brief Tests functionality of SimDB's core functionality,
 * including schema creation, INSERT/UPDATE/DELETE
 */

#include "simdb/test/SimDBTester.h"

//Core database headers
#include "simdb/ObjectManager.h"
#include "simdb/ObjectRef.h"
#include "simdb/TableRef.h"
#include "simdb/utils/ObjectQuery.h"
#include "simdb/utils/BlobHelpers.h"

//SQLite-specific headers
#include "simdb/impl/sqlite/SQLiteConnProxy.h"
#include "simdb/impl/sqlite/TransactionUtils.h"
#include "simdb/impl/sqlite/Errors.h"
#include <sqlite3.h>

//Standard headers
#include <math.h>

#define DB_DIR "test_dbs"

TEST_INIT;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

#define CREATE_SQL_SCHEMA(obj_mgr, schema)                                     \
  obj_mgr.disableWarningMessages();                                            \
  obj_mgr.createDatabaseFromSchema(                                            \
    schema, std::unique_ptr<simdb::DbConnProxy>(new simdb::SQLiteConnProxy));

void testBadSql()
{
    PRINT_ENTER_TEST

    simdb::ObjectManager obj_mgr(DB_DIR);
    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("Dummy")
        .addColumn("Dummy", dt::int32_t);

    CREATE_SQL_SCHEMA(obj_mgr, schema);

    auto db_proxy = dynamic_cast<const simdb::SQLiteConnProxy*>(
        obj_mgr.getDbConn());
    EXPECT_TRUE(db_proxy != nullptr);
    EXPECT_THROW(simdb::eval_sql(db_proxy, "THIS IS NOT VALID SQL"));
}

void testBadFile()
{
    PRINT_ENTER_TEST

    const std::string fname = "test.db";
    {
        std::ofstream invalid_file(fname);
        invalid_file << "This is not a valid SQLite database!";
    }

    simdb::ObjectManager obj_mgr(DB_DIR);
    EXPECT_FALSE(obj_mgr.connectToExistingDatabase(fname));
}

void testInvalidSchema()
{
    PRINT_ENTER_TEST

    simdb::ObjectManager obj_mgr(DB_DIR);
    using dt = simdb::ColumnDataType;

    simdb::Schema schema_with_nonscalar_cols;
    schema_with_nonscalar_cols.addTable("Numbers")
        .addColumn("MyScalar",    dt::double_t)
        .addColumn("MyNonScalar", dt::double_t)
            ->setDimensions({4,7,2});

    EXPECT_THROW(CREATE_SQL_SCHEMA(obj_mgr, schema_with_nonscalar_cols));

    simdb::Schema schema_with_zero_dims;
    schema_with_zero_dims.addTable("Numbers")
        .addColumn("MyScalar",   dt::double_t)
        .addColumn("MyZeroDims", dt::double_t)
            ->setDimensions({3,0,5});

    EXPECT_THROW(CREATE_SQL_SCHEMA(obj_mgr, schema_with_zero_dims));
}

void testSqlSchema()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;

    //Start by manually creating a Customers table with
    //properties First, Last, Age, RewardsBal, and Password.
    //These are chosen to include ints, strings, doubles,
    //and blobs, which are the column data types currently
    //supported.
    simdb::Schema schema;

    //Set default values for all supported data types
    const std::string default_first_name = "George";
    const std::string default_last_name = "Washington";
    const int32_t default_age = 67;
    const double default_rewards_bal = 1000000.00;

    schema.addTable("Customers")
        .addColumn("First",          dt::string_t)
            ->setDefaultValue(
                default_first_name)
        .addColumn("Last",           dt::string_t)
            ->setDefaultValue(
                default_last_name)
        .addColumn("Age",            dt::int32_t)
            ->setDefaultValue(
                default_age)
        .addColumn("RewardsBal",     dt::double_t)
            ->setDefaultValue(
                default_rewards_bal)
        .addColumn("Password",       dt::blob_t);

    //Make sure we cannot try to set a default value for a blob
    EXPECT_THROW(
        schema.addTable("Blobs")
            .addColumn("Foo", dt::blob_t)
                ->setDefaultValue(0)
    );

    simdb::DatabaseID customer1_id = 0, customer2_id = 0, customer3_id = 0;
    std::string db_file_path;

    //Make up some random-length (10 to 100 values) array of
    //numbers that represent some kind of password
    auto random_password = []() {
        auto random_len = rand() % 91 + 10;
        std::vector<double> password(random_len);
        for (auto & val : password) {
            val = rand() * M_PI;
        }
        return password;
    };

    const std::vector<double> customer1_password = random_password();
    const std::vector<double> customer2_password = random_password();

    std::tuple<std::string, std::string, int32_t, double, std::vector<double>> customer1_info =
        std::make_tuple("Alice", "Smith", 29, 74.28, customer1_password);

    std::tuple<std::string, std::string, int32_t, double, std::vector<double>> customer2_info =
        std::make_tuple("Bob", "Thompson", 41, 104.56, customer2_password);

    {
        //Create the physical database from these schema objects
        simdb::ObjectManager obj_mgr(DB_DIR);
        CREATE_SQL_SCHEMA(obj_mgr, schema);
        db_file_path = obj_mgr.getDatabaseFile();

        std::unique_ptr<simdb::TableRef> customers_tbl = obj_mgr.getTable("Customers");

        //Create two customer records
        std::unique_ptr<simdb::ObjectRef> customer1 = customers_tbl->createObjectWithArgs(
            "First",      std::get<0>(customer1_info),
            "Last",       std::get<1>(customer1_info),
            "Age",        std::get<2>(customer1_info),
            "RewardsBal", std::get<3>(customer1_info));

        std::unique_ptr<simdb::ObjectRef> customer2 = customers_tbl->createObjectWithArgs(
            "First",      std::get<0>(customer2_info),
            "Last",       std::get<1>(customer2_info),
            "Age",        std::get<2>(customer2_info),
            "RewardsBal", std::get<3>(customer2_info));

        //Make sure the AUTOINCREMENT is working - database IDs
        //should be unique across records in the same table
        customer1_id = customer1->getId();
        customer2_id = customer2->getId();
        EXPECT_NOTEQUAL(customer1_id, customer2_id);

        //Helper to make a Blob descriptor out of an array of numbers
        auto make_password_blob = [](const std::vector<double> & password) {
            simdb::Blob blob;
            blob.data_ptr = password.data();
            blob.num_bytes = password.size() * sizeof(double);
            return blob;
        };

        auto password1_blob = make_password_blob(std::get<4>(customer1_info));
        auto password2_blob = make_password_blob(std::get<4>(customer2_info));

        customer1->setPropertyBlob("Password", password1_blob);
        customer2->setPropertyBlob("Password", password2_blob);

        //Now create a third customer, but do not specify any of the
        //column values. We will use this customer record to ensure
        //the default column values we specified took hold.
        std::unique_ptr<simdb::ObjectRef> customer3 = customers_tbl->createObject();
        customer3_id = customer3->getId();

        //Also verify that we get a null TableRef if we ask for
        //a table that does not exist
        std::unique_ptr<simdb::TableRef> bad_table = obj_mgr.getTable("does-not-exist");
        EXPECT_EQUAL(bad_table.get(), nullptr);

        //Verify that we get a null ObjectRef if we ask for a record
        //from a valid table, but a non-existant database ID.
        std::unique_ptr<simdb::ObjectRef> bad_record = obj_mgr.findObject("Customers", 12345);
        EXPECT_EQUAL(bad_record.get(), nullptr);

        //Go through the ObjectManager::findObjects() API for a few
        //use cases, and verify the results.
        std::vector<std::unique_ptr<simdb::ObjectRef>> retrieved_customers;

        obj_mgr.findObjects("Customers", {customer1_id, customer2_id}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 2);
        EXPECT_EQUAL(retrieved_customers[0]->getId(), customer1_id);
        EXPECT_EQUAL(retrieved_customers[1]->getId(), customer2_id);

        obj_mgr.findObjects("Customers", {customer2_id, customer1_id}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 2);
        EXPECT_EQUAL(retrieved_customers[0]->getId(), customer2_id);
        EXPECT_EQUAL(retrieved_customers[1]->getId(), customer1_id);

        obj_mgr.findObjects("Customers", {customer1_id, 12345}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 2);
        EXPECT_EQUAL(retrieved_customers[0]->getId(), customer1_id);
        EXPECT_TRUE(retrieved_customers[1] == nullptr);

        obj_mgr.findObjects("Customers", {12345, customer1_id}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 2);
        EXPECT_EQUAL(retrieved_customers[1]->getId(), customer1_id);
        EXPECT_TRUE(retrieved_customers[0] == nullptr);

        obj_mgr.findObjects("Customers", {customer1_id}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 1);
        EXPECT_EQUAL(retrieved_customers[0]->getId(), customer1_id);

        obj_mgr.findObjects("Customers", {12345}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 1);
        EXPECT_TRUE(retrieved_customers[0] == nullptr);

        obj_mgr.findObjects("Customers", {}, retrieved_customers);
        EXPECT_EQUAL(retrieved_customers.size(), 3);
        EXPECT_EQUAL(retrieved_customers[0]->getId(), customer1_id);
        EXPECT_EQUAL(retrieved_customers[1]->getId(), customer2_id);
        EXPECT_EQUAL(retrieved_customers[2]->getId(), customer3_id);
    }

    //The previous connection has gone out of scope and is closed.
    //All we have is the full path to the database file, so let's
    //try to connect to it again and inspect the record values.
    simdb::ObjectManager obj_mgr(DB_DIR);
    EXPECT_TRUE(obj_mgr.connectToExistingDatabase(db_file_path));

    //Get back customer1 info and verify the fields
    std::unique_ptr<simdb::ObjectRef> retrieved_customer1 =
        obj_mgr.findObject("Customers", customer1_id);

    EXPECT_EQUAL(retrieved_customer1->getId(), customer1_id);

    EXPECT_EQUAL(retrieved_customer1->getPropertyString("First"),
                 std::get<0>(customer1_info));

    EXPECT_EQUAL(retrieved_customer1->getPropertyString("Last"),
                 std::get<1>(customer1_info));

    EXPECT_EQUAL(retrieved_customer1->getPropertyInt32("Age"),
                 std::get<2>(customer1_info));

    EXPECT_EQUAL(retrieved_customer1->getPropertyDouble("RewardsBal"),
                 std::get<3>(customer1_info));

    std::vector<double> customer1_retrieved_password;
    retrieved_customer1->getPropertyBlob("Password", customer1_retrieved_password);
    EXPECT_EQUAL(customer1_retrieved_password, customer1_password);

    //Get back customer2 info and verify the fields
    std::unique_ptr<simdb::ObjectRef> retrieved_customer2 =
        obj_mgr.findObject("Customers", customer2_id);

    EXPECT_EQUAL(retrieved_customer2->getId(), customer2_id);

    EXPECT_EQUAL(retrieved_customer2->getPropertyString("First"),
                 std::get<0>(customer2_info));

    EXPECT_EQUAL(retrieved_customer2->getPropertyString("Last"),
                 std::get<1>(customer2_info));

    EXPECT_EQUAL(retrieved_customer2->getPropertyInt32("Age"),
                 std::get<2>(customer2_info));

    EXPECT_EQUAL(retrieved_customer2->getPropertyDouble("RewardsBal"),
                 std::get<3>(customer2_info));

    std::vector<double> customer2_retrieved_password;
    retrieved_customer2->getPropertyBlob("Password", customer2_retrieved_password);
    EXPECT_EQUAL(customer2_retrieved_password, customer2_password);

    //Get back customer3 info and verify the fields (DEFAULTS)
    std::unique_ptr<simdb::ObjectRef> retrieved_customer3 =
        obj_mgr.findObject("Customers", customer3_id);

    EXPECT_EQUAL(retrieved_customer3->getId(), customer3_id);

    EXPECT_EQUAL(retrieved_customer3->getPropertyString("First"),
                 default_first_name);

    EXPECT_EQUAL(retrieved_customer3->getPropertyString("Last"),
                 default_last_name);

    EXPECT_EQUAL(retrieved_customer3->getPropertyInt32("Age"),
                 default_age);

    EXPECT_EQUAL(retrieved_customer3->getPropertyDouble("RewardsBal"),
                 default_rewards_bal);
}

void testSqlSchemaColumnModifiers()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;

    {
        simdb::Schema schema;

        EXPECT_NOTHROW(
            schema.addTable("Customers")
                .addColumn("LastName",  dt::string_t)
                    ->indexAgainst(
                          "FirstName")
                .addColumn("FirstName", dt::string_t)
        );

        simdb::ObjectManager obj_mgr(DB_DIR);
        EXPECT_NOTHROW(CREATE_SQL_SCHEMA(obj_mgr, schema));

        auto tbl = obj_mgr.getTable("Customers");
        EXPECT_TRUE(tbl != nullptr);

        std::unique_ptr<simdb::ObjectRef> customer;
        const std::string first_name = "George";
        const std::string last_name = "Washington";

        EXPECT_NOTHROW(
            customer = tbl->createObjectWithArgs(
                "FirstName", first_name,
                "LastName", last_name)
        );

        EXPECT_EQUAL(customer->getPropertyString("FirstName"), first_name);
        EXPECT_EQUAL(customer->getPropertyString("LastName"), last_name);
    }

    {
        simdb::Schema schema;

        //Create a schema, but make a typo in one of the column names.
        //It should not throw an exception until we try to give it to
        //an ObjectManager for database instantiation.

        EXPECT_NOTHROW(
            schema.addTable("Customers")
                .addColumn("LastName",  dt::string_t)
                    ->indexAgainst(
                          "FristName")
                .addColumn("FirstName", dt::string_t)
        );

        simdb::ObjectManager obj_mgr(DB_DIR);
        EXPECT_THROW(CREATE_SQL_SCHEMA(obj_mgr, schema));
    }
}

void testBasicDataTypes()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    schema.addTable("DTypes")
        .addColumn("A", dt::int8_t)
        .addColumn("B", dt::uint8_t)
        .addColumn("C", dt::int16_t)
        .addColumn("D", dt::uint16_t)
        .addColumn("E", dt::int32_t)
        .addColumn("F", dt::uint32_t)
        .addColumn("G", dt::int64_t)
        .addColumn("H", dt::uint64_t)
        .addColumn("I", dt::string_t)
        .addColumn("J", dt::char_t)
        .addColumn("K", dt::float_t)
        .addColumn("L", dt::double_t)
        .addColumn("M", dt::blob_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    EXPECT_NOTHROW(CREATE_SQL_SCHEMA(obj_mgr, schema));

    const int8_t A = -5;
    const uint8_t B = 10;
    const int16_t C = -20;
    const uint16_t D = 40;
    const int32_t E = -80;
    const uint32_t F = 160;
    const int64_t G = -320;
    const uint64_t H = 640;
    const std::string I = "minus seven twenty";
    const char J = '3';
    const float K = 0.14;
    const double L = 0.00159265359;
    const std::vector<int32_t> M = {0, 1, 2, 3, 4};

    auto dtypes = obj_mgr.getTable("DTypes");
    auto row = dtypes->createObject();

    row->setPropertyInt8   ("A", A);
    row->setPropertyUInt8  ("B", B);
    row->setPropertyInt16  ("C", C);
    row->setPropertyUInt16 ("D", D);
    row->setPropertyInt32  ("E", E);
    row->setPropertyUInt32 ("F", F);
    row->setPropertyInt64  ("G", G);
    row->setPropertyUInt64 ("H", H);
    row->setPropertyString ("I", I);
    row->setPropertyChar   ("J", J);
    row->setPropertyFloat  ("K", K);
    row->setPropertyDouble ("L", L);

    simdb::Blob blob_m;
    blob_m.data_ptr = M.data();
    blob_m.num_bytes = M.size() * sizeof(int32_t);
    row->setPropertyBlob("M", blob_m);

    EXPECT_EQUAL(row->getPropertyInt8   ("A"), A);
    EXPECT_EQUAL(row->getPropertyUInt8  ("B"), B);
    EXPECT_EQUAL(row->getPropertyInt16  ("C"), C);
    EXPECT_EQUAL(row->getPropertyUInt16 ("D"), D);
    EXPECT_EQUAL(row->getPropertyInt32  ("E"), E);
    EXPECT_EQUAL(row->getPropertyUInt32 ("F"), F);
    EXPECT_EQUAL(row->getPropertyInt64  ("G"), G);
    EXPECT_EQUAL(row->getPropertyUInt64 ("H"), H);
    EXPECT_EQUAL(row->getPropertyString ("I"), I);
    EXPECT_EQUAL(row->getPropertyChar   ("J"), J);
    EXPECT_EQUAL(row->getPropertyFloat  ("K"), K);
    EXPECT_EQUAL(row->getPropertyDouble ("L"), L);

    std::vector<int32_t> m;
    row->getPropertyBlob("M", m);
    EXPECT_EQUAL(m, M);

    row = dtypes->createObjectWithArgs(
        "A", A, "B", B, "C", C, "D", D,
        "E", E, "F", F, "G", G, "H", H,
        "I", I, "J", J, "K", K, "L", L,
        "M", M);

    EXPECT_EQUAL(row->getPropertyInt8   ("A"), A);
    EXPECT_EQUAL(row->getPropertyUInt8  ("B"), B);
    EXPECT_EQUAL(row->getPropertyInt16  ("C"), C);
    EXPECT_EQUAL(row->getPropertyUInt16 ("D"), D);
    EXPECT_EQUAL(row->getPropertyInt32  ("E"), E);
    EXPECT_EQUAL(row->getPropertyUInt32 ("F"), F);
    EXPECT_EQUAL(row->getPropertyInt64  ("G"), G);
    EXPECT_EQUAL(row->getPropertyUInt64 ("H"), H);
    EXPECT_EQUAL(row->getPropertyString ("I"), I);
    EXPECT_EQUAL(row->getPropertyChar   ("J"), J);
    EXPECT_EQUAL(row->getPropertyFloat  ("K"), K);
    EXPECT_EQUAL(row->getPropertyDouble ("L"), L);

    int8_t A2;
    uint8_t B2;
    int16_t C2;
    uint16_t D2;
    int32_t E2;
    uint32_t F2;
    int64_t G2;
    uint64_t H2;
    std::string I2;
    char J2;
    float K2;
    double L2;
    std::vector<int32_t> M2;

    simdb::ObjectQuery query(obj_mgr, "DTypes");
    query.addConstraints("Id", simdb::constraints::equal, row->getId());

    query.writeResultIterationsTo(
        "A", &A2, "B", &B2, "C", &C2, "D", &D2,
        "E", &E2, "F", &F2, "G", &G2, "H", &H2,
        "I", &I2, "J", &J2, "K", &K2, "L", &L2,
        "M", &M2);

    EXPECT_TRUE(query.executeQuery()->getNext());

    EXPECT_EQUAL(A, A2);
    EXPECT_EQUAL(B, B2);
    EXPECT_EQUAL(C, C2);
    EXPECT_EQUAL(D, D2);
    EXPECT_EQUAL(E, E2);
    EXPECT_EQUAL(F, F2);
    EXPECT_EQUAL(G, G2);
    EXPECT_EQUAL(H, H2);
    EXPECT_EQUAL(I, I2);
    EXPECT_EQUAL(J, J2);
    EXPECT_EQUAL(K, K2);
    EXPECT_EQUAL(L, L2);
    EXPECT_EQUAL(M, M2);
}

void test64BitInts()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("My64BitInts")
        .addColumn("MySigned", dt::int64_t)
        .addColumn("MyUnsigned", dt::uint64_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    constexpr int64_t min_signed = std::numeric_limits<int64_t>::min();
    constexpr int64_t max_signed = std::numeric_limits<int64_t>::max();

    constexpr uint64_t min_unsigned = std::numeric_limits<uint64_t>::min();
    constexpr uint64_t max_unsigned = std::numeric_limits<uint64_t>::max();

    auto tbl = obj_mgr.getTable("My64BitInts");

    auto row = tbl->createObject();
    row->setPropertyInt64("MySigned", min_signed);
    row->setPropertyUInt64("MyUnsigned", min_unsigned);
    EXPECT_EQUAL(row->getPropertyInt64("MySigned"), min_signed);
    EXPECT_EQUAL(row->getPropertyUInt64("MyUnsigned"), min_unsigned);

    row->setPropertyInt64("MySigned", max_signed);
    row->setPropertyUInt64("MyUnsigned", max_unsigned);
    EXPECT_EQUAL(row->getPropertyInt64("MySigned"), max_signed);
    EXPECT_EQUAL(row->getPropertyUInt64("MyUnsigned"), max_unsigned);

    row = tbl->createObjectWithArgs(
        "MySigned", min_signed, "MyUnsigned", min_unsigned);
    EXPECT_EQUAL(row->getPropertyInt64("MySigned"), min_signed);
    EXPECT_EQUAL(row->getPropertyUInt64("MyUnsigned"), min_unsigned);

    row = tbl->createObjectWithArgs(
        "MySigned", max_signed, "MyUnsigned", max_unsigned);
    EXPECT_EQUAL(row->getPropertyInt64("MySigned"), max_signed);
    EXPECT_EQUAL(row->getPropertyUInt64("MyUnsigned"), max_unsigned);

    std::unique_ptr<simdb::ObjectQuery> query(
        new simdb::ObjectQuery(obj_mgr, "My64BitInts"));

    auto verify_int64 = [&](const int64_t expected) {
        int64_t actual = 0;
        query->writeResultIterationsTo("MySigned", &actual);

        auto result_iter = query->executeQuery();
        while (result_iter->getNext()) {
            EXPECT_EQUAL(actual, expected);
        }
    };

    query->addConstraints("MySigned", simdb::constraints::equal, min_signed);
    verify_int64(min_signed);

    query->addConstraints("MySigned", simdb::constraints::equal, max_signed);
    verify_int64(max_signed);

    auto verify_uint64 = [&](const uint64_t expected) {
        uint64_t actual = 0;
        query->writeResultIterationsTo("MyUnsigned", &actual);

        auto result_iter = query->executeQuery();
        while (result_iter->getNext()) {
            EXPECT_EQUAL(actual, expected);
        }
    };

    query->addConstraints("MyUnsigned", simdb::constraints::equal, min_unsigned);
    verify_uint64(min_unsigned);

    query->addConstraints("MyUnsigned", simdb::constraints::equal, max_unsigned);
    verify_uint64(max_unsigned);
}

void testObjectQuery()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;

    simdb::Schema schema;

    schema.addTable("ReportHeader")
        .addColumn("ReportName", dt::string_t)
        .addColumn("StartTime",  dt::uint64_t)
        .addColumn("EndTime",    dt::uint64_t);

    schema.addTable("StatInstValues")
        .addColumn("TimeseriesChunkID", dt::int32_t)
        .addColumn("RawBytes",          dt::blob_t)
        .addColumn("NumPts",            dt::int32_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    std::unique_ptr<simdb::TableRef> header_tbl = obj_mgr.getTable("ReportHeader");

    struct RecordProps {
        std::string report_name;
        uint64_t start_time;
        uint64_t end_time;
    };

    //-------------------  Mini-test #1 ---------------------//

    RecordProps record1;
    record1.report_name = "ObjectQueryTest1";
    record1.start_time = 5000;
    record1.end_time = 100000;

    std::unique_ptr<simdb::ObjectRef> obj1 = header_tbl->createObjectWithArgs(
        "ReportName", record1.report_name,
        "StartTime",  record1.start_time,
        "EndTime",    record1.end_time);

    RecordProps record2;
    record2.report_name = "ObjectQueryTest2";
    record2.start_time = 6000;
    record2.end_time = 97000;

    std::unique_ptr<simdb::ObjectRef> obj2 = header_tbl->createObjectWithArgs(
        "ReportName", record2.report_name,
        "StartTime",  record2.start_time,
        "EndTime",    record2.end_time);

    RecordProps record3;
    record3.report_name = record2.report_name;
    record3.start_time = 5500;
    record3.end_time = 114000;

    std::unique_ptr<simdb::ObjectRef> obj3 = header_tbl->createObjectWithArgs(
        "ReportName", record3.report_name,
        "StartTime",  record3.start_time,
        "EndTime",    record3.end_time);

    //Now that we have a few records, let's ask for them back
    simdb::ObjectQuery query(obj_mgr, "ReportHeader");
    RecordProps retrieved;

    //Look for records with StartTime>5200 AND EndTime<120000
    //  (should be record2 and record3)
    query.addConstraints(
        "StartTime", simdb::constraints::greater, 5200,
        "EndTime", simdb::constraints::less, 120000);

    query.writeResultIterationsTo(
        "ReportName", &retrieved.report_name,
        "StartTime", &retrieved.start_time,
        "EndTime", &retrieved.end_time);

    std::unique_ptr<simdb::ResultIter> result = query.executeQuery();

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record2.report_name);
    EXPECT_EQUAL(retrieved.start_time, record2.start_time);
    EXPECT_EQUAL(retrieved.end_time, record2.end_time);

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record3.report_name);
    EXPECT_EQUAL(retrieved.start_time, record3.start_time);
    EXPECT_EQUAL(retrieved.end_time, record3.end_time);

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //-------------------  Mini-test #2 ---------------------//

    //Look for records with ReportName="ObjectQueryTest2" AND EndTime>=97000
    //  (should also be record2 and record3)
    query.addConstraints(
        "ReportName", simdb::constraints::equal, record2.report_name,
        "EndTime", simdb::constraints::greater_equal, record2.end_time);

    query.writeResultIterationsTo(
        "StartTime", &retrieved.start_time,
        "EndTime", &retrieved.end_time);

    result = query.executeQuery();

    //Before validating the answers, clear out the old structs
    retrieved.report_name.clear();
    retrieved.start_time = 0;
    retrieved.end_time = 0;

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.start_time, record2.start_time);
    EXPECT_EQUAL(retrieved.end_time, record2.end_time);
    //Note that since we did *not* ask for ReportName or any other
    //iteration values, this field should still be empty.
    EXPECT_TRUE(retrieved.report_name.empty());

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.start_time, record3.start_time);
    EXPECT_EQUAL(retrieved.end_time, record3.end_time);
    //Note that since we did *not* ask for ReportName or any other
    //iteration values, this field should still be empty.
    EXPECT_TRUE(retrieved.report_name.empty());

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //Run another query looking for records with StartTime<50
    //  (should be none)
    query.addConstraints(
        "StartTime", simdb::constraints::less, 50);

    query.writeResultIterationsTo(
        "StartTime", &retrieved.start_time,
        "EndTime", &retrieved.end_time);

    result = query.executeQuery();
    EXPECT_FALSE(result->getNext());

    //Run another query without any constraints
    //  (should get all three records)
    query.writeResultIterationsTo(
        "ReportName", &retrieved.report_name,
        "StartTime", &retrieved.start_time,
        "EndTime", &retrieved.end_time);

    result = query.executeQuery();

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record1.report_name);
    EXPECT_EQUAL(retrieved.start_time, record1.start_time);
    EXPECT_EQUAL(retrieved.end_time, record1.end_time);

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record2.report_name);
    EXPECT_EQUAL(retrieved.start_time, record2.start_time);
    EXPECT_EQUAL(retrieved.end_time, record2.end_time);

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record3.report_name);
    EXPECT_EQUAL(retrieved.start_time, record3.start_time);
    EXPECT_EQUAL(retrieved.end_time, record3.end_time);

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //Now let's make some table entries for StatInstValues.
    //This has blob columns in it, which we will add to
    //result set iterators to verify.
    std::unique_ptr<simdb::TableRef> si_values_tbl =
        obj_mgr.getTable("StatInstValues");

    std::vector<double> raw_si1 = {1, 3, 5, 7, 9};
    std::vector<double> raw_si2 = {2, 4, 6, 8};
    const simdb::DatabaseID ts_chunk_id = 40;

    std::unique_ptr<simdb::ObjectRef> si_chunk1 = si_values_tbl->createObject();
    si_chunk1->setPropertyInt32("TimeseriesChunkID", ts_chunk_id);

    simdb::Blob blob_desc1;
    blob_desc1.data_ptr = &raw_si1[0];
    blob_desc1.num_bytes = raw_si1.size() * sizeof(double);
    si_chunk1->setPropertyBlob("RawBytes", blob_desc1);
    si_chunk1->setPropertyInt32("NumPts", (int)raw_si1.size());

    std::unique_ptr<simdb::ObjectRef> si_chunk2 = si_values_tbl->createObject();
    si_chunk2->setPropertyInt32("TimeseriesChunkID", ts_chunk_id);

    simdb::Blob blob_desc2;
    blob_desc2.data_ptr = &raw_si2[0];
    blob_desc2.num_bytes = raw_si2.size() * sizeof(double);
    si_chunk2->setPropertyBlob("RawBytes", blob_desc2);
    si_chunk2->setPropertyInt32("NumPts", (int)raw_si2.size());

    //Now run a query to get back both blobs one at a time.
    //We should be able to incrementally pass over numerous
    //chunks while only using one vector<double> to do so.
    simdb::ObjectQuery query2(obj_mgr, "StatInstValues");

    int num_retrieved_si_values = 0;
    std::vector<double> retrieved_si_values;

    query2.addConstraints(
        "TimeseriesChunkID", simdb::constraints::equal, ts_chunk_id);

    query2.writeResultIterationsTo(
        "RawBytes", &retrieved_si_values,
        "NumPts", &num_retrieved_si_values);

    result = query2.executeQuery();

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(num_retrieved_si_values, (int)raw_si1.size());
    EXPECT_EQUAL(retrieved_si_values, raw_si1);

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(num_retrieved_si_values, (int)raw_si2.size());
    EXPECT_EQUAL(retrieved_si_values, raw_si2);

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //-------------------  Mini-test #3 ---------------------//

    header_tbl->deleteAllObjects();

    //Verify the behavior of the "in_set" / "not_in_set"
    //constraints. Run some queries that look like this:
    //  SELECT * FROM MyTable WHERE ThisInteger IN (4,56,99)
    //  SELECT * FROM MyTable WHERE ThisString NOT IN ('fiz','baz')
    //  etc.
    RecordProps record4;
    record4.report_name = "Foo";
    record4.end_time = 14000;

    RecordProps record5;
    record5.report_name = "Bar";
    record5.end_time = 14000;

    RecordProps record6;
    record6.report_name = "Biz";
    record6.end_time = 16000;

    RecordProps record7;
    record7.report_name = "Baz";
    record7.end_time = 22000;

    std::unique_ptr<simdb::ObjectRef> objA = header_tbl->createObjectWithArgs(
        "ReportName", record4.report_name,
        "EndTime",    record4.end_time);

    std::unique_ptr<simdb::ObjectRef> objB = header_tbl->createObjectWithArgs(
        "ReportName", record5.report_name,
        "EndTime",    record5.end_time);

    std::unique_ptr<simdb::ObjectRef> objC = header_tbl->createObjectWithArgs(
        "ReportName", record6.report_name,
        "EndTime",    record6.end_time);

    std::unique_ptr<simdb::ObjectRef> objD = header_tbl->createObjectWithArgs(
        "ReportName", record7.report_name,
        "EndTime",    record7.end_time);

    //Run a query to get all records with report name that
    //is either "Bar" or "Baz" (record5 and record7)
    query.addConstraints(
        "ReportName", simdb::constraints::in_set, {"Bar","Baz"});

    query.writeResultIterationsTo(
        "ReportName", &retrieved.report_name,
        "EndTime", &retrieved.end_time);

    result = query.executeQuery();

    //Before advancing the iterator, clear out the retrieved
    //record data structure
    retrieved.report_name.clear();
    retrieved.end_time = 0;

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record5.report_name);
    EXPECT_EQUAL(retrieved.end_time, record5.end_time);

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record7.report_name);
    EXPECT_EQUAL(retrieved.end_time, record7.end_time);

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //Run a query to get all records with an end time that
    //is NOT in (14000,22000) - expect only one returned
    //result, record6.
    query.addConstraints(
        "EndTime", simdb::constraints::not_in_set, {14000,22000});

    query.writeResultIterationsTo(
        "ReportName", &retrieved.report_name,
        "EndTime", &retrieved.end_time);

    result = query.executeQuery();

    //Before advancing the iterator, clear out the retrieved
    //record data structure
    retrieved.report_name.clear();
    retrieved.end_time = 0;

    EXPECT_TRUE(result->getNext());
    EXPECT_EQUAL(retrieved.report_name, record6.report_name);
    EXPECT_EQUAL(retrieved.end_time, record6.end_time);

    //There should not be any more in this result set
    EXPECT_FALSE(result->getNext());

    //-------------------  Mini-test #4 ---------------------//

    si_values_tbl->deleteAllObjects();

    //This mini-test is going to verify that we can recover raw
    //blobs from the database as vectors of a specific data type.
    //For example, store SI values as char vectors (which is what
    //a blob is), but read them back as std::vector<double>, which
    //is the original data type (this simple example assumes no
    //compression and just illustrates the point).
    const std::vector<int16_t> mini_test4_raw_si1 = {4, 6, 7, 2, 4, 8};
    std::unique_ptr<simdb::ObjectRef> mini_test4_blob_ref =
        si_values_tbl->createObject();

    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &mini_test4_raw_si1[0];
    blob_descriptor.num_bytes = mini_test4_raw_si1.size()*sizeof(int16_t);
    mini_test4_blob_ref->setPropertyBlob("RawBytes", blob_descriptor);

    //We wrote the int16_t vector into the database as a blob, which
    //effectively stripped away the fact that it was *specifically*
    //int16_t. Let's verify that even though the blobs are just raw
    //char vectors, we can ask for them back in their original vector
    //form (original data type, without the need to reinterpret_cast).
    std::vector<int16_t> mini_test4_retrieved_si1;
    query2.writeResultIterationsTo("RawBytes", &mini_test4_retrieved_si1);

    auto mini_test4_result_iter = query2.executeQuery();
    EXPECT_TRUE(mini_test4_result_iter->getNext());
    EXPECT_EQUAL(mini_test4_retrieved_si1, mini_test4_raw_si1);
    EXPECT_FALSE(mini_test4_result_iter->getNext());

    si_values_tbl->deleteAllObjects();

    //Do this mini test again with a blob of floats
    const std::vector<float> mini_test4_raw_si2 = {-1, -9, 500, 334};
    mini_test4_blob_ref = si_values_tbl->createObject();

    blob_descriptor.data_ptr = &mini_test4_raw_si2[0];
    blob_descriptor.num_bytes = mini_test4_raw_si2.size()*sizeof(float);
    mini_test4_blob_ref->setPropertyBlob("RawBytes", blob_descriptor);

    std::vector<float> mini_test4_retrieved_si2;
    query2.writeResultIterationsTo("RawBytes", &mini_test4_retrieved_si2);
    mini_test4_result_iter = query2.executeQuery();
    EXPECT_TRUE(mini_test4_result_iter->getNext());
    EXPECT_EQUAL(mini_test4_retrieved_si2, mini_test4_raw_si2);
    EXPECT_FALSE(mini_test4_result_iter->getNext());

    //-------------------  Mini-test #5 ---------------------//

    //Let's make a small schema with some double columns, insert
    //a few records, and run queries against it. The ObjectQuery
    //code uses stringifiers to put together the SQL statements,
    //and floating point stringifiers should be robust regardless
    //of how many sigificant digits the column values have.

    simdb::Schema DoublesSchema;
    DoublesSchema.addTable("Doubles")
        .addColumn("Foo", dt::double_t);

    simdb::ObjectManager doubles_obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(doubles_obj_mgr, DoublesSchema);

    auto doubles_tbl = doubles_obj_mgr.getTable("Doubles");
    auto doubles1 = doubles_tbl->createObject();
    auto doubles2 = doubles_tbl->createObject();
    auto doubles3 = doubles_tbl->createObject();

    const double foo1 = 3.0;
    const double foo2 = 7.8899239572345;
    const double foo3 = (0.1 + 0.1 + 0.1);

    doubles1->setPropertyDouble("Foo", foo1);
    doubles2->setPropertyDouble("Foo", foo2);
    doubles3->setPropertyDouble("Foo", foo3);

    simdb::ObjectQuery doubles_query(doubles_obj_mgr, "Doubles");

    double stored_foo;
    doubles_query.writeResultIterationsTo("Foo", &stored_foo);
    doubles_query.addConstraints("Foo", simdb::constraints::equal, foo1);

    auto result_iter = doubles_query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(stored_foo, foo1);
    EXPECT_FALSE(result_iter->getNext());

    doubles_query.writeResultIterationsTo("Foo", &stored_foo);
    doubles_query.addConstraints("Foo", simdb::constraints::equal, foo2);

    result_iter = doubles_query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(stored_foo, foo2);
    EXPECT_FALSE(result_iter->getNext());

    doubles_query.writeResultIterationsTo("Foo", &stored_foo);
    doubles_query.addConstraints("Foo", simdb::constraints::equal, foo3);

    result_iter = doubles_query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(stored_foo, foo3);
    EXPECT_FALSE(result_iter->getNext());

    //-------------------  Mini-test #3 ---------------------//

    //Verify the behavior of ObjectQuery::countMatches()
    header_tbl->deleteAllObjects();

    const std::string hello_world = "hello_world.csv";
    const std::string fizz_buzz = "fizz_buzz.json";

    header_tbl->createObjectWithArgs("ReportName", hello_world,
                                     "StartTime", 1000,
                                     "EndTime", 5000000);

    header_tbl->createObjectWithArgs("ReportName", hello_world,
                                     "StartTime", 2000,
                                     "EndTime", 4500000);

    header_tbl->createObjectWithArgs("ReportName", fizz_buzz,
                                     "StartTime", 1000,
                                     "EndTime", 5000000);

    simdb::ObjectQuery count_query(obj_mgr, "ReportHeader");

    //Zero-constraint queries always should find all records
    //in this table
    EXPECT_EQUAL(count_query.countMatches(), 3);

    //Add a constraint and ask again...
    count_query.addConstraints("ReportName",
                               simdb::constraints::equal,
                               "hello_world.csv");
    EXPECT_EQUAL(count_query.countMatches(), 2);

    //Add a second constraint and ask again...
    count_query.addConstraints("StartTime",
                               simdb::constraints::greater_equal,
                               1800);
    EXPECT_EQUAL(count_query.countMatches(), 1);

    //Add a third constraint which matches no records, and
    //verify the query returns zero
    count_query.addConstraints("EndTime",
                               simdb::constraints::less,
                               3000000);
    EXPECT_EQUAL(count_query.countMatches(), 0);
}

void testObjectQueryOptions()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    schema.addTable("Metadata")
        .addColumn("A", dt::int32_t)
        .addColumn("B", dt::string_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    auto meta = obj_mgr.getTable("Metadata");

    auto row1 = meta->createObject();
    row1->setPropertyInt32("A", 5);
    row1->setPropertyString("B", "foo");

    auto row2 = meta->createObject();
    row2->setPropertyInt32("A", 8);
    row2->setPropertyString("B", "abc");

    auto row3 = meta->createObject();
    row3->setPropertyInt32("A", 3);
    row3->setPropertyString("B", "bar");

    int a;
    std::string b;

    simdb::ObjectQuery query(obj_mgr, "Metadata");
    query.writeResultIterationsTo("A", &a, "B", &b);
    query.orderBy(simdb::OrderBy("A", simdb::ASC));

    auto result_iter = query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 3);
    EXPECT_EQUAL(b, "bar");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 5);
    EXPECT_EQUAL(b, "foo");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 8);
    EXPECT_EQUAL(b, "abc");
    EXPECT_FALSE(result_iter->getNext());

    query.writeResultIterationsTo("A", &a, "B", &b);
    query.orderBy(simdb::OrderBy("B", simdb::DESC));

    result_iter = query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 5);
    EXPECT_EQUAL(b, "foo");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 3);
    EXPECT_EQUAL(b, "bar");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 8);
    EXPECT_EQUAL(b, "abc");
    EXPECT_FALSE(result_iter->getNext());

    query.writeResultIterationsTo("A", &a, "B", &b);
    query.orderBy(simdb::OrderBy("A", simdb::DESC));
    query.setLimit(1);

    result_iter = query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 8);
    EXPECT_EQUAL(b, "abc");
    EXPECT_FALSE(result_iter->getNext());

    query.writeResultIterationsTo("A", &a, "B", &b);
    EXPECT_NOTHROW(query.setLimit(0));

    result_iter = query.executeQuery();
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 5);
    EXPECT_EQUAL(b, "foo");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 8);
    EXPECT_EQUAL(b, "abc");
    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(a, 3);
    EXPECT_EQUAL(b, "bar");
    EXPECT_FALSE(result_iter->getNext());
}

void testObjectCreationArgs()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    const int32_t default_a = 88;
    const uint64_t default_b = 10000;
    const double default_c = 100.55;
    const std::string default_d = "someDefaultString";
    const char * default_e = "someDefaultLiteral";

    schema.addTable("DTypes")
        .addColumn("MyInt32",             dt::int32_t)
            ->setDefaultValue(default_a)
        .addColumn("MyUInt64",            dt::uint64_t)
            ->setDefaultValue(default_b)
        .addColumn("MyDouble",            dt::double_t)
            ->setDefaultValue(default_c)
        .addColumn("MyString",            dt::string_t)
            ->setDefaultValue(default_d)
        .addColumn("MyLiteral",           dt::string_t)
            ->setDefaultValue(default_e)
        .addColumn("MyBlob",              dt::blob_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    const int32_t a = 95;
    const uint64_t b = 4000;
    const double c = 5.678;
    const std::string d = "foo";
    const char * e = "helloWorld";
    const std::vector<double> f = {1.3, 1.4, 5.6, 8.8};

    auto dtype_table = obj_mgr.getTable("DTypes");

    auto record1 = dtype_table->createObjectWithArgs("MyInt32", a);
    EXPECT_EQUAL(record1->getPropertyInt32("MyInt32"), a);

    auto record2 = dtype_table->createObjectWithArgs("MyUInt64", b);
    EXPECT_EQUAL(record2->getPropertyUInt64("MyUInt64"), b);

    auto record3 = dtype_table->createObjectWithArgs("MyDouble", c);
    EXPECT_EQUAL(record3->getPropertyDouble("MyDouble"), c);

    auto record4 = dtype_table->createObjectWithArgs("MyString", d);
    EXPECT_EQUAL(record4->getPropertyString("MyString"), d);

    auto record5 = dtype_table->createObjectWithArgs("MyLiteral", e);
    EXPECT_EQUAL(record5->getPropertyString("MyLiteral"), e);

    auto record6 = dtype_table->createObjectWithArgs("MyBlob", f);
    std::vector<double> my_blob;
    record6->getPropertyBlob("MyBlob", my_blob);
    EXPECT_EQUAL(my_blob, f);

    const int32_t a2 = 50;
    const uint64_t b2 = 99999;
    const double c2 = 5.848;
    const std::string d2 = "mightyDucks";
    const char * e2 = "helloAgain";
    const std::vector<double> f2 = {4.5, 5.6, 6.7, 7.8};

    auto validate_multi_arg = [&](std::unique_ptr<simdb::ObjectRef> & record) {
        EXPECT_EQUAL(record->getPropertyInt32("MyInt32"), a2);
        EXPECT_EQUAL(record->getPropertyUInt64("MyUInt64"), b2);
        EXPECT_EQUAL(record->getPropertyDouble("MyDouble"), c2);
        EXPECT_EQUAL(record->getPropertyString("MyString"), d2);
        EXPECT_EQUAL(record->getPropertyString("MyLiteral"), e2);

        std::vector<double> my_blob2;
        record->getPropertyBlob("MyBlob", my_blob2);
        EXPECT_EQUAL(my_blob2, f2);
    };

    //Ensure the variadic function works correctly. Mix up the
    //input arguments so those that have special enable_if handling
    //appear at the beginning and the end of the parameter pack.

    //Blobs at the end
    auto record7 = dtype_table->createObjectWithArgs(
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2,
        "MyString", d2,
        "MyLiteral", e2,
        "MyBlob", f2);

    validate_multi_arg(record7);

    //String literals at the end
    auto record8 = dtype_table->createObjectWithArgs(
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2,
        "MyString", d2,
        "MyBlob", f2,
        "MyLiteral", e2);

    validate_multi_arg(record8);

    //Standard strings at the end
    auto record9 = dtype_table->createObjectWithArgs(
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2,
        "MyBlob", f2,
        "MyLiteral", e2,
        "MyString", d2);

    validate_multi_arg(record9);

    //Blobs at the beginning
    auto record10 = dtype_table->createObjectWithArgs(
        "MyBlob", f2,
        "MyLiteral", e2,
        "MyString", d2,
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2);

    validate_multi_arg(record10);

    //String literals at the beginning
    auto record11 = dtype_table->createObjectWithArgs(
        "MyLiteral", e2,
        "MyBlob", f2,
        "MyString", d2,
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2);

    validate_multi_arg(record11);

    //Standard strings at the beginning
    auto record12 = dtype_table->createObjectWithArgs(
        "MyString", d2,
        "MyLiteral", e2,
        "MyBlob", f2,
        "MyInt32", a2,
        "MyUInt64", b2,
        "MyDouble", c2);

    validate_multi_arg(record12);

    //Create a record with an empty blob column value. This should
    //result in a record with all default values filled in.
    std::vector<double> empty_blob;
    auto record13 = dtype_table->createObjectWithArgs("MyBlob", empty_blob);

    EXPECT_EQUAL(record13->getPropertyInt32("MyInt32"), default_a);
    EXPECT_EQUAL(record13->getPropertyUInt64("MyUInt64"), default_b);
    EXPECT_EQUAL(record13->getPropertyDouble("MyDouble"), default_c);
    EXPECT_EQUAL(record13->getPropertyString("MyString"), default_d);
    EXPECT_EQUAL(record13->getPropertyString("MyLiteral"), default_e);

    my_blob.clear();
    record13->getPropertyBlob("MyBlob", my_blob);
    EXPECT_TRUE(my_blob.empty());
}

void testObjectDeletionArgs()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    schema.addTable("DTypes")
        .addColumn("MyInt32",   dt::int32_t)
        .addColumn("MyUInt64",  dt::uint64_t)
        .addColumn("MyDouble",  dt::double_t)
        .addColumn("MyString",  dt::string_t)
        .addColumn("MyLiteral", dt::string_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    //Fill up a table with a bunch of records. We will pick off
    //several of these records to delete at a time and verify
    //the deletion happened correctly.
    auto dtype_table = obj_mgr.getTable("DTypes");

    const std::vector<std::string> foo_strings = {
        "fooA", "fooB", "fooC", "fooD", "fooE",
        "fooF", "fooG", "fooH", "fooI", "fooJ"
    };

    //
    //     MyInt32   MyUInt64   MyDouble   MyString   MyLiteral
    //    --------- ---------- ---------- ---------- -----------
    //     10        5000        3.5       fooA       barA
    //     12        5100        4.5       fooB       barB
    //     14        5200        5.5       fooC       barC
    //     16        5300        6.5       fooD       barD
    //     18        5400        7.5       fooE       barE
    //     20        5500        8.5       fooF       barF
    //     22        5600        9.5       fooG       barG
    //     24        5700       10.5       fooH       barH
    //     26        5800       11.5       fooI       barI
    //     28        5900       12.5       fooJ       barJ
    //
    std::set<simdb::DatabaseID> remaining_record_ids;
    std::unique_ptr<simdb::ObjectRef> record;

    record = dtype_table->createObjectWithArgs("MyInt32", 10,
                                               "MyUInt64", 5000,
                                               "MyDouble", 3.5,
                                               "MyString", foo_strings[0],
                                               "MyLiteral", "barA");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 12,
                                               "MyUInt64", 5100,
                                               "MyDouble", 4.5,
                                               "MyString", foo_strings[1],
                                               "MyLiteral", "barB");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 14,
                                               "MyUInt64", 5200,
                                               "MyDouble", 5.5,
                                               "MyString", foo_strings[2],
                                               "MyLiteral", "barC");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 16,
                                               "MyUInt64", 5300,
                                               "MyDouble", 6.5,
                                               "MyString", foo_strings[3],
                                               "MyLiteral", "barD");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 18,
                                               "MyUInt64", 5400,
                                               "MyDouble", 7.5,
                                               "MyString", foo_strings[4],
                                               "MyLiteral", "barE");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 20,
                                               "MyUInt64", 5500,
                                               "MyDouble", 8.5,
                                               "MyString", foo_strings[5],
                                               "MyLiteral", "barF");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 22,
                                               "MyUInt64", 5600,
                                               "MyDouble", 9.5,
                                               "MyString", foo_strings[6],
                                               "MyLiteral", "barG");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 24,
                                               "MyUInt64", 5700,
                                               "MyDouble", 10.5,
                                               "MyString", foo_strings[7],
                                               "MyLiteral", "barH");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 26,
                                               "MyUInt64", 5800,
                                               "MyDouble", 11.5,
                                               "MyString", foo_strings[8],
                                               "MyLiteral", "barI");
    remaining_record_ids.insert(record->getId());

    record = dtype_table->createObjectWithArgs("MyInt32", 28,
                                               "MyUInt64", 5900,
                                               "MyDouble", 12.5,
                                               "MyString", foo_strings[9],
                                               "MyLiteral", "barJ");
    remaining_record_ids.insert(record->getId());

    std::vector<std::unique_ptr<simdb::ObjectRef>> remaining_records;
    obj_mgr.findObjects("DTypes", {}, remaining_records);
    EXPECT_EQUAL(remaining_records.size(), remaining_record_ids.size());

    for (const auto & row : remaining_records) {
        EXPECT_FALSE(remaining_record_ids.find(row->getId()) ==
                     remaining_record_ids.end());
    }

    //Let's start by removing records one at a time with a
    //single match constraint.
    auto verify_deletion = [&](const std::set<simdb::DatabaseID> & remaining_ids) {
        std::vector<std::unique_ptr<simdb::ObjectRef>> remaining_objs;
        obj_mgr.findObjects("DTypes", {}, remaining_objs);
        EXPECT_EQUAL(remaining_objs.size(), remaining_ids.size());

        for (const auto & obj : remaining_objs) {
            EXPECT_FALSE(remaining_ids.find(obj->getId()) == remaining_ids.end());
        }
    };

    dtype_table->deleteObjectsWhere("MyInt32", simdb::constraints::equal, 10);
    remaining_record_ids.erase(1);
    verify_deletion(remaining_record_ids);

    dtype_table->deleteObjectsWhere("MyUInt64", simdb::constraints::equal, 5100);
    remaining_record_ids.erase(2);
    verify_deletion(remaining_record_ids);

    dtype_table->deleteObjectsWhere("MyDouble", simdb::constraints::equal, 5.5);
    remaining_record_ids.erase(3);
    verify_deletion(remaining_record_ids);

    dtype_table->deleteObjectsWhere("MyString", simdb::constraints::equal, foo_strings[3]);
    remaining_record_ids.erase(4);
    verify_deletion(remaining_record_ids);

    dtype_table->deleteObjectsWhere("MyLiteral", simdb::constraints::equal, "barE");
    remaining_record_ids.erase(5);
    verify_deletion(remaining_record_ids);

    //Now let's remove a record with a multi-argument match constraint.
    dtype_table->deleteObjectsWhere("MyInt32", simdb::constraints::equal, 20,
                                    "MyString", simdb::constraints::equal, foo_strings[5],
                                    "MyLiteral", simdb::constraints::equal, "barF");

    remaining_record_ids.erase(6);
    verify_deletion(remaining_record_ids);

    //Let's remove two more records using the "is in" constraint.
    //We will do this for an integer column first.
    dtype_table->deleteObjectsWhere("MyInt32", simdb::constraints::in_set, {22,24});

    remaining_record_ids.erase(7);
    remaining_record_ids.erase(8);
    verify_deletion(remaining_record_ids);

    //Remove two more records using the "is in" constraint, this
    //time against a string column.
    dtype_table->deleteObjectsWhere("MyString",
                                    simdb::constraints::in_set,
                                    {"fooI","fooJ"});

    remaining_record_ids.clear();
    verify_deletion(remaining_record_ids);
}

void testObjectUpdateArgs()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    schema.addTable("DTypes")
        .addColumn("MyInt32",  dt::int32_t)
        .addColumn("MyUInt64", dt::uint64_t)
        .addColumn("MyDouble", dt::double_t)
        .addColumn("MyString", dt::string_t)
        .addColumn("MyBlob",   dt::blob_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    auto dtype_table = obj_mgr.getTable("DTypes");

    //Create objects with an initial set of column values.
    //We will overwrite those values in batches shortly.
    dtype_table->createObjectWithArgs("MyInt32", 10,
                                      "MyUInt64", 3000,
                                      "MyDouble", 345.75,
                                      "MyString", "hello");

    dtype_table->createObjectWithArgs("MyInt32", 12,
                                      "MyUInt64", 3100,
                                      "MyDouble", 545.50,
                                      "MyString", "helloAgain");

    dtype_table->createObjectWithArgs("MyInt32", 14,
                                      "MyUInt64", 3200,
                                      "MyDouble", 745.25,
                                      "MyString", "goodbye");

    //Overwrite the MyDouble columns to 123.45 and the MyString
    //columns to "justOverwritten" for the records whose MyDouble
    //value is less than 700 and whose MyUInt64 value is greater
    //than or equal to 3050.
    auto num_updated_rows = dtype_table->
        updateRowValues("MyDouble", 123.45, "MyString", "justOverwritten").
        forRecordsWhere("MyDouble", simdb::constraints::less, 700,
                        "MyUInt64", simdb::constraints::greater_equal, 3050);

    //Verify the update.
    EXPECT_EQUAL(num_updated_rows, 1);

    std::vector<std::unique_ptr<simdb::ObjectRef>> updated_records;
    obj_mgr.findObjects("DTypes", {}, updated_records);
    EXPECT_EQUAL(updated_records.size(), 3);

    simdb::ObjectRef * remaining_record1 = updated_records[0].get();
    EXPECT_EQUAL(remaining_record1->getPropertyInt32("MyInt32"), 10);
    EXPECT_EQUAL(remaining_record1->getPropertyUInt64("MyUInt64"), 3000);
    EXPECT_EQUAL(remaining_record1->getPropertyDouble("MyDouble"), 345.75);
    EXPECT_EQUAL(remaining_record1->getPropertyString("MyString"), "hello");

    simdb::ObjectRef * remaining_record2 = updated_records[1].get();
    EXPECT_EQUAL(remaining_record2->getPropertyInt32("MyInt32"), 12);
    EXPECT_EQUAL(remaining_record2->getPropertyUInt64("MyUInt64"), 3100);
    EXPECT_EQUAL(remaining_record2->getPropertyDouble("MyDouble"), 123.45);
    EXPECT_EQUAL(remaining_record2->getPropertyString("MyString"), "justOverwritten");

    simdb::ObjectRef * remaining_record3 = updated_records[2].get();
    EXPECT_EQUAL(remaining_record3->getPropertyInt32("MyInt32"), 14);
    EXPECT_EQUAL(remaining_record3->getPropertyUInt64("MyUInt64"), 3200);
    EXPECT_EQUAL(remaining_record3->getPropertyDouble("MyDouble"), 745.25);
    EXPECT_EQUAL(remaining_record3->getPropertyString("MyString"), "goodbye");

    //Overwrite the MyDouble columns to 777.777 for the records
    //whose MyString value is 'hello' or 'goodbye'.
    num_updated_rows = dtype_table->
        updateRowValues("MyDouble", 777.777).
        forRecordsWhere("MyString", simdb::constraints::in_set, {"hello", "goodbye"});

    //Verify the update.
    EXPECT_EQUAL(num_updated_rows, 2);

    obj_mgr.findObjects("DTypes", {}, updated_records);
    EXPECT_EQUAL(updated_records.size(), 3);

    remaining_record1 = updated_records[0].get();
    EXPECT_EQUAL(remaining_record1->getPropertyInt32("MyInt32"), 10);
    EXPECT_EQUAL(remaining_record1->getPropertyUInt64("MyUInt64"), 3000);
    EXPECT_EQUAL(remaining_record1->getPropertyDouble("MyDouble"), 777.777);
    EXPECT_EQUAL(remaining_record1->getPropertyString("MyString"), "hello");

    remaining_record2 = updated_records[1].get();
    EXPECT_EQUAL(remaining_record2->getPropertyInt32("MyInt32"), 12);
    EXPECT_EQUAL(remaining_record2->getPropertyUInt64("MyUInt64"), 3100);
    EXPECT_EQUAL(remaining_record2->getPropertyDouble("MyDouble"), 123.45);
    EXPECT_EQUAL(remaining_record2->getPropertyString("MyString"), "justOverwritten");

    remaining_record3 = updated_records[2].get();
    EXPECT_EQUAL(remaining_record3->getPropertyInt32("MyInt32"), 14);
    EXPECT_EQUAL(remaining_record3->getPropertyUInt64("MyUInt64"), 3200);
    EXPECT_EQUAL(remaining_record3->getPropertyDouble("MyDouble"), 777.777);
    EXPECT_EQUAL(remaining_record3->getPropertyString("MyString"), "goodbye");

    //Overwrite the MyString columns to "allThreeRecords" for the records
    //whose MyDouble value is 123.45 or 777.777
    num_updated_rows = dtype_table->
        updateRowValues("MyString", "allThreeRecords").
        forRecordsWhere("MyDouble", simdb::constraints::in_set, {123.45, 777.777});

    //Verify the update.
    EXPECT_EQUAL(num_updated_rows, 3);

    obj_mgr.findObjects("DTypes", {}, updated_records);
    EXPECT_EQUAL(updated_records.size(), 3);

    remaining_record1 = updated_records[0].get();
    EXPECT_EQUAL(remaining_record1->getPropertyInt32("MyInt32"), 10);
    EXPECT_EQUAL(remaining_record1->getPropertyUInt64("MyUInt64"), 3000);
    EXPECT_EQUAL(remaining_record1->getPropertyDouble("MyDouble"), 777.777);
    EXPECT_EQUAL(remaining_record1->getPropertyString("MyString"), "allThreeRecords");

    remaining_record2 = updated_records[1].get();
    EXPECT_EQUAL(remaining_record2->getPropertyInt32("MyInt32"), 12);
    EXPECT_EQUAL(remaining_record2->getPropertyUInt64("MyUInt64"), 3100);
    EXPECT_EQUAL(remaining_record2->getPropertyDouble("MyDouble"), 123.45);
    EXPECT_EQUAL(remaining_record2->getPropertyString("MyString"), "allThreeRecords");

    remaining_record3 = updated_records[2].get();
    EXPECT_EQUAL(remaining_record3->getPropertyInt32("MyInt32"), 14);
    EXPECT_EQUAL(remaining_record3->getPropertyUInt64("MyUInt64"), 3200);
    EXPECT_EQUAL(remaining_record3->getPropertyDouble("MyDouble"), 777.777);
    EXPECT_EQUAL(remaining_record3->getPropertyString("MyString"), "allThreeRecords");

    //Overwrite the MyInt32 columns to 10, the MyUInt64 columns to 1000,
    //the MyDouble columns to 99.123, and the MyString columns to "totalReset"
    //for every record in this table.
    num_updated_rows = dtype_table->
        updateRowValues("MyInt32", 10,
                        "MyUInt64", 1000,
                        "MyDouble", 99.123,
                        "MyString", "totalReset").
        forAllRecords();

    //Verify the update.
    EXPECT_EQUAL(num_updated_rows, 3);

    obj_mgr.findObjects("DTypes", {}, updated_records);
    EXPECT_EQUAL(updated_records.size(), 3);

    for (size_t idx = 0; idx < num_updated_rows; ++idx) {
        auto & row = updated_records[idx];
        EXPECT_EQUAL(row->getPropertyInt32("MyInt32"), 10);
        EXPECT_EQUAL(row->getPropertyUInt64("MyUInt64"), 1000);
        EXPECT_EQUAL(row->getPropertyDouble("MyDouble"), 99.123);
        EXPECT_EQUAL(row->getPropertyString("MyString"), "totalReset");
    }

    //Test updates of blob columns
    const std::vector<double> orig_blob = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &orig_blob[0];
    blob_descriptor.num_bytes = orig_blob.size() * sizeof(double);
    for (size_t idx = 0; idx < num_updated_rows; ++idx) {
        auto & row = updated_records[idx];
        row->setPropertyBlob("MyBlob", blob_descriptor);
    }

    updated_records[0]->setPropertyString("MyString", "hello");
    updated_records[1]->setPropertyString("MyString", "hello");
    updated_records[2]->setPropertyString("MyString", "world");

    const std::vector<double> new_blob = {500, 600, 700, 800};
    dtype_table->updateRowValues("MyBlob", new_blob).
                 forRecordsWhere("MyString", simdb::constraints::equal, "hello");

    //First record's blob should have been overwritten...
    std::vector<double> test_blob;
    updated_records[0]->getPropertyBlob("MyBlob", test_blob);
    EXPECT_EQUAL(test_blob, new_blob);

    //Second record's blob should also have been overwritten...
    test_blob.clear();
    updated_records[1]->getPropertyBlob("MyBlob", test_blob);
    EXPECT_EQUAL(test_blob, new_blob);

    //But the third record, whose MyString did *not* equal "hello",
    //should still have the same blob values as before.
    test_blob.clear();
    updated_records[2]->getPropertyBlob("MyBlob", test_blob);
    EXPECT_EQUAL(test_blob, orig_blob);
}

void testTableRefErrors()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;
    simdb::Schema schema;

    schema.addTable("DTypes")
        .addColumn("MyInt32", dt::int32_t)
        .addColumn("MyBlob",  dt::blob_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    auto dtype_table = obj_mgr.getTable("DTypes");
    dtype_table->createObjectWithArgs("MyInt32", 100);

    const std::vector<double> my_blob = {4, 5, 6, 7};
    const auto & updater = dtype_table->updateRowValues("MyInt32", 200, "MyBlob", my_blob);
    EXPECT_THROW(dtype_table->createObjectWithArgs("MyInt32", 500));
    (void) updater;
}

void testTableRefObjectReturn()
{
    PRINT_ENTER_TEST

    using dt = simdb::ColumnDataType;

    simdb::Schema schema;
    schema.addTable("Dummy")
        .addColumn("x", dt::double_t)
        .addColumn("y", dt::string_t);

    simdb::ObjectManager obj_mgr(DB_DIR);
    CREATE_SQL_SCHEMA(obj_mgr, schema);

    auto table = obj_mgr.getTable("Dummy");
    auto record1 = table->createObject();
    EXPECT_NOTEQUAL(record1.get(), nullptr);

    table->neverReturnObjectRefsOnCreate();
    auto record2 = table->createObject();
    EXPECT_EQUAL(record2.get(), nullptr);

    table->alwaysReturnObjectRefsOnCreate();
    auto record3 = table->createObject();
    EXPECT_NOTEQUAL(record3.get(), nullptr);

    simdb::ObjectQuery query(obj_mgr, "Dummy");
    EXPECT_EQUAL(query.countMatches(), 3);
}

int main()
{
    srand(time(0));

    testBadSql();
    testBadFile();
    testInvalidSchema();
    testSqlSchema();
    testSqlSchemaColumnModifiers();
    testBasicDataTypes();
    test64BitInts();
    testObjectQuery();
    testObjectQueryOptions();
    testObjectCreationArgs();
    testObjectDeletionArgs();
    testObjectUpdateArgs();
    testTableRefErrors();
    testTableRefObjectReturn();

    REPORT_ERROR;
    return ERROR_CODE;
}
