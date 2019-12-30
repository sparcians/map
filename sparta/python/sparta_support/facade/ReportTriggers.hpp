
#ifndef __PYTHON_FACADE_REPORT_TRIGGERS_H__
#define __PYTHON_FACADE_REPORT_TRIGGERS_H__

#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/utils/SpartaException.hpp"

#include <boost/python.hpp>
#include <boost/python/ptr.hpp>

namespace sparta {
namespace facade {

class ReportTriggers;

//Utility to help with pretty print indentation
auto indent = [](const size_t num_indents) {
    return std::string(num_indents, ' ');
};

/*!
 * \brief Helper class used to make report trigger configuration
 * easier in SPARTA's Python shell
 */
class ReportTrigger
{
public:
    //Enum containing the trigger types available
    enum class Type {
        START,
        UPDATE_COUNT,
        UPDATE_CYCLES,
        UPDATE_TIME,
        STOP
    };

    ReportTrigger(const Type type,
                  const std::string & expression,
                  facade::ReportTriggers * container) :
        expression_(expression),
        type_(type),
        container_(container)
    {}

    ReportTrigger(const ReportTrigger & rhs) = default;

    //Main trigger expression, for example:
    //  'top.core0.rob.stats.total_number_retired >= 2500'
    const std::string & getExpression() const {
        return expression_;
    }

    //Enable or disable a trigger object without physically
    //removing it from the ReportTriggers object that it
    //belongs to
    void enable();
    void disable();
    bool isEnabled() const;

    //Get yaml equivalent string of a particular trigger type
    //   Type::START         ->  "start"
    //   Type::UPDATE_COUNT  ->  "update-count"
    //   ...
    static const std::string & getSerializationKey(const ReportTrigger::Type type) {
        static const std::map<Type, std::string> trigger_type_map = {
            { ReportTrigger::Type::START, "start" },
            { ReportTrigger::Type::UPDATE_COUNT, "update-count" },
            { ReportTrigger::Type::UPDATE_CYCLES, "update-cycles" },
            { ReportTrigger::Type::UPDATE_TIME, "update-time" },
            { ReportTrigger::Type::STOP, "stop" }
        };

        auto iter = trigger_type_map.find(type);
        sparta_assert(iter != trigger_type_map.end());
        return iter->second;
    }

    //Return yaml equivalent string of our trigger type
    const std::string & getSerializationKey() const {
        return ReportTrigger::getSerializationKey(type_);
    }

    //Return type of trigger (start, update-count, stop, etc.)
    Type getType() const {
        return type_;
    }

    //Pretty print information about this trigger
    void showInfo() const {
        showInfoWithIndentation(0);
    }

    void showInfoWithIndentation(const size_t num_indents) const {
        std::ostringstream info;
        info << indent(num_indents) << std::setw(20) << std::left << "Expression:"
             << "'" << expression_ << "'\n";
        info << indent(num_indents) << std::setw(20) << std::left << "Type: "
             << getSerializationKey() << "\n";
        std::cout << info.str() << std::endl;
    }

    void lockFurtherChanges() {
        locked_ = true;
    }

private:
    const std::string expression_;
    const Type type_;
    facade::ReportTriggers * container_ = nullptr;
    bool locked_ = false;
};

/*!
 * \brief Helper class used to make report trigger creation and
 * configuration easier in SPARTA's Python shell
 */
class ReportTriggers
{
public:
    explicit ReportTriggers(app::NamedExtensions & desc_extensions) :
        desc_extensions_(desc_extensions)
    {
        //Autopopulate any triggers that are already present in
        //the report yaml file
        auto iter = desc_extensions_.find("trigger");
        if (iter != desc_extensions_.end()) {
            const app::TriggerKeyValues & trigger_map =
                boost::any_cast<app::TriggerKeyValues&>(
                    iter->second);

            for (const auto & kv : trigger_map) {
                utils::ValidValue<ReportTrigger::Type> type;
                utils::ValidValue<std::string> expression;

                if (kv.first == "start") {
                    type = ReportTrigger::Type::START;
                } else if (kv.first == "update-count") {
                    type = ReportTrigger::Type::UPDATE_COUNT;
                } else if (kv.first == "update-cycles") {
                    type = ReportTrigger::Type::UPDATE_CYCLES;
                } else if (kv.first == "update-time") {
                    type = ReportTrigger::Type::UPDATE_TIME;
                } else if (kv.first == "stop") {
                    type = ReportTrigger::Type::STOP;
                } else {
                    //The trigger type given in the yaml file is not recognized,
                    //but let's hold off on throwing an exception. Just because
                    //we don't recognize it does not mean that ReportRepository
                    //won't either - let that class have the final say on whether
                    //this should throw an exception.
                    std::cout << "WARNING - Unrecognized trigger type encountered "
                              << "while creating a sparta.ReportTriggers object: "
                              << kv.first << std::endl;
                    continue;
                }

                expression = kv.second;
                addTriggerByTypeAndExpression(type, expression);
            }
        }
    }

