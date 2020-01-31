/*!
 * \file CoreDatabase_test.cpp
 *
 * \brief Database tests for SimDB functionality that is not
 * specific to any particular database format (SQLite, HDF5,
 * etc.)
 */

#include "simdb/test/SimDBTester.hpp"

//Core database headers
#include "simdb/schema/DatabaseRoot.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/utils/uuids.hpp"
#include "simdb/utils/MathUtils.hpp"
#include "simdb/utils/StringUtils.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/Errors.hpp"

//SQLite-specific includes
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/impl/hdf5/HDF5ConnProxy.hpp"

//Standard headers
#include <random>

#define DB_DIR "test_dbs"

#define PRINT_ENTER_TEST                                                              \
  std::cout << std::endl;                                                             \
  std::cout << "*************************************************************"        \
            << "*** Beginning '" << __FUNCTION__ << "'"                               \
            << "*************************************************************"        \
            << std::endl;

//! Schema builder for the Strings namespace.
void StringsSchemaBuilder(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("Strings")
        .addColumn("First", dt::string_t)
        .addColumn("Second", dt::string_t);

    schema.addTable("Metadata")
        .addColumn("Name", dt::string_t)
        .addColumn("Value", dt::string_t);
}

void testNamespaceSchemas()
{
    PRINT_ENTER_TEST

    simdb::DatabaseRoot db_root(DB_DIR);
    auto strings_namespace = db_root.getNamespace("Strings");
    EXPECT_NOTEQUAL(strings_namespace, nullptr);

    //Since we registered a schema builder for the Strings
    //namespace, we should expect certain tables to be in
    //the schema (autopopulated).
    EXPECT_TRUE(strings_namespace->hasSchema());
    EXPECT_TRUE(strings_namespace->hasTableNamed("Metadata"));

    //Verify that table MoreMetadata is not in the schema,
    //and then verify that we are able to add this table
    //to the Strings namespace schema ourselves.
    EXPECT_FALSE(strings_namespace->hasTableNamed("MoreMetadata"));

    strings_namespace->addToSchema([](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("MoreMetadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Alias", dt::string_t);
    });

    EXPECT_TRUE(strings_namespace->hasTableNamed("MoreMetadata"));

    const simdb::Table * more_metadata_table =
        strings_namespace->getTableNamed("MoreMetadata");

    //Verify an exception is thrown if we attempt to add
    //a table that already exists in this namespaces's
    //schema. But it should only throw if the table we
    //attempt to add has a different column configuration
    //than the existing schema table of the same name.
    EXPECT_THROW(strings_namespace->addToSchema([](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("MoreMetadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Alais", dt::string_t); //Typo intentional
    }));

    //Verify that an exception is NOT thrown if we attempt
    //to add a table that already exists by the same name,
    //but the column configuration of the table we try to
    //add is identical to the table that is already there.
    EXPECT_NOTHROW(strings_namespace->addToSchema([](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("MoreMetadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Alias", dt::string_t); //No typo this time
    }));

    //Double check that the namespace returns the same
    //table pointer for "MoreMetadata", since we did not
    //actually create another table by that name. It simply
    //gets ignored.
    EXPECT_EQUAL(strings_namespace->getTableNamed("MoreMetadata"),
                 more_metadata_table);

    //Verify that we can ask the namespace for one of its
    //tables when we pass in the fully qualified table name.
    //It is advised not to do this, but if the qualified table
    //name matches exactly, DatabaseNamespace allows it.
    const std::string valid_qualified_table_name =
        std::string("Strings") + simdb::Table::NS_DELIM + "MoreMetadata";

    EXPECT_EQUAL(strings_namespace->getTableNamed(valid_qualified_table_name),
                 more_metadata_table);

    //Verify that we can ask the namespace for one of its
    //tables using a fully qualified table name, where the
    //namespace is correct but the unqualified table name
    //does not exist. It should not throw; it should return
    //nullptr.
    const std::string nonexistent_qualified_table_name =
        std::string("Strings") + simdb::Table::NS_DELIM + "DoesNotExist";

    EXPECT_EQUAL(
        strings_namespace->getTableNamed(nonexistent_qualified_table_name),
        nullptr);

    //Edge case: Use the correct namespace, but leave the
    //unqualified table name blank. Should return null.
    const std::string valid_ns_empty_table_name =
        std::string("Strings") + simdb::Table::NS_DELIM;

    EXPECT_EQUAL(
        strings_namespace->getTableNamed(valid_ns_empty_table_name),
        nullptr);

    //Edge case: Pass in only the namespace delimiter.
    //Should return null.
    const std::string empty_ns_empty_table_name =
        std::string(1, simdb::Table::NS_DELIM);

    EXPECT_EQUAL(
        strings_namespace->getTableNamed(empty_ns_empty_table_name),
        nullptr);

    //Edge case: Pass in an invalid namespace, and an
    //empty unqualified table name. Should throw.
    const std::string invalid_ns_empty_table_name =
        std::string("Striings") + simdb::Table::NS_DELIM;

    EXPECT_THROW(
        strings_namespace->getTableNamed(invalid_ns_empty_table_name));

    //This test only worked on DatabaseNamespace schemas;
    //no ObjectManager's / DbConnProxy's should have been
    //created.
    EXPECT_FALSE(strings_namespace->databaseConnectionEstablished());
}

