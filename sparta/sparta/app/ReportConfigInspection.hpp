// <ReportConfigInspection> -*- C++ -*-


/*!
 * \file ReportConfigInspection.hpp
 *
 * Collection of helper methods to inspect report / report trigger
 * configuration for a given simulation.
 */

#ifndef __REPORT_CONFIG_INSPECTION_H__
#define __REPORT_CONFIG_INSPECTION_H__

#include "sparta/app/ReportDescriptor.hpp"

namespace sparta {
namespace app {

//! \brief Ask this descriptor if it has at least one trigger
bool hasAnyReportTriggers(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any start trigger
bool hasStartTrigger(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any update trigger
bool hasUpdateTrigger(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any stop trigger
bool hasStopTrigger(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any toggle trigger
bool hasToggleTrigger(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any on-demand trigger
bool hasOnDemandTrigger(const ReportDescriptor * rd);

//! \brief Ask this descriptor if it has any *start* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceStartTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name);

//! \brief Ask this descriptor if it has any *update* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceUpdateTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name);

//! \brief Ask this descriptor if it has any *stop* trigger
//! that is configured to listen to a NotificationSource by
//! the given name.
bool hasNotifSourceStopTriggerNamed(
    const ReportDescriptor * rd,
    const std::string & notif_source_name);

//! \brief Ask this descriptor for its full trigger expression
//! of the given type. Examples:
//! \code
//!     auto start_expr = getTriggerExpression(rd, "start");
//!     if (start_expr.isValid()) {
//!         std::cout << "Start trigger has expression: "
//!                   << start_expr.getValue() << std::endl;
//!     } else {
//!         std::cout << "No start trigger found for report '"
//!                   << rd->dest_file << "'" << std::endl;
//!     }
//!
//!     auto update_expr = getTriggerExpression(rd, "update-cycles");
//!     //...
//! \endcode
//!
//! \note If valid, the returned string will have all whitespace
//! characters removed from it. This ensures consistent returned
//! strings for things like:
//!     trigger:                       |  trigger:
//!       start: "notif.foobar >= 9"   |    start: "notif.foobar  >=  9"
//!
//! Which are functionally identical.
utils::ValidValue<std::string> getTriggerExpression(
    const ReportDescriptor * rd,
    const std::string & yaml_type);

//! \brief Get the notif.THIS_NAME of the descriptor's start trigger.
//! Example:
//! \code
//!     //YAML file looked like this:
//!     //    report:
//!     //      ...
//!     //      trigger:
//!     //        start: notif.my_start_channel == 100
//!     auto start_notif_name = getNotifSourceForStartTrigger(rd);
//!     sparta_assert(start_notif_name.isValid(),
//!                 "Expected a notif-based start trigger");
//!     std::cout << "Start trigger was driven by the NotificationSource named '"
//!               << start_notif_name.getValue() << std::endl;
//!
//! \note If valid, the returned string will have whitespace
//! characters removed. More specifically, these strings are
//! trimmed which removes whitespace from the outer edges of
//! the string:
//!       " foobar  " --> "foobar"
//!
//! There will be no whitespace left after trimming a notif
//! source name.
utils::ValidValue<std::string> getNotifSourceForStartTrigger(
    const ReportDescriptor * rd);

//! \brief Get the notif.THIS_NAME of the descriptor's update trigger.
//! See above for code example.
//! \note See above for a note on whitespace removal.
utils::ValidValue<std::string> getNotifSourceForUpdateTrigger(
    const ReportDescriptor * rd);

//! \brief Get the notif.THIS_NAME of the descriptor's stop trigger.
//! See above for code example.
//! \note See above for a note on whitespace removal.
utils::ValidValue<std::string> getNotifSourceForStopTrigger(
    const ReportDescriptor * rd);

} // namespace app
} // namespace sparta

#endif
