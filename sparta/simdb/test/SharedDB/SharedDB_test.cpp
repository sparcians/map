/*!
 * \file SharedDB_test.cpp
 * \brief This file contains tests which verify database contents
 * when more than one database connection / async task queue was
 * used to write the data to disk asynchronously.
 */

#include "simdb/test/SimDBTester.hpp"
#include "simdb/schema/DatabaseRoot.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/utils/MathUtils.hpp"

#include <random>
#include <climits>

#define DB_DIR "test_dbs"

using ObjectDatabase = simdb::ObjectManager::ObjectDatabase;

TEST_INIT

#define PRINT_ENTER_TEST                                                       \
  std::cout << std::endl;                                                      \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'"                        \
            << "*************************************************************" \
            << std::endl;

//! Registered schema builder for the Random namespace.
void buildRandNumbersSchema(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("Numbers")
        .addColumn("RandInt", dt::int32_t)
        .addColumn("RandFloat", dt::float_t)
        .addColumn("RandDouble", dt::double_t);
}

//! Registered schema builder for the Incrementing namespace.
void buildIncNumbersSchema(simdb::Schema & schema)
{
    using dt = simdb::ColumnDataType;

    schema.addTable("Numbers")
        .addColumn("IncrementingInt", dt::int32_t)
        .addColumn("IncrementingFloat", dt::float_t)
        .addColumn("IncrementingDouble", dt::double_t);
}

//! Factory method DatabaseRoot will invoke when it needs
//! to create ObjectManager's bound to SQLite database files.
simdb::DbConnProxy * createSQLiteProxy()
{
    return new simdb::SQLiteConnProxy;
}

//! Test data structure used for writing, reading, and
//! verifying record values in a database.
struct TestData {
    int32_t ival = 0;
    float fval = 0;
    double dval = 0;
};

struct RandomDataFactory {
    TestData makeRandom() {
        TestData d;
        d.ival = simdb::utils::chooseRand<int32_t>();
        d.fval = simdb::utils::chooseRand<float>();
        d.dval = simdb::utils::chooseRand<double>();
        return d;
    }
};

//! Test data structure used for writing, reading, and
//! verifying record values in a database.
struct IncrementingDataFactory {
    TestData makeRandom() {
        TestData d;

        d.ival = curr_ival + (rand() % 100) + 1;
        d.ival = curr_fval + ((rand() % 100) * 3.14) + 1;
        d.dval = curr_dval + ((rand() % 100) * 75.123) + 1;

        curr_ival = d.ival;
        curr_fval = d.fval;
        curr_dval = d.dval;

        if (curr_ival < 0 || curr_fval < 0 || curr_dval < 0) {
            throw simdb::DBException("Overflow detected");
        }
        return d;
    }

private:
    int32_t curr_ival = 0;
    float curr_fval = 0;
    double curr_dval = 0;
};

//Helper class which holds onto randomly generated
//data structures for database writes, keeping those
//structures in memory so we can verify the database
//independently.
template <typename DataT, typename FactoryT>
class Answers
{
public:
    explicit Answers(const size_t max_num_structs) :
        max_num_structs_(max_num_structs)
    {
        assert(max_num_structs_ > 0);
        data_.reserve(max_num_structs_);
    }

    const DataT & createRandomData() {
        if (data_.size() < data_.capacity()) {
            data_inds_.emplace_back(data_.size());
            data_.emplace_back(data_factory_.makeRandom());
            return data_.back();
        }
        data_inds_.emplace_back(rand() % data_.capacity());
        return data_[data_inds_.back()];
    }

    size_t getNumDataStructs() const {
        return data_inds_.size();
    }

    const DataT & getDataAtIndex(const size_t idx) const {
        return data_.at(data_inds_.at(idx));
    }

private:
    const size_t max_num_structs_;
    std::vector<DataT> data_;
    FactoryT data_factory_;
    std::vector<size_t> data_inds_;
};

using RandAnswers = Answers<TestData, RandomDataFactory>;
using IncAnswers = Answers<TestData, IncrementingDataFactory>;