void testNamespaceRecords()
{
    PRINT_ENTER_TEST

    simdb::DatabaseRoot db_root(DB_DIR);
    auto numbers_namespace = db_root.getNamespace("Numbers");

    EXPECT_NOTEQUAL(numbers_namespace, nullptr);
    EXPECT_TRUE(numbers_namespace->hasSchema());

    //Test data structure for the Numbers namespace.
    struct Numbers {
        struct Data {
            int32_t first;
            double second;
        };
        Data data;

        struct Metadata {
            std::string name;
            int64_t value;
        };
        Metadata metadata;

        struct MoreMetadata {
            std::string name;
            double value;
        };
        MoreMetadata more_metadata;

        static Numbers createRandom() {
            Numbers n;
            n.data.first = simdb::utils::chooseRand<int32_t>();
            n.data.second = simdb::utils::chooseRand<double>();
            n.metadata.name = simdb::utils::chooseRand<std::string>();
            n.metadata.value = simdb::utils::chooseRand<int64_t>();
            n.more_metadata.name = simdb::utils::chooseRand<std::string>();
            n.more_metadata.value = rand() * 3.14;
            return n;
        }
    };

    //Before we try to create any records, verify that no
    //database connection has been made yet.
    EXPECT_FALSE(numbers_namespace->databaseConnectionEstablished());

    //Now ask for the ObjectDatabase from this namespace.
    //This should trigger the physical database connection
    //to be made.
    auto numbers_db = numbers_namespace->getDatabase();
    EXPECT_TRUE(numbers_namespace->databaseConnectionEstablished());

    //Create a record using the default Numbers schema.
    auto record_values = Numbers::createRandom();
    auto numbers_tbl = numbers_db->getTable("Numbers");

    auto numbers_row1 = numbers_tbl->createObjectWithArgs(
        "First", record_values.data.first,
        "Second", record_values.data.second);

    //Use the ObjectDatabase::findObject() method to ask
    //the database for the ObjectRef wrapping the record
    //we just created.
    auto recovered_numbers_row1 = numbers_db->findObject(
        "Numbers", numbers_row1->getId());

    //Validate the record values.
    EXPECT_EQUAL(recovered_numbers_row1->getPropertyInt32("First"),
                 record_values.data.first);

    EXPECT_EQUAL(recovered_numbers_row1->getPropertyDouble("Second"),
                 record_values.data.second);

    //Now add a new table that was not in the hard-coded /
    //registered schema builder for this namespace.
    numbers_namespace->addToSchema([](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("MoreMetadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Value", dt::double_t);
    });

    //Create another record, this time for the MoreMetadata
    //table we just added to the namespace schema.
    record_values = Numbers::createRandom();
    auto more_metadata_tbl = numbers_db->getTable("MoreMetadata");

    auto more_metadata_row1 = more_metadata_tbl->createObjectWithArgs(
        "Name", record_values.more_metadata.name,
        "Value", record_values.more_metadata.value);

    //Again, use the ObjectDatabase::findObject() method to
    //ask for the ObjectRef which wraps this MoreMetadata
    //record.
    auto recovered_more_metadata_row1 = numbers_db->findObject(
        "MoreMetadata", more_metadata_row1->getId());

    //Validate the record values.
    EXPECT_EQUAL(recovered_more_metadata_row1->getPropertyString("Name"),
                 record_values.more_metadata.name);

    EXPECT_EQUAL(recovered_more_metadata_row1->getPropertyDouble("Value"),
                 record_values.more_metadata.value);

    //Verify ObjectDatabase::findObjects() - create another
    //MoreMetadata record first so we have multiple results
    //from findObjects() we can verify.
    std::vector<Numbers> find_objs_expected_ans = {record_values};
    record_values = Numbers::createRandom();
    find_objs_expected_ans.emplace_back(record_values);

    auto more_metadata_row2 = more_metadata_tbl->createObjectWithArgs(
        "Name", record_values.more_metadata.name,
        "Value", record_values.more_metadata.value);

    std::vector<simdb::DatabaseID> record_ids = {
        more_metadata_row1->getId(),
        more_metadata_row2->getId()
    };

    std::vector<std::unique_ptr<simdb::ObjectRef>> recovered_more_metadata_rows;

    numbers_db->findObjects(
        "MoreMetadata", record_ids, recovered_more_metadata_rows);

    EXPECT_EQUAL(recovered_more_metadata_rows.size(),
                 record_ids.size());

    //Verify the first MoreMetadata record.
    EXPECT_NOTEQUAL(recovered_more_metadata_rows[0].get(), nullptr);

    EXPECT_EQUAL(recovered_more_metadata_rows[0]->getPropertyString("Name"),
                 find_objs_expected_ans[0].more_metadata.name);

    EXPECT_EQUAL(recovered_more_metadata_rows[0]->getPropertyDouble("Value"),
                 find_objs_expected_ans[0].more_metadata.value);

    //Verify the second MoreMetadata record.
    EXPECT_NOTEQUAL(recovered_more_metadata_rows[1].get(), nullptr);

    EXPECT_EQUAL(recovered_more_metadata_rows[1]->getPropertyString("Name"),
                 find_objs_expected_ans[1].more_metadata.name);

    EXPECT_EQUAL(recovered_more_metadata_rows[1]->getPropertyDouble("Value"),
                 find_objs_expected_ans[1].more_metadata.value);

    //Verify that we can use ObjectQuery to find records instead
    //of just using findObject(s)() with database ID(s).
    std::unique_ptr<simdb::ObjectQuery> query = numbers_db->
        createObjectQueryForTable("MoreMetadata");

    EXPECT_NOTEQUAL(query.get(), nullptr);

    //Set up the query to look for the second MoreMetadata
    //record we just created above.
    query->addConstraints(
        "Name", simdb::constraints::equal,
        record_values.more_metadata.name);

    std::string name_from_obj_query;
    double value_from_obj_query;

    query->writeResultIterationsTo(
        "Name", &name_from_obj_query,
        "Value", &value_from_obj_query);

    auto result_iter = query->executeQuery();

    //We should have found one record...
    EXPECT_TRUE(result_iter->getNext());

    //...and only one record.
    EXPECT_FALSE(result_iter->getNext());

    //Validate the record properties.
    EXPECT_EQUAL(name_from_obj_query, record_values.more_metadata.name);
    EXPECT_EQUAL(value_from_obj_query, record_values.more_metadata.value);
}

