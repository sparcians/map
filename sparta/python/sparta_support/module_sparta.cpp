// <module_sparta> -*- C++ -*-


/*!
 * \file module_sparta.cpp
 * \brief "sparta" python module
 */

#include <memory>
#include <sstream>
#include <cstdio>
#include <unordered_map>
#include <algorithm>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/control/TemporaryRunController.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/events/SchedulingPhases.hpp"

#include "python/sparta_support/Completer.hpp"
#include "python/sparta_support/TreePathCompleter.hpp"
#include "python/sparta_support/module_sparta.hpp"
#include "python/sparta_support/PythonInterpreter.hpp"
#include "python/sparta_support/facade/ReportDescriptor.hpp"
#include "sparta/statistics/dispatch/archives/ReportStatisticsArchive.hpp"
#include "sparta/report/db/ReportTimeseries.hpp"
#include "sparta/report/db/ReportHeader.hpp"
#include "sparta/report/db/format/toCSV.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/async/AsyncTaskEval.hpp"

#include <boost/python.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/format.hpp>
#include <pythonrun.h>
#include <boost/python/raw_function.hpp>
//#include <boost/python/numpy.hpp>

namespace bp = boost::python;
// namespace np = boost::python::numpy;

//! Utility to make an empty numpy array
template <typename ArrayDataT>
bp::object makeEmptyArray()
{
    // Py_intptr_t shape[1] = {0};
    // auto dtype = np::dtype::get_builtin<ArrayDataT>();
    // np::ndarray empty_arr = np::zeros(1, shape, dtype);
    // return empty_arr;
    return bp::object(bp::borrowed(Py_None));
}

/*!
 * \brief Write a boost::python::object to an ostream
 */
template <typename T>
std::ostream& operator<<(T& s, const bp::object& obj)
{
    const uint32_t BUF_LEN = 1024;
    char buf[BUF_LEN];
    buf[0] = '\0';
    FILE* buf_file = fmemopen(buf, BUF_LEN-1, "w");
    if(buf_file == nullptr){
        throw sparta::SpartaException("Failed to allocate buffer for printing Python object");
    }

    try{
        PyObject_Print(obj.ptr(), buf_file, Py_PRINT_RAW);
    }catch(...){
        fclose(buf_file);
        throw;
    }
    s << buf;
    if(ftell(buf_file) == BUF_LEN){
        s << "(output truncated due to internal buffer limitations)";
    }
    fclose(buf_file);

    return s;
}

/*!
 * \brief Stringize a boost::python::object
 */
std::string stringize(const bp::object& obj)
{
    std::stringstream ss;
    ss << obj;
    return ss.str();
}

bp::object placeholder_classobj;


class RegisterFieldAccessSmartObj {
    std::vector<sparta::Register::Field*> fields_;
public:
    RegisterFieldAccessSmartObj() = delete;
    RegisterFieldAccessSmartObj(const RegisterFieldAccessSmartObj&) = delete;
    RegisterFieldAccessSmartObj(RegisterFieldAccessSmartObj&&) = delete;
    RegisterFieldAccessSmartObj& operator=(const RegisterFieldAccessSmartObj&) = delete;

    /*!
     * \brief Only constructor
     */
    RegisterFieldAccessSmartObj(const std::vector<sparta::Register::Field*>& fields)
    {
        // Copy vector locally
        for(auto* p : fields) {
            fields_.push_back(p);
        }
    }

    /*!
     * \brief Retrieve a register field by name
     */
    sparta::Register::Field* getByName(const std::string& name) const {
        //! \todo Replace this linear search with a map
        for(auto* p : fields_){
            if(p->getName() == name){
                return p;
            }
        }

        PyErr_SetString(PyExc_AttributeError,
                        (boost::format("Field set has no fiekd named '%s'") % name).str().c_str());
        throw bp::error_already_set();
    }

    /*!
     * \brief Retrieve a register field by index in the field list (not an architectural index)
     */
    sparta::Register::Field* getByIndex(uint32_t idx) const {
        return fields_.at(idx);
    }
};


/*!
 * \brief boost return policy converter for vector of string pointers to a list of strs
 */
class StrPtrVecToPyStrList {
public:
    typedef std::vector<const std::string*> in_type;

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static bool convertible() { return boost::is_same<T, in_type>::value; }

            /*!
             * \brief Perform the conversion from a vector of string pointers to a python list of python strs
             */
            PyObject* operator()(const in_type& v) const {
                bp::list l = bp::list();
                for (auto& sp : v) {
                    l.append(bp::str(*sp));
                }
                return bp::incref(l.ptr());
            }

            const PyTypeObject* get_pytype() const { return 0; }
        };
    };
};

/*!
 * \brief boost return policy converter for vector of strings to a list of strs
 */
class StrVecToPyStrList {
public:
    typedef std::vector<std::string> in_type;

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static bool convertible() { return boost::is_same<T, in_type>::value; }

            /*!
             * \brief Perform the conversion from a vector of string pointers to a python list of python strs
             */
            PyObject* operator()(const in_type& v) const {
                bp::list l = bp::list();
                for (auto& sp : v) {
                    l.append(bp::str(sp));
                }
                return bp::incref(l.ptr());
            }

            const PyTypeObject* get_pytype() const { return 0; }
        };
    };
};

/*!
 * \brief boost return policy converter for vector of TreeNode pointers to a list of strs
 */
class NodePtrVecToPyNodeList {
public:
    typedef std::vector<sparta::TreeNode*> in_type;

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static bool convertible() { return boost::is_same<T, in_type>::value; }

            /*!
             * \brief Perform the conversion from a vector of TreeNode pointers to a
             * python list of python strs
             */
            PyObject* operator()(const in_type& v) const {
                bp::list l = bp::list();
                WrapperCache<sparta::TreeNode> wrapper;
                for (sparta::TreeNode* n : v) {
                    l.append(wrapper.wrap(n));
                }
                return bp::incref(l.ptr());
            }

            const PyTypeObject* get_pytype() const { return 0; }
        };
    };
};

/*!
 * \brief boost return policy converter for vector of ParameterTree::Node pointers
 *  to a python list of python strings.
 */
class ParameterTreeNodePtrVecToPyNodeList {
public:
    typedef std::vector<sparta::ParameterTree::Node*> in_type;

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static bool convertible() { return boost::is_same<T, in_type>::value; }

            /*!
             * \brief Perform the conversion from a vector of ParameterTree::Node pointers to a
             * python list of python strs
             */
            PyObject* operator()(const in_type& v) const {
                bp::list l = bp::list();
                WrapperCache<sparta::ParameterTree::Node> wrapper;
                for (sparta::ParameterTree::Node* n : v) {
                    l.append(wrapper.wrap(n));
                }
                return bp::incref(l.ptr());
            }
            const PyTypeObject* get_pytype() const { return 0; }
        };
    };
};

/*!
 * \brief boost return policy converter for vector register Fields to a
 * smart object for getting fields by name or index
 */
class RegisterFieldVectorToPyFieldSmartObj {
public:
    typedef std::vector<sparta::Register::Field*> in_type;

    /*!
     * \brief Boost Python return_value_converter "apply" protocol requirement
     */
    template <typename T>
    struct apply {
        struct type {
            static bool convertible() { return boost::is_same<T, in_type>::value; }

            /*!
             * \brief Perform the conversion from a vector of Field pointers
             * the the smart object which is managed by the Python object
             */
            PyObject* operator()(const in_type& v) const {
                auto n = new RegisterFieldAccessSmartObj(v);
                //boost::shared_ptr<RegisterFieldAccessSmartObj> obj(n);
                WrapperCache<RegisterFieldAccessSmartObj> wrapper;
                bp::object pyobj = wrapper.wrap(n);
                return bp::incref(pyobj.ptr());
            }

            const PyTypeObject* get_pytype() const { return 0; }
        };
    };
};


//template<typename T>
//struct SimpleConverter {
//    static PyObject* convert(std::shared_ptr<sparta::RootTreeNode> obj) {
//        return bp::incref(bp::object(bp::ptr(obj.get())).ptr());
//    }
//};

/*!
 * \brief Wrapper map definition. Holds all wrappers generated for the shell
 */
std::unordered_map<const void*, bp::object> WrapperMap::wrapper_map_;

/*!
 * \brief Helper for checking if an object has an attribute
 * \note This can indirectly invoke __getattr__ if the target attribute
 * is not resolved by Python normally.
 */
bool hasattr(bp::object obj, const std::string& attr)
{
    return PyObject_HasAttrString(obj.ptr(), attr.c_str());
}

// Function disambiguation (choose specific overload or const-vs-nonconst method)
const sparta::Clock* (sparta::TreeNode::*TreeNode_getClock)(void) = &sparta::TreeNode::getClock; // Const
sparta::TreeNode* (sparta::TreeNode::*TreeNode_getRoot)(void) = &sparta::TreeNode::getRoot; // Non-const
const std::string& (sparta::TreeNode::*TreeNode_getGroupName)(void) const = &sparta::TreeNode::getGroup;
const sparta::TreeNode::ChildrenVector (sparta::TreeNode::*TreeNode_getChildren)(void) const =
    &sparta::TreeNode::getChildren;
sparta::TreeNode* (sparta::TreeNode::*TreeNode_getParent)(void) = &sparta::TreeNode::getParent; // Non-const
sparta::TreeNode* (sparta::TreeNode::*TreeNode_getChild)(const std::string&, bool) =
    &sparta::TreeNode::getChild; // Non-const
sparta::Register::Field* (sparta::Register::*Register_getField)(const std::string&) =
    &sparta::Register::getField; // Non-const
std::vector<sparta::Register::Field*>& (sparta::Register::*Register_getFields)(void) =
    &sparta::Register::getFields; // Non-const
sparta::RootTreeNode* (sparta::app::Simulation::*Simulation_getRoot)(void) =
    &sparta::app::Simulation::getRoot; // Non-const

// Function disambiguation for ParameterTree and ParameterTree::Node class.
sparta::ParameterTree::Node* (sparta::ParameterTree::Node::*Node_getParent)(void) =
    &sparta::ParameterTree::Node::getParent; // Non-const
sparta::ParameterTree::Node* (sparta::ParameterTree::Node::*Node_getRoot)(void) =
    &sparta::ParameterTree::Node::getRoot; // Non-const
sparta::ParameterTree* (sparta::ParameterTree::Node::*Node_getOwner)(void) =
    &sparta::ParameterTree::Node::getOwner; // Non-const
std::vector<sparta::ParameterTree::Node*> (sparta::ParameterTree::Node::*Node_getChildren)(void) =
    &sparta::ParameterTree::Node::getChildren; // Non-const
uint32_t (sparta::ParameterTree::*PTree_getUnreadValueNodes)(std::vector<sparta::ParameterTree::Node*>*) =
    &sparta::ParameterTree::getUnreadValueNodes; // Non-const
sparta::ParameterTree::Node* (sparta::ParameterTree::*PTree_tryGet)(const std::string&, const bool) =
    &sparta::ParameterTree::tryGet; // Non-const
sparta::ParameterTree::Node* (sparta::ParameterTree::*PTree_getRoot)() =
    &sparta::ParameterTree::getRoot; // Non-const
sparta::ParameterTree& (sparta::app::SimulationConfiguration::*Sim_getArchTreeNonConst)() =
    &sparta::app::SimulationConfiguration::getArchUnboundParameterTree; // Non-const
sparta::ParameterTree& (sparta::app::SimulationConfiguration::*Sim_getTreeNonConst)() =
    &sparta::app::SimulationConfiguration::getUnboundParameterTree; // Non-const

// Function disambiguation for ReportConfiguration class
sparta::app::ReportDescriptorCollection* (sparta::app::ReportConfiguration::*ReportConfiguration_getDescriptors)(void) =
    &sparta::app::ReportConfiguration::getDescriptors; // Non-const

/*!
 * \brief Create a breakpoint manager
 */
