#pragma once

#include <cstdint>
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
} // end namespace sparta