void testNamespaceWithoutSchemaBuilder()
{
    PRINT_ENTER_TEST

    simdb::DatabaseRoot db_root(DB_DIR);
    auto no_schema_namespace = db_root.getNamespace("NoSchemaBuilder");
    EXPECT_NOTEQUAL(no_schema_namespace, nullptr);

    EXPECT_FALSE(no_schema_namespace->databaseConnectionEstablished());

    no_schema_namespace->addToSchema([](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("PalindromePhrases")
            .addColumn("Fiz", dt::string_t)
            .addColumn("Fuz", dt::double_t);
    });

    EXPECT_FALSE(no_schema_namespace->databaseConnectionEstablished());
    auto db = no_schema_namespace->getDatabase();
    EXPECT_TRUE(no_schema_namespace->databaseConnectionEstablished());

    struct Data {
        std::string fiz;
        double fuz;

        static Data createRandom() {
            Data d;
            d.fiz = simdb::utils::chooseRand<std::string>();
            d.fuz = simdb::utils::chooseRand<double>();
            return d;
        }
    };

    Data expected1 = Data::createRandom();
    Data expected2 = Data::createRandom();
    Data expected3 = Data::createRandom();
    Data expected4 = Data::createRandom();

    //Overwrite the randomly generated 'fiz' values
    //so we can get multiple records using ObjectQuery
    //in a deterministic way.
    expected1.fiz = "a toyota";
    expected2.fiz = "race fast";
    expected3.fiz = "safe car";
    expected4.fiz = "a toyota";

    auto table = db->getTable("PalindromePhrases");

    table->createObjectWithArgs(
        "Fiz", expected1.fiz,
        "Fuz", expected1.fuz);

    table->createObjectWithArgs(
        "Fiz", expected2.fiz,
        "Fuz", expected2.fuz);

    table->createObjectWithArgs(
        "Fiz", expected3.fiz,
        "Fuz", expected3.fuz);

    table->createObjectWithArgs(
        "Fiz", expected4.fiz,
        "Fuz", expected4.fuz);

    std::unique_ptr<simdb::ObjectQuery> query =
        db->createObjectQueryForTable("PalindromePhrases");

    query->addConstraints(
        "Fiz", simdb::constraints::equal, "a toyota");

    std::string actual_fiz;
    double actual_fuz;

    query->writeResultIterationsTo(
        "Fiz", &actual_fiz,
        "Fuz", &actual_fuz);

    auto result_iter = query->executeQuery();

    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(actual_fiz, expected1.fiz);
    EXPECT_EQUAL(actual_fuz, expected1.fuz);

    EXPECT_TRUE(result_iter->getNext());
    EXPECT_EQUAL(actual_fiz, expected4.fiz);
    EXPECT_EQUAL(actual_fuz, expected4.fuz);

    //There should have been exactly two matches found.
    EXPECT_FALSE(result_iter->getNext());
}

