// <SimulationInfo> -*- C++ -*-

#include <rapidjson/rapidjson.h>

#include "sparta/app/SimulationInfo.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/utils/ObjectQuery.hpp"

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

#ifndef SPARTA_VERSION
#define SPARTA_VERSION "unknown"
#endif

namespace sparta{
    SimulationInfo SimulationInfo::sim_inst_; // Must be constructed after TimeManager
    std::stack<SimulationInfo*> SimulationInfo::sim_inst_stack_;
    const char SimulationInfo::sparta_version[] = SPARTA_VERSION;

    SimulationInfo::SimulationInfo(const simdb::ObjectManager & sim_db,
                                   const simdb::DatabaseID obj_mgr_db_id,
                                   const simdb::DatabaseID report_node_id) :
        SimulationInfo()
    {
        sim_db.safeTransaction([&]() {
           simdb::ObjectQuery global_meta_query(sim_db, "SimInfo");

           global_meta_query.addConstraints(
               "ObjMgrID", simdb::constraints::equal, obj_mgr_db_id);

           global_meta_query.writeResultIterationsTo(
               "Name", &sim_name_,
               "Cmdline", &command_line_,
               "Exe", &executable_,
               "SimulatorVersion", &simulator_version_,
               "Repro", &reproduction_info_,
               "SpartaVersion", &sparta_version_,
               "Start", &start_time_);

           auto result_iter = global_meta_query.executeQuery();
           if (!result_iter->getNext()) {
               throw SpartaException("Unable to locate a SimInfo record with ")
                   << "ObjMgrID equal to " << obj_mgr_db_id;
           }

           //Clear out any strings that were "unset" in the database.
           //SimDB's "unset" is equal to std::string's empty().
           auto clear_unset_string = [](std::string & str) {
                                         if (str == "unset") {
                                             str.clear();
                                         }
                                     };
           clear_unset_string(sim_name_);
           clear_unset_string(command_line_);
           clear_unset_string(executable_);
           clear_unset_string(simulator_version_);
           clear_unset_string(reproduction_info_);
           clear_unset_string(start_time_);

           //Apply any report-specific metadata we find.
           if (report_node_id > 0) {
               simdb::ObjectQuery report_meta_query(sim_db, "RootReportNodeMetadata");

               report_meta_query.addConstraints(
                   "ReportNodeID", simdb::constraints::equal, report_node_id,
                   "Name", simdb::constraints::equal, "Elapsed");

               std::string elapsed;
               report_meta_query.writeResultIterationsTo("Value", &elapsed);
               result_iter = report_meta_query.executeQuery();
               if (result_iter->getNext()) {
                   //Note that even if the "Elapsed" column was not set,
                   //we still want our ValidValue to be isValid(). Clear
                   //the 'elapsed' string if needed, but set the ValidValue
                   //either way.
                   clear_unset_string(elapsed);
                   db_elapsed_time_ = elapsed;
               }
           }

           //We currently only allow at most *one* non-singleton
           //SimulationInfo object, strictly for the purpose of
           //generating reports from a SimDB outside of a simulation.
           sparta_assert(sim_inst_stack_.empty(), "You cannot create more than "
                         "one SimulationInfo object outside of a simulation.");

           sim_inst_stack_.push(this);
       });
    }

    /*!
     * \brief Instantiate a SimulationInfo object from a json, json_reduced,
     * json_detail, or js_json report file.
     *
     * \param json_fin Input file stream for the JSON report file
     * \param json_kvpairs Optional output argument to get all the
     * name-value pairs of SimulationInfo properties found in the file.
     */
    SimulationInfo::SimulationInfo(std::ifstream & json_fin,
                                   std::map<std::string, std::string> * json_kvpairs) :
        SimulationInfo()
    {
        if (!json_fin) {
            return;
        }

        sparta_assert(sim_inst_stack_.empty(), "You cannot create more than "
                      "one SimulationInfo object outside of a simulation.");

        sim_inst_stack_.push(this);

        namespace rj = rapidjson;
        rj::IStreamWrapper isw(json_fin);
        rj::Document doc;
        doc.ParseStream(isw);
        sparta_assert(doc.IsObject());

        if (!doc.HasMember("siminfo")) {
            return;
        }

        sparta_assert(doc["siminfo"].IsObject());
        rj::Value & node = doc["siminfo"];

        for (auto iter = node.MemberBegin(); iter != node.MemberEnd(); ++iter) {
            const std::string value = iter->value.GetString();
            const std::string key = iter->name.GetString();
            const utils::lowercase_string lower_key = key;

            if (lower_key == "name") {
                sim_name_ = value;
            } else if (lower_key == "sim_version") {
                simulator_version_ = value;
            } else if (lower_key == "reproduction") {
                reproduction_info_ = value;
            }

            if (json_kvpairs) {
                (*json_kvpairs)[key] = value;
            }
        }
    }

}