    ReportTriggers(const ReportTriggers & rhs) = default;

    //Keyword arguments 'addTrigger()' method. The various uses are:
    //
    //   trig = triggers.addTrigger(start='top.core0...')
    //   trig = triggers.addTrigger(update_count='top.core0...')
    //   trig = triggers.addTrigger(update_cycles='1500')
    //   ...
    //
    //And so on. Note that this is the only addTrigger() method that
    //is exposed to Python.
    static boost::python::object addTrigger(boost::python::tuple args,
                                            boost::python::dict kwargs)
    {
        ReportTriggers & self = boost::python::extract<ReportTriggers&>(args[0]);
        args = boost::python::tuple(args.slice(1, boost::python::_));

        utils::ValidValue<ReportTrigger::Type> type;
        utils::ValidValue<std::string> expression;

        //Parse kwargs arguments
        boost::python::list keys = kwargs.keys();
        assertLengthOfKeysEquals_(keys, 1);

        boost::python::extract<std::string> extracted_key(keys[0]);
        if (!extracted_key.check()) {
            throw SpartaException(
                "Invalid Python dictionary key encountered while evaluating "
                "sparta.ReportTriggers.addTrigger() method call: ") << keys[0];
        }
        const std::string key = extracted_key;

        //Get the trigger type and expression from this key
        if (key == "start") {
            type = ReportTrigger::Type::START;
        } else if (key == "update_count") {
            type = ReportTrigger::Type::UPDATE_COUNT;
        } else if (key == "update_cycles") {
            type = ReportTrigger::Type::UPDATE_CYCLES;
        } else if (key == "update_time") {
            type = ReportTrigger::Type::UPDATE_TIME;
        } else if (key == "stop") {
            type = ReportTrigger::Type::STOP;
        } else {
            throw SpartaException(
                "Invalid Python dictionary key encountered while "
                "evaluating a sparta.ReportReportTriggers.addTrigger() "
                "method call: ") << key;
        }

        boost::python::extract<std::string> extracted_expression(kwargs[key]);
        if (!extracted_expression.check()) {
            throw SpartaException(
                "Invalid Python dictionary value encountered while "
                "evaluating sparta.ReportTriggers.addTrigger() method "
                "call: ") << kwargs[key];
        }
        expression = static_cast<std::string>(extracted_expression);

        sparta_assert(type.isValid());
        sparta_assert(expression.isValid());

        //Create the trigger and return it as a boost::python::object,
        //but don't add it to the WrapperCache. These trigger objects
        //are basically just user-friendly back pointers to the actual
        //app::ReportDescriptor we are configuring in Python.
        ReportTrigger * trigger = self.addTriggerByTypeAndExpression(
            type, expression);

        return boost::python::object(boost::python::ptr(trigger));
    }

