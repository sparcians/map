// <simdb> -*- C++ -*-

#include "simdb/async/TimerThread.hpp"
#include "simdb/schema/Schema.hpp"
#include "simdb/schema/DatabaseRoot.hpp"

/*!
 * \brief Static initializations in the SimDB module.
 */

namespace simdb
{
    std::atomic<uint64_t> TimerThread::current_num_task_threads_{0};
    bool TimerThread::stress_testing_ = false;
    std::map<std::string, std::string> DatabaseRoot::db_types_by_namespace_;
    std::map<std::string, std::vector<SchemaBuildFcn>> DatabaseRoot::schema_builders_by_namespace_;
    std::map<std::string, ProxyCreateFcn> DatabaseRoot::proxy_creators_by_db_type_;
}
