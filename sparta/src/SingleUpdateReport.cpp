// <SingleUpdateReport> -*- C++ -*-

#include "sparta/report/db/SingleUpdateReport.hpp"

#include <cassert>
#include <utility>
#include <zlib.h>

#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"

namespace sparta {
namespace db {

SingleUpdateReport::SingleUpdateReport(
    std::unique_ptr<simdb::ObjectRef> obj_ref) :
    obj_ref_(std::move(obj_ref))
{
}

SingleUpdateReport::SingleUpdateReport(
    const simdb::ObjectManager & obj_mgr,
    const simdb::DatabaseID root_report_node_id) :
    root_report_node_id_(root_report_node_id)
{
    std::unique_ptr<simdb::TableRef> si_values_tbl =
        obj_mgr.getTable("SingleUpdateStatInstValues");

    obj_ref_ = si_values_tbl->createObjectWithArgs(
        "RootReportNodeID", root_report_node_id);
}

int SingleUpdateReport::getId() const
{
    return obj_ref_->getId();
}

void SingleUpdateReport::writeStatisticInstValues(
    const std::vector<double> & si_values)
{
    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &si_values[0];
    blob_descriptor.num_bytes = si_values.size() * sizeof(double);

    auto table = obj_ref_->getObjectManager().getTable(
        "SingleUpdateStatInstValues");

    if (!table->updateRowValues("RawBytes", blob_descriptor,
                                "NumPts", (int)si_values.size(),
                                "WasCompressed", 0).
                forRecordsWhere("RootReportNodeID",
                                simdb::constraints::equal,
                                root_report_node_id_))
    {
        throw SpartaException(
            "Unable to write uncompressed SI blob to the database");
    }
}

void SingleUpdateReport::writeCompressedStatisticInstValues(
    const std::vector<char> & compressed_si_values,
    const uint32_t original_num_si_values)
{
    simdb::Blob blob_descriptor;
    blob_descriptor.data_ptr = &compressed_si_values[0];
    blob_descriptor.num_bytes = original_num_si_values * sizeof(double);

    auto table = obj_ref_->getObjectManager().getTable(
        "SingleUpdateStatInstValues");

    if (!table->updateRowValues("RawBytes", blob_descriptor,
                                "NumPts", (int)original_num_si_values,
                                "WasCompressed", 1).
                forRecordsWhere("RootReportNodeID",
                                simdb::constraints::equal,
                                root_report_node_id_))
    {
        throw SpartaException(
            "Unable to write compressed SI blob to the database");
    }
}

void SingleUpdateReport::getStatisticInstValues(
    std::vector<double> & si_values)
{
    const auto & obj_mgr = obj_ref_->getObjectManager();

    //SELECT NumPts,WasCompressed,RawBytes
    //FROM SingleUpdateStatInstValues
    //WHERE Id=<my_id>
    simdb::ObjectQuery si_query(obj_mgr, "SingleUpdateStatInstValues");
    si_query.addConstraints("Id", simdb::constraints::equal, getId());

    int num_si_values = 0, was_compressed = 0;
    std::vector<char> compressed_blob;

    si_query.writeResultIterationsTo(
        "NumPts", &num_si_values,
        "WasCompressed", &was_compressed,
        "RawBytes", &compressed_blob);

    auto result_iter = si_query.executeQuery();
    assert(result_iter != nullptr);

    if (!result_iter->getNext()) {
        //There is no SI data for this record. Clear and
        //shrink the output vector.
        si_values.clear();
        si_values.shrink_to_fit();
        return;
    }

    //Single-update records should have only one SI row.
    if (result_iter->getNext()) {
        throw SpartaException("Unexpectedly found multiple records ")
            << "in the SingleUpdateStatInstValues table with row Id "
            << getId();
    }

    //We currently *only* support single-update reports (json, html, etc.)
    //in compressed format.
    if (!was_compressed) {
        throw SpartaException("Unexpectedly found a single-update report which ")
            << "had uncompressed SI values stored in the database";
    }

    //Re-inflate the compressed SI blob
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;

    //Setup the source stream
    infstream.avail_in = (uInt)(compressed_blob.size());
    infstream.next_in = (Bytef*)(&compressed_blob[0]);

    //Setup the destination stream
    si_values.resize(num_si_values);
    infstream.avail_out = (uInt)(si_values.size() * sizeof(double));
    infstream.next_out = (Bytef*)(&si_values[0]);

    //Inflate it!
    inflateInit(&infstream);
    inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
}

} // namespace db
} // namespace sparta
