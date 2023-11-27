// <module_sparta> -*- C++ -*-


/*!
 * \file module_sparta.h
 * \brief "sparta" python module
 */

#pragma once


#include "sparta/sparta.hpp" // For global defines
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/Parameter.hpp"


#include <boost/python.hpp>
#include <boost/python/ptr.hpp>
#include <boost/ref.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "python/sparta_support/facade/ReportDescriptor.hpp"
#include "python/sparta_support/facade/ReportTriggers.hpp"
#include "sparta/statistics/dispatch/archives/StatisticsArchives.hpp"
#include "sparta/statistics/dispatch/streams/StatisticsStreams.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/dynamic_pipeline/GenericUnit.hpp"
#include "sparta/dynamic_pipeline/GenericResourceFactory.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticDef.hpp"

namespace sparta {
    namespace app {
        class Simulation;
    }
}

namespace bp = boost::python;

/*!
 * \brief Placeholder type for sparta node attributes until they are actually
 * requested
 */
class PlaceholderObject {
public:
    PlaceholderObject() = default;
    PlaceholderObject(PlaceholderObject&&) = delete;
    PlaceholderObject(const PlaceholderObject&) = delete;
};

/*!
 * \brief Static map
 * \note Lifetime is probably unimportant since python will not be initialized
 * until PythonInterpreter is created long after staticialization init is
 * complete
 */
class WrapperMap {
public:
    static std::unordered_map<const void*, bp::object> wrapper_map_;
};

/*!
 * \brief Templated helper for generating new wrappers
 */
template <typename T, typename Enable=void>
struct casting_wrapper {
    bp::object new_wrapper(const T* n) const;
    void prepopulate(const T* n, bp::object& obj) const;
};

/*!
 * \brief Templated cache for wrapper instances to be shared by any boost python
 * wrapped function that is returning a sparta pointer that needs be wrapped and
 * could re-use an existing wrapper (same object).
 *
 * Can be registered with a boost python return_value_policy in a function
 * wrapper. Can also be used to manually get a wrapper for some object.
 *
 * Contains a static (templated) map of sparta object pointers to python object
 * references which wrap those pointers.
 *
 * \todo Should consider switching to python weakref.ref objects instead of
 * strong references. Then the map could save memory by purging wrappers
 *
 * Example:
 * \code
 * // A) In function wrapper
 def("getXObject", &getXObject,
 return_value_policy<WrapperCache<sparta::XObjectType>>());

 * // B) Manually (i.e. for publishing an object to the global ns)
 * // given: sparta::XObjectType* px
 * global_ns_dict["my_x_obj"] = WrapperCache::wrap(px); // Infers SpartaObjT
 * \endcode
 */
template<typename SpartaObjT>
struct WrapperCache {

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static constexpr bool convertible() {
                return std::is_same<T, SpartaObjT*>::value or
                    std::is_same<T, SpartaObjT&>::value;
            }

            /*!
             * \brief Perform the wrapping
             */
            PyObject* operator()(const SpartaObjT* n) const {
                auto obj = wrap(n);
                return bp::incref(obj.ptr());
            }

            /*!
             * \brief Perform the wrapping, Overload for non-pointer type.
             */
            PyObject* operator()(const SpartaObjT& n) const {
                return operator()(std::addressof(n));
            }

