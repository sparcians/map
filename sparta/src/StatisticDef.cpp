
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"

namespace sparta {

StatisticDef::AutoContextCounterDeregistration::AutoContextCounterDeregistration(
    const StatisticDef * sd) :
    sd_(sd)
{
}

StatisticDef::AutoContextCounterDeregistration::~AutoContextCounterDeregistration()
{
    trigger::ContextCounterTrigger::deregisterContextCounterAggregateFcns(sd_);
}

} // namespace sparta
