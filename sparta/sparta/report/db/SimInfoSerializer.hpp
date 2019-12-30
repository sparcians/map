// <SimInfoSerializer> -*- C++ -*-

#ifndef __SPARTA_SIMDB_SIMINFO_SERIALIZER_H__
#define __SPARTA_SIMDB_SIMINFO_SERIALIZER_H__

#include "simdb/ObjectManager.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/utils/StringUtils.hpp"

namespace sparta {
namespace db {

/*!
 * \brief This class handles SimDB serialization of all
 * SimulationInfo metadata.
 */
class SimInfoSerializer
{
public:
    //! Construct with a reference to a SimulationInfo object
    //! you wish to serialize.
    explicit SimInfoSerializer(const SimulationInfo & sim_info) :
        sim_info_(sim_info)
    {}

    //! Noncopyable, immovable
    SimInfoSerializer(const SimInfoSerializer &) = delete;
    SimInfoSerializer & operator=(const SimInfoSerializer &) = delete;
    SimInfoSerializer(SimInfoSerializer &&) = delete;
    SimInfoSerializer & operator=(SimInfoSerializer &&) = delete;

    //! Manually set/override SimulationInfo properties.
    //! These will be used in place of the equivalent
    //! values found in the SimulationInfo object that
    //! was given to our constructor.
    //!
    //! \note The prop_name argument is not case sensitive.
    //!
    //! \note Properties set more than once will not warn
    //! or throw an exception. The most recent prop_value
    //! given for the prop_name set more than once will be
    //! used to write the SimulationInfo record.
    //!
    //! \warning The properties "Other" and "ObjMgrID" are
    //! not allowed to be set manually. Attempts to do so
    //! will result in an exception.
    void setPropertyString(const std::string & prop_name,
                           const std::string & prop_value)
    {
        const utils::lowercase_string lower_prop_name = prop_name;
        if (lower_prop_name == "other" || lower_prop_name == "objmgrid") {
            throw SpartaException("SimInfoSerializer::setPropertyString() called ")
                << "with prop_name '" << prop_name << "'. This is not allowed.";
        }
        prop_kvpairs_[prop_name] = prop_value;
    }

    //! Write the contents of this SimulationInfo object to
    //! the given database.
    void serialize(const simdb::ObjectManager & sim_db)
    {
        sim_db.safeTransaction([&]() {
            std::unique_ptr<simdb::TableRef> sim_info_tbl = sim_db.getTable("SimInfo");
            if (sim_info_tbl == nullptr) {
                //We could hit this code if the ObjectManager was
                //connected to a database with a custom schema. The
                //SI schema has a SimInfo table, but other user-defined
                //schemas propably don't.
                std::cout << "SimInfoSerializer could not find the "
                          << "SimInfo table. If this database is from "
                          << "a user-defined schema, you can ignore this "
                          << "warning. Database file is '"
                          << sim_db.getDatabaseFile() << "'"
                          << std::endl;
                return;
            }

            //Helper to extract any property values that were
            //manually set via our API, as opposed to being
            //taken from the SimulationInfo object itself.
            auto get_prop_val = [this](const std::string & metadata_name,
                                       const std::string & metadata_value)
                -> std::string
            {
                const utils::lowercase_string lower_metadata_name = metadata_name;
                auto manual_prop_iter = prop_kvpairs_.find(lower_metadata_name);
                if (manual_prop_iter != prop_kvpairs_.end()) {
                    return manual_prop_iter->second;
                }
                return metadata_value;
            };

            //Loop over the name-value pairs returned by the
            //SimulationInfo object, and write them into the
            //SimInfo table.
            std::unique_ptr<simdb::ObjectRef> sim_info_record;
            const auto sim_info_header = sim_info_.getHeaderPairs();
            for (const auto & nv : sim_info_header) {
                const std::string & metadata_name = nv.first;
                const std::string & metadata_value = nv.second;

                //Some name-value pairs are returned empty from
                //the SimulationInfo. We can't serialize those.
                if (metadata_name.empty() || metadata_value.empty()) {
                    continue;
                }

                //The "Elapsed" piece of metadata is not actually
                //a member variable of the SimulationInfo class.
                //It is retrieved on demand from the TimeManager
                //for each report as they are written to disk. We
                //therefore do not serialize it as part of the
                //global / simulator-wide metadata "SimInfo"
                //table.
                if (metadata_name == "Elapsed") {
                    continue;
                }

                //Since we have an early continue in this loop,
                //make sure we only put a record in the SimInfo
                //table if we have any non-empty metadata info
                //to write.
                if (sim_info_record == nullptr) {
                    sim_info_record = sim_info_tbl->createObject();
                }

                const std::string header_val = get_prop_val(metadata_name, metadata_value);
                sim_info_record->setPropertyString(metadata_name, header_val);
            }

            if (sim_info_record == nullptr) {
                return;
            }

            const std::string & working_dir = sim_info_.working_dir;
            const std::string working_dir_val = get_prop_val("WorkingDir", working_dir);
            if (!working_dir_val.empty()) {
                sim_info_record->setPropertyString("WorkingDir", working_dir_val);
            }

            const std::string & sparta_version = sim_info_.sparta_version;
            const std::string sparta_version_val = get_prop_val("SpartaVersion", sparta_version);
            if (!sparta_version_val.empty()) {
                sim_info_record->setPropertyString("SpartaVersion", sparta_version_val);
            }

            const std::string & repro_info = sim_info_.reproduction_info;
            const std::string repro_info_val = get_prop_val("Repro", repro_info);
            if (!repro_info_val.empty()) {
                sim_info_record->setPropertyString("Repro", repro_info_val);
            }

            if (!sim_info_.other.empty()) {
                std::ostringstream oss;
                if (sim_info_.other.size() == 1) {
                    oss << sim_info_.other[0];
                } else {
                    for (size_t idx = 0; idx < sim_info_.other.size() - 1; ++idx) {
                        oss << sim_info_.other[idx] << ",";
                    }
                    oss << sim_info_.other.back();
                }
                sim_info_record->setPropertyString("Other", oss.str());
            }

            //We use the ObjectManager's unique ID to link these
            //SimInfo records back to other database entities, such
            //as report records. Those reports would be serialized
            //to other database tables, and have this same ObjMgrID
            //as a column in those tables to make the connection.
            sim_info_record->setPropertyInt32("ObjMgrID", sim_db.getId());
        });
    }

private:
    const SimulationInfo & sim_info_;
    std::map<utils::lowercase_string, std::string> prop_kvpairs_;
};

} // namespace db
} // namespace sparta

#endif