    //This addTrigger() method is called from the raw_function (kwargs)
    //version of addTrigger() above. Note that this is not exposed to
    //Python directly - you have to go through the kwargs overload in
    //order to instantiate a sparta.ReportTrigger in Python.
    ReportTrigger * addTriggerByTypeAndExpression(
        const ReportTrigger::Type type,
        const std::string & expression)
    {
        if (locked_) {
            throw SpartaException("Triggers can no longer be changed");
        }

        if (desc_extensions_.find("trigger") == desc_extensions_.end()) {
            app::TriggerKeyValues empty_map;
            desc_extensions_["trigger"] = empty_map;
        }

        //Get a reference to the ReportDescriptor's extensions map
        app::TriggerKeyValues & trigger_map =
            boost::any_cast<app::TriggerKeyValues&>(
                desc_extensions_["trigger"]);

        //Keep in mind that this trigger collection can only have at most
        //one of each: start, update, and stop. We should clear out any
        //previously created triggers now if needed.
        auto clear_triggers = [&](const ReportTrigger::Type type) {
            const bool exists =
                enabled_triggers_.find(type)  != enabled_triggers_.end() ||
                disabled_triggers_.find(type) != disabled_triggers_.end();

            if (exists) {
                const std::string & existing_trigger_type_str =
                    ReportTrigger::getSerializationKey(type);

                std::cout << "A trigger of type '" << existing_trigger_type_str
                          << "' exists and will be removed." << std::endl;

                enabled_triggers_.erase(type);
                disabled_triggers_.erase(type);
                trigger_map.erase(existing_trigger_type_str);
            }
        };

        switch (type) {
        case ReportTrigger::Type::START:
        case ReportTrigger::Type::STOP: {
            clear_triggers(type);
            break;
        }
        case ReportTrigger::Type::UPDATE_COUNT:
        case ReportTrigger::Type::UPDATE_CYCLES:
        case ReportTrigger::Type::UPDATE_TIME: {
            clear_triggers(ReportTrigger::Type::UPDATE_COUNT);
            clear_triggers(ReportTrigger::Type::UPDATE_CYCLES);
            clear_triggers(ReportTrigger::Type::UPDATE_TIME);
            break;
        }
        }

        // Get the yaml key for this type of trigger ('start', 'update-time', etc.)
        const std::string & type_str = ReportTrigger::getSerializationKey(type);

        //Add / overwrite the expression for this type of trigger
        trigger_map[type_str] = expression;

        //Hold onto the newly created trigger. We give the trigger a back
        //pointer to 'this' object so it can inform us if Python users call
        //methods like 'enable()' or 'disable()' directly on the ReportTrigger
        //object. We always want the descriptor's extensions map to be up to
        //date whenever methods are called that change trigger state.
        auto new_trigger = std::make_shared<ReportTrigger>(type, expression, this);
        enabled_triggers_[type] = new_trigger;

        return new_trigger.get();
    }

    ReportTrigger * getStartTrigger() const {
        auto iter = enabled_triggers_.find(ReportTrigger::Type::START);
        return iter != enabled_triggers_.end() ? iter->second.get() : nullptr;
    }

    ReportTrigger * getUpdateTrigger() const {
        //There is no valid use case where we have more than one update trigger,
        //so we check for all update trigger types before returning anything.
        ReportTrigger * trigger = nullptr;
        size_t num_update_triggers = 0;

        auto iter = enabled_triggers_.find(ReportTrigger::Type::UPDATE_COUNT);
        if (iter != enabled_triggers_.end()) {
            trigger = iter->second.get();
            ++num_update_triggers;
        }

        sparta_assert(num_update_triggers <= 1);
        iter = enabled_triggers_.find(ReportTrigger::Type::UPDATE_CYCLES);
        if (iter != enabled_triggers_.end()) {
            trigger = iter->second.get();
            ++num_update_triggers;
        }

        sparta_assert(num_update_triggers <= 1);
        iter = enabled_triggers_.find(ReportTrigger::Type::UPDATE_TIME);
        if (iter != enabled_triggers_.end()) {
            trigger = iter->second.get();
            ++num_update_triggers;
        }

        sparta_assert(num_update_triggers <= 1);
        return trigger;
    }

    ReportTrigger * getStopTrigger() const {
        auto iter = enabled_triggers_.find(ReportTrigger::Type::STOP);
        return iter != enabled_triggers_.end() ? iter->second.get() : nullptr;
    }

    void enable(const ReportTrigger::Type type) {
        if (locked_) {
            throw SpartaException("Triggers can no longer be changed");
        }
        auto iter = disabled_triggers_.find(type);
        if (iter != disabled_triggers_.end()) {
            enabled_triggers_[type] = iter->second;
            disabled_triggers_.erase(type);
        }
    }

    void disable(const ReportTrigger::Type type) {
        if (locked_) {
            throw SpartaException("Triggers can no longer be changed");
        }
        auto iter = enabled_triggers_.find(type);
        if (iter != enabled_triggers_.end()) {
            disabled_triggers_[type] = iter->second;
            enabled_triggers_.erase(type);
        }
    }

    bool isEnabled(const ReportTrigger::Type type) const {
        return enabled_triggers_.find(type) != enabled_triggers_.end();
    }

    static std::string getNoTriggersMessage(const size_t num_indents = 0) {
        return indent(num_indents) + "No triggers have been set.\n";
    }

    //Pretty print information about these triggers
    void showInfo() const {
        showInfoWithIndentation(0);
    }