//Schema builder used for the "macros edge cases" unit test below.
void BuildFooSchema1(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("MyTable")
        .addColumn("x", dt::string_t)
        .addColumn("y", dt::double_t);
}

//Schema builder used for the "macros edge cases" unit test below.
void BuildFooSchema2(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("MyTable")
        .addColumn("x", dt::string_t)
        .addColumn("y", dt::int32_t);
}

//Schema builder used for the "macros edge cases" unit test below.
void BuildIdenticalFooSchema1(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("MyTable")
        .addColumn("x", dt::string_t)
        .addColumn("y", dt::double_t);
}

//Connection proxy factory used for "macros edge cases" unit test below.
simdb::DbConnProxy * CreateFooProxy1()
{
    return new simdb::SQLiteConnProxy;
}

//Connection proxy factory used for "macros edge cases" unit test below.
simdb::DbConnProxy * CreateFooProxy2()
{
    return new simdb::SQLiteConnProxy;
}

//Connection proxy factory used for "macros edge cases" unit test below.
simdb::DbConnProxy * CreateFooProxy3()
{
    return new simdb::HDF5ConnProxy;
}

//Test structure used in testRegistrationMacrosEdgeCases() below.
struct TestOverlap {
    double a, b;
    float c, d;
    int16_t e, f;
    std::string g, h;