bp::object getBreakpointManager(sparta::app::Simulation* sim)
{
    bp::object o = WrapperCache<sparta::app::Simulation>().get_wrapper(sim); // must exist already
    if(hasattr(o, "__bm__") == false){
        //! \todo Load this from a pure python module by importing it (e.g. sparta_bm) and
        //! instantiating a BreakpointManager instance
        bp::object main = bp::import("__main__");
        bp::object global_ns = main.attr("__dict__");
        bp::object bmcls(
                         bp::borrowed(
                                      PyRun_String("class BreakpointManager(object):\n"
                                                   "    def __init__(self, sim):\n"
                                                   "        self.__sim = sim\n"
                                                   "    def print_sim(self):\n"
                                                   "        print self.__sim\n"
                                                   "\n",
                                                   Py_file_input,
                                                   global_ns.ptr(),
                                                   nullptr)));
        o.attr("__bm__") = bmcls(o); // Instantiate with reference to owning sim
    }
    return o.attr("__bm__");
}

bp::object getSimulationConfiguration(sparta::app::Simulation* sim)
{
    auto sim_config = sim->getSimulationConfiguration();
    return WrapperCache<sparta::app::SimulationConfiguration>().wrap(sim_config);
}

bp::object TemporaryRunControl_run_rejectargs_1(sparta::control::TemporaryRunControl*, bp::object)
{
    PyErr_SetString(PyExc_AttributeError,
                    "run command does not take any arguments. For a constrained "
                    "run, use runi or runc to run with an instruction or cycle "
                    "limit");
    throw bp::error_already_set();
}

bp::object TemporaryRunControl_run_rejectargs_2(sparta::control::TemporaryRunControl*, bp::object, bp::object)
{
    PyErr_SetString(PyExc_AttributeError,
                    "run command does not take any arguments. For a constrained "
                    "run, use runi or runc to run with an instruction or cycle "
                    "limit");
    throw bp::error_already_set();
}

/*!
 * \brief Look into a register for a field based on it's name or
 * (non-architectural) index
 */
bp::object Register_getItem(sparta::Register* r, bp::object i)
{
    bp::extract<uint32_t> get_int(i);
    if(get_int.check()){
        uint32_t field_idx = get_int();
        WrapperCache<sparta::Register::Field> wrapper;
        sparta::Register::Field* field = r->getFields().at(field_idx); // Exception on bad index
        return wrapper.wrap(field);
    }

    bp::extract<std::string> get_str(i);
    if(get_str.check()){

        WrapperCache<sparta::Register::Field> wrapper;
        auto field = r->getField(get_str()); // Exception on bad field name
        return wrapper.wrap(field);
    }

    PyErr_SetString(PyExc_AttributeError,
                    "temp");
    //(boost::format("Register cannot look up field of unkonwn "
    //               "type '%s'. Type must be an int or str") % i).str().c_str());
    throw bp::error_already_set();
}

/*!
 * \brief Call TreeNode::stringize while setting the pretty=false arg.
 * \note std::bind or boost::bind might be able to replace this type of
 * work-around for the fact that attrribute methods cannot require a
 * parameter, even if all parameters have defaults.
 */
std::string TreeNode_stringize_0args(sparta::TreeNode const * n)
{
    return n->stringize(false);
}

bp::object TreeNode___getattribute__(sparta::TreeNode* n, const std::string& attr)
{
    std::cout << " getattribute called on '" << attr << "'\n";
    bp::object o = WrapperCache<sparta::TreeNode>().wrap(n);
    //o.attr('__getattr__old_');

    bp::object cls(bp::handle<>(PyObject_Type(o.ptr())));
    bp::object base_cls = cls.attr("__bases__")[0];
    //std::cout << "base cls: " << base_cls << std::endl;
    //bp::object base_getattr = base_cls.attr("__getattribute__");

    //bp::object d = base_getattr(o, "__dict__");
    bp::object d = base_cls.attr("__dict__");

    //bp::object d = o.attr("__dict__");
    //std::cout << "d: " << d << std::endl;
    if(attr == "__dict__"){
        return d;
    }else if(hasattr(base_cls, attr)){
        return base_cls.attr(attr.c_str());
    }else if(d.contains(attr) == false){
        return d[attr];
        //PyErr_SetString(PyExc_AttributeError, (boost::format("Class instance has no attribute '%s'") % attr).str().c_str());
        //throw bp::error_already_set();
    }

    bp::object v = d[attr];
    //if(PyObject_HasAttrString(v.ptr(), "__placeholder_node") == false){
    //    return v; // Done
    //}
    if(PyObject_IsInstance(v.ptr(), placeholder_classobj.ptr()) == false){
        std::cout << "Not an instance!\n";
        return v;
    }

    std::cout << " Case B\n";
    d["attr"] = WrapperCache<sparta::TreeNode>().wrap(n->getChild(attr));
    return d["attr"];
}

bp::object TreeNode___getattr__(sparta::TreeNode* n, const std::string& attr)
{
    //std::cout << " getattr called on '" << attr << "'\n";

    // NOTE: This logic mirrors the content of __members__ published by
    // TreeNode wrapper population.

    bp::object o = WrapperCache<sparta::TreeNode>().get_wrapper(n); // must exist already

    // Check that this attribute is in the __members__ list before trying to
    // resolve it as a child/group of this node.
    // If python reaches this point
    if(attr != "__members__" && hasattr(o, "__members__")){ // members should not reach __getattr__ unless used early one in construction
        //bp::object pymembers = base_cls.attr("__members__");
        bp::object pymembers = o.attr("__members__");
        //std::cout << "members = ";
        //PyObject_Print(pymembers.ptr(), stdout, Py_PRINT_RAW);
        //std::cout << std::endl;
        bp::object pyattr = bp::str(attr);
        if(pymembers.contains(pyattr) == true){
            // Search children of this node for the chosen attribute
            for(auto* c : n->getChildren()) {
                if(c->getName() == attr){

                    bp::object d = o.attr("__dict__");
                    auto pc = WrapperCache<sparta::TreeNode>().wrap(c);
                    d[attr] = pc;
                    return pc;
                }
            }

            //! \todo Look up groups and aliases as well
            //! \todo Build a map of identifiers to objects into treenode for easier
            //        enumeration here. This map could be shared with casting_wrapper
            //        too
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError,
                    (boost::format("Class instance has no attribute '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

void TreeNode___setattr__(sparta::TreeNode* n, const std::string& attr, bp::object val)
{
    bp::object o = WrapperCache<sparta::TreeNode>().get_wrapper(n); // must exist already

    if(attr != "__members__" && hasattr(o, "__members__")){
        bp::object pymembers = o.attr("__members__");
        if(pymembers.contains(bp::str(attr)) == true){
            PyErr_SetString(PyExc_AttributeError,
                            (boost::format("Cannot set reserved child node/group/alias attribute '%s'") % attr).str().c_str());
            throw bp::error_already_set();

            //! \todo forward value to the attribute since we can't replace it.
            //! In some cases, this may make sense, but it's not Pythonic.
            //! Maybe for setting a parameter it would be more natural to accept
            //! param.x=5
        }
    }

    bp::str attr_str(attr);

    // Final, non-recursive built-in setattr function
    if(PyObject_GenericSetAttr(o.ptr(), attr_str.ptr(), val.ptr()) !=0){
        throw bp::error_already_set();
    }
}

bp::object ReportDescriptors__getattr__(sparta::app::ReportDescriptorCollection * n,
                                        const std::string & attr)
{
    //Wrapper must exist already - 'get_wrapper' method will throw if it is not there
    bp::object o = WrapperCache<sparta::app::ReportDescriptorCollection>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search enabled descriptors for the chosen one, and if found, cache it
            // in this object's dictionary
            const std::vector<std::string> desc_names = n->getAllDescriptorNames();
            for (const auto & name : desc_names) {
                if (name == attr) {
                    sparta::app::ReportDescriptor * rd = &n->getDescriptorByName(name);
                    bp::object d = o.attr("__dict__");
                    auto pc = WrapperCache<sparta::app::ReportDescriptor>().wrap(rd);
                    d[attr] = pc;
                    return pc;
                }
            }
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError,
                    (boost::format("There is no report descriptor "
                                   "named '%s'") % attr).str().c_str());
    throw bp::error_already_set();

    sparta_assert(!"Unreachable");
    return bp::object();
}

void ReportDescriptors__setattr__(sparta::app::ReportDescriptorCollection * n,
                                  const std::string & attr,
                                  bp::object val)
{
    //Wrapper must exist already - 'get_wrapper' method will throw if it is not there
    bp::object o = WrapperCache<sparta::app::ReportDescriptorCollection>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        if (pymembers.contains(bp::str(attr))) {
            PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                                 "Cannot set reserved attribute '%s'") % attr).str().c_str());
            throw bp::error_already_set();
        }
    }

    bp::str attr_str(attr);

    // Final, non-recursive built-in setattr function
    if (PyObject_GenericSetAttr(o.ptr(), attr_str.ptr(), val.ptr())) {
        throw bp::error_already_set();
    }
}

// In-memory cache of offline archive handles. These are owned outright
// by C++ in global scope, since there is no app::Simulation.
typedef sparta::statistics::StatisticsArchives OfflineArchive;
std::unordered_map<std::string, std::shared_ptr<OfflineArchive>> offline_archives;

sparta::statistics::StatisticsArchives * StatisticsArchives__import(
                                                                  const std::string & db_dir)
{
    auto iter = offline_archives.find(db_dir);
    if (iter == offline_archives.end()) {
        offline_archives[db_dir] = std::make_shared<OfflineArchive>(db_dir);
        iter = offline_archives.find(db_dir);
    }
    return iter->second.get();
}

// In-memory cache of timeseries wrappers. These are used both during
// simulation and outside of a simulation, and are put in global scope.
// After simulation, there is no clear owner of a database wrapper, so
// we are making these objects global.
std::unordered_map<std::string,
                   std::shared_ptr<sparta::db::ReportTimeseries>> loaded_db_timeseries;

// In-memory cache of SQLite database connections. These are used outside
// of a simulation to connect to an existing database. Without a simulator,
// ownership of database connections is vague at best, so we just put them
// in global scope.
std::unordered_map<std::string,
                   std::shared_ptr<simdb::ObjectManager>> db_connections;

simdb::ObjectManager * ReportTimeseries__connectToDatabase(
    const std::string & db_fullpath)
{
    //First check if this DB connection is already alive
    auto iter = db_connections.find(db_fullpath);
    if (iter != db_connections.end()) {
        return iter->second.get();
    }

    //Let's check the database path/filename of the simulator's "sim_db"
    //object that was published to the Python namespace, if there was one.
    //Having multiple connections open for the same database file may cause
    //performance issues.
    bp::object main = bp::import("__main__");
    bp::object global_ns = main.attr("__dict__");

    if (global_ns.contains(bp::str("sim_db"))) {
        simdb::ObjectManager * sim_db =
            bp::extract<simdb::ObjectManager*>(global_ns["sim_db"]);

        sparta_assert(sim_db != nullptr);
        if (sim_db->getDatabaseFile() == db_fullpath) {
            return sim_db;
        }
    }

    //Instantiate and cache a new database connection
    std::shared_ptr<simdb::ObjectManager> obj_mgr(new simdb::ObjectManager);
    if (!obj_mgr->connectToExistingDatabase(db_fullpath)) {
        //The ObjectManager rejected this database file for some reason
        std::cout << "ERROR! This is not a valid database file: "
                  << db_fullpath << std::endl;
        return nullptr;
    }

    //Looks good. Cache it in memory and return it.
    db_connections[db_fullpath] = obj_mgr;
    return obj_mgr.get();
}