            const PyTypeObject* get_pytype() const { return 0; }
        };
    };


    /*!
     * \brief Access the wrapper cache for SpartaObjT and return an existing
     * wrapper if one already exists - or a new one otherwise
     */
    static bp::object wrap(const SpartaObjT* n) {
        if(n == nullptr){
            return bp::object(bp::borrowed(Py_None));
        }
        // The wrapper map is a typeless map, but boost appears to be smart enough to use RTTI
        // to figure out the actual subclass and generate the appropriate wrapper for it, which
        // means that there should be no problem where class b is wrapped usign a pointer to
        // it's base classs A when it is actually an instance of B. The wrapper will be generated
        // for B.
        auto result = WrapperMap::wrapper_map_.find(static_cast<const void*>(n));
        if(result == WrapperMap::wrapper_map_.end()){
            casting_wrapper<SpartaObjT> cw;
            auto obj = cw.new_wrapper(n);

            // First, add to wrapper map
            WrapperMap::wrapper_map_[n] = obj;
            // TODO: use weak refers or proxies in the map: PyWeakref_NewRef, PyWeakref_NewProxy

            // After the node is available in the wrapper map, try pre-populating with
            // tree-related attributes such as children

            cw.prepopulate(n, obj);

            return obj;
        }else{
            return result->second;
        }
    }

    /*!
     * \brief Access the wrapper cache for SpartaObjT and return an existing
     * wrapper if one already exists - or a new one otherwise, Overload for non-pointer type.
     */
    static bp::object wrap(const SpartaObjT& n) {
        return wrap(std::addressof(n));
    }

    /*!
     * \brief Access the wrapper cache for SpartaObjT and raise an exception if it doesn't already
     exist
    */
    static bp::object get_wrapper(const SpartaObjT* n) {
        // The wrapper map is a typeless map, but boost appears to be smart enough to use RTTI
        // to figure out the actual subclass and generate the appropriate wrapper for it, which
        // means that there should be no problem where class b is wrapped using a pointer to
        // it's base classs A when it is actually an instance of B. The wrapper will be generated
        // for B.
        if(n == nullptr){
            return bp::object(bp::borrowed(Py_None));
        }
        auto result = WrapperMap::wrapper_map_.find(static_cast<const void*>(n));
        if(result == WrapperMap::wrapper_map_.end()){
            throw sparta::SpartaException("Wrapper for ") << n << " does not yet exist";
        }else{
            return result->second;
        }
    }

    /*!
     * \brief Access the wrapper cache for SpartaObjT and raise an exception if it doesn't already
     exist. Overload for non-pointer type.
    */
    static bp::object get_wrapper(const SpartaObjT& n) {
        return get_wrapper(std::addressof(n));
    }
};

/*!
 * \brief Remove any element from the wrapper cache. This will
 * return true if the element was removed, false if it was not
 * found or could not be removed for any other reason
 */
static bool removeElementFromWrapperCache(const void * n) noexcept
{
    auto iter = WrapperMap::wrapper_map_.find(n);
    if (iter != WrapperMap::wrapper_map_.end()) {
        WrapperMap::wrapper_map_.erase(iter);
        return true;
    }
    return false;
}

/*!
 * \brief Wrapper for TreeNode objects
 */
template <typename T>
struct casting_wrapper <T, typename std::enable_if<std::is_base_of<sparta::TreeNode, T>::value>::type>
{
    bp::object new_wrapper(const T* n) {
        // Help boost python by downcasting to the lowest proper type if possible before
        // wrapping. bp should do this already but support is not perfect. It appears that
        // for templated subclass instances where an immediate base class is wrapped
        // (instead of the templated inst), that the wrapper walks al the way up to TreeNode
        // to get it's wrapper instead of using the immediate baseclass.
        bp::object obj;
        if(dynamic_cast<const sparta::ParameterBase*>(n) != nullptr){
            obj = bp::object(bp::ptr(n->template getAs<const sparta::ParameterBase>()));
        }else{
            obj = bp::object(bp::ptr(n));
        }

        return obj;
    }