    static TestOverlap makeRandom() {
        TestOverlap test;
        test.a = simdb::utils::chooseRand<double>();
        test.b = simdb::utils::chooseRand<double>();
        test.c = simdb::utils::chooseRand<float>();
        test.d = simdb::utils::chooseRand<float>();
        test.e = simdb::utils::chooseRand<int16_t>();
        test.f = simdb::utils::chooseRand<int16_t>();
        test.g = simdb::utils::chooseRand<std::string>();
        test.h = simdb::utils::chooseRand<std::string>();
        return test;
    }

    void clear() {
        a = b = c = d = e = f = 0;
        g = h = "";
    }
};

//Equivalence check for EXPECT_EQUAL(TestOverlap, TestOverlap)
bool operator==(const TestOverlap & lhs, const TestOverlap & rhs)
{
    return lhs.a == rhs.a &&
           lhs.b == rhs.b &&
           lhs.c == rhs.c &&
           lhs.d == rhs.d &&
           lhs.e == rhs.e &&
           lhs.f == rhs.f &&
           lhs.g == rhs.g &&
           lhs.h == rhs.h;
}

//Equivalence check for EXPECT_NOTEQUAL(TestOverlap, TestOverlap)
bool operator!=(const TestOverlap & lhs, const TestOverlap & rhs)
{
    return !(lhs == rhs);
}

//Stream operator EXPECT_EQUAL(TestOverlap, TestOverlap) needs.
std::ostream & operator<<(std::ostream & os, const TestOverlap & val)
{
    os << "  a (double):  " << val.a << "\n"
       << "  b (double):  " << val.b << "\n"
       << "  c (float):   " << val.c << "\n"
       << "  d (float):   " << val.d << "\n"
       << "  e (int16_t): " << val.e << "\n"
       << "  f (int16_t): " << val.f << "\n"
       << "  g (string):  " << val.g << "\n"
       << "  h (string):  " << val.h << "\n"
       << std::endl;

    return os;
}

