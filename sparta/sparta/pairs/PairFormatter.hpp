#pragma once

#include <cstdint>
#include <locale>
#include <vector>

namespace sparta {
    enum class PairFormatter : uint16_t {
        DECIMAL = 0,
        OCTAL = 1,
        HEX = 2
    };
    
    using PairFormatterInt = std::underlying_type_t<PairFormatter>;
    
    struct PairFormatReader : std::ctype<char> {
        PairFormatReader() :
            std::ctype<char>(getTable())
        {
        }
    
        /**
         * Returns the table of characters that should be treated as whitespace
         */
        static inline std::ctype_base::mask const* getTable() {
            static thread_local std::vector<std::ctype_base::mask> rc(table_size, std::ctype_base::mask());
            rc[':'] = std::ctype_base::space;
            rc['\n'] = std::ctype_base::space;
    
            return &rc[0];
        }
    };
    
    // Vector used for pipeViewer Formatting.
    using PairFormatterVector = std::vector<PairFormatter>;
} // end namespace sparta
