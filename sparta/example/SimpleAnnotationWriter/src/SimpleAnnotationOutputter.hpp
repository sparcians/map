#include <cstdint>
#include <fstream>
#include <limits>
#include <map>

class SimpleAnnotationOutputter {
    private:
        static constexpr uint64_t ROOT_CLOCK_ID = 1;

        const uint64_t interval_;
        const std::string sim_info_file_name_;
        const std::string clock_file_name_;
        const std::string location_file_name_;
        uint64_t cur_tick_ = 0;
        uint64_t next_interval_end_;
        uint64_t next_transaction_id_ = 0;

        uint64_t next_clock_id_ = ROOT_CLOCK_ID;
        std::map<uint64_t, std::pair<uint64_t, std::string>> clocks_;
        mutable bool wrote_clocks_ = false;

        uint64_t next_location_id_ = 0;
        std::map<uint64_t, std::pair<uint64_t, std::string>> locations_;
        mutable bool wrote_locations_ = false;

        sparta::pipeViewer::Outputter outputter_;

    public:
        SimpleAnnotationOutputter(const std::string& file_path, const uint64_t interval) :
            interval_(interval),
            sim_info_file_name_(file_path + "simulation.info"),
            clock_file_name_(file_path + "clock.dat"),
            location_file_name_(file_path + "location.dat"),
            next_interval_end_(interval),
            outputter_(file_path, interval)
        {
        }

        uint64_t addClock(const uint64_t period, const std::string& clock) {
            const uint64_t id = ++next_clock_id_;
            clocks_.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(period, clock));
            return id;
        }

        void writeSimInfoFile() const {
            std::fstream sim_info(sim_info_file_name_, std::ios::out);
        }

        void writeClockFile() const {
            sparta_assert(!clocks_.empty(), "At least one clock must be defined!");
            std::fstream clock_file(clock_file_name_, std::ios::out);
            clock_file << '1' << std::endl
                       << '1' << std::endl
                       << ROOT_CLOCK_ID << ",Root,1,1,1" << std::endl;
            for(const auto& clock: clocks_) {
                clock_file << clock.first << ',' << clock.second.second << ',' << clock.second.first << ",1,1" << std::endl;
            }
            wrote_clocks_ = true;
        }

        uint64_t addLocation(const uint64_t clock_id, const std::string& location) {
            const uint64_t id = ++next_location_id_;
            locations_.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(clock_id, location));
            return id;
        }

        void writeLocationFile() const {
            sparta_assert(!locations_.empty(), "At least one location must be defined!");
            std::fstream location_file(location_file_name_, std::ios::out);
            location_file << '1' << std::endl;
            for(const auto& loc: locations_) {
                location_file << loc.first << ',' << loc.second.second << ',' << loc.second.first << std::endl;
            }
            wrote_locations_ = true;
        }

        void startAnnotations() {
            writeSimInfoFile();
            writeClockFile();
            writeLocationFile();
            outputter_.writeIndex();
        }

        void tick() {
            ++cur_tick_;
            if(cur_tick_ == next_interval_end_) {
                outputter_.writeIndex();
                next_interval_end_ += interval_;
            }
        }

        void writeAnnotation(const uint64_t location_id, const std::string& data, const uint64_t length = 1) {
            sparta_assert(wrote_clocks_, "Must call writeClockFile() before writing any annotations!");
            sparta_assert(wrote_locations_, "Must call writeLocationFile() before writing any annotations!");
            sparta_assert(locations_.count(location_id) != 0,
                          "Attempted to write annotation for invalid location ID: " << location_id);
            sparta_assert(data.size() < std::numeric_limits<uint16_t>::max(),
                          "Annotation string length is limited to " << std::numeric_limits<uint16_t>::max());
            annotation_t anno;
            anno.time_Start = cur_tick_;
            anno.time_End = cur_tick_ + length;

            anno.parent_ID = 0;
            anno.transaction_ID = next_transaction_id_++;
            anno.display_ID = anno.transaction_ID & 0xfff;
            anno.location_ID = location_id;
            anno.flags = is_Annotation;
            if(anno.time_End > next_interval_end_) {
                anno.flags |= CONTINUE_FLAG;
            }

            anno.control_Process_ID = 0;
            anno.length = static_cast<uint16_t>(data.size() + 1);
            anno.annt = data.c_str();
            outputter_.writeTransaction(anno);
        }
};