void testRegistrationMacrosEdgeCases()
{
    PRINT_ENTER_TEST

    //Typical registration - same as other tests above.
    EXPECT_NOTHROW(REGISTER_SIMDB_NAMESPACE(Strings, SQLite));

    //Test case-insensitivity. Aside from that, double-
    //registering the Strings namespace for the SQLite
    //database type is ignored...
    EXPECT_NOTHROW(REGISTER_SIMDB_NAMESPACE(StRiNgS, sqlITE));

    //...BUT if we try to double-register the same namespace
    //for HDF5, SimDB currently does not allow it.
    EXPECT_THROW(REGISTER_SIMDB_NAMESPACE(Strings, HDF5));

    //Typical registration - attach a default schema builder
    //to the registered namespace.
    EXPECT_NOTHROW(REGISTER_SIMDB_SCHEMA_BUILDER(Foo, BuildFooSchema1));

    //Even though namespace Foo already has a schema builder,
    //and this second builder we're trying to register here has
    //a different table/column configuration than the schema
    //already registered for namespace Foo, it still should not
    //throw right away. If we try to access the namespace Foo
    //however, *then* we expect it to throw.
    EXPECT_NOTHROW(REGISTER_SIMDB_SCHEMA_BUILDER(Foo, BuildFooSchema2));

    //Ignored registration - same namespace Foo, but this
    //schema builder produces a table/column configuration
    //which is identical to the schema already defined for
    //namespace Foo.
    EXPECT_NOTHROW(REGISTER_SIMDB_SCHEMA_BUILDER(Foo, [](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("MyTable")
            .addColumn("x", dt::string_t)
            .addColumn("y", dt::double_t);
    }));

    //Proxy factory registration should never throw, but
    //note that the first two factories are going to be
    //ignored; only the third will take effect.
    EXPECT_NOTHROW(REGISTER_SIMDB_PROXY_CREATE_FUNCTION(Foo, CreateFooProxy1));
    EXPECT_NOTHROW(REGISTER_SIMDB_PROXY_CREATE_FUNCTION(Foo, CreateFooProxy2));
    EXPECT_NOTHROW(REGISTER_SIMDB_PROXY_CREATE_FUNCTION(Foo, []() {
        return new simdb::SQLiteConnProxy;
    }));

    simdb::DatabaseRoot db_root(DB_DIR);
    EXPECT_NOTHROW(REGISTER_SIMDB_NAMESPACE(Foo, SQLite));
    EXPECT_THROW((void)db_root.getNamespace("Foo"));

    //Let's register a few schema builders with SimDB.
    //Each of these builders will add its own tables
    //to the same namespace, and there will be some
    //overlap in the table/column definitions. But
    //none of the table configurations conflicts with
    //other callbacks' table configurations, so SimDB
    //should be able to combine them.
    REGISTER_SIMDB_NAMESPACE(SchemaOverlap, SQLite);
    REGISTER_SIMDB_SCHEMA_BUILDER(SchemaOverlap, [](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;
        schema.addTable("Overlap1")
            .addColumn("a", dt::double_t)
            .addColumn("b", dt::double_t);
        schema.addTable("Overlap2")
            .addColumn("c", dt::float_t)
            .addColumn("d", dt::float_t);
    });
    REGISTER_SIMDB_SCHEMA_BUILDER(SchemaOverlap, [](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;
        schema.addTable("Overlap2")
            .addColumn("c", dt::float_t)
            .addColumn("d", dt::float_t);
        schema.addTable("Overlap3")
            .addColumn("e", dt::int16_t)
            .addColumn("f", dt::int16_t);
    });
    REGISTER_SIMDB_SCHEMA_BUILDER(SchemaOverlap, [](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;
        schema.addTable("Overlap3")
            .addColumn("e", dt::int16_t)
            .addColumn("f", dt::int16_t);
        schema.addTable("Overlap4")
            .addColumn("g", dt::string_t)
            .addColumn("h", dt::string_t);
    });

    auto overlap_namespace = db_root.getNamespace("SchemaOverlap");
    EXPECT_NOTEQUAL(overlap_namespace, nullptr);

    auto overlap_table1 = overlap_namespace->
        getDatabase()->getTable("Overlap1");

    auto overlap_table2 = overlap_namespace->
        getDatabase()->getTable("Overlap2");

    auto overlap_table3 = overlap_namespace->
        getDatabase()->getTable("Overlap3");

    auto overlap_table4 = overlap_namespace->
        getDatabase()->getTable("Overlap4");

    std::vector<TestOverlap> overlap_values;
    overlap_values.reserve(10);
    for (size_t idx = 0; idx < overlap_values.capacity(); ++idx) {
        overlap_values.emplace_back(TestOverlap::makeRandom());
        const TestOverlap & input_data = overlap_values.back();

        overlap_table1->createObjectWithArgs(
            "a", input_data.a,
            "b", input_data.b);

        overlap_table2->createObjectWithArgs(
            "c", input_data.c,
            "d", input_data.d);

        overlap_table3->createObjectWithArgs(
            "e", input_data.e,
            "f", input_data.f);

        overlap_table4->createObjectWithArgs(
            "g", input_data.g,
            "h", input_data.h);
    }

    auto overlap_db = overlap_namespace->getDatabase();

    auto overlap_query1 = overlap_db->createObjectQueryForTable("Overlap1");
    EXPECT_EQUAL(overlap_query1->countMatches(), overlap_values.size());

    auto overlap_query2 = overlap_db->createObjectQueryForTable("Overlap2");
    EXPECT_EQUAL(overlap_query2->countMatches(), overlap_values.size());

    auto overlap_query3 = overlap_db->createObjectQueryForTable("Overlap3");
    EXPECT_EQUAL(overlap_query3->countMatches(), overlap_values.size());

    auto overlap_query4 = overlap_db->createObjectQueryForTable("Overlap4");
    EXPECT_EQUAL(overlap_query4->countMatches(), overlap_values.size());

    TestOverlap actual;
    auto all_clear = [&]() {
        actual.clear();
    };

    overlap_query1->writeResultIterationsTo("a", &actual.a, "b", &actual.b);
    overlap_query2->writeResultIterationsTo("c", &actual.c, "d", &actual.d);
    overlap_query3->writeResultIterationsTo("e", &actual.e, "f", &actual.f);
    overlap_query4->writeResultIterationsTo("g", &actual.g, "h", &actual.h);
    all_clear();

    size_t overlap_results_idx = 0;
    auto overlap_results_iter1 = overlap_query1->executeQuery();
    auto overlap_results_iter2 = overlap_query2->executeQuery();
    auto overlap_results_iter3 = overlap_query3->executeQuery();
    auto overlap_results_iter4 = overlap_query4->executeQuery();
    auto all_get_next = [&]() {
        return overlap_results_iter1->getNext() &&
               overlap_results_iter2->getNext() &&
               overlap_results_iter3->getNext() &&
               overlap_results_iter4->getNext();
    };

    while (all_get_next()) {
        const auto & expected = overlap_values.at(overlap_results_idx++);
        EXPECT_EQUAL(actual, expected);
        all_clear();
    }
}

