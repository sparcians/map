
#include "sparta/kernel/MemoryProfiler.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/parsers/YAMLTreeEventHandler.hpp"
#include "sparta/utils/ValidValue.hpp"

#include <gperftools/malloc_extension.h>

#include <map>
#include <set>
#include <stack>
#include <filesystem>

namespace sparta {

class ProfilerConfig {
public:
    std::string report_name;
    std::string dest_file;
    std::set<MemoryProfiler::Phase> phases;
    std::string update_type;
    std::string update_expression;

    ProfilerConfig(const std::string & report_name,
                   const std::string & dest_file,
                   const std::set<MemoryProfiler::Phase> & phases,
                   const std::string & update_type,
                   const std::string & update_expression) :
        report_name(report_name),
        dest_file(dest_file),
        phases(phases),
        update_type(update_type),
        update_expression(update_expression)
    {}
};

typedef std::vector<ProfilerConfig> ProfilerConfigurations;

class MemoryProfilerConfigFileParserYAML
{
    class MemoryProfilerConfigFileEventHandlerYAML :
        public sparta::YAMLTreeEventHandler
    {
    private:
        std::stack<bool> in_report_stack_;
        std::string report_name_;
        std::string dest_file_;
        std::set<MemoryProfiler::Phase> phases_;
        std::string update_type_;
        std::string update_expression_;
        ProfilerConfigurations configs_;

        /*!
         * \brief Reserved keywords for this parser's dictionary
         */
        static constexpr char KEY_CONTENT[]         = "content";
        static constexpr char KEY_MEM_REPORT[]      = "memory-report";
        static constexpr char KEY_NAME[]            = "name";
        static constexpr char KEY_DEST_FILE[]       = "dest_file";
        static constexpr char KEY_PHASES[]          = "phases";
        static constexpr char KEY_UPDATE_COUNT[]    = "update-count";
        static constexpr char KEY_UPDATE_CYCLE[]    = "update-cycles";
        static constexpr char KEY_UPDATE_TIME[]     = "update-time";
        static constexpr char KEY_UPDATE_WHENEVER[] = "update-whenever";

        virtual bool handleEnterMap_(
            const std::string & key,
            NavVector & context) override final
        {
            (void) context;

            if (key == KEY_CONTENT) {
                return false;
            }

            if (key == KEY_MEM_REPORT) {
                if (!in_report_stack_.empty()) {
                    throw SpartaException(
                        "Nested memory report definitions are not supported");
                }
                prepareForNextConfig_();
                in_report_stack_.push(true);
                return false;
            }

            if (!key.empty()) {
                throw SpartaException(
                    "Unrecognized key found in memory profile definition file: ") << key;
            }

            return false;
        }

        virtual void handleLeafScalar_(
            TreeNode * n,
            const std::string & value,
            const std::string & assoc_key,
            const std::vector<std::string> & captures,
            node_uid_t uid) override final
        {
            (void) n;
            (void) captures;
            (void) uid;

            if (assoc_key == KEY_NAME) {
                report_name_ = value;
            } else if (assoc_key == KEY_DEST_FILE) {
                dest_file_ = value;
            } else if (assoc_key == KEY_PHASES) {
                std::string no_whitespace(value);
                boost::erase_all(no_whitespace, " ");
                std::vector<std::string> phases;
                boost::split(phases, no_whitespace, boost::is_any_of(","));

                bool all_phases = false;
                for (const auto & phase : phases) {
                    if (phase == "build") {
                        phases_.insert(MemoryProfiler::Phase::Build);
                    } else if (phase == "configure") {
                        phases_.insert(MemoryProfiler::Phase::Configure);
                    } else if (phase == "bind") {
                        phases_.insert(MemoryProfiler::Phase::Bind);
                    } else if (phase == "simulate") {
                        phases_.insert(MemoryProfiler::Phase::Simulate);
                    } else if (phase == "all") {
                        all_phases = true;
                    } else {
                        throw SpartaException("Invalid memory profile phase specified: ") << phase;
                    }
                }

                if (all_phases) {
                    if (phases.size() > 1) {
                        std::cerr << "The following memory profiler phases were specified: \n"
                                  << "\t" << value << "\n"
                                  << "Note that the 'all' keyword has forced every simulation "
                                  << "phase to be included in the profile." << std::endl;
                    }
                    phases_.clear();
                }
            } else {
                sparta_assert(assoc_key == KEY_UPDATE_COUNT ||
                            assoc_key == KEY_UPDATE_CYCLE ||
                            assoc_key == KEY_UPDATE_TIME  ||
                            assoc_key == KEY_UPDATE_WHENEVER);
                update_type_ = assoc_key;
                update_expression_ = value;
            }
        }

        virtual bool handleExitMap_(
            const std::string & key,
            const NavVector & context) override final
        {
            (void) context;

            if (key == KEY_MEM_REPORT) {
                sparta_assert(!report_name_.empty());
                sparta_assert(!dest_file_.empty());
                sparta_assert(!update_type_.empty());
                sparta_assert(!update_expression_.empty());

                in_report_stack_.pop();

                configs_.emplace_back(
                    report_name_,
                    dest_file_,
                    phases_,
                    update_type_,
                    update_expression_);
            }

            return false;
        }

        virtual bool isReservedKey_(
            const std::string & key) const override final
        {
            return (key == KEY_CONTENT           ||
                    key == KEY_MEM_REPORT        ||
                    key == KEY_NAME              ||
                    key == KEY_DEST_FILE         ||
                    key == KEY_PHASES            ||
                    key == KEY_UPDATE_COUNT      ||
                    key == KEY_UPDATE_CYCLE      ||
                    key == KEY_UPDATE_TIME       ||
                    key == KEY_UPDATE_WHENEVER);
        }

        void prepareForNextConfig_() {
            report_name_ = "Memory usage statistics";
            dest_file_ = "mem-stats.csv";
            update_type_ = "update-cycles";
            update_expression_ = "10k";
        }

    public:
        MemoryProfilerConfigFileEventHandlerYAML(
            const std::string & def_file,
            NavVector device_trees) :
                sparta::YAMLTreeEventHandler(def_file, device_trees, false)
        {
        }

        const ProfilerConfigurations & getConfigs() const
        {
            return configs_;
        }
    };

