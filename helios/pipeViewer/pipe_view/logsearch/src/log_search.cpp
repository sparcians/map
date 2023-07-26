#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

/*!
 * \brief Helper for quickly scanning through a SPARTA log file based on
 * the "{TICK CYCLE ..." line format.
 *
 * Example
 * \code
 * LogSearch s(myfile);
 * uint64_t tick = 12345678;
 * uint64_t loc = s.getLocationByTick(tick);
 * if(loc == s.BAD_LOCATION){
 *     // do nothing
 * }else{
 *     // open myfile, seekg to loc, and then readline() until done
 * }
 * loc = s.getLocationByTick(tick + 10, loc); // Can starting from previous file loc to save time.
 * // reject BAD_LOCATION return value, read from file, repeat above...
 * \endcode
 *
 * \todo This should support reading lines directly from the file as well
 */
class LogSearch
{
    std::ifstream in_;
    uint64_t file_bytes_ = 0;

public:

    static const uint64_t BAD_LOCATION = std::numeric_limits<uint64_t>::max();

    explicit LogSearch(const std::string& filename)
        : in_(filename)
    {
        file_bytes_ = 0;
        if(in_.is_open()){
            in_.seekg(0, in_.end);
            const auto len = in_.tellg();
            if(len >= 0){
                file_bytes_ = static_cast<uint64_t>(len);
            }
        }
    }

    uint64_t getLocationByTick(uint64_t tick, uint64_t earlier_location=0)
    {
        // Early out for no file or empty file
        if(in_.is_open() == false || file_bytes_ == 0){
            return BAD_LOCATION;
        }

        const uint32_t BUFFER_SIZE = 512;
        char buffer[BUFFER_SIZE];

        if(earlier_location >= file_bytes_){
            return BAD_LOCATION;
        }

        in_.seekg((std::streampos)earlier_location);

        uint64_t last_line_pos = earlier_location; // Assume at start of line
        bool got_newline = true;
        while(in_.good()){
            char chr;
            in_.get(chr);
            if(chr == '\n'){
                last_line_pos = in_.tellg();
                got_newline = true;
                continue;
            }
            // If prev char was newline
            if(got_newline){
                if(chr == '{'){
                    // Parse out the tick value
                    char* bufptr = buffer;
                    while(!in_.eof()){
                        in_.get(chr);
                        if(chr == ' '){
                            break;
                        }
                        *bufptr = chr;
                        bufptr++;
                    }
                    const uint64_t tickval = strtoull(buffer, &bufptr, 10);
                    if(tickval >= tick){
                        return last_line_pos; // Found a line containing the chosen tick or later!
                    }
                }
            }
            got_newline = false;
        }

        return BAD_LOCATION;
    }
};
