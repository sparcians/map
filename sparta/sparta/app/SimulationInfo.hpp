// <SimulationInfo.h> -*- C++ -*-


/**
 * \file SimulationInfo.hpp
 * \brief Simulation information container for tracking the status of the
 * simulation.
 */

#pragma once

#include <errno.h>
#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <stack>
#include <fstream>
#include <map>
#include <memory>
#include <utility>

#include "sparta/utils/TimeManager.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"

#include "simdb/schema/DatabaseTypedefs.hpp"
namespace simdb { class ObjectManager; }

namespace sparta
{

/*!
 * \brief Helper class for building a set of lines through an ostream-like
 * interface
 */
class LineStringStream
{
    std::vector<std::string> lines_; //!< Contained lines
    std::stringstream stream_; //!< Current string

public:

    typedef std::ostream& (*manip_t)(std::ostream&);

    //!< Default Constructor
    LineStringStream() = default;

    /*!
     * \brief Insertion operator on this LogObject.
     * \return This LogObject
     *
     * Appends object to internal ostringstream
     */
    template <class T>
    LineStringStream& operator<<(const T& t) {
        stream_ << t;
        return *this;
    }

    /*!
     * \brief Handler for stream modifiers (e.g. endl)
     */
    LineStringStream& operator<<(manip_t f) {
        if(f == static_cast<manip_t>(std::endl)){
            addNewLine();
        }else{
            f(stream_);
        }
        return *this;
    }

    /*!
     * \brief Add a new line to the list. This is also called when std::endl
     * is inserted into this stream
     */
    void addNewLine(){
        lines_.push_back(stream_.str());
        stream_.str(""); // Clear
    }

    /*!
     * \brief Gets the lines including whathever line is currently being
     * built
     * \warning This is too expensive to call in the critical path
     */
    std::vector<std::string> getLines() const {
        std::vector<std::string> result = lines_; // Copy
        result.push_back(stream_.str());
        return result;
    }
}; // class LineStringStream

/*!
 * \class SimulationInfo
 * \brief Contains information describing the simulation instance
 * for the purpose of identifying the simulation and possible recreating it.
 * This information should be written to all log files, reports, cmdline, etc.
 * \note Since this is just useful information and does not change the function
 * of a simulator, it does not protect itself from run-time changes.
 */
class SimulationInfo
{
    std::string sim_name_;           //!< See sim_name
    std::string command_line_;       //!< See command_line
    std::string working_dir_;        //!< See working_dir
    std::string executable_;         //!< See executable
    std::string simulator_version_;  //!< See simulator_version
    std::string reproduction_info_;  //!< See reproduction_info
    std::string start_time_;         //!< See start_time
    std::string sparta_version_;       //!< See sparta_version
    std::vector<std::string> other_; //!< See other

    //! Variables which are populated from SimDB tables:
    utils::ValidValue<std::string>  db_elapsed_time_;

    //! The singleton SimulationInfo is the "real" object, but we
    //! need to temporarily "swap in" other SimulationInfo objects
    //! that were recreated from a SimDB. We'll use a stack of these
    //! 'this' pointers to achieve this.
    //!
    //! The reason we do this is so we don't have to make
    //! changes in all the report formatters that are already
    //! calling SimulationInfo::getInstance() for their report
    //! metadata.
    static std::stack<SimulationInfo*> sim_inst_stack_;

    static SimulationInfo sim_inst_; //!< Static simulation information

public:

    /*!
     * \brief Simulator application name. Note that multiple simulators could
     * exist in the same process space, so this should be the application
     * containing those simulations. If there is only one simulation, this
     * should usually match the simulation name.
     */
    const std::string& sim_name;

    /*!
     * \brief Simulator application instance command-line
     */
    const std::string& command_line;

    /*!
     * \brief Simulator application instance working dir
     */
    const std::string& working_dir;

    /*!
     * \brief Executable being run
     */
    const std::string& executable;

    /*!
     * \brief Simulator Version of the simulator itself
     */
    const std::string& simulator_version;

    /*!
     * \brief The version of SPARTA
     */
    static const char sparta_version[];

    /*!
     * \brief versions/buildnums/tags of simulator and
     *        dependencies necessary for reproducing the build from a
     *        version control system
     */
    const std::string& reproduction_info;

    /*!
     * \brief Time at which the simulation started (roughly)
     */
    const std::string& start_time;

    /*!
     * \brief other simulator information
     */
    const std::vector<std::string>& other;

    /*!
     * \brief Gets the SimulationInfo singleton instance
     */
    static SimulationInfo& getInstance() {
        if (sim_inst_stack_.empty()) {
            return sim_inst_;
        }
        return *sim_inst_stack_.top();
    }