bp::object StatisticsArchives__getattr__(sparta::statistics::StatisticsArchives * n,
                                         const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<StatisticsArchives>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the archives for the one whose name matches this
            // attribute, and if found, cache it in this object's dictionary
            auto root = n->getRootByName(attr);
            sparta_assert(root);

            auto wrapped_archive = WrapperCache<RootArchiveNode>().wrap(root);
            bp::object d = o.attr("__dict__");
            d[attr] = wrapped_archive;
            return wrapped_archive;
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no archive named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

// Get a data series wrapper from a leaf ArchiveNode
auto get_wrapper_for_data_series_node = [](sparta::statistics::ArchiveNode * n) {
    sparta::statistics::ArchiveDataSeries * ds = n->createDataSeries();
    return WrapperCache<sparta::statistics::ArchiveDataSeries>().wrap(ds);
};

// Get a wrapper for a middle ArchiveNode (not a leaf / no data)
auto get_wrapper_for_hierarchy_node = [](sparta::statistics::ArchiveNode * n) {
    sparta_assert(!n->getChildren().empty());
    return WrapperCache<sparta::statistics::ArchiveNode>().wrap(n);
};

bp::object RootArchiveNode__getattr__(sparta::statistics::RootArchiveNode * n,
                                      const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<RootArchiveNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the child nodes for the one whose name matches this
            // attribute, and if found, cache it in this object's dictionary
            for (const auto & child : n->getChildren()) {
                if (child->getName() == attr) {
                    ArchiveNode * child_ptr = child.get();

                    // If this child node has no children of its own, return it as
                    // an ArchiveDataSeries object.
                    bp::object child_archive_node;
                    if (child->getChildren().empty()) {
                        child_archive_node = get_wrapper_for_data_series_node(child_ptr);
                    }

                    // Otherwise, the tree is still being navigated to find the
                    // data series of interest. Return another ArchiveNode.
                    else {
                        child_archive_node = get_wrapper_for_hierarchy_node(child_ptr);
                    }

                    // Cache this object in its parent's Python dictionary
                    bp::object d = o.attr("__dict__");
                    d[attr] = child_archive_node;
                    return child_archive_node;
                }
            }

            // If the user is asking for the "triggers" property, see if we can
            // create and cache a ReportTriggers object
            if (attr == "triggers") {
                auto trigger_kvs = n->tryGetMetadataValue<sparta::app::TriggerKeyValues>("trigger");
                if (trigger_kvs) {
                    std::shared_ptr<sparta::app::NamedExtensions> trigger_extensions(
                                                                                   new sparta::app::NamedExtensions);

                    // The metadata of type app::TriggerKeyValues is just an
                    // unordered_map<string,string> for key-value pairs of
                    // trigger types ("start", "update-time", etc.) and
                    // trigger expressions ("top.core0... >= 1000", etc.)
                    boost::any trigger_kvs_any(*trigger_kvs);
                    (*trigger_extensions)["trigger"] = trigger_kvs_any;

                    // We now have the trigger info inside an app::NamedExtensions object.
                    // That is the same data type that held the app::ReportDescriptor's
                    // trigger info originally. We can create a facade::ReportTriggers
                    // object to wrap the triggers for Python from it.
                    std::shared_ptr<sparta::facade::ReportTriggers> py_triggers(
                                                                              new sparta::facade::ReportTriggers(*trigger_extensions));

                    // Someone needs to own this triggers object for the lifetime of
                    // the Python variable, so we can just put it in the RootArchiveNode's
                    // own general-purpose metadata map.
                    n->setMetadataValue("py_trigger_extensions", trigger_extensions);
                    n->setMetadataValue("py_triggers", py_triggers);

                    // Trigger objects can only be altered from Python during the configuration
                    // phase of the simulation, not during or after simulation
                    py_triggers->lockFurtherChanges();

                    bp::object wrapped_py_triggers =
                        WrapperCache<sparta::facade::ReportTriggers>().wrap(py_triggers.get());

                    bp::object d = o.attr("__dict__");
                    d[attr] = wrapped_py_triggers;
                    return wrapped_py_triggers;
                }
            }
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no statistic node named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

bp::object ArchiveNode__getattr__(sparta::statistics::ArchiveNode * n,
                                  const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<ArchiveNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the child nodes for the one whose name matches this
            // attribute, and if found, cache it in this object's dictionary
            for (const auto & child : n->getChildren()) {
                if (child->getName() == attr) {
                    ArchiveNode * child_ptr = child.get();

                    // If this child node has no children of its own, return it as
                    // an ArchiveDataSeries object.
                    bp::object child_archive_node;
                    if (child->getChildren().empty()) {
                        child_archive_node = get_wrapper_for_data_series_node(child_ptr);
                    }

                    // Otherwise, the tree is still being navigated to find the
                    // data series of interest. Return another ArchiveNode.
                    else {
                        child_archive_node = get_wrapper_for_hierarchy_node(child_ptr);
                    }

                    // Cache this object in its parent's Python dictionary
                    bp::object d = o.attr("__dict__");
                    d[attr] = child_archive_node;
                    return child_archive_node;
                }
            }
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no statistic node named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

void RootArchiveNode__setattr__(sparta::statistics::RootArchiveNode * n,
                                const std::string & attr,
                                bp::object val)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<RootArchiveNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        if (pymembers.contains(bp::str(attr))) {
            PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                                 "Cannot set reserved attribute '%s'") % attr).str().c_str());
            throw bp::error_already_set();
        }
    }

    bp::str attr_str(attr);

    // Final, non-recursive built-in setattr function
    if (PyObject_GenericSetAttr(o.ptr(), attr_str.ptr(), val.ptr())) {
        throw bp::error_already_set();
    }
}

void ArchiveNode__setattr__(sparta::statistics::ArchiveNode * n,
                            const std::string & attr,
                            bp::object val)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<ArchiveNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        if (pymembers.contains(bp::str(attr))) {
            PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                                 "Cannot set reserved attribute '%s'") % attr).str().c_str());
            throw bp::error_already_set();
        }
    }

    bp::str attr_str(attr);

    // Final, non-recursive built-in setattr function
    if (PyObject_GenericSetAttr(o.ptr(), attr_str.ptr(), val.ptr())) {
        throw bp::error_already_set();
    }
}

bp::object ArchiveDataSeries__getRange(
                                       sparta::statistics::ArchiveDataSeries & ar,
                                       const int32_t from_index,
                                       const int32_t to_index)
{
    (void)ar;
    (void)from_index;
    (void) to_index;
    // Range validation
#if 0
    if (from_index > to_index) {
        PyErr_SetString(PyExc_IndexError,
                        "The 'from' index should be less than or equal to 'to' index'");
        throw bp::error_already_set();
    }

    // Range validation
    if (from_index >= 0 && to_index >= 0 &&
        static_cast<size_t>(from_index) < ar.size() &&
        static_cast<size_t>(to_index) < ar.size()) {

        // Range requested is valid. Get a copy of the data
        // and wrap it in a Python object.
        bp::tuple shape = bp::make_tuple(to_index - from_index + 1);
        bp::tuple stride = bp::make_tuple(sizeof(double));
        bp::object own;

        np::dtype dt = np::dtype::get_builtin<double>();

        np::ndarray arr = np::from_data(
                                        ar.getDataReference().data() + from_index,
                                        dt, shape, stride, own);

        return arr;
    }
#endif
    PyErr_SetString(PyExc_IndexError, "Index out of range");
    throw bp::error_already_set();
}

bp::object ArchiveDataSeries__getAllData(sparta::statistics::ArchiveDataSeries & ar)
{
    // Don't throw for empty archives, just return []
    if (ar.empty()) {
        return makeEmptyArray<double>();
    }

    static const int32_t from_index = 0;
    const int32_t to_index = ar.size() - 1;
    return ArchiveDataSeries__getRange(ar, from_index, to_index);
}

bp::object StatisticsStreams__getattr__(sparta::statistics::StatisticsStreams * n,
                                        const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<StatisticsStreams>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the streams for the one whose name matches this
            // attribute, and if found, cache it in this object's dictionary
            auto root = n->getRootByName(attr);
            sparta_assert(root);

            auto wrapped_stream = WrapperCache<StreamNode>().wrap(root);
            bp::object d = o.attr("__dict__");
            d[attr] = wrapped_stream;
            return wrapped_stream;
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no stream named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

bp::object StreamNode__getattr__(sparta::statistics::StreamNode * n,
                                 const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<StreamNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the child nodes for the one whose name matches this
            // attribute, and if found, cache it in this object's dictionary
            for (const auto & child : n->getChildren()) {
                if (child->getName() == attr) {
                    StreamNode * child_ptr = child.get();
                    auto wrapped_node = WrapperCache<StreamNode>().wrap(child_ptr);

                    // Cache this object in its parent's Python dictionary
                    bp::object d = o.attr("__dict__");
                    d[attr] = wrapped_node;
                    return wrapped_node;
                }
            }
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no statistic node named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

void StreamNode__setattr__(sparta::statistics::StreamNode * n,
                           const std::string & attr,
                           bp::object val)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<StreamNode>().get_wrapper(n);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        if (pymembers.contains(bp::str(attr))) {
            PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                                 "Cannot set reserved attribute '%s'") % attr).str().c_str());
            throw bp::error_already_set();
        }
    }

    bp::str attr_str(attr);

    // Final, non-recursive built-in setattr function
    if (PyObject_GenericSetAttr(o.ptr(), attr_str.ptr(), val.ptr())) {
        throw bp::error_already_set();
    }
}

std::string StreamNode__str__(sparta::statistics::StreamNode & node)
{
    return node.getFullPath();
}

std::string StreamNode__getFullPath(sparta::statistics::StreamNode & node)
{
    return node.getFullPath();
}

bp::object StreamNode__getBufferedData(sparta::statistics::StreamNode & node)
{
    (void) node;
    // std::queue<std::vector<double>> data_queue;
    // node.getBufferedStreamData(data_queue);

    // if (data_queue.empty()) {
    //     return makeEmptyArray<double>();
    // }

    // //Helper that converts a std::vector<double> into a boost::python::list
    // auto cpp_vec_to_py_array = [](const std::vector<double> & data)
    // {
    //     Py_intptr_t shape[1] = { static_cast<Py_intptr_t>(data.size()) };
    //     np::ndarray arr = np::zeros(1, shape, np::dtype::get_builtin<double>());
    //     std::copy(data.begin(), data.end(), reinterpret_cast<double*>(arr.get_data()));
    //     return arr;
    // };

    bp::list data_packets;

    //Until the C++/Python communication is asynchronous, we should
    //always have exactly one packet of data (or none). Async consumers
    //will result in streams containing any number of packets.
    // sparta_assert(data_queue.size() == 1, "Unexpected async behavior detected!");

    // const auto & data_packet = data_queue.front();
    // data_packets.append(cpp_vec_to_py_array(data_packet));
    // data_queue.pop();

    return std::move(data_packets);
}

bp::object StreamNode__streamTo(bp::tuple args, bp::dict kwargs)
{
    using namespace sparta::statistics;
    (void) kwargs;

    bp::object self = args[0];
    bp::object sink_type = args[1];

    //Initialize the C++ stream at this node location. This
    //call to the initialize() method will be short-circuited
    //if this node has already been assigned to a Python sink.
    //One stream node can feed any number of Python sinks, but
    //the C++ source object doesn't need to be re-initialized.
    StreamNode & node = bp::extract<StreamNode&>(self);
    node.initialize();

    bp::object main = bp::import("__main__");
    auto global_ns = main.attr("__dict__");
    auto stream_mgr = global_ns["__stream_manager"];
    stream_mgr.attr("addStream")(boost::ref(node), sink_type);

    //Nothing to return. We could potentially return the
    //instantiated Python object itself, but this part of
    //the C++/Python interface hasn't been explicitly
    //designed yet.
    return bp::object(bp::borrowed(Py_None));
}

//! Recreate a sparta::Report and its sparta::StatisticInstance objects from
//! from the provided report node database ID, living in the provided
//! ObjectManager (SimDB). You may call this method for any report format
//! *except* csv and csv_cumulative:
//!
//!    txt, text, html, htm, js_json, jjson, python, py, json, JSON,
//!    json_reduced, JSON_reduced, json_detail, JSON_detail, gnuplot,
//!    gplt, stats_mapping
//!
void LOCAL_SimulationDatabase__createReport(
    const simdb::ObjectManager * sim_db,
    const int report_db_id,
    const std::string & filename,
    const std::string & format,
    const sparta::Scheduler * scheduler)
{
    if (!sparta::Report::createFormattedReportFromDatabase(
             *sim_db, report_db_id, filename, format, scheduler))
    {
        std::cout << "Unable to create report file '"
                  << filename << "' " << std::endl;
    }
}

enum class SimDBReportType {
    AutoSummary,
    Json,
    JsonReduced,
    JsonDetail,
    JsJson,
    Html,
    Text,
    PyDictionary,
    GnuPlot,
    StatsMapping
};

