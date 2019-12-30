// <ReportHeader> -*- C++ -*-

#include "sparta/report/db/ReportHeader.hpp"

#include <utility>

#include "sparta/report/db/ReportTimeseries.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/Constraints.hpp"

namespace sparta {
namespace db {

/*!
 * \brief This class is just a user-friendly wrapper around
 * report metadata, so it simply forwards set/get requests
 * right to its ObjectRef (record wrapper)
 */

ReportHeader::ReportHeader(std::unique_ptr<simdb::ObjectRef> obj_ref) :
    obj_ref_(std::move(obj_ref))
{
}

ReportHeader::ReportHeader(const simdb::ObjectManager & obj_mgr)
{
    obj_ref_ = obj_mgr.getTable("ReportHeader")->createObject();
}

uint64_t ReportHeader::getId() const
{
    return obj_ref_->getId();
}

const simdb::ObjectRef & ReportHeader::getObjectRef()const
{
    return *obj_ref_;
}

simdb::ObjectRef & ReportHeader::getObjectRef()
{
    return *obj_ref_;
}

void ReportHeader::setOwningTimeseries(const ReportTimeseries & ts)
{
    obj_ref_->setPropertyInt32("TimeseriesID", ts.getId());
}

void ReportHeader::setReportName(const std::string & report_name)
{
    obj_ref_->setPropertyString("ReportName", report_name);
}

void ReportHeader::setReportStartTime(const uint64_t start_time)
{
    obj_ref_->setPropertyUInt64("StartTime", start_time);
}

void ReportHeader::setReportEndTime(const uint64_t end_time)
{
    obj_ref_->setPropertyUInt64("EndTime", end_time);
}

void ReportHeader::setSourceReportDescDestFile(
    const std::string & fname)
{
    obj_ref_->setPropertyString("DestFile", fname);
}

void ReportHeader::setCommaSeparatedSILocations(
    const std::string & si_locations)
{
    obj_ref_->setPropertyString("SILocations", si_locations);
}

void ReportHeader::setSourceReportNumStatInsts(
    const uint32_t num_stat_insts)
{
    obj_ref_->setPropertyInt32("NumStatInsts", num_stat_insts);
}

std::string ReportHeader::getReportName() const
{
    return obj_ref_->getPropertyString("ReportName");
}

uint64_t ReportHeader::getReportStartTime() const
{
    return obj_ref_->getPropertyUInt64("StartTime");
}

uint64_t ReportHeader::getReportEndTime() const
{
    return obj_ref_->getPropertyUInt64("EndTime");
}

std::string ReportHeader::getSourceReportDescDestFile() const
{
    return obj_ref_->getPropertyString("DestFile");
}

std::string ReportHeader::getCommaSeparatedSILocations() const
{
    return obj_ref_->getPropertyString("SILocations");
}

void ReportHeader::setStringMetadata(
    const std::string & name,
    const std::string & value)
{
    const simdb::ObjectManager & obj_mgr = obj_ref_->getObjectManager();
    auto metadata_tbl = obj_mgr.getTable("StringMetadata");
    constexpr auto equals = simdb::constraints::equal;

    if (metadata_tbl->updateRowValues("MetadataValue", value).
                      forRecordsWhere("ReportHeaderID", equals, getId(),
                                      "MetadataName", equals, name))
    {
        return;
    } else {
        metadata_tbl->createObjectWithArgs(
            "ReportHeaderID", getId(),
            "MetadataName", name,
            "MetadataValue", value);
    }
}

std::string ReportHeader::getStringMetadata(const std::string & name) const
{
    const simdb::ObjectManager & obj_mgr = obj_ref_->getObjectManager();

    simdb::ObjectQuery query(obj_mgr, "StringMetadata");
    query.addConstraints(
        "ReportHeaderID", simdb::constraints::equal, getId(),
        "MetadataName", simdb::constraints::equal, name);

    std::string metadata_value;
    query.writeResultIterationsTo("MetadataValue", &metadata_value);
    query.executeQuery()->getNext();

    return metadata_value;
}

std::map<std::string, std::string> locGetAllStringMetadata(
    const ReportHeader & header)
{
    const simdb::ObjectManager & obj_mgr =
        header.getObjectRef().getObjectManager();

    simdb::ObjectQuery query(obj_mgr, "StringMetadata");
    query.addConstraints("ReportHeaderID", simdb::constraints::equal, header.getId());

    std::string metadata_name, metadata_value;
    query.writeResultIterationsTo("MetadataName", &metadata_name,
                                  "MetadataValue", &metadata_value);

    std::map<std::string, std::string> metadata_pairs;
    auto result_iter = query.executeQuery();
    while (result_iter->getNext()) {
        metadata_pairs[metadata_name] = metadata_value;
    }
    return metadata_pairs;
}

std::map<std::string, std::string> ReportHeader::getAllStringMetadata() const
{
    std::map<std::string, std::string> all_metadata = locGetAllStringMetadata(*this);
    std::map<std::string, std::string> visible_metadata;
    for (const auto & md : all_metadata) {
        if (md.first.find("__") != 0) {
            visible_metadata[md.first] = md.second;
        }
    }
    return visible_metadata;
}

std::map<std::string, std::string> ReportHeader::getAllHiddenStringMetadata() const
{
    std::map<std::string, std::string> all_metadata = locGetAllStringMetadata(*this);
    std::map<std::string, std::string> hidden_metadata;
    for (const auto & md : all_metadata) {
        if (md.first.find("__") == 0 && md.first.size() > 2) {
            const std::string culled_metadata_name = md.first.substr(2);
            hidden_metadata[culled_metadata_name] = md.second;
        }
    }
    return hidden_metadata;
}

} // namespace db
} // namespace sparta