    /*!
     * \brief Destruction
     */
    ~SimulationInfo() {
        if (!sim_inst_stack_.empty()) {
            sim_inst_stack_.pop();
        }
    }

    /*!
     * \brief Default Constructor
     */
    SimulationInfo() :
        start_time_(TimeManager::getTimeManager().getLocalTime()),
        sim_name(sim_name_),
        command_line(command_line_),
        working_dir(working_dir_),
        executable(executable_),
        simulator_version(simulator_version_),
        reproduction_info(reproduction_info_),
        start_time(start_time_),
        other(other_)
    {;}

    /*!
     * \brief Copy-constructor
     */
    SimulationInfo(const SimulationInfo& rhp) :
        SimulationInfo()
    {
        sim_name_ = rhp.sim_name_;
        command_line_ = rhp.command_line_;
        working_dir_ = rhp.working_dir_;
        executable_ = rhp.executable_;
        simulator_version_ = rhp.simulator_version_;
        reproduction_info_ = rhp.reproduction_info_;
        other_ = rhp.other_;
    }

    /*!
     * \brief Not move-constructable
     */
    SimulationInfo(SimulationInfo&&) = delete;

    /*!
     * \brief Assignable
     */
    SimulationInfo& operator=(const SimulationInfo& rhp) {
        sim_name_ = rhp.sim_name_;
        command_line_ = rhp.command_line_;
        working_dir_ = rhp.working_dir_;
        executable_ = rhp.executable_;
        simulator_version_ = rhp.simulator_version_;
        reproduction_info_ = rhp.reproduction_info_;
        start_time_ = rhp.start_time_;
        other_ = rhp.other_;

        return *this;
    }

    /*!
     * \brief SimulationInfo constructor
     */
    SimulationInfo(const std::string& _sim_name,
                   const std::string& _command_line,
                   const std::string& _working_dir,
                   const std::string& _executable,
                   const std::string& _simulator_version,
                   const std::string& _reproduction_info,
                   const std::vector<std::string> & _other) :
        SimulationInfo()
    {
        sim_name_ = _sim_name;
        command_line_ = _command_line;
        working_dir_ = _working_dir;
        executable_  = _executable;
        simulator_version_ = _simulator_version;
        reproduction_info_ = _reproduction_info;
        other_ = _other;
    }

    /*!
     * \brief SimulationInfo constructor with automatic command-line
     * reconstruction
     * \note Automatically determine working directory using getcwd. Consider
     * this when deciding where to call this method.
     */
    SimulationInfo(const std::string& _sim_name,
                   int argc,
                   char** argv,
                   const std::string& _simulator_version,
                   const std::string& _reproduction_info,
                   const std::vector<std::string> & _other) :
        SimulationInfo()
    {
        sim_name_ = _sim_name;
        setCommandLine(argc, argv);
        simulator_version_ = _simulator_version;
        reproduction_info_ = _reproduction_info;
        other_ = _other;

        const uint32_t PATH_SIZE = 2048;
        char tmp[PATH_SIZE];
        if (getcwd(tmp, PATH_SIZE) != nullptr) {
            working_dir_ = tmp;
        } else {
            std::stringstream ss;
            ss << "<error " << errno << " determining working directory>";
            working_dir_ = ss.str();
        }
    }

    /*!
     * \brief Recreate a SimulationInfo object from the
     * provided SimInfo record with the given ObjMgrID.
     *
     * \note While this SimDB-created SimulationInfo object
     * is in scope, calls to SimulationInfo::getInstance()
     * will *not* return the singleton. It will return
     * the object that you recreated using this "SimDB
     * constructor". It is not recommended to use this
     * constructor during an actual SPARTA simulation!
     */
    SimulationInfo(const simdb::ObjectManager & sim_db,
                   const simdb::DatabaseID obj_mgr_db_id,
                   const simdb::DatabaseID report_node_id = 0);
    /*!
     * \brief Instantiate a SimulationInfo object from a json, json_reduced,
     * json_detail, or js_json report file.
     *
     * \param json_fin Input file stream for the JSON report file
     * \param json_kvpairs Optional output argument to get all the
     * name-value pairs of SimulationInfo properties found in the file.
     */
    SimulationInfo(std::ifstream & json_fin,
                   std::map<std::string, std::string> * json_kvpairs = nullptr);

