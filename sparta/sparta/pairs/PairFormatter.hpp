#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sparta {
    enum class PairFormatter : uint16_t {
        DECIMAL = 0,
        OCTAL = 1,
        HEX = 2
    };

    using PairFormatterInt = std::underlying_type_t<PairFormatter>;

    // Vector used for pipeViewer Formatting.
    using PairFormatterVector = std::vector<PairFormatter>;

    /**
     * \brief Argos \c DataTypeNodes.SpecialFormatters value for a pair formatter.
     * \return \c "HEX", \c "OCT", or empty for default decimal formatting.
     */
    inline std::string pairFormatterToSpecialFormatString(PairFormatter formatter) {
        switch(formatter) {
            case PairFormatter::HEX:
                return "HEX";
            case PairFormatter::OCTAL:
                return "OCT";
            default:
                return "";
        }
    }
} // end namespace sparta