    void showInfoWithIndentation(const size_t num_indents) const {
        if (!anyTriggerExists_()) {
            std::cout << getNoTriggersMessage(num_indents+1) << std::endl;
            return;
        }

        auto start_trigger = getStartTrigger();
        if (start_trigger) {
            std::cout << indent(num_indents) << "Start:\n";
            start_trigger->showInfoWithIndentation(num_indents+1);
        }

        auto update_trigger = getUpdateTrigger();
        if (update_trigger) {
            std::cout << indent(num_indents) << "Update:\n";
            update_trigger->showInfoWithIndentation(num_indents+1);
        }

        auto stop_trigger = getStopTrigger();
        if (stop_trigger) {
            std::cout << indent(num_indents) << "Stop:\n";
            stop_trigger->showInfoWithIndentation(num_indents+1);
        }

        //If we only have disabled triggers, we should still print
        //some message saying so
        if (!start_trigger && !update_trigger && !stop_trigger) {
            std::cout << indent(num_indents)
                      << "(all triggers have been disabled)"
                      << std::endl;
        }
    }

    //Turn all of the internal triggers into a map<k,v> where:
    //  key   (string) = yaml keyword specifying trigger type
    //  value (string) = trigger expression
    //
    //Examples of YAML keywords for trigger types include:
    //  "start"
    //  "update-count"
    //  "update-cycles"
    //  "update-time"
    //  "stop"
    app::TriggerKeyValues getTriggerMap() const {
        app::TriggerKeyValues trigger_map;

        auto iter = desc_extensions_.find("trigger");
        if (iter != desc_extensions_.end()) {
            trigger_map = boost::any_cast<app::TriggerKeyValues>(
                iter->second);
        }

        return trigger_map;
    }

    void lockFurtherChanges() {
        locked_ = true;
        auto start_trigger = getStartTrigger();
        if (start_trigger) {
            start_trigger->lockFurtherChanges();
        }
        auto update_trigger = getUpdateTrigger();
        if (update_trigger) {
            update_trigger->lockFurtherChanges();
        }
        auto stop_trigger = getStopTrigger();
        if (stop_trigger) {
            stop_trigger->lockFurtherChanges();
        }
    }

private:
    bool anyTriggerExists_() const {
        auto iter = desc_extensions_.find("trigger");
        if (iter == desc_extensions_.end()) {
            return false;
        }

        //We should never have a "trigger" extension with nothing in it
        sparta_assert(!iter->second.empty());
        return true;
    }

    static void assertLengthOfKeysEquals_(const boost::python::list & keys,
                                          const int len)
    {
        if (boost::python::len(keys) != len) {
            throw SpartaException(
                "Incorrect number of arguments. The way to call this method is:  \n"
                "    trigger = <obj>.addTrigger(<trigger_type>=<expression>)     \n"
                "Where:                                                          \n"
                "    <obj> is your sparta.ReportTriggers object                    \n"
                "    <trigger_type> is the start/update/stop trigger you want:   \n"
                "        start                                                   \n"
                "        update_count                                            \n"
                "        update_cycles                                           \n"
                "        update_time                                             \n"
                "        stop                                                    \n"
                "        ...                                                     \n"
                "And <expression> is a string expression such as:                \n"
                "    'top.core0.rob.stats.total_number_retired >= 1000'          \n"
                "    '150 ns'                                                    \n"
                "    ...");
        }
    }

    app::NamedExtensions & desc_extensions_;

    std::unordered_map<
        ReportTrigger::Type,
        std::shared_ptr<ReportTrigger>> enabled_triggers_;

    std::unordered_map<
        ReportTrigger::Type,
        std::shared_ptr<ReportTrigger>> disabled_triggers_;

    bool locked_ = false;
};

inline void ReportTrigger::enable()
{
    if (locked_) {
        throw SpartaException("Triggers can no longer be changed");
    }
    if (isEnabled()) {
        std::cout << "Trigger is already enabled." << std::endl;
        return;
    }
    container_->enable(type_);
}

inline void ReportTrigger::disable()
{
    if (locked_) {
        throw SpartaException("Triggers can no longer be changed");
    }
    if (!isEnabled()) {
        std::cout << "Trigger is already disabled." << std::endl;
        return;
    }
    container_->disable(type_);
}

inline bool ReportTrigger::isEnabled() const
{
    return container_->isEnabled(type_);
}

} // namespace facade
} // namespace sparta

#endif