    void prepopulate(const T* n, bp::object& obj) const {
        bp::list members;

        // Add child nodes to wrapper as placeholders
        for(auto* c : n->getChildren()){
            //boost::shared_ptr<PlaceholderObject> ph(new PlaceholderObject());
            //obj.attr(c->getName().c_str()) = bp::ptr(ph);

            members.append(bp::str(c->getName()));
            //bp::setattr(obj,
            //            c->getName().c_str(),
            //            bp::object(bp::ptr(new PlaceholderObject())));
        }

        //! \todo: Add child groups (iterable)
        //! \todo: Add child aliases

        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for Non-TreeNode objects
 */
template <typename T>
struct casting_wrapper <T, typename std::enable_if<!std::is_base_of<sparta::TreeNode, T>::value>::type>
{
    bp::object new_wrapper(const T* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const T* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::Resource
 */
template <>
struct casting_wrapper <sparta::Resource>
{
    bp::object new_wrapper(const sparta::Resource* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::Resource* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::Unit
 */
template <>
struct casting_wrapper <sparta::Unit>
{
    bp::object new_wrapper(const sparta::Unit* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::Unit* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::dynamic_pipeline::GenericUnit::GenericUnitParameterSet
 */
template <>
struct casting_wrapper <sparta::dynamic_pipeline::GenericUnit::GenericUnitParameterSet>
{
    bp::object new_wrapper(const sparta::dynamic_pipeline::GenericUnit::GenericUnitParameterSet* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::dynamic_pipeline::GenericUnit::GenericUnitParameterSet* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::dynamic_pipeline::GenericUnit
 */
template <>
struct casting_wrapper <sparta::dynamic_pipeline::GenericUnit>
{
    bp::object new_wrapper(const sparta::dynamic_pipeline::GenericUnit* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::dynamic_pipeline::GenericUnit* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::dynamic_pipeline::GenericResourceFactory
 */
template <>
struct casting_wrapper <sparta::dynamic_pipeline::GenericResourceFactory>
{
    bp::object new_wrapper(const sparta::dynamic_pipeline::GenericResourceFactory* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::dynamic_pipeline::GenericResourceFactory* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::ResourceFactoryBase
 */
template <>
struct casting_wrapper <sparta::ResourceFactoryBase>
{
    bp::object new_wrapper(const sparta::ResourceFactoryBase* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::ResourceFactoryBase* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::GUFactory
 */
typedef sparta::dynamic_pipeline::GenericUnit GU;
typedef GU::GenericUnitParameterSet GUPS;
typedef sparta::ResourceFactory<GU, GUPS> GUFactory;
template <>
struct casting_wrapper <GUFactory>
{
    bp::object new_wrapper(const GUFactory* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const GUFactory* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::ResourceTreeNode
 */
template <>
struct casting_wrapper <sparta::ResourceTreeNode>
{
    bp::object new_wrapper(const sparta::ResourceTreeNode* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::ResourceTreeNode* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::EventSet
 */
template <>
struct casting_wrapper <sparta::EventSet>
{
    bp::object new_wrapper(const sparta::EventSet* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::EventSet* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::StatisticSet
 */
template <>
struct casting_wrapper <sparta::StatisticSet>
{
    bp::object new_wrapper(const sparta::StatisticSet* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::StatisticSet* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::PortSet
 */
template <>
struct casting_wrapper <sparta::PortSet>
{
    bp::object new_wrapper(const sparta::PortSet* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::PortSet* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::Port
 */
template <>
struct casting_wrapper <sparta::Port>
{
    bp::object new_wrapper(const sparta::Port* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::Port* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::InPort
 */
template <>
struct casting_wrapper <sparta::InPort>
{
    bp::object new_wrapper(const sparta::InPort* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::InPort* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::OutPort
 */
template <>
struct casting_wrapper <sparta::OutPort>
{
    bp::object new_wrapper(const sparta::OutPort* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::OutPort* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::InstrumentationNode
 */
template <>
struct casting_wrapper <sparta::InstrumentationNode>
{
    bp::object new_wrapper(const sparta::InstrumentationNode* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::InstrumentationNode* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::CounterBase
 */
template <>
struct casting_wrapper <sparta::CounterBase>
{
    bp::object new_wrapper(const sparta::CounterBase* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::CounterBase* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::CycleCounter
 */
template <>
struct casting_wrapper <sparta::CycleCounter>
{
    bp::object new_wrapper(const sparta::CycleCounter* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::CycleCounter* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::Counter
 */
template <>
struct casting_wrapper <sparta::Counter>
{
    bp::object new_wrapper(const sparta::Counter* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::Counter* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::StatisticDef
 */
template <>
struct casting_wrapper <sparta::StatisticDef>
{
    bp::object new_wrapper(const sparta::StatisticDef* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::StatisticDef* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::app::SimulationConfiguration
 */
template <>
struct casting_wrapper <sparta::app::SimulationConfiguration>
{
    bp::object new_wrapper(const sparta::app::SimulationConfiguration* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::app::SimulationConfiguration* n, bp::object& obj) {
        (void) n;
        (void) obj;
    }
};

/*!
 * \brief Wrapper for sparta::app::ReportDescriptorCollection
 */
template <>
struct casting_wrapper <sparta::app::ReportDescriptorCollection>
{
    bp::object new_wrapper(const sparta::app::ReportDescriptorCollection* n) const {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::app::ReportDescriptorCollection* n, bp::object& obj) {
        bp::list members;

        const std::vector<std::string> names = n->getAllDescriptorNames();
        for (const auto & name : names) {
            members.append(bp::str(name));
        }

        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for StatisticsArchives objects
 */
template <>
struct casting_wrapper <sparta::statistics::StatisticsArchives>
{
    bp::object new_wrapper(const sparta::statistics::StatisticsArchives* n) {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::statistics::StatisticsArchives* n, bp::object& obj) const {
        bp::list members;
        const std::vector<std::string> archive_names = n->getRootNames();
        for (const auto & name : archive_names) {
            //We need to replace '.' with '_' so the archives are
            //accessible via tab completion
            const std::string report_filename = name;
            std::string archive_name = report_filename;
            boost::replace_all(archive_name, ".", "_");
            n->mapRootNameToReportFilename(archive_name, report_filename);

            //Tab completion will now work. Reports with a name like
            //'out.csv' will be tab-completable to 'out_csv'
            members.append(bp::str(archive_name));
        }
        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for RootArchiveNode objects
 */
template <>
struct casting_wrapper <sparta::statistics::RootArchiveNode>
{
    bp::object new_wrapper(const sparta::statistics::RootArchiveNode* n) {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::statistics::RootArchiveNode* n, bp::object& obj) const {
        bp::list members;
        for(auto & c : n->getChildren()){
            //Ensure that there are no '.' characters in the child
            //name. This would prevent tab completion from working.
            sparta_assert(c->getName().find(".") == std::string::npos);
            members.append(bp::str(c->getName()));
        }

        auto trigger_kvs = n->tryGetMetadataValue<sparta::app::TriggerKeyValues>("trigger");
        if (trigger_kvs) {
            members.append(bp::str("triggers"));
        }
        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for ArchiveNode objects
 */
template <>
struct casting_wrapper <sparta::statistics::ArchiveNode>
{
    bp::object new_wrapper(const sparta::statistics::ArchiveNode* n) {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::statistics::ArchiveNode* n, bp::object& obj) const {
        bp::list members;
        for(auto & c : n->getChildren()){
            //Ensure that there are no '.' characters in the child
            //name. This would prevent tab completion from working.
            sparta_assert(c->getName().find(".") == std::string::npos);
            members.append(bp::str(c->getName()));
        }
        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for StatisticsStreams objects
 */
template <>
struct casting_wrapper <sparta::statistics::StatisticsStreams>
{
    bp::object new_wrapper(const sparta::statistics::StatisticsStreams * n) {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::statistics::StatisticsStreams * n, bp::object & obj) const {
        bp::list members;
        const std::vector<std::string> stream_names = n->getRootNames();
        for (const auto & name : stream_names) {
            //We need to replace '.' with '_' so the streams are
            //accessible via tab completion
            const std::string report_filename = name;
            std::string stream_name = report_filename;
            boost::replace_all(stream_name, ".", "_");
            n->mapRootNameToReportFilename(stream_name, report_filename);

            //Tab completion will now work. Reports with a name like
            //'out.csv' will be tab-completable to 'out_csv'
            members.append(bp::str(stream_name));
        }
        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for StreamNode objects
 */
template <>
struct casting_wrapper <sparta::statistics::StreamNode>
{
    bp::object new_wrapper(const sparta::statistics::StreamNode * n) {
        return bp::object(bp::ptr(n));
    }

    void prepopulate(const sparta::statistics::StreamNode * n, bp::object & obj) const {
        bp::list members;
        for (const auto & c : n->getChildren()) {
            //Ensure that there are no '.' characters in the child
            //name. This would prevent tab completion from working.
            sparta_assert(c->getName().find(".") == std::string::npos);
            members.append(bp::str(c->getName()));
        }
        bp::setattr(obj, "__members__", members);
    }
};

/*!
 * \brief Wrapper for the Simulation Database object
 */
template <>
struct casting_wrapper <simdb::ObjectManager>
{
    bp::object new_wrapper(const simdb::ObjectManager * sim_db) {
        return bp::object(bp::ptr(sim_db));
    }

    void prepopulate(const simdb::ObjectManager * sim_db, bp::object & obj) const {
        bp::list members;
        if (!sim_db->getQualifiedTableName("Timeseries", "Stats").empty()) {
            std::vector<std::unique_ptr<simdb::ObjectRef>> timeseries_obj_refs;
            sim_db->findObjects("Timeseries", {}, timeseries_obj_refs);

            for (auto & timeseries_obj_ref : timeseries_obj_refs) {
                sparta_assert(timeseries_obj_ref != nullptr,
                            "Unexpected null timeseries returned from the database");
                sparta::db::ReportTimeseries ts(std::move(timeseries_obj_ref));
                std::string dest_file = ts.getHeader().getSourceReportDescDestFile();
                if (dest_file.empty()) {
                    throw sparta::SpartaException("Encountered a timeseries record in the ")
                        << "database that did not have its DestFile column value set. "
                        << "See database file '" << sim_db->getDatabaseFile() << "' to "
                        << "investigate (table=\"Timeseries\", rowid="
                        << timeseries_obj_ref->getId() << ").";
                }

                //Python will not allow file separators in member names.
                //In the case of CSV files written to a subdirectory like:
                //          <pwd>
                //             foo/
                //                bar/out.csv
                //
                //The dest_file could be something like:
                //  "example/CoreModel/AccuracyCheckedDBs/foo/bar/out.csv"
                //
                //Let's just take the file name, "out.csv", and use that
                //to prepopulate the python object members.
                auto slash = dest_file.find_last_of("/");
                if (slash < dest_file.size() - 1) {
                    dest_file = dest_file.substr(slash + 1);
                }

                //Python will not allow dots in member names
                boost::replace_all(dest_file, ".", "_");
                members.append(bp::str(dest_file));
            }
        }
        bp::setattr(obj, "__members__", members);
    }
};

void TreeNode___setattr__(sparta::TreeNode* n, const std::string& attr, bp::object val);

class ResourceTreeNodeWrapper{
public:
    static boost::shared_ptr<sparta::ResourceTreeNode> makeResourceTreeNode(sparta::TreeNode* n,
                                                                          const std::string& name,
                                                                          const std::string& group,
                                                                          uint32_t group_idx,
                                                                          const std::string& desc,
                                                                          sparta::ResourceFactoryBase* f){
        boost::shared_ptr<sparta::ResourceTreeNode> rtn(new sparta::ResourceTreeNode(n,
                                                                                 name,
                                                                                 group,
                                                                                 group_idx,
                                                                                 desc,
                                                                                 f));
        bp::object bp_object_rtn = WrapperCache<sparta::ResourceTreeNode>().wrap(rtn.get());
        TreeNode___setattr__(n, name, bp_object_rtn);
        return rtn;
    }
};


template<typename T>
class PortWrapper{
public:
    static boost::shared_ptr<sparta::DataOutPort<T>> makeOutPort(sparta::TreeNode* portset,
                                                               const std::string& name,
                                                               bool presume_zero_delay = false){
        boost::shared_ptr<sparta::DataOutPort<T>> outport(new sparta::DataOutPort<T>(portset,
                                                                                 name,
                                                                                 presume_zero_delay));
        bp::object bp_object_outport = WrapperCache<sparta::DataOutPort<T>>().wrap(outport.get());
        TreeNode___setattr__(portset, name, bp_object_outport);
        return outport;
    }

    static boost::shared_ptr<sparta::DataInPort<T>> makeInPort(sparta::TreeNode* portset,
                                                             const std::string& name,
                                                             sparta::SchedulingPhase delivery_phase,
                                                             sparta::Clock::Cycle delay){
        boost::shared_ptr<sparta::DataInPort<T>> inport(new sparta::DataInPort<T>(portset,
                                                                              name,
                                                                              delivery_phase,
                                                                              delay));
        bp::object bp_object_inport = WrapperCache<sparta::DataInPort<T>>().wrap(inport.get());
        TreeNode___setattr__(portset, name, bp_object_inport);
        return inport;
    }
};
