// <Clock> -*- C++ -*-

#include "sparta/simulation/Clock.hpp"

#include <utility>

#include "simdb/ObjectManager.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/ObjectRef.hpp"

namespace sparta {

//! Persist clock hierarchy in the provided database,
//! treating 'this' sparta::Clock as the hierarchy root.
simdb::DatabaseID Clock::serializeTo(
    const simdb::ObjectManager & sim_db) const
{
    auto clock_tbl = sim_db.getTable("ClockHierarchy");
    if (!clock_tbl) {
        return 0;
    }

    std::map<const Clock*, simdb::DatabaseID> db_ids;

    sim_db.safeTransaction([&]() {
        recursSerializeToTable_(*clock_tbl, 0, db_ids);
    });

    auto iter = db_ids.find(this);
    sparta_assert(iter != db_ids.end());
    return iter->second;
}

//! Persist clock hierarchy in the provided database,
//! recursing down through child nodes as necessary.
void Clock::recursSerializeToTable_(
    simdb::TableRef & clock_tbl,
    const simdb::DatabaseID parent_clk_id,
    std::map<const Clock*, simdb::DatabaseID> & db_ids) const
{
    auto row = clock_tbl.createObjectWithArgs(
        "ParentClockID", parent_clk_id,
        "Name", getName(),
        "Period", getPeriod(),
        "FreqMHz", frequency_mhz_,
        "RatioToParent", static_cast<double>(getRatio()));

    for (const auto child : children_) {
        child->recursSerializeToTable_(clock_tbl, row->getId(), db_ids);
    }

    db_ids[this] = row->getId();
}

}