int main()
{
    //! At minimum, we must register our database namespaces
    //! with an associated database type (SQLite, HDF5, etc.)
    REGISTER_SIMDB_NAMESPACE(Strings, SQLite);
    REGISTER_SIMDB_NAMESPACE(Numbers, SQLite);

    //! Schema definitions for each SimDB namespace can either
    //! be registered with this macro, or inlined with a lambda
    //! in user code. It also works with a combination of the
    //! two: hard code all tables you always need for your
    //! database namespace, put it in a schema builder callback,
    //! and register it with this macro. You can request the
    //! DatabaseNamespace object from the DatabaseRoot later
    //! on and add extra tables if you need to. The schemas
    //! will be combined under the hood.
    REGISTER_SIMDB_SCHEMA_BUILDER(Strings, StringsSchemaBuilder);
    REGISTER_SIMDB_SCHEMA_BUILDER(Numbers, [](simdb::Schema & schema) {
        using dt = simdb::ColumnDataType;

        schema.addTable("Numbers")
            .addColumn("First", dt::int32_t)
            .addColumn("Second", dt::double_t);

        schema.addTable("Metadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Value", dt::int64_t);
    });

    //! In order to access the ObjectManager for the database
    //! namespace object we'll create, we need to register a
    //! factory method to create the appropriate DbConnProxy
    //! subclass.
    REGISTER_SIMDB_PROXY_CREATE_FUNCTION(SQLite, []() {
        return new simdb::SQLiteConnProxy;
    });

    //! Let's also register a SQLite namespace without any
    //! schema build function to go with it. We will define
    //! the schema ourselves with a call to the addToSchema()
    //! method that SimDB provides.
    REGISTER_SIMDB_NAMESPACE(NoSchemaBuilder, SQLite);

    testNamespaceSchemas();
    testNamespaceRecords();
    testNamespaceWithoutSchemaBuilder();
    testRegistrationMacrosEdgeCases();
}
