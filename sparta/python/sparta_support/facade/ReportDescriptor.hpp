
#ifndef __PYTHON_FACADE_REPORT_DESCRIPTOR_H__
#define __PYTHON_FACADE_REPORT_DESCRIPTOR_H__

#include "sparta/app/ReportDescriptor.hpp"
#include "python/sparta_support/facade/ReportTriggers.hpp"

/*!
 * \brief Expose the sparta::app::ReportDescriptor class to Python.
 * There is no "facade" version of this class, but there are report
 * trigger extensions that are kept in the descriptors's boost::any
 * map. The facade::ReportTrigger(s) classes are just user-friendly
 * wrappers around that boost::any map;
 */
namespace sparta {
namespace facade {

//! Get the ReportTriggers container for an app::ReportDescriptor
//! object, creating a new (empty) one if needed.
inline facade::ReportTriggers * getTriggers(app::ReportDescriptor * rd)
{
    if (!rd->isEnabled()) {
        std::cout << "This descriptor has been disabled" << std::endl;
        return nullptr;
    }

    std::shared_ptr<facade::ReportTriggers> trigger_extensions;
    auto iter = rd->extensions_.find("python-triggers");
    if (iter == rd->extensions_.end()) {
        trigger_extensions.reset(new facade::ReportTriggers(rd->extensions_));
        rd->extensions_["python-triggers"] = trigger_extensions;
    } else {
        trigger_extensions = boost::any_cast<std::shared_ptr<
            facade::ReportTriggers>>(iter->second);
    }

    sparta_assert(trigger_extensions != nullptr);
    return trigger_extensions.get();
}

//! Pretty print information about an app::ReportDescriptor
inline void showReportDescriptorInfo(const app::ReportDescriptor * rd)
{
    if (!rd->isEnabled()) {
        std::cout << "This descriptor has been disabled" << std::endl;
        return;
    }

    std::cout << "Descriptor information:\n";
    std::cout << indent(2) << rd->stringize() << "\n\n";
    std::cout << "Trigger information:\n";

    auto iter = rd->extensions_.find("python-triggers");
    if (iter == rd->extensions_.end()) {
        //We don't have any triggers created from Python, but we
        //may have triggers that were already present in a yaml
        //file. The sparta.ReportTriggers class can help us parse
        //the descriptor extensions for any yaml-created triggers.
        auto extensions = rd->extensions_;
        facade::ReportTriggers show_info_triggers(extensions);
        show_info_triggers.showInfoWithIndentation(1);
    } else {
        const std::shared_ptr<facade::ReportTriggers> & trigger_extensions =
            boost::any_cast<const std::shared_ptr<facade::ReportTriggers>&>(
                iter->second);

        //If 'python-triggers' exists in the map, that means it
        //was set in module_sparta, and should never be null
        sparta_assert(trigger_extensions != nullptr);
        trigger_extensions->showInfoWithIndentation(1);
    }
}

//! Print out the YAML equivalent of this app::ReportDescriptor.
//! This is useful if you want to quickly prototype some report
//! trigger configurations in Python, and then "finalize" it into
//! YAML that you can copy and paste into a report YAML file.
inline void serializeDescriptorYamlToOStream(app::ReportDescriptor * rd,
                                             std::ostream & os)
{
    if (!rd->isEnabled()) {
        os << "This descriptor has been disabled" << std::endl;
        return;
    }

    os << indent(2) <<   "report:                                \n"
       << indent(4) <<     "pattern:   " << rd->loc_pattern << " \n"
       << indent(4) <<     "def_file:  " << rd->def_file    << " \n"
       << indent(4) <<     "dest_file: " << rd->dest_file   << " \n"
       << indent(4) <<     "format:    " << rd->format      << " \n";
    //                      trigger:
    //                        start: <my start expression>
    //                        update-count: <my update expression>
    //                        ...

    const facade::ReportTriggers * triggers = getTriggers(rd);
    sparta_assert(triggers);

    auto trigger_map = triggers->getTriggerMap();
    if (!trigger_map.empty()) {
        os << indent(4) << "trigger:\n";
    }
    for (const auto & trigger_kv : trigger_map) {
        os << indent(6)
           << trigger_kv.first << ": "
           << trigger_kv.second << "\n";
    }
}

//! Print out the YAML equivalent of this app::ReportDescriptor to stdout
inline void serializeDescriptorToYaml(app::ReportDescriptor * rd)
{
    serializeDescriptorYamlToOStream(rd, std::cout);
}

} // namespace facade
} // namespace sparta

//! Boost.Python constructor with keyword arguments
namespace boost {
namespace python {

inline object ReportDescriptor_ctor_with_kwargs(tuple args, const dict & kwargs)
{
    object self = args[0];
    args = tuple(args.slice(1, _));

    //TODO: We need a more robust way to protect against this code
    //from getting called during erroneous constructor calls such as:
    //
    //  rd = sparta.ReportDescriptor(True)     // Does not match any init<>,
    //                                       // so that should be a Python
    //                                       // error immediately
    sparta_assert(len(args) == 0);

    std::string pattern, def_file, dest_file, format;

    //Parse kwargs arguments
    list keys = kwargs.keys();

    for (int kw_idx = 0; kw_idx < len(keys); ++kw_idx) {
        //Get the key as a string
        extract<std::string> extracted_key(keys[kw_idx]);
        if (!extracted_key.check()) {
            throw sparta::SpartaException(
                "Invalid Python dictionary key encountered while "
                "evaluating a sparta.ReportDescriptor constructor call: ") << keys[kw_idx];
        }
        const std::string key = extracted_key;

        //Get the value from the key
        if (key == "pattern") {
            extract<std::string> extracted_val(kwargs[key]);
            if (!extracted_val.check()) {
                throw sparta::SpartaException(
                    "Invalid Python dictionary value encountered while "
                    "evaluating a sparta.ReportDescriptor constructor call for "
                    "the 'pattern' argument: ") << kwargs[key];
            }
            pattern = extracted_val;
        } else if (key == "def_file") {
            extract<std::string> extracted_val(kwargs[key]);
            if (!extracted_val.check()) {
                throw sparta::SpartaException(
                    "Invalid Python dictionary value encountered while "
                    "evaluating a sparta.ReportDescriptor constructor call for "
                    "the 'def_file' argument: ") << kwargs[key];
            }
            def_file = extracted_val;
        } else if (key == "dest_file") {
            extract<std::string> extracted_val(kwargs[key]);
            if (!extracted_val.check()) {
                throw sparta::SpartaException(
                    "Invalid Python dictionary value encountered while "
                    "evaluating a sparta.ReportDescriptor constructor call for "
                    "the 'dest_file' argument: ") << kwargs[key];
            }
            dest_file = extracted_val;
        } else if (key == "format") {
            extract<std::string> extracted_val(kwargs[key]);
            if (!extracted_val.check()) {
                throw sparta::SpartaException(
                    "Invalid Python dictionary value encountered while "
                    "evaluating a sparta.ReportDescriptor constructor call for "
                    "the 'format' argument: ") << kwargs[key];
            }
            format = extracted_val;
        } else {
            throw sparta::SpartaException(
                "Invalid Python dictionary key encountered while evaluating "
                "a sparta.ReportDescriptor constructor call: ") << key;
        }
    }

    //Create and finalize object
    return self.attr("__init__")(pattern, def_file, dest_file, format);
}

} // namespace python
} // namespace boost

#endif
