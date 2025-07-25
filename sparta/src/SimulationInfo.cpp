// <SimulationInfo> -*- C++ -*-

#include <rapidjson/rapidjson.h>

#include "sparta/app/SimulationInfo.hpp"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

#ifndef SPARTA_VERSION
#define SPARTA_VERSION "unknown"
#endif

namespace sparta{
    SimulationInfo SimulationInfo::sim_inst_; // Must be constructed after TimeManager
    const char SimulationInfo::sparta_version[] = SPARTA_VERSION;

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