void testObjMgrsSharingSameFile(const size_t num_tasks = 100)
{
    PRINT_ENTER_TEST

    simdb::DatabaseRoot db_root(DB_DIR);

    simdb::DatabaseNamespace * rand_namespace =
        db_root.getNamespace("Random");
    EXPECT_TRUE(rand_namespace->hasSchema());

    simdb::DatabaseNamespace * inc_namespace =
        db_root.getNamespace("Incrementing");
    EXPECT_TRUE(inc_namespace->hasSchema());

    ObjectDatabase * rand_db =
        rand_namespace->getDatabase();

    ObjectDatabase * inc_db =
        inc_namespace->getDatabase();

    //Helper class which takes ObjectDatabase ownership,
    //and queues database write requests onto separate
    //ObjectManager task queues. These requests should
    //end up in the same database file despite using
    //multiple ObjectManager's and multiple task queues.
    class TaskScheduler {
    public:
        TaskScheduler(ObjectDatabase * rand_db,
                      ObjectDatabase * inc_db,
                      RandAnswers & rand_answers,
                      IncAnswers & inc_answers) :
            rand_db_(rand_db),
            inc_db_(inc_db),
            rand_answers_(rand_answers),
            inc_answers_(inc_answers)
        {}

        void scheduleOne() {
            const TestData & rand_db_data = rand_answers_.createRandomData();
            const TestData & inc_db_data = inc_answers_.createRandomData();

            std::unique_ptr<simdb::WorkerTask> rand_db_task(
                new RandDbTaskWriter(rand_db_, rand_db_data));

            std::unique_ptr<simdb::WorkerTask> inc_db_task(
                new IncDbTaskWriter(inc_db_, inc_db_data));

            rand_db_->getObjectManager()->getTaskQueue()->
                addWorkerTask(std::move(rand_db_task));

            inc_db_->getObjectManager()->getTaskQueue()->
                addWorkerTask(std::move(inc_db_task));
        }

    private:
        //Helper class which adds records to the Random namespace
        //on the worker thread.
        class RandDbTaskWriter : public simdb::WorkerTask {
        public:
            RandDbTaskWriter(ObjectDatabase * rand_db,
                             const TestData & rand_db_data) :
                rand_db_(rand_db),
                rand_db_data_(rand_db_data)
            {}

        private:
            void completeTask() override {
                rand_db_->getTable("Numbers")->createObjectWithArgs(
                    "RandInt", rand_db_data_.ival,
                    "RandFloat", rand_db_data_.fval,
                    "RandDouble", rand_db_data_.dval);
            }

            ObjectDatabase * rand_db_ = nullptr;
            TestData rand_db_data_;
        };

        //Helper class which adds records to the Incrementing
        //namespace on the worker thread.
        class IncDbTaskWriter : public simdb::WorkerTask {
        public:
            IncDbTaskWriter(ObjectDatabase * inc_db,
                            const TestData & inc_db_data) :
                inc_db_(inc_db),
                inc_db_data_(inc_db_data)
            {}

        private:
            void completeTask() override {
                inc_db_->getTable("Numbers")->createObjectWithArgs(
                    "IncrementingInt", inc_db_data_.ival,
                    "IncrementingFloat", inc_db_data_.fval,
                    "IncrementingDouble", inc_db_data_.dval);
            }

            ObjectDatabase * inc_db_ = nullptr;
            TestData inc_db_data_;
        };

        ObjectDatabase * rand_db_ = nullptr;
        ObjectDatabase * inc_db_ = nullptr;
        RandAnswers & rand_answers_;
        IncAnswers & inc_answers_;
    };

    RandAnswers rand_answers(100);
    IncAnswers inc_answers(100);

    //Before handing the ObjectDatabase's over to the
    //TaskScheduler, ask for their database file names,
    //and ask them to create ObjectQuery's we can use
    //later to verify the database records' values.
    const std::string & rand_db_fname = rand_db->getDatabaseFile();

    std::unique_ptr<simdb::ObjectQuery> rand_db_query =
        rand_db->createObjectQueryForTable("Numbers");

    const std::string & inc_db_fname = inc_db->getDatabaseFile();

    std::unique_ptr<simdb::ObjectQuery> inc_db_query =
        inc_db->createObjectQueryForTable("Numbers");

    auto task_controller = db_root.getTaskController();
    rand_db->getTaskQueue()->addToTaskController(task_controller);
    inc_db->getTaskQueue()->addToTaskController(task_controller);

    TaskScheduler scheduler(
        std::move(rand_db), std::move(inc_db),
        rand_answers, inc_answers);

    for (size_t idx = 0; idx < num_tasks; ++idx) {
        scheduler.scheduleOne();
    }

    task_controller->flushQueue();
    task_controller->stopThread();

    //Start from scratch with new database connections to
    //these database files.
    simdb::ObjectManager rand_obj_mgr(DB_DIR);
    simdb::ObjectManager inc_obj_mgr(DB_DIR);

    EXPECT_TRUE(rand_obj_mgr.connectToExistingDatabase(rand_db_fname));
    EXPECT_TRUE(inc_obj_mgr.connectToExistingDatabase(inc_db_fname));

    rand_obj_mgr.safeTransaction([&]() {
        int32_t rand_ival;
        float rand_fval;
        double rand_dval;

        rand_db_query->writeResultIterationsTo(
            "RandInt", &rand_ival,
            "RandFloat", &rand_fval,
            "RandDouble", &rand_dval);

        EXPECT_EQUAL(rand_db_query->countMatches(),
                     rand_answers.getNumDataStructs());

        auto rand_query_iter = rand_db_query->executeQuery();
        for (size_t idx = 0; idx < rand_answers.getNumDataStructs(); ++idx) {
            EXPECT_TRUE(rand_query_iter->getNext());
            const TestData & actual = rand_answers.getDataAtIndex(idx);
            EXPECT_EQUAL(actual.ival, rand_ival);
            EXPECT_WITHIN_EPSILON(actual.fval, rand_fval);
            EXPECT_WITHIN_EPSILON(actual.dval, rand_dval);
        }
    });

    inc_obj_mgr.safeTransaction([&]() {
        int32_t rand_ival;
        float rand_fval;
        double rand_dval;

        inc_db_query->writeResultIterationsTo(
            "IncrementingInt", &rand_ival,
            "IncrementingFloat", &rand_fval,
            "IncrementingDouble", &rand_dval);

        EXPECT_EQUAL(inc_db_query->countMatches(),
                     inc_answers.getNumDataStructs());

        auto inc_query_iter = inc_db_query->executeQuery();
        for (size_t idx = 0; idx < inc_answers.getNumDataStructs(); ++idx) {
            EXPECT_TRUE(inc_query_iter->getNext());
            const TestData & actual = inc_answers.getDataAtIndex(idx);
            EXPECT_EQUAL(actual.ival, rand_ival);
            EXPECT_WITHIN_EPSILON(actual.fval, rand_fval);
            EXPECT_WITHIN_EPSILON(actual.dval, rand_dval);
        }
    });
}

int main(int argc, char ** argv)
{
    REGISTER_SIMDB_NAMESPACE(Random, SQLite);
    REGISTER_SIMDB_NAMESPACE(Incrementing, SQLite);
    REGISTER_SIMDB_PROXY_CREATE_FUNCTION(SQLite, createSQLiteProxy);
    REGISTER_SIMDB_SCHEMA_BUILDER(Random, buildRandNumbersSchema);
    REGISTER_SIMDB_SCHEMA_BUILDER(Incrementing, buildIncNumbersSchema);

    if (argc > 1) {
        size_t perf_num_tasks = atoi(argv[1]);
        testObjMgrsSharingSameFile(perf_num_tasks);
    } else {
        testObjMgrsSharingSameFile();
    }

    REPORT_ERROR;
    return ERROR_CODE;
}