template <SimDBReportType type>
void SimulationDatabase__createReport(
    simdb::ObjectManager * sim_db,
    const int report_db_id,
    const std::string & filename,
    const sparta::Scheduler * scheduler)
{
    using rt = SimDBReportType;

    switch (type) {
        case rt::AutoSummary: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "auto", scheduler);
            break;
        }
        case rt::Json: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "json", scheduler);
            break;
        }
        case rt::JsonReduced: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "json_reduced", scheduler);
            break;
        }
        case rt::JsonDetail: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "json_detail", scheduler);
            break;
        }
        case rt::JsJson: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "js_json", scheduler);
            break;
        }
        case rt::Html: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "html", scheduler);
            break;
        }
        case rt::Text: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "txt", scheduler);
            break;
        }
        case rt::PyDictionary: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "python", scheduler);
            break;
        }
        case rt::GnuPlot: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "gnuplot", scheduler);
            break;
        }
        case rt::StatsMapping: {
            LOCAL_SimulationDatabase__createReport(
                sim_db, report_db_id, filename, "stats_mapping", scheduler);
            break;
        }
    }
}

//! At the end of SPARTA simulations, the database report contents
//! may be checked for accuracy against a known correct/expected
//! report. For debugging purposes, any discrepancies encountered
//! during this post-simulation verification step may be put into
//! the database. This method prints the verification summaries.
void LOCAL_SimulationDatabase__printVerifFailureSummary(
    const simdb::ObjectManager & sim_db,
    const simdb::DatabaseID report_verif_result_id,
    const simdb::DatabaseID sim_info_id)
{
    std::string failure_summary;
    simdb::ObjectQuery summary_query(sim_db, "ReportVerificationFailureSummaries");

    summary_query.addConstraints(
        "ReportVerificationResultID",
        simdb::constraints::equal,
        report_verif_result_id);

    summary_query.writeResultIterationsTo("FailureSummary", &failure_summary);
    auto result_iter = summary_query.executeQuery();
    if (result_iter->getNext()) {
        std::cout << failure_summary << "\n";
    }

    simdb::ObjectQuery sim_info_query(sim_db, "SimInfo");
    sim_info_query.addConstraints("Id", simdb::constraints::equal, sim_info_id);

    std::string sinfo_name, sinfo_cmdline, sinfo_workingdir, sinfo_simversion;
    std::string sinfo_exe, sinfo_spartaversion, sinfo_repro, sinfo_other;

    sim_info_query.writeResultIterationsTo(
        "Name", &sinfo_name,
        "Cmdline", &sinfo_cmdline,
        "WorkingDir", &sinfo_workingdir,
        "Exe", &sinfo_exe,
        "SimulatorVersion", &sinfo_simversion,
        "SpartaVersion", &sinfo_spartaversion,
        "Repro", &sinfo_repro,
        "Other", &sinfo_other);

    auto change_unset_to_hyphen = [](std::string & str) {
        if (str == "unset") {
            str = "-";
        }
    };
    change_unset_to_hyphen(sinfo_name);
    change_unset_to_hyphen(sinfo_cmdline);
    change_unset_to_hyphen(sinfo_workingdir);
    change_unset_to_hyphen(sinfo_exe);
    change_unset_to_hyphen(sinfo_simversion);
    change_unset_to_hyphen(sinfo_spartaversion);
    change_unset_to_hyphen(sinfo_repro);
    change_unset_to_hyphen(sinfo_other);

    result_iter = sim_info_query.executeQuery();
    if (result_iter->getNext()) {
        std::cout << "    Name: " << sinfo_name << "\n";
        std::cout << "    Cmdline: " << sinfo_cmdline << "\n";
        std::cout << "    WorkingDir: " << sinfo_workingdir << "\n";
        std::cout << "    Exe: " << sinfo_exe << "\n";
        std::cout << "    SimulatorVersion: " << sinfo_simversion << "\n";
        std::cout << "    SpartaVersion: " << sinfo_spartaversion << "\n";
        std::cout << "    Repro: " << sinfo_repro << "\n";
        std::cout << "    Other: " << sinfo_other << "\n\n";
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

//! Utility to print out a high-level pass/fail summary for all
//! report verification checks run on the given database.
void SimulationDatabase__printVerificationSummary(
    simdb::ObjectManager * sim_db,
    const bool verbose)
{
    sim_db->safeTransaction([&]() {
        bool summary_header_printed = false;
        auto print_summary_header = [&summary_header_printed, sim_db]() {
            if (!summary_header_printed) {
                std::cout << "- - - - - - - - Report Verification Summary - - - - - - - " << std::endl;
                std::cout << "  (" << sim_db->getDatabaseFile() << ")\n" << std::endl;
                summary_header_printed = true;
            }
        };
        if (verbose) {
            print_summary_header();
        }
        if (verbose) {
            std::cout << "PASSED:\n";
            simdb::ObjectQuery passed_query(*sim_db, "ReportVerificationResults");
            passed_query.addConstraints("Passed", simdb::constraints::equal, 1);

            std::string dest_file;
            int is_timeseries;
            passed_query.writeResultIterationsTo(
                "DestFile", &dest_file, "IsTimeseries", &is_timeseries);

            auto result_iter = passed_query.executeQuery();
            std::set<std::string> passed_timeseries, passed_non_timeseries;
            while (result_iter->getNext()) {
                if (is_timeseries) {
                    passed_timeseries.insert(dest_file);
                } else {
                    passed_non_timeseries.insert(dest_file);
                }
            }

            std::cout << "  Timeseries...\n";
            if (passed_timeseries.empty()) {
                std::cout << "    (none)\n";
            } else {
                for (const auto & pass : passed_timeseries) {
                    std::cout << "    " << pass << "\n";
                }
            }

            std::cout << std::endl;
            std::cout << "  Non-timeseries...\n";
            if (passed_non_timeseries.empty()) {
                std::cout << "    (none)\n";
            } else {
                for (const auto & pass : passed_non_timeseries) {
                    std::cout << "    " << pass << "\n";
                }
            }
            std::cout << "\n" << std::endl;
        }

        bool fail_header_printed = false;
        auto print_failure_header = [&fail_header_printed]() {
            if (!fail_header_printed) {
                std::cout << "FAILED:\n";
                fail_header_printed = true;
            }
        };

        simdb::ObjectQuery failed_query(*sim_db, "ReportVerificationResults");
        failed_query.addConstraints("Passed", simdb::constraints::equal, 0);

        if (verbose || failed_query.countMatches() > 0) {
            print_summary_header();
            print_failure_header();
        }

        std::string dest_file;
        int is_timeseries;
        simdb::DatabaseID result_verif_id;
        simdb::DatabaseID sim_info_id;

        failed_query.writeResultIterationsTo(
            "Id", &result_verif_id,
            "DestFile", &dest_file,
            "IsTimeseries", &is_timeseries,
            "SimInfoID", &sim_info_id);

        using ResultVerifAndSimInfoIDs = std::pair<simdb::DatabaseID, simdb::DatabaseID>;
        auto result_iter = failed_query.executeQuery();
        std::map<std::string, ResultVerifAndSimInfoIDs> failed_timeseries;
        std::map<std::string, ResultVerifAndSimInfoIDs> failed_non_timeseries;

        while (result_iter->getNext()) {
            if (is_timeseries) {
                failed_timeseries[dest_file] =
                    std::make_pair(result_verif_id, sim_info_id);
            } else {
                failed_non_timeseries[dest_file] =
                    std::make_pair(result_verif_id, sim_info_id);
            }
        }

        if (!verbose && failed_timeseries.empty() && failed_non_timeseries.empty()) {
            return;
        } else {
            print_failure_header();
        }

        if (verbose || !failed_timeseries.empty()) {
            std::cout << "  Timeseries...\n";
            if (failed_timeseries.empty()) {
                std::cout << "    (none)\n";
            } else {
                for (const auto & fail : failed_timeseries) {
                    std::cout << "    " << fail.first << "\n";
                    LOCAL_SimulationDatabase__printVerifFailureSummary(
                        *sim_db, fail.second.first, fail.second.second);
                    std::cout << "  + + + + + + + + + + + + + + + + + + +\n";
                }
            }
        }

        if (verbose || !failed_non_timeseries.empty()) {
            std::cout << "  Non-timeseries...\n";
            if (failed_non_timeseries.empty()) {
                std::cout << "    (none)\n";
            } else {
                for (const auto & fail : failed_non_timeseries) {
                    std::cout << "    " << fail.first << "\n";
                    LOCAL_SimulationDatabase__printVerifFailureSummary(
                        *sim_db, fail.second.first, fail.second.second);
                    std::cout << "  + + + + + + + + + + + + + + + + + + +\n";
                }
            }
        }

        std::cout << std::endl;
    });
}

//! Utility to print out a high-level pass/fail summary for all
//! report verification checks run on all databases (*.db files)
//! found in the given directory.
void SimulationDatabase__printAllVerificationSummaries(
    const std::string & simdb_dir,
    const bool verbose)
{
    namespace bfs = boost::filesystem;

    auto p = bfs::path(simdb_dir);
    if (!bfs::exists(p) || !bfs::is_directory(p)) {
        std::cout << "Not a valid directory: '" << simdb_dir << "'\n" << std::endl;
        return;
    }

    for (bfs::directory_iterator iter(p); iter != bfs::directory_iterator(); ++iter) {
        if (bfs::is_regular_file(iter->status()) && iter->path().extension().string() == ".db") {
            const std::string db_full_filename = iter->path().string();
            bfs::path db_path(db_full_filename);
            const std::string db_filename = db_path.stem().string() + db_path.extension().string();
            simdb::ObjectManager sim_db(simdb_dir);
            if (sim_db.connectToExistingDatabase(db_filename)) {
                SimulationDatabase__printVerificationSummary(&sim_db, verbose);
            } else {
                std::cout << "Unable to open database file: '" << db_filename << "'\n";
            }
        }
    }
}

//! Hidden utility which prints out a "<dest_file>.expected" and a
//! "<dest_file>.actual" report file that was saved / deep copied
//! into the database during the post-simulation report verification
//! check.
void SimulationDatabase__getVerificationFailureReportDiffs(
    simdb::ObjectManager * sim_db,
    const std::string & orig_dest_file)
{
    sim_db->safeTransaction([&]() {
        simdb::ObjectQuery query(*sim_db, "ReportVerificationDeepCopyFiles");
        query.addConstraints("DestFile", simdb::constraints::equal, orig_dest_file);

        std::string expected, actual;
        query.writeResultIterationsTo("Expected", &expected, "Actual", &actual);

        std::vector<std::pair<std::string, std::string>> deep_copy_files;
        auto result_iter = query.executeQuery();
        size_t suffix_idx = 1;
        while (result_iter->getNext()) {
            const std::string suffix = (suffix_idx++ > 1) ?
                ("." + boost::lexical_cast<std::string>(suffix_idx)) : "";
            const std::string dest_file_expected = orig_dest_file + ".expected" + suffix;
            const std::string dest_file_actual = orig_dest_file + ".actual" + suffix;

            std::ofstream fout_expected(dest_file_expected);
            fout_expected << expected;

            std::ofstream fout_actual(dest_file_actual);
            fout_actual << actual;

            deep_copy_files.emplace_back(dest_file_expected, dest_file_actual);
        }

        if (!deep_copy_files.empty()) {
            std::cout << "The following files can be diff'd for discrepancies:\n";
            for (const auto & diff_files : deep_copy_files) {
                std::cout << diff_files.first << "\n" << diff_files.second << "\n";
            }
            std::cout << std::endl;
        }
    });
}

//! Go through the given directory, and look into each "*.db" file we find
//! for any reports that failed the post-simulation verification check.
void SimulationDatabase__getVerificationFailuresInDir(
    const std::string & simdb_dir)
{
    namespace bfs = boost::filesystem;
    auto p = bfs::path(simdb_dir);
    if (!bfs::exists(p) || !bfs::is_directory(p)) {
        std::cout << "Not a valid directory: '" << simdb_dir << "'\n" << std::endl;
        return;
    }

    std::map<std::string, std::set<std::string>> db_subdirs_with_failures;
    for (bfs::directory_iterator iter(p); iter != bfs::directory_iterator(); ++iter) {
        if (bfs::is_regular_file(iter->status()) && iter->path().extension().string() == ".db") {
            const std::string db_full_filename = iter->path().string();
            bfs::path db_path(db_full_filename);
            const std::string db_filename = db_path.stem().string() + db_path.extension().string();
            simdb::ObjectManager sim_db(simdb_dir);
            if (sim_db.connectToExistingDatabase(db_filename)) {
                 simdb::ObjectQuery query(sim_db, "ReportVerificationDeepCopyFiles");

                 std::string filename;
                 query.writeResultIterationsTo("DestFile", &filename);

                 auto result_iter = query.executeQuery();
                 while (result_iter->getNext()) {
                     SimulationDatabase__getVerificationFailureReportDiffs(&sim_db, filename);
                     db_subdirs_with_failures[db_filename].insert(filename);
                 }
            }
        }
    }

    if (!db_subdirs_with_failures.empty()) {
        std::cout << "The following database files had report "
                  << "verification failures:\n";
        for (const auto & subdir : db_subdirs_with_failures) {
            std::cout << "\tDatabase file " << subdir.first << " had failures in:\n";
            for (const auto & dest_file : subdir.second) {
                std::cout << "\t\t" << dest_file << "\n";
            }
            std::cout << "\n";
        }
    } else {
        std::cout << "This directory contained no database files "
                  << "with report verification failures.\n";
    }
    std::cout << std::endl;
}

bp::object SimulationDatabase__getattr__(simdb::ObjectManager * sim_db,
                                         const std::string & attr)
{
    using namespace sparta::statistics;

    bp::object o = WrapperCache<simdb::ObjectManager>().get_wrapper(sim_db);

    if (attr != "__members__" && hasattr(o, "__members__")) {
        bp::object pymembers = o.attr("__members__");
        bp::object pyattr = bp::str(attr);
        if (pymembers.contains(pyattr)) {
            // Search through the available timeseries records for the one whose
            // source report descriptor's dest_file matches this attribute, and if
            // found, cache it in this object's dictionary
            std::vector<std::unique_ptr<simdb::ObjectRef>> timeseries_obj_refs;
            sim_db->findObjects("Timeseries", {}, timeseries_obj_refs);

            for (auto & timeseries_obj_ref : timeseries_obj_refs) {
                sparta_assert(timeseries_obj_ref != nullptr,
                            "Unexpected null timeseries returned from the database");
                auto ts = std::make_shared<sparta::db::ReportTimeseries>(std::move(timeseries_obj_ref));
                auto dest_file = ts->getHeader().getSourceReportDescDestFile();

                auto slash = dest_file.find_last_of("/");
                if (slash < dest_file.size() - 1) {
                    dest_file = dest_file.substr(slash + 1);
                }

                boost::replace_all(dest_file, ".", "_");
                if (attr == dest_file) {
                    //This is the timeseries report we're looking for
                    loaded_db_timeseries[dest_file] = ts;

                    auto wrapped_timeseries = WrapperCache<sparta::db::ReportTimeseries>().wrap(ts.get());
                    bp::object d = o.attr("__dict__");
                    d[attr] = wrapped_timeseries;
                    return wrapped_timeseries;
                }
            }
        }
    }

    // Failed to get this attribute
    PyErr_SetString(PyExc_AttributeError, (boost::format(
                                                         "There is no timeseries named '%s'") % attr).str().c_str());
    throw bp::error_already_set();
}

void ASYNC_SIM_ENGINE_SYNCHRONIZE()
{
    bp::object main = bp::import("__main__");
    bp::object global_ns = main.attr("__dict__");

    if (global_ns.contains(bp::str("__db_queue"))) {
        simdb::AsyncTaskEval * db_queue =
            bp::extract<simdb::AsyncTaskEval*>(global_ns["__db_queue"]);

        sparta_assert(db_queue != nullptr);
        db_queue->emitPreFlushNotification();
        db_queue->flushQueue();
    }
}

bp::object LOCAL_ReportTimeseries__getPythonArrayFromSIValues(
                                                              const std::vector<std::vector<double>> & si_values)
{
    (void)si_values;
    // if (si_values.empty()) {
    //     return makeEmptyArray<double>();
    // }

    // //Helper that converts a std::vector<double> into a boost::python::list
    // auto cpp_vec_to_py_array = [](const std::vector<double> & data)
    // {
    //     Py_intptr_t shape[1] = { static_cast<Py_intptr_t>(data.size()) };
    //     np::ndarray arr = np::zeros(1, shape, np::dtype::get_builtin<double>());
    //     std::copy(data.begin(), data.end(), reinterpret_cast<double*>(arr.get_data()));
    //     return arr;
    // };

    bp::list py_arr;
    // for (const auto & blob : si_values) {
    //     py_arr.append(cpp_vec_to_py_array(blob));
    // }
    return std::move(py_arr);
}

bp::object ReportTimeseries__getValuesInTimeRange(sparta::db::ReportTimeseries * ts,
                                                  const uint64_t start_picoseconds,
                                                  const uint64_t end_picoseconds)
{
    ASYNC_SIM_ENGINE_SYNCHRONIZE();

    std::vector<std::vector<double>> si_values;
    ts->getStatisticInstValuesBetweenSimulatedPicoseconds(
                                                          start_picoseconds, end_picoseconds, si_values);

    return LOCAL_ReportTimeseries__getPythonArrayFromSIValues(si_values);
}

bp::object ReportTimeseries__getValuesInClockRange(sparta::db::ReportTimeseries * ts,
                                                   const uint64_t start_cycle,
                                                   const uint64_t end_cycle)
{
    ASYNC_SIM_ENGINE_SYNCHRONIZE();

    std::vector<std::vector<double>> si_values;
    ts->getStatisticInstValuesBetweenRootClockCycles(
                                                     start_cycle, end_cycle, si_values);

    return LOCAL_ReportTimeseries__getPythonArrayFromSIValues(si_values);
}

bp::object ReportTimeseries__getAllValues(sparta::db::ReportTimeseries * ts)
{
    ASYNC_SIM_ENGINE_SYNCHRONIZE();

    std::vector<std::vector<double>> si_values;
    static const auto start = std::numeric_limits<uint64_t>::min();
    static const auto end = std::numeric_limits<uint64_t>::max();
    ts->getStatisticInstValuesBetweenSimulatedPicoseconds(start, end, si_values);

    return LOCAL_ReportTimeseries__getPythonArrayFromSIValues(si_values);
}

bp::object ReportTimeseries__toCSV(sparta::db::ReportTimeseries * ts,
                                   const std::string & csv_filename)
{
    ASYNC_SIM_ENGINE_SYNCHRONIZE();

    sparta::db::format::toCSV(ts, csv_filename);

    return bp::object(bp::borrowed(Py_None));
}

/*!
 * \brief Define the "sparta" module and wrappers for all necessary components
 *
 * \note This is initialized manually from within the PythonInterpreter and
 *       does not need to come from another shared ibrary
 */
BOOST_PYTHON_MODULE(sparta)
{
    using namespace boost::python;

    //np::initialize();

    placeholder_classobj =
        class_<PlaceholderObject, boost::shared_ptr<PlaceholderObject>, boost::noncopyable>
        ("PlaceholderObject")
        ;

    enum_<sparta::PhasedObject::TreePhase>("TreePhase")
        .value("BUILDING",    sparta::PhasedObject::TREE_BUILDING)
        .value("CONFIGURING", sparta::PhasedObject::TREE_CONFIGURING)
        .value("FINALIZING",  sparta::PhasedObject::TREE_FINALIZING)
        .value("FINALIZED",   sparta::PhasedObject::TREE_FINALIZED)
        .value("TEARDOWN",    sparta::PhasedObject::TREE_TEARDOWN)
        ;

    class_<sparta::TreeNode, boost::noncopyable>
        ("TreeNode", init<sparta::TreeNode*,
         const std::string&,
         const std::string&,
         sparta::TreeNode::group_idx_type,
         const std::string&,
         bool>())
        .add_property("_clock",
                      make_function(TreeNode_getClock, return_value_policy<WrapperCache<sparta::Clock>>())
                      )
        //.def("getName", &sparta::TreeNode::getName, return_value_policy<copy_const_reference>())
        .add_property("_parent",
                      make_function(TreeNode_getParent, return_value_policy<WrapperCache<sparta::TreeNode>>()) // getter
                      )
        .add_property("_root",
                      make_function(TreeNode_getRoot, return_value_policy<WrapperCache<sparta::TreeNode>>()) // getter
                      )
        .add_property("_name",
                      make_function(&sparta::TreeNode::getName, return_value_policy<copy_const_reference>()) // getter
                      )
        .add_property("_desc",
                      make_function(&sparta::TreeNode::getDesc, return_value_policy<copy_const_reference>()) // getter
                      )
        .add_property("_group_name",
                      make_function(TreeNode_getGroupName, return_value_policy<copy_const_reference>()) // getter
                      )
        .add_property("_group_idx", &sparta::TreeNode::getGroupIdx)
        .add_property("_builtin", &sparta::TreeNode::isBuiltin)
        .add_property("_tags",
                      make_function(&sparta::TreeNode::getTags, return_value_policy<StrPtrVecToPyStrList>()) // getter
                      )
        .add_property("_children",
                      make_function(TreeNode_getChildren, return_value_policy<NodePtrVecToPyNodeList>()) // getter
                      )
        .def("_get_child", TreeNode_getChild, (bp::arg("location"), bp::arg("must_exist")=true),
             return_value_policy<WrapperCache<sparta::TreeNode>>()) // getter
        .add_property("_aliases",
                      make_function(&sparta::TreeNode::getAliases,
                                    return_value_policy<StrVecToPyStrList>()) // getter
                      )
        .add_property("_sim",
                      make_function(&sparta::TreeNode::getSimulation,
                                    return_value_policy<WrapperCache<sparta::app::Simulation>>())
                      )
        .add_property("_attached", &sparta::TreeNode::isAttached)
        .def("_add_tag", &sparta::TreeNode::addTag)
        .def("_add_alias", &sparta::TreeNode::addAlias)
        .def("_add_aliases", &sparta::TreeNode::addAliases)
        .add_property("_node_uid", &sparta::TreeNode::getNodeUID)
        .add_property("_hidden", &sparta::TreeNode::isHidden, &sparta::TreeNode::markHidden) // getter, setter
        .add_property("_string", &TreeNode_stringize_0args) // Cannot specify named args in a property. Must wrap
        .add_property("_anonymous", &sparta::TreeNode::isAnonymous)
        .add_property("_expired", &sparta::TreeNode::isExpired)
        .add_property("_indexable_by_group", &sparta::TreeNode::isIndexableByGroup)
        .add_property("_location", &sparta::TreeNode::getLocation) // Returns string instance - no need for policy
        .add_property("_level", &sparta::TreeNode::getLevel)
        .add_property("_phase", &sparta::TreeNode::getPhase)
        .add_property("_is_building", &sparta::TreeNode::isBuilding)
        .add_property("_is_built", &sparta::TreeNode::isBuilt)
        .add_property("_is_configured", &sparta::TreeNode::isConfigured)
        .add_property("_is_configuring", &sparta::TreeNode::isConfiguring)
        .add_property("_is_finalized", &sparta::TreeNode::isFinalized)
        .add_property("_is_finalizing", &sparta::TreeNode::isFinalizing)
        .add_property("_is_tearing_down", &sparta::TreeNode::isTearingDown)
        //.def("__getattribute__", &TreeNode___getattribute__)
        .def("__getattr__", &TreeNode___getattr__)
        .def("__setattr__", &TreeNode___setattr__)
        //.def("__contains__", &MyClass::Py_SetAttr)

        .def("__str__", &sparta::TreeNode::stringize, (bp::arg("pretty")=false))
        .def("__repr__", &sparta::TreeNode::stringize, (bp::arg("pretty")=false))

        // Tree Queries
        //   findChildrenByTag
        //   isDescendantOf

        // Tree-Building
        //   addChild

        // Notification functions
        //   getPossibleNotifications
        //   dumpPossibleNotifications
        //   locateNotificationSources
        //   getPossibleSubtreeNotifications
        //   dumpPossibleSubtreeNotifications
        //   canGenerateNotification [x2]
        //   register/deregisterForNnotification registration actions
        //   hasObserversRegisteredForNotification
        //   getDelegatesRegisteredForNotification
        //

        //! \todo: (maybe) Make iterable with __iter__
        ;

    class_<sparta::GlobalTreeNode, bases<sparta::TreeNode>, boost::noncopyable>
        ("GlobalTreeNode")
        ;

    class_<sparta::RootTreeNode, bases<sparta::TreeNode>, boost::noncopyable>
        ("RootTreeNode", init<const std::string&,
         const std::string&,
         sparta::app::Simulation*,
         sparta::GlobalTreeNode*>())
        .add_property("_search_scope",
                      make_function(&sparta::RootTreeNode::getSearchScope,
                                    return_value_policy<WrapperCache<sparta::GlobalTreeNode>>())
                      )

        // Tree-Building phase support
        //   enterConfiguring
        //   enterFinalized
        //   bindTreeEarly
        //   bindTreeLate
        //   enterTeardown
        ;

    class_<sparta::ParameterSet, bases<sparta::TreeNode>, boost::noncopyable>
        ("ParameterSet", init<sparta::TreeNode*>())
        ;

    // Untemplated parameter.
    //! \todo Support specific-value accesses
    class_<sparta::ParameterBase, bases<sparta::TreeNode>, boost::noncopyable>
        ("Parameter", no_init)
        .add_property("value_str", &sparta::ParameterBase::getValueAsString, &sparta::ParameterBase::setValueFromString)
        .add_property("_type_name", &sparta::ParameterBase::getTypeName)
        .add_property("_default_str", &sparta::ParameterBase::getDefaultAsString)
        .def("_get_value_str_at", &sparta::ParameterBase::getValueAsStringAt)
        .def("_get_item_value_str_at", &sparta::ParameterBase::getItemValueFromString)
        //subclass//.def("_ignore", &sparta::ParameterBase::ignore)
        //subclass//.def("_unread", &sparta::ParameterBase::unread)
        .add_property("_is_vector", &sparta::ParameterBase::isVector)
        .add_property("_dimensionality", &sparta::ParameterBase::getDimensionality)
        //subclass//.add_property("_display_base", &sparta::ParameterBase::getNumericDisplayBase, &sparta::ParameterBase::setNumericDisplayBase)

        // set value from python object and cast to internal type
        // get value as internal type and convert to python object
        ;

    // Unnecessary
    //class_<sparta::Parameter<uint32_t>, bases<sparta::ParameterBase>, boost::noncopyable>
    //    ("Parameter_uint32_t", no_init)
    //    ;

    class_<sparta::RegisterSet, bases<sparta::TreeNode>, boost::noncopyable>
        ("RegisterSet", no_init)
        ;

    class_<sparta::Register, bases<sparta::TreeNode>, boost::noncopyable>
        ("Register", no_init)
        .add_property("_reg_id", &sparta::Register::getID)
        .add_property("_reg_group_num", &sparta::Register::getGroupNum)
        .add_property("_reg_group_name", &sparta::Register::getGroupName)
        .add_property("_reg_group_idx", &sparta::Register::getGroupIdx)
        .add_property("_num_bytes", &sparta::Register::getNumBytes)
        .add_property("__len__", &sparta::Register::getNumBytes)
        .add_property("_num_bits", &sparta::Register::getNumBits)
        //.add_property("_num_fields", &sparta::Register::getNumFields) // use len(x._fields)
        .add_property("_banked", &sparta::Register::isBanked)
        .add_property("_fields",
                      make_function(Register_getFields,
                                    return_value_policy<RegisterFieldVectorToPyFieldSmartObj>())
                      )
        .add_property("_subset_of", &sparta::Register::getSubsetOf)
        .add_property("_subset_offset", &sparta::Register::getSubsetOffset)
        .add_property("_hint_flags", &sparta::Register::getHintFlags)
        .add_property("_reg_domain", &sparta::Register::getRegDomain)
        .add_property("_value_str", &sparta::Register::getValueAsByteString)
        .def("_in_bank", &sparta::Register::isInBank)
        .add_property("_value_byte_str", &sparta::Register::getValueAsByteString)
        .add_property("_write_mask_bit_str", &sparta::Register::getWriteMaskAsBitString)
        .add_property("_write_mask_byte_str", &sparta::Register::getWriteMaskAsByteString)

        //! \todo Implement variable-size readers and writers with proper use of templated interface
        //! Large integer writes (e.g. 128b) from Python will have to be transformed into multiple
        //! 64b index writes through SPARTA.
        //! Also implement write by index.
        //.def("_read", &sparta::Register::read)
        //.def("_write", &sparta::Register::write)
        //.def("_peek", &sparta::Register::peek)
        //.def("_poke", &sparta::Register::poke)
        //.def("_get_write_mask", &sparta::Register::getWriteMask) // templated on access

        .def("__getitem__", &Register_getItem) // For: reg['FIELDX'] or reg[0:3] ??
        //.def("__setitem__", &Register_setItem) // For: reg['FIELDX'] = 3
        //.def("__getslice__", &Register_getSlice) // For: reg[8,3] = 23

        //! \todo Implement some more methods
        //.def("_poke_unmasked", &sparta::Register::pokeUnmasked)
        //.def("_mask_value", &sparta::Register::maskValue)

        .def("_get_field", Register_getField,
             return_value_policy<WrapperCache<sparta::Register::Field>>()) // Get by name
        ;

    class_<RegisterFieldAccessSmartObj, boost::shared_ptr<RegisterFieldAccessSmartObj>, boost::noncopyable>
        ("RegisterFieldAccessSmartObj", no_init)
        .def("by_name", &RegisterFieldAccessSmartObj::getByName, return_value_policy<WrapperCache<sparta::Register::Field>>())
        .def("by_index", &RegisterFieldAccessSmartObj::getByIndex, return_value_policy<WrapperCache<sparta::Register::Field>>())
        .def("__getitem__", &RegisterFieldAccessSmartObj::getByIndex, return_value_policy<WrapperCache<sparta::Register::Field>>())
        ;

    class_<sparta::Register::Field, bases<sparta::TreeNode>, boost::noncopyable>
        ("RegisterField", no_init)
        .def_readonly("_low_bit", &sparta::Register::Field::getLowBit)
        .def_readonly("_high_bit", &sparta::Register::Field::getHighBit)
        .def_readonly("_readonly", &sparta::Register::Field::isReadOnly)
        .def_readonly("_bits", &sparta::Register::Field::getNumBits)
        ;

    //! Wrapping sparta::ParameterTree::Node properties and methods.
    class_<sparta::ParameterTree::Node, boost::noncopyable>("Node", no_init)
        // Name of the parameter tree node.
        .add_property("name",
                      make_function(&sparta::ParameterTree::Node::getName,
                                    return_value_policy<copy_const_reference>()))
        // Parent of the parameter tree node.
        .add_property("parent",
                      make_function(Node_getParent,
                                    return_value_policy<WrapperCache<sparta::ParameterTree::Node>>()))
        // Root of the parameter tree node.
        .add_property("root",
                      make_function(Node_getRoot,
                                    return_value_policy<WrapperCache<sparta::ParameterTree::Node>>()))
        // Owner of the parameter tree node.
        .add_property("owner",
                      make_function(Node_getOwner,
                                    return_value_policy<WrapperCache<sparta::ParameterTree>>()))
        // Full path to this node.
        .add_property("path",
                      &sparta::ParameterTree::Node::getPath)
        // Check if node is root.
        .add_property("is_root",
                      &sparta::ParameterTree::Node::isRoot)
        // Check read count of node.
        .add_property("read_count",
                      &sparta::ParameterTree::Node::getReadCount)
        // Check if node has value.
        .add_property("has_value",
                      &sparta::ParameterTree::Node::hasValue)
        // Get vector of children nodes.
        .add_property("children",
                      make_function(Node_getChildren,
                                    return_value_policy<ParameterTreeNodePtrVecToPyNodeList>()))
        // Get value of node.
        .add_property("value",
                      make_function(&sparta::ParameterTree::Node::getValue,
                                    return_value_policy<copy_const_reference>()))
        // Get origin of node.
        .add_property("origin",
                      make_function(&sparta::ParameterTree::Node::getValue,
                                    return_value_policy<copy_const_reference>()))
        // Check if node is required.
        .add_property("is_required",
                      &sparta::ParameterTree::Node::isRequired)
        // Get required count of node.
        .add_property("required_count",
                      &sparta::ParameterTree::Node::getRequiredCount)
        // Read the node once more.
        .def("read",
             &sparta::ParameterTree::Node::incrementReadCount)
        // Show information from node.
        .def("showInfo",
             +[](sparta::ParameterTree::Node& node){
                 return node.recursePrint(std::cout, 0);})
        // Set this node with a string value.
        .def("setValue",
             +[](sparta::ParameterTree::Node& node, const std::string& val,
                 bool required = true, const std::string& origin = ""){
                 sparta_assert(node.getChildren().empty(),
                             "Setting value to a non-leaf node is not allowed.");
                 return node.setValue(val, required, origin);},
             (bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Set this node with a double value.
        .def("setValue",
             +[](sparta::ParameterTree::Node& node, const double& val,
                 bool required = true, const std::string& origin = ""){
                 sparta_assert(node.getChildren().empty(),
                             "Setting value to a non-leaf node is not allowed.");
                 return node.setValue(std::to_string(val), required, origin);},
             (bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Set this node with an array of values.
        .def("setValue",
             +[](sparta::ParameterTree::Node& node, const bp::list& val,
                 bool required = true, const std::string& origin = ""){
                 sparta_assert(node.getChildren().empty(),
                             "Setting value to a non-leaf node is not allowed.");
                 if(len(val) == 0){
                     return node.setValue("", required, origin);
                 }
                 std::stringstream ss;
                 std::string token;
                 std::size_t i;
                 for(i = 0; i < static_cast<std::size_t>(len(val) - 1); ++i){
                     token = bp::extract<std::string>(str(val[i]));
                     ss << token << ", ";
                 }
                 token = bp::extract<std::string>(str(val[i]));
                 ss << token;
                 return node.setValue(ss.str(), required, origin);},
             (bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Create node and set with a string value.
        .def("setChild",
             +[](sparta::ParameterTree::Node& node, const std::string& path,
                 const std::string& val, bool required, const std::string& origin = ""){
                 std::string full_path = node.getPath();

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 if(!full_path.empty() && !non_const_path.empty()){
                     full_path += ".";
                 }
                 full_path += non_const_path;
                 const auto find_node = node.getOwner()->tryGet(full_path, false);
                 if(find_node){
                     sparta_assert(find_node->getChildren().empty(),
                                 "Setting value to non-leaf node is not allowed.");
                 }

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(full_path, std::addressof(*node.getOwner()));
                 return node.set(non_const_path, val, required, origin);
             },
             (bp::arg("path"), bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Create node and set with a double value.
        .def("setChild",
             +[](sparta::ParameterTree::Node& node, const std::string& path,
                 const double& val, bool required, const std::string& origin = ""){
                 std::string full_path = node.getPath();

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 if(!full_path.empty() && !non_const_path.empty()){
                     full_path += ".";
                 }
                 full_path += non_const_path;
                 const auto find_node = node.getOwner()->tryGet(full_path, false);
                 if(find_node){
                     sparta_assert(find_node->getChildren().empty(),
                                 "Setting value to non-leaf node is not allowed.");
                 }

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(full_path, std::addressof(*node.getOwner()));
                 return node.set(non_const_path, std::to_string(val), required, origin);
             },
             (bp::arg("path"), bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Create node and set with an array of values.
        .def("setChild",
             +[](sparta::ParameterTree::Node& node, const std::string& path,
                 const bp::list& val, bool required, const std::string& origin = ""){
                 std::string full_path = node.getPath();

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 if(!full_path.empty() && !non_const_path.empty()){
                     full_path += ".";
                 }
                 full_path += non_const_path;
                 const auto find_node = node.getOwner()->tryGet(full_path, false);
                 if(find_node){
                     sparta_assert(find_node->getChildren().empty(),
                                 "Setting value to non-leaf node is not allowed.");
                 }
                 if(len(val) == 0){
                     return node.set(non_const_path, "", required, origin);
                 }
                 std::stringstream ss;
                 std::string token;
                 std::size_t i;
                 for(i = 0; i < static_cast<std::size_t>(len(val) - 1); ++i){
                     token = bp::extract<std::string>(str(val[i]));
                     ss << token << ", ";
                 }
                 token = bp::extract<std::string>(str(val[i]));
                 ss << token;

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(full_path, std::addressof(*node.getOwner()));
                 return node.set(non_const_path, ss.str(), required, origin);
             },
             (bp::arg("path"), bp::arg("val"), bp::arg("required") = true, bp::arg("origin") = ""))
        // Get a child for setting parameter, creating it, if needed.
        .def("createChild",
             +[](sparta::ParameterTree::Node& node, const std::string& path,
                 bool required){
                 std::string full_path = node.getPath();

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 if(!full_path.empty() && !non_const_path.empty()){
                     full_path += ".";
                 }
                 full_path += non_const_path;

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(full_path, std::addressof(*node.getOwner()));
                 return node.create(non_const_path, required);},
             (bp::arg("path"), bp::arg("required") = true),
             return_value_policy<WrapperCache<sparta::ParameterTree::Node>>())
        // Create a child of this node as most recent.
        .def("addChild",
             +[](sparta::ParameterTree::Node& node, const std::string& name,
                 bool required){
                 std::string full_path = node.getPath();

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_name = sparta::utils::eliminate_whitespace(name);

                 if(!full_path.empty() && !non_const_name.empty()){
                     full_path += ".";
                 }
                 full_path += non_const_name;

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(full_path, std::addressof(*node.getOwner()));
                 return node.addChild(non_const_name, required);},
             (bp::arg("name"), bp::arg("required") = true),
             return_value_policy<WrapperCache<sparta::ParameterTree::Node>>())
        // Increment the required count of node.
        .def("increaseRequired",
             &sparta::ParameterTree::Node::incRequired)
        // Unrequire this node and all its children.
        .def("unrequire",
             &sparta::ParameterTree::Node::unrequire)
        // Append another tree as a child of this node.
        .def("appendTree",
             &sparta::ParameterTree::Node::appendTree)
        // Get child node by concrete path name.
        .def("getChild",
             +[](sparta::ParameterTree::Node& node, const std::string& name){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_name = sparta::utils::eliminate_whitespace(name);

                 return node.getChild(non_const_name);},
             return_value_policy<WrapperCache<sparta::ParameterTree::Node>>())
        // Dunder method __repr__ of node class.
        .def("__repr__",
             +[](sparta::ParameterTree::Node& node){
                 std::ostringstream oss;
                 node.dump(oss);
                 return oss.str();})
        // Dunder method __str__ of node class.
        .def("__str__",
             +[](sparta::ParameterTree::Node& node){
                 std::ostringstream oss;
                 node.dump(oss);
                 return oss.str();})
        // Store methods which will be killed after Configuration phase.
        .add_property("__cull_methods__",
                      make_function(
                                    +[](sparta::ParameterTree::Node&){
                                        std::vector<std::string> write_method_list {
                                            "appendTree", "unrequire", "increaseRequired",
                                                "addChild", "createChild", "setChild", "setValue", "read"};
                                        return write_method_list;},
                                    return_value_policy<StrVecToPyStrList>()));

    //! Wrapping sparta::ParameterTree properties and methods.
    class_<sparta::ParameterTree, boost::noncopyable>("ParameterTree", init<>())
        // Get root of this tree.
        .add_property("root",
                      make_function(PTree_getRoot,
                                    return_value_policy<WrapperCache<sparta::ParameterTree::Node>>()))
        // Clear all content from this tree.
        .def("clear",
             &sparta::ParameterTree::clear)
        // Check of node from path is required.
        .def("isRequired",
             +[](sparta::ParameterTree& p_tree, const std::string& path){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 return p_tree.isRequired(non_const_path);})
        // Unrequire a node from this tree.
        .def("unrequire",
             +[](sparta::ParameterTree& p_tree, const std::string& path){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 p_tree.unrequire(non_const_path);})
        // Check if node has been read.
        .def("isRead",
             +[](sparta::ParameterTree& p_tree, const std::string& path){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 return p_tree.isRead(non_const_path);})
        // Pretty print this tree.
        .def("showTree",
             +[](sparta::ParameterTree& ptree){
                 return ptree.recursePrint(std::cout);})
        // Check if node from path has value.
        .def("hasValue",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 const bool must_be_leaf){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 return p_tree.hasValue(non_const_path, must_be_leaf);},
             (bp::arg("path"), bp::arg("must_be_leaf") = false))
        // Check if node from path exists.
        .def("exists",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 bool must_be_leaf){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 return p_tree.exists(non_const_path, must_be_leaf);},
             (bp::arg("path"), bp::arg("must_be_leaf") = false))
        // Get number of nodes in tree with values but not read yet.
        .def("unread_nodes",
             PTree_getUnreadValueNodes)
        // Merge two trees together.
        .def("merge",
             &sparta::ParameterTree::merge)
        // Get node with concrete path.
        .def("getNode",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 bool must_be_leaf){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);
                 return p_tree.tryGet(non_const_path, must_be_leaf);},
             (bp::arg("path"), bp::arg("must_be_leaf") = false),
             return_value_policy<WrapperCache<sparta::ParameterTree::Node>>())
        // Create and set value to node from concrete path with str value.
        .def("set",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 const std::string& value, bool required, const std::string& origin = ""){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(non_const_path, std::addressof(p_tree));
                 return p_tree.set(non_const_path, value, required, origin);},
             (bp::arg("path"), bp::arg("value"), bp::arg("required") = true,
              bp::arg("origin") = ""))
        // Create and set value to node from concrete path with double value.
        .def("set",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 const double value, bool required, const std::string& origin = ""){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(non_const_path, std::addressof(p_tree));
                 return p_tree.set(non_const_path, std::to_string(value), required, origin);},
             (bp::arg("path"), bp::arg("value"), bp::arg("required") = true,
              bp::arg("origin") = ""))
        // Create and set value to node from concrete path with array of values.
        .def("set",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 const bp::list& value, bool required, const std::string& origin = ""){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(non_const_path, std::addressof(p_tree));
                 if(len(value) == 0){
                     return p_tree.set(non_const_path, "", required, origin);
                 }
                 std::stringstream ss;
                 std::string token;
                 std::size_t i;
                 for(i = 0; i < static_cast<std::size_t>(len(value) - 1); ++i){
                     token = bp::extract<std::string>(str(value[i]));
                     ss << token << ", ";
                 }
                 token = bp::extract<std::string>(str(value[i]));
                 ss << token;
                 return p_tree.set(non_const_path, ss.str(), required, origin);},
             (bp::arg("path"), bp::arg("value"), bp::arg("required") = true,
              bp::arg("origin") = ""))
        // Create node fron concrete path.
        .def("create",
             +[](sparta::ParameterTree& p_tree, const std::string& path,
                 bool required){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_path = sparta::utils::eliminate_whitespace(path);

                 // Send the new path and node information to the regex-engine and update it.
                 sparta::updateCompleter(non_const_path, std::addressof(p_tree));
                 return p_tree.create(non_const_path, required);},
             (bp::arg("path"), bp::arg("required") = false),
             return_value_policy<WrapperCache<sparta::ParameterTree::Node>>())
        // Store methods which will be killed after Configuration phase.
        .add_property("__cull_methods__",
                      make_function(
                                    +[](sparta::ParameterTree&){
                                        std::vector<std::string> write_method_list {
                                            "set", "create", "merge",
                                                "unrequire", "clear"};
                                        return write_method_list;},
                                    return_value_policy<StrVecToPyStrList>()));

    class_<sparta::app::SimulationConfiguration, boost::noncopyable>("SimulationConfiguration", no_init)
        //Setting parameter values and applying config/arch yamls:
        //    * Note that there are two overloads, each of which has a third optional argument...
        //          void processParameter(const std::string &, const std::string &, const bool = false)
        //          void processParameter(const std::string &, const double, const bool = false)
        .def("processParameter",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& pattern,
                 const std::string& value, const bool optional = false){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_pattern = sparta::utils::eliminate_whitespace(pattern);
                 sim_config.processParameter(non_const_pattern, value, optional);

                 // Build the Regex-engine from the parameter string and value.
                 buildCompleter(sim_config.getUnboundParameterTree(),
                                std::addressof((sim_config.*Sim_getTreeNonConst)()),
                                "parameter", non_const_pattern);
             },
             (bp::arg("pattern"), bp::arg("value"), bp::arg("optional") = false))
        .def("processParameter",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& pattern,
                 const double value, const bool optional = false){

                 // Remove spaces from path as C++ would not accept this.
                 std::string non_const_pattern = sparta::utils::eliminate_whitespace(pattern);
                 sim_config.processParameter(non_const_pattern, std::to_string(value), optional);

                 // Build the Regex-engine from the parameter string and value.
                 buildCompleter(sim_config.getUnboundParameterTree(),
                                std::addressof((sim_config.*Sim_getTreeNonConst)()),
                                "parameter", non_const_pattern);
             },
             (bp::arg("pattern"), bp::arg("value"), bp::arg("optional") = false))
        .def("processConfigFile",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& pattern,
                 const std::string& filename, bool is_final = true){
                 sim_config.processConfigFile(pattern, filename, is_final);

                 // Build the Regex-engine from the config-file.
                 buildCompleter(sim_config.getUnboundParameterTree(),
                                std::addressof((sim_config.*Sim_getTreeNonConst)()),
                                "parameter");
             },
             (bp::arg("pattern"), bp::arg("filename"), bp::arg("optional") = false))
        .def("processArch",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& filename){
                 sim_config.processArch("", filename);

                 // Build the Regex-engine from the arch-file.
                 buildCompleter(sim_config.getArchUnboundParameterTree(),
                                std::addressof((sim_config.*Sim_getArchTreeNonConst)()),
                                "architecture");
             })
        //Adding include paths to find your yaml files:
        .def("addArchSearchPath",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& dir){
                 sim_config.addArchSearchPath(dir);})
        .def("addConfigSearchPath",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& dir){
                 sim_config.addConfigSearchPath(dir);})
        //Metadata & printing utilities:
        .def("addRunMetadata",
             +[](sparta::app::SimulationConfiguration& sim_config, const std::string& name,
                 const std::string& value){
                 sim_config.addRunMetadata(name, value);})
        .def("stringizeRunMetadata",
             +[](sparta::app::SimulationConfiguration& sim_config){
                 return sim_config.stringizeRunMetadata();})
        //Inspecting your configurations:
        .add_property("config_ptree",
                      make_function(Sim_getTreeNonConst,
                                    return_value_policy<WrapperCache<sparta::ParameterTree>>()))
        .add_property("arch_ptree",
                      make_function(Sim_getArchTreeNonConst,
                                    return_value_policy<WrapperCache<sparta::ParameterTree>>()))
        // Store methods which will be killed after Configuration phase.
        .add_property("__cull_methods__",
                      make_function(
                                    +[](sparta::app::SimulationConfiguration&){
                                        std::vector<std::string> write_method_list {
                                            "processParameter", "processArch", "addArchSearchPath",
                                                "addConfigSearchPath", "addRunMetadata", "processConfigFile"};
                                        return write_method_list;},
                                    return_value_policy<StrVecToPyStrList>()));

    class_<sparta::facade::ReportTriggers>("ReportTriggers", no_init)
        .def(init<const sparta::facade::ReportTriggers &>())
        .def("addTrigger", raw_function(&sparta::facade::ReportTriggers::addTrigger, 1))
        .add_property("start_trigger",
                      make_function(&sparta::facade::ReportTriggers::getStartTrigger,
                                    return_internal_reference<>())
                      )
        .add_property("update_trigger",
                      make_function(&sparta::facade::ReportTriggers::getUpdateTrigger,
                                    return_internal_reference<>())
                      )
        .add_property("stop_trigger",
                      make_function(&sparta::facade::ReportTriggers::getStopTrigger,
                                    return_internal_reference<>())
                      )
        .def("showInfo", &sparta::facade::ReportTriggers::showInfo)
        ;

    class_<sparta::facade::ReportTrigger>("ReportTrigger", no_init)
        .def(init<const sparta::facade::ReportTrigger &>())
        .add_property("expression",
                      make_function(&sparta::facade::ReportTrigger::getExpression,
                                    return_value_policy<copy_const_reference>())
                      )
        .def("enable", &sparta::facade::ReportTrigger::enable)
        .def("disable", &sparta::facade::ReportTrigger::disable)
        .add_property("enabled", &sparta::facade::ReportTrigger::isEnabled)
        .add_property("type", &sparta::facade::ReportTrigger::getType)
        .def("showInfo", &sparta::facade::ReportTrigger::showInfo)
        ;

    enum_<sparta::facade::ReportTrigger::Type>("TriggerType")
        .value("START", sparta::facade::ReportTrigger::Type::START)
        .value("UPDATE_COUNT", sparta::facade::ReportTrigger::Type::UPDATE_COUNT)
        .value("UPDATE_CYCLES", sparta::facade::ReportTrigger::Type::UPDATE_CYCLES)
        .value("UPDATE_TIME", sparta::facade::ReportTrigger::Type::UPDATE_TIME)
        .value("STOP", sparta::facade::ReportTrigger::Type::STOP)
        ;

    class_<sparta::app::ReportConfiguration, boost::noncopyable>("ReportConfiguration", no_init)
        .add_property("descriptors",
                      make_function(ReportConfiguration_getDescriptors,
                                    return_value_policy<
                                    WrapperCache<sparta::app::ReportDescriptorCollection>>())
                      )
        .def("addReport", &sparta::app::ReportConfiguration::addReport)
        .def("addReportsFromYaml", &sparta::app::ReportConfiguration::addReportsFromYaml)
        .def("removeReport", &sparta::app::ReportConfiguration::removeReportByName)
        .def("addMemoryReportsFromYaml", &sparta::app::ReportConfiguration::addMemoryReportsFromYaml)
        .def("toYaml", &sparta::app::ReportConfiguration::serializeAllDescriptorsToYaml)
        .def("showInfo", &sparta::app::ReportConfiguration::showAllReportDescriptorInfo)
        ;

    class_<sparta::app::ReportDescriptorCollection, boost::noncopyable>("ReportDescriptors", no_init)
        .def("__getattr__", &ReportDescriptors__getattr__)
        .def("__setattr__", &ReportDescriptors__setattr__)
        ;

    class_<sparta::app::ReportDescriptor>("ReportDescriptor", no_init)
        .def("__init__", raw_function(ReportDescriptor_ctor_with_kwargs),
             "ReportDescriptor constructor with kwargs")
        .def(init<std::string, std::string, std::string, std::string>())
        .def(init<const sparta::app::ReportDescriptor &>())
        .def_readwrite("def_file", &sparta::app::ReportDescriptor::def_file)
        .def_readwrite("dest_file", &sparta::app::ReportDescriptor::dest_file)
        .def_readwrite("pattern", &sparta::app::ReportDescriptor::loc_pattern)
        .def_readwrite("format", &sparta::app::ReportDescriptor::format)
        .add_property("triggers",
                      make_function(&sparta::facade::getTriggers,
                                    return_internal_reference<>())
                      )
        .def("toYaml", &sparta::facade::serializeDescriptorToYaml)
        .def("showInfo", &sparta::facade::showReportDescriptorInfo)
        ;

    def("importArchives", &StatisticsArchives__import,
        return_value_policy<WrapperCache<sparta::statistics::StatisticsArchives>>())
        ;

    class_<sparta::statistics::StatisticsArchives, boost::noncopyable>("StatisticsArchives", no_init)
        .def(init<std::string>())
        .def("__getattr__", &StatisticsArchives__getattr__)
        .def("saveTo", &sparta::statistics::StatisticsArchives::saveTo)
        ;

    class_<sparta::statistics::RootArchiveNode, boost::noncopyable>("RootArchiveNode", no_init)
        .def("__getattr__", &RootArchiveNode__getattr__)
        .def("__setattr__", &RootArchiveNode__setattr__)
        ;

    class_<sparta::statistics::ArchiveNode, boost::noncopyable>("ArchiveNode", no_init)
        .def("__getattr__", &ArchiveNode__getattr__)
        .def("__setattr__", &ArchiveNode__setattr__)
        ;

    class_<sparta::statistics::ArchiveDataSeries, boost::noncopyable>("ArchiveDataSeries", no_init)
        .def("__getitem__", &sparta::statistics::ArchiveDataSeries::getValueAt)
        .def("getDataInRange", &ArchiveDataSeries__getRange)
        .def("getAllData", &ArchiveDataSeries__getAllData)
        ;

    class_<sparta::statistics::StatisticsStreams, boost::noncopyable>("StatisticsStreams", no_init)
        .def("__getattr__", &StatisticsStreams__getattr__)
        ;

    class_<sparta::statistics::StreamNode, boost::noncopyable>("StreamNode", no_init)
        .def("__getattr__", &StreamNode__getattr__)
        .def("__setattr__", &StreamNode__setattr__)
        .def("__str__", &StreamNode__str__)
        .def("getFullPath", &StreamNode__getFullPath)
        .def("getBufferedData", &StreamNode__getBufferedData)
        .def("streamTo", raw_function(&StreamNode__streamTo, 2))
        ;

    class_<simdb::ObjectManager, boost::noncopyable>("SimulationDatabase", no_init)
        .def("__getattr__", &SimulationDatabase__getattr__)
        .def("toAutoSummary", &SimulationDatabase__createReport<SimDBReportType::AutoSummary>)
        .def("toJson", &SimulationDatabase__createReport<SimDBReportType::Json>)
        .def("toJsonReduced", &SimulationDatabase__createReport<SimDBReportType::JsonReduced>)
        .def("toJsonDetail", &SimulationDatabase__createReport<SimDBReportType::JsonDetail>)
        .def("toJsJson", &SimulationDatabase__createReport<SimDBReportType::JsJson>)
        .def("toHtml", &SimulationDatabase__createReport<SimDBReportType::Html>)
        .def("toText", &SimulationDatabase__createReport<SimDBReportType::Text>)
        .def("toDictionary", &SimulationDatabase__createReport<SimDBReportType::PyDictionary>)
        .def("toGnuPlot", &SimulationDatabase__createReport<SimDBReportType::GnuPlot>)
        .def("toStatsMapping", &SimulationDatabase__createReport<SimDBReportType::StatsMapping>)
        ;

    class_<simdb::AsyncTaskEval, boost::noncopyable>("DatabaseQueue", no_init)
        ;

    def("connectToDatabase", &ReportTimeseries__connectToDatabase,
        return_value_policy<WrapperCache<simdb::ObjectManager>>())
        ;

    def("__printFailedVerificationSummaries", &SimulationDatabase__printAllVerificationSummaries)
        ;

    def("__getFailedVerificationFiles", &SimulationDatabase__getVerificationFailuresInDir)
        ;

    class_<sparta::db::ReportTimeseries, boost::noncopyable>("ReportTimeseries", no_init)
        .def("getHeader", &sparta::db::ReportTimeseries::getHeader,
             return_internal_reference<>())
        .def("getValuesInTimeRange", &ReportTimeseries__getValuesInTimeRange)
        .def("getValuesInClockRange", &ReportTimeseries__getValuesInClockRange)
        .def("getAllValues", &ReportTimeseries__getAllValues)
        .def("toCSV", &ReportTimeseries__toCSV)
        ;

    class_<sparta::db::ReportHeader, boost::noncopyable>("ReportHeader", no_init)
        .def("getReportName", &sparta::db::ReportHeader::getReportName)
        .def("getReportStartTime", &sparta::db::ReportHeader::getReportStartTime)
        .def("getReportEndTime", &sparta::db::ReportHeader::getReportEndTime)
        .def("setStringMetadata", &sparta::db::ReportHeader::setStringMetadata)
        .def("getStringMetadata", &sparta::db::ReportHeader::getStringMetadata)
        ;

    class_<sparta::app::Simulation, boost::noncopyable>("Simulation", no_init)
        .add_property("root", // "top" object, typically
                      make_function(Simulation_getRoot,
                                    return_value_policy<WrapperCache<sparta::RootTreeNode>>()) // Defer to wrapper cache
                      )
        ////const ParameterTree& getUnboundParameterTree() { return ptree_; }
        .add_property("ready_to_run", &sparta::app::Simulation::readyToRun)
        ////ResourceSet* getResourceSet() noexcept { return &res_list_; }
        .add_property("sim_name",
                      make_function(&sparta::app::Simulation::getSimName,
                                    return_value_policy<copy_const_reference>())
                      )

        // Published to global namespace by PythonInterpreter
        .add_property("rc",
                      make_function(&sparta::app::Simulation::getRunControlInterface,
                                    return_value_policy<WrapperCache<sparta::control::TemporaryRunControl>>())
                      )
        .add_property("bm", &getBreakpointManager);

    // Status hooks
    // build phase
    // configuration phase
    // finalizing phase
    // bind phase
    // first run!
    // end-of-running (or exception state)
    // teardown (no more safe access) (maybe not a state so much as halting the python interpreter)
    ;

    // A const sparta Clock
    //! \todo Like most classes here, this may need to be non-const with a ctor if it is going
    //! to support tree-building in python
    class_<sparta::Clock, bases<sparta::TreeNode>, boost::noncopyable>
        ("Clock", no_init)
        ;

    using sparta::control::TemporaryRunControl;

    uint64_t (TemporaryRunControl::*TemporaryRunControl_getCurrentCycle_byname)(const std::string&) const = &TemporaryRunControl::getCurrentCycle;
    uint64_t (TemporaryRunControl::*TemporaryRunControl_getCurrentCycle_byptr)(const sparta::Clock*) const = &TemporaryRunControl::getCurrentCycle;

    void (TemporaryRunControl::*TemporaryRunControl_runc_byname)(uint64_t cycles_max, const std::string&) = &TemporaryRunControl::runc;
    void (TemporaryRunControl::*TemporaryRunControl_runc_byptr)(uint64_t cycles_max, const sparta::Clock*) = &TemporaryRunControl::runc;

    class_<TemporaryRunControl, boost::noncopyable>("TemporaryRunControl", no_init)

        .def("get_curtick", &TemporaryRunControl::getCurrentTick)
        .def("get_curcycle", TemporaryRunControl_getCurrentCycle_byptr)
        .def("get_curcycle", TemporaryRunControl_getCurrentCycle_byname, (bp::arg("clk")=bp::object()))
        .def("get_curinst", &TemporaryRunControl::getCurrentInst)

        .def("runi", &TemporaryRunControl::runi, (bp::arg("instructions_max")=std::numeric_limits<uint64_t>::max()))
        .def("runc", TemporaryRunControl_runc_byname, (bp::arg("cycles_max"), bp::arg("clk_name")))
        //.def("runc", TemporaryRunControl_runc_byptr, (bp::arg("cycles_max"), bp::arg("clk")=bp::object())) // default null (root) clock
        .def("runc", TemporaryRunControl_runc_byptr, (bp::arg("cycles_max"), bp::arg("clk")))
        //.def("runx", TemporaryRunControl_runx_byname, ...
        //.def("runx", TemporaryRunControl_runx_byptr, ...
        .def("run", &TemporaryRunControl::run, "Run the simulator indefinitely. Simulation can be stopped by Ctrl+C or by any breakpoints or triggers") // unconstrained run
        .def("run", TemporaryRunControl_run_rejectargs_1, "") // Run function with 1 arg that rejects the call with an exception
        .def("run", TemporaryRunControl_run_rejectargs_2, "") // Run function with 2 args that rejects the call with an exception
        //.def("stop" // doc about being disabled

        // Single-Stepping
        //.def("stepi") // step 1 instruction
        //.def("stepc") //  step 1 cycle
        //.def("stept") // step 1 tick
        //.def("step_sched_evtgrp") // step scheduler event group
        //.def("step_sched_evt") // step scheduler event

        //.def("state") // Formatted string containing tick/cycle/clocks, etc...

        ;

    class_<sparta::python::PythonInterpreter, boost::noncopyable>("PythonInterpreter", no_init)
        .def("exit", &sparta::python::PythonInterpreter::asyncExit, (bp::arg("exit_code")=0))
        .def("_hook_shell_initialized", &sparta::python::PythonInterpreter::IPyShellInitialized)
        .def("_hook_pre_prompt", &sparta::python::PythonInterpreter::IPyPrePrompt)
        ;

    // Include export file for dynamic pipeline execution
    #include "DynamicEffort.hpp"

    //to_python_converter<std::shared_ptr<sparta::RootTreeNode>, SimpleConverter>();
}