    /*!
     * \brief Get the SPARTA version string for this SimulationInfo object.
     * Most of the time, this will be SimulationInfo::sparta_version (const /
     * global). But there are some SimDB/report workflows that need to
     * create or recreate SimulationInfo objects with a different SPARTA
     * version string.
     */
    std::string getSpartaVersion() const {
        if (sparta_version_.empty()) {
            return SimulationInfo::sparta_version;
        }
        return sparta_version_;
    }

    /*!
     * \brief Assign command_line_ and executable_ from args
     * \todo Re-escape quotes or remove need for quotes by escaping everything
     */
    void setCommandLine(int argc, char** argv) {
        if(argc > 0){
            executable_ = argv[0];
        }
        std::stringstream cmdln;
        for(int i = 0; i < argc; ++i){
            if(i > 0){
                cmdln << " ";
            }
            size_t arglen = strlen(argv[i]);
            if(arglen == 0
               || memchr(argv[i], ' ', arglen) != nullptr
               || memchr(argv[i], '\t', arglen) != nullptr
               || memchr(argv[i], '\n', arglen) != nullptr){
                cmdln << '"' << argv[i] << '"';
            }else{
                cmdln << argv[i];
            }
        }
        command_line_ = cmdln.str();
    }

    /*!
     * \brief Assign command line from string
     */
    void setCommandLine(const std::string& cmdline) {
        command_line_ = cmdline;
    }

    /*!
     * \brief Add other information to the simulation
     */
    void addOtherInfo(const std::string& _other){
        other_.push_back(_other);
    }

    /*!
     * \brief Write this information to an ostream
     * \param line_start String to write at the start of each line (per field)
     * \param line_end String to write at the end of each line (per field).
     * This is typically "\n", but can be anything.
     * \param show_field_names Show the names of each field before printing the
     * values
     */
    template <typename StreamType=std::ostream>
    void write(StreamType& o,
               const std::string& line_start="# ",
               const std::string& line_end="\n",
               bool show_field_names=true) const {

        auto pairs = getHeaderPairs();
        for(auto p : pairs){
            o << line_start;
            if(show_field_names){
                o << std::left << std::setw(10) << (p.first + ":");
            }
            o << p.second << line_end;
        }

        if(other.size() > 0){
            o << line_start << "Other:" << line_end;
            for(const std::string& oi : other){
                o << line_start << "  " << oi << line_end;
            }
        }
    }

    /*!
     * \brief Generate a string using the write function without needing to
     * construct a temporary stringstream
     */
    template <typename ...Args>
    std::string stringize(const Args& ...args) const {
        std::stringstream ss;
        write(ss, args...);
        return ss.str();
    }

    /*!
     * \brief Generate a vector of lines
     * \note line_end is ignored
     * \warning This is too expensive to call in the critical path
     * \warnign The output of this function may be subject to change
     */
    template <typename ...Args>
    std::vector<std::string> stringizeToLines(const Args& ...args) const {
        LineStringStream ss;
        write(ss, args...);
        return ss.getLines();
    }

    /*!
     * \brief Gets (name, value) pairs for each header entry
     */
    std::vector<std::pair<std::string, std::string>> getHeaderPairs() const {
        std::vector<std::pair<std::string, std::string>> result;
        result.emplace_back("Name", sim_name);
        result.emplace_back("Cmdline", command_line);
        result.emplace_back("Exe", executable);
        result.emplace_back("SimulatorVersion", simulator_version);
        result.emplace_back("Repro", reproduction_info);
        result.emplace_back("Start", start_time);

        //SimDB-recreated SimulationInfo objects have their
        //elapsed time values stored in the database directly.
        if (db_elapsed_time_.isValid()) {
            result.emplace_back("Elapsed", db_elapsed_time_.getValue());
        }

        //Normal use of the SimulationInfo::getInstance() singleton,
        //which is used during live SPARTA simulations, computes the
        //elapsed time value via the TimeManager.
        else {
            std::stringstream timestr;
            timestr << TimeManager::getTimeManager().getSecondsElapsed() << 's';
            result.emplace_back("Elapsed", timestr.str());
        }

        last_captured_elapsed_time_ = result.back().second;
        return result;
    }

    /*!
     * \brief Return the very last 'Elapsed' time that this object
     * got from the TimeManager. This is reset with each call to
     * getHeaderPairs().
     */
    const utils::ValidValue<std::string> & getLastCapturedElapsedTime() const {
        return last_captured_elapsed_time_;
    }

private:
    mutable utils::ValidValue<std::string> last_captured_elapsed_time_;

}; // class SimulationInfo

/*!
 * \brief ostream insertion operator for SimulationInfo
 */
inline std::ostream& operator<<(std::ostream& o, const SimulationInfo& info) {
    info.write(o);
    return o;
}

} // namespace sparta