    std::unique_ptr<MemoryProfilerConfigFileEventHandlerYAML> handler_;

public:
    explicit MemoryProfilerConfigFileParserYAML(const std::string & def_file) :
        fin_(),
        parser_(),
        def_file_(def_file)
    {
        sparta_assert(std::filesystem::exists(def_file_),
                   ("File '" + def_file + "' cannot be found"));
        fin_.open(def_file.c_str(), std::ios::in);
        sparta_assert(fin_.is_open());
        parser_.reset(new YP::Parser(fin_));
    }

    explicit MemoryProfilerConfigFileParserYAML(std::istream & content) :
        fin_(),
        parser_(new YP::Parser(content)),
        def_file_("<istream>")
    {
    }

    const ProfilerConfigurations & parseConfigurations(sparta::TreeNode * context)
    {
        std::shared_ptr<sparta::YAMLTreeEventHandler::NavNode> scope(
            new sparta::YAMLTreeEventHandler::NavNode({
                nullptr, context, {}, 0}));

        handler_.reset(new MemoryProfilerConfigFileEventHandlerYAML(def_file_, {scope}));

        while(parser_->HandleNextDocument(*((YP::EventHandler*)handler_.get()))) {}

        return handler_->getConfigs();
    }

private:
    std::ifstream fin_;
    std::unique_ptr<YP::Parser> parser_;
    std::string def_file_;
};

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_CONTENT[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_MEM_REPORT[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_NAME[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_DEST_FILE[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_PHASES[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_UPDATE_COUNT[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_UPDATE_CYCLE[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_UPDATE_TIME[];

constexpr char MemoryProfilerConfigFileParserYAML::
    MemoryProfilerConfigFileEventHandlerYAML::KEY_UPDATE_WHENEVER[];

class MemoryProfiler::Impl
{
public:
    Impl(const std::string & def_file,
         TreeNode * context,
         app::Simulation * sim)
    {
        if (def_file == "@") {
            setDefaults_(context, sim);
        } else if (def_file == "1") {
            setDefaultsForStdoutDump_(context, sim);
        } else {
            MemoryProfilerConfigFileParserYAML parser(def_file);
            auto & cfgs = parser.parseConfigurations(context);
            prepareProfiler_(cfgs, context, sim);
        }
    }

    void enteringPhase(const MemoryProfiler::Phase phase)
    {
        for (auto & prof : profilers_) {
            prof.enteringPhase(phase);
        }
    }

    void exitingPhase(const MemoryProfiler::Phase phase)
    {
        for (auto & prof : profilers_) {
            prof.exitingPhase(phase);
        }
    }

    void saveReport()
    {
        for (auto & prof : profilers_) {
            prof.saveReport();
        }
    }

    void saveReportToStream(std::ostream & os)
    {
        for (auto & prof : profilers_) {
            prof.saveReportToStream(os);
        }
    }

private:
    class ProfileRuntime
    {
    public:
        ProfileRuntime(const ProfilerConfig & cfg,
                       TreeNode * context,
                       app::Simulation * sim) :
            report_name_(cfg.report_name),
            dest_file_(cfg.dest_file),
            phases_(cfg.phases),
            update_type_(cfg.update_type),
            update_expression_(cfg.update_expression),
            context_(context),
            sim_(sim)
        {
            if (!dest_file_.empty() && dest_file_ != "1" &&
                std::filesystem::extension(dest_file_) != ".csv") {
                throw SpartaException("Memory statistics must be saved to a *.csv file, "
                                    "not '") << dest_file_ << "' (bad file extension)";
            }
        }

        void enteringPhase(const MemoryProfiler::Phase phase)
        {
            if (!tracking_(phase)) {
                return;
            }
            if (phase == MemoryProfiler::Phase::Simulate) {
                startUpdateTrigger_();
            }
            current_phase_ = phase;
        }

        void exitingPhase(const MemoryProfiler::Phase phase)
        {
            if (!tracking_(phase)) {
                return;
            }
            if (phase == MemoryProfiler::Phase::Simulate) {
                stopUpdateTrigger_();
            }
            takeSnapshot_();
            current_phase_.clearValid();
        }

        void saveReport()
        {
            if (dest_file_.empty() || dest_file_ == "1") {
                writeUnformattedMemStatsToStream_(std::cout);
            } else {
                saveReportToFile_();
            }
        }

        void saveReportToStream(std::ostream & os)
        {
            writeUnformattedMemStatsToStream_(os);
        }

        void saveReportToFile_() {
            std::ofstream fout(dest_file_);

            if (!fout) {
                throw SpartaException("Unable to open output file for "
                                    "writing: '") << dest_file_ << "'";
            }

            /*
             * Memory usage reports are formatted as follows (heap
             * allocation values are in bytes):
             *
             *      # <report name>
             *      # <update type>:<update expression>
             *      # Phase,     Current,     Max
             *        Build,       12345,   12345
             *        Configure,   12675,   12675
             *        Bind,        12981,   12981
             *        Simulate,    13405,   13405
             *                     12987,   13405
             *                     14992,   14992
             *                     14560,   14992
             *                      ...      ...
             */

            //First write header information to the CSV report
            fout << "# " << report_name_ << "\n";
            fout << "# " << update_type_ << ":" << update_expression_ << "\n";
            fout << "Phase,Current,Max\n";

            for (auto & snapshots_by_phase : snapshots_by_phase_) {
                switch (snapshots_by_phase.first) {
                    case MemoryProfiler::Phase::Build: {
                        fout << "Build,";
                        break;
                    }
                    case MemoryProfiler::Phase::Configure: {
                        fout << "Configure,";
                        break;
                    }
                    case MemoryProfiler::Phase::Bind: {
                        fout << "Bind,";
                        break;
                    }
                    case MemoryProfiler::Phase::Simulate: {
                        fout << "Simulate,";
                        break;
                    }
                    default:
                        sparta_assert("Unreachable");
                        break;
                }

                bool pad_csv_cell = false;
                while (!snapshots_by_phase.second.empty()) {
                    if (pad_csv_cell) {
                        fout << ",";
                    }
                    pad_csv_cell = true;

                    const auto & snapshot = snapshots_by_phase.second.front();
                    fout << snapshot.first << "," << snapshot.second << "\n";
                    snapshots_by_phase.second.pop();
                }
            }

            std::cout << "  [profile] Wrote memory usage report to \"" << dest_file_ << "\"\n";
        }

        void writeUnformattedMemStatsToStream_(std::ostream & os) {
            /*
             * Memory usage reports are formatted as follows (heap
             * allocation values are in bytes):
             *
             *      # <report name>
             *      # <update type>:<update expression>
             *      # Phase       Current   Max
             *        Build       12345     12345
             *        Configure   12675     12675
             *        Bind        12981     12981
             *        Simulate    13405     13405
             *                    12987     13405
             *                    14992     14992
             *                    14560     14992
             *                     ...       ...
             */

            //First write header information to the console
            os << "# " << report_name_ << "\n";
            os << "# " << update_type_ << ":" << update_expression_ << "\n";
            os << "# Phase          Current     Max\n";

            for (auto & snapshots_by_phase : snapshots_by_phase_) {
                os << std::left << std::setfill(' ') << std::setw(17);
                switch (snapshots_by_phase.first) {
                    case MemoryProfiler::Phase::Build: {
                        os << "  Build        ";
                        break;
                    }
                    case MemoryProfiler::Phase::Configure: {
                        os << "  Configure    ";
                        break;
                    }
                    case MemoryProfiler::Phase::Bind: {
                        os << "  Bind         ";
                        break;
                    }
                    case MemoryProfiler::Phase::Simulate: {
                        os << "  Simulate     ";
                        break;
                    }
                    default:
                        sparta_assert("Unreachable");
                        break;
                }

                bool pad_whitespace = false;
                while (!snapshots_by_phase.second.empty()) {
                    if (pad_whitespace) {
                        os << "                 ";
                    }
                    pad_whitespace = true;

                    const auto & snapshot = snapshots_by_phase.second.front();
                    os << std::left << std::setfill(' ') << std::setw(12);
                    os << snapshot.first;
                    os << std::left << std::setfill(' ') << std::setw(12);
                    os << snapshot.second << "\n";
                    snapshots_by_phase.second.pop();
                }
            }
        }

    private:
        void startUpdateTrigger_() {
            auto configure = [&](TreeNode * context) {
                auto cb = CREATE_SPARTA_HANDLER(ProfileRuntime, takeSnapshot_);
                action_on_update_ = RescheduleActionOnUpdate::Reschedule;

                if (update_type_ == "update-count") {
                    update_trigger_.reset(new trigger::ExpressionCounterTrigger(
                        "MemorySnapshot", cb, update_expression_, 0, context));
                }

                else if (update_type_ == "update-cycles") {
                    update_trigger_.reset(new trigger::ExpressionCycleTrigger(
                        "MemorySnapshot", cb, update_expression_, context));
                }

                else if (update_type_ == "update-time") {
                    update_trigger_.reset(new trigger::ExpressionTimeTrigger(
                        "MemorySnapshot", cb, update_expression_, context));
                }

                else if (update_type_ == "update-whenever") {
                    update_trigger_.reset(new trigger::ExpressionTrigger(
                        "MemorySnapshot", cb, update_expression_, context, nullptr));

                    const auto & internals = update_trigger_->getInternals();
                    if (internals.num_counter_triggers_ > 0 ||
                        internals.num_cycle_triggers_ > 0 ||
                        internals.num_time_triggers_ > 0) {
                        throw SpartaException(
                            "Only 'notif.*' triggers are allowed in 'update-whenever' expressions");
                    }

                    action_on_update_ = RescheduleActionOnUpdate::StayActive;
                }

                else {
                    throw SpartaException("Unrecognized memory profile update type "
                                        "found: '") << update_type_ << "'";
                }
            };

            try {
                configure(context_);
            } catch (...) {
                configure(sim_->getRoot()->getSearchScope());
            }
        }

        void stopUpdateTrigger_() {
            update_trigger_.reset();
        }

        inline bool tracking_(const MemoryProfiler::Phase phase) const {
            return (phases_.empty() || phases_.find(phase) != phases_.end());
        }

        void takeSnapshot_() {
            //Use gperftools malloc extension to request the current allocated bytes
            static const char CURRENT_ALLOC[] = "generic.current_allocated_bytes";

            size_t allocated_bytes = 0;
            MallocExtension::instance()->GetNumericProperty(CURRENT_ALLOC, &allocated_bytes);

            //For each snapshot, keep track of the current allocated bytes, as well as the
            //maximum allocated bytes (running max over all snapshots)
            if (!max_heap_bytes_.isValid()) {
                max_heap_bytes_ = allocated_bytes;
            } else {
                max_heap_bytes_ = std::max(max_heap_bytes_.getValue(), allocated_bytes);
            }

            //Separate snapshots by simulation phase so the report stays organized and clear
            snapshots_by_phase_[current_phase_].push(
                std::make_pair(allocated_bytes, max_heap_bytes_.getValue()));

            if (update_trigger_ == nullptr) {
                return;
            }

            //Reschedule the update trigger to keep getting calls to this 'takeSnapshot_()'
            //method. This will continue to get hit over and over until the end of the simulate
            //phase.
            if (action_on_update_ == RescheduleActionOnUpdate::Reschedule) {
                update_trigger_->reschedule();
            } else if (action_on_update_ == RescheduleActionOnUpdate::StayActive) {
                update_trigger_->stayActive();
                update_trigger_->awaken();
            }
        }

        std::string report_name_;
        std::string dest_file_;
        std::set<MemoryProfiler::Phase> phases_;
        utils::ValidValue<MemoryProfiler::Phase> current_phase_;

        std::string update_type_;
        std::string update_expression_;
        std::unique_ptr<trigger::ExpressionTrigger> update_trigger_;
        TreeNode * context_ = nullptr;
        app::Simulation * sim_ = nullptr;

        enum class RescheduleActionOnUpdate {
            Reschedule,
            StayActive
        };
        utils::ValidValue<RescheduleActionOnUpdate> action_on_update_;

        typedef std::pair<size_t, size_t> HeapUsageSnapshot;
        utils::ValidValue<size_t> max_heap_bytes_;
        std::map<MemoryProfiler::Phase, std::queue<HeapUsageSnapshot>> snapshots_by_phase_;
    };

    void setDefaults_(TreeNode * context, app::Simulation * sim) {
        ProfilerConfig cfg(
            "Memory usage statistics", "mem-stats.csv", {},
            "update-cycles", "10k");
        prepareProfiler_({cfg}, context, sim);
    }

    void setDefaultsForStdoutDump_(TreeNode * context, app::Simulation * sim) {
        ProfilerConfig cfg(
            "Memory usage statistics", "", {},
            "update-cycles", "10k");
        prepareProfiler_({cfg}, context, sim);
    }

    void prepareProfiler_(const ProfilerConfigurations & configs,
                          TreeNode * context,
                          app::Simulation * sim) {
        for (const auto & cfg : configs) {
            profilers_.emplace_back(cfg, context, sim);
        }
    }

    std::vector<ProfileRuntime> profilers_;
};

MemoryProfiler::MemoryProfiler(const std::string & def_file,
                               TreeNode * context,
                               app::Simulation * sim) :
    impl_(new MemoryProfiler::Impl(def_file, context, sim))
{
}

void MemoryProfiler::enteringPhase(const Phase phase)
{
    impl_->enteringPhase(phase);
}

void MemoryProfiler::exitingPhase(const Phase phase)
{
    impl_->exitingPhase(phase);
}

void MemoryProfiler::saveReport()
{
    impl_->saveReport();
}

void MemoryProfiler::saveReportToStream(std::ostream & os)
{
    impl_->saveReportToStream(os);
}

}
