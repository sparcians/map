
#include <cstdint>
#include <ostream>
#include <string>

#include "sparta/report/format/CSV.hpp"
#include "sparta/trigger/SkippedAnnotators.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta {
class Report;

namespace report {
namespace format {

void CSV::skipRows_(std::ostream & out,
                    const sparta::trigger::SkippedAnnotatorBase * annotator,
                    const Report * r) const
{
    uint32_t total_num_stats = 0;
    getTotalNumStatsForReport_(r, total_num_stats);
    sparta_assert(total_num_stats >= 1);
    --total_num_stats;

    const std::string annotation = annotator->currentAnnotation();
    if (annotation.find(",") != std::string::npos) {
        throw SpartaException(
            "SkippedAnnotatorBase subclass returned an annotation "
            "containing a ',' which is invalid: '") << annotation << "'";
    }

    out << annotation << std::string(total_num_stats, ',') << "\n";
}

} // namespace format
} // namespace report
} // namespace sparta
