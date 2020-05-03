// <interpreter_inst> -*- C++ -*-


/*!
 * \file interpreter_inst.cpp
 * \brief Instantiates python interpreter instance
 */

#include <cstdio>
#include <thread>
#include <boost/bind.hpp>
#include <boost/ref.hpp>

#include "sparta/sparta.hpp" // For global defines
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/app/Simulation.hpp"
#include "python/sparta_support/PythonInterpreter.hpp"
#include "python/sparta_support/module_sparta.hpp"
#include "sparta/control/TemporaryRunController.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/statistics/dispatch/archives/StatisticsArchives.hpp"
#include "sparta/statistics/dispatch/streams/StreamController.hpp"
#include "simdb/async/AsyncTaskEval.hpp"

static const std::thread::id main_thread_id = std::this_thread::get_id();

//! Temporary method used to ensure that all calls into the
//! PythonStreamController object are done on the main thread.
//! The C++/Python communication is not thread safe yet.
inline bool IsCalledFromMainThread()
{
    return (std::this_thread::get_id() == main_thread_id);
}

wchar_t* getWC(char *c)
{
    const size_t c_size = strlen(c) + 1;
    wchar_t* wc = new wchar_t[c_size];
    mbstowcs(wc, c, c_size);
    return wc;
}

#include <boost/python.hpp>
#include <boost/python/ptr.hpp>
#include <pythonrun.h>

// in module_sparta
PyMODINIT_FUNC PyInit_sparta(void);

namespace bp = boost::python;

namespace sparta {

namespace python {

PythonInterpreter* PythonInterpreter::SingleInstanceForce::curinstance_ = nullptr;

bp::object getGlobalNS() {
    bp::object main = bp::import("__main__");
    return main.attr("__dict__");
}

/*!
 * \brief C++/Python implementation of the report stream controller
 * interface. This will forward requests from the simulation / run
 * controller to Python clients to start/stop consuming data, grab
 * and send pending SI data to registered Python objects, etc.
 */
class PythonStreamController : public statistics::StreamController
{
private:
    /*!
     * \brief For now, all Python stream objects are flushed in the
     * main thread. We ultimately may use a dedicated thread to
     * process streams, or use a Python event loop to poll the SI
     * values buffer at a fixed interval for new data.
     */
    void startStreaming_() override {
    }

    /*!
     * \brief Tell the Python stream manager singleton to grab
     * all pending / buffered SI data and forward it along to
     * its registered Python client sinks.
     */
    void processStreams_() override {
        sparta_assert(IsCalledFromMainThread(),
                    "Python cannot be invoked off the main thread yet!");

        auto global_ns = getGlobalNS();
        auto mgr_pyobj = global_ns["__stream_manager"];
        mgr_pyobj.attr("processStreams")();
    }

    /*!
     * \brief As long as Python clients are fed data from the main
     * thread, our start/stop streaming methods have nothing they
     * need to do.
     */
    void stopStreaming_() override {
    }
};

void sigint_handler(int sig_num, siginfo_t * info, void * ucontext)
{
    if(sig_num != SIGINT){
        return; // Should not have gotten this handler called in this case
    }

    if(PythonInterpreter::SingleInstanceForce::getCurInstance()){
        PythonInterpreter::SingleInstanceForce::getCurInstance()->handleSIGINT(info, ucontext);
    }
}

PythonInterpreter::PythonInterpreter(const std::string& progname, const std::string& homedir,
                                     int argc, char** argv) :
    sif_(this),
    ipython_inst_(nullptr, [](PyObject* p)->void{Py_XDECREF(p);})
{
    sparta_assert(false == Py_IsInitialized(),
                "Attempted to initialize Python when already initialized with name \""
                << Py_GetProgramName() << "\"");

    progname_.reset(new char[progname.size() + 1]);
    strcpy(progname_.get(), progname.c_str());
    std::basic_string<wchar_t> temp_progname(progname.begin(), progname.end());
    Py_SetProgramName(temp_progname.c_str());

    // Set homedir if specified. Otherwise, this is retrieved from the PYTHONHOME envar
    if(homedir.size() != 0){
        homedir_.reset(new char[homedir.size() + 1]);
        strcpy(homedir_.get(), homedir.c_str());
        std::basic_string<wchar_t> temp_homedir(homedir.begin(), homedir.end());
        Py_SetProgramName(temp_homedir.c_str());
    }

    PyImport_AppendInittab("sparta", &PyInit_sparta);

    // Actually initialize. Fatal error on failure
    Py_Initialize();

    sparta_assert(argc > 0, "Must specify argc/argv in Python interpreter to initialize sys.argv");
    sparta_assert(argv != nullptr, "Null argv with nonzero argc (" << argc << ")");

    wchar_t* temp_argv = getWC(*argv);
    PySys_SetArgvEx(argc, &temp_argv, 0 /*do not update sys.path with argv[0] or cwd*/);

    // RAII GIL example
    //{
    //    LocalGIL g; (void) g;
    //
    //    // WORK ...
    //}

    // Test
    //PyThreadState* tstate = PyEval_SaveThread();
    //PyEval_RestoreThread(tstate);
    //PyThreadState* tstate = PyThreadState_Get();

    // Multiple interpreters
    //PyThreadState* tstate = Py_NewInterpreter();
    //Py_EndInterpreter(tstate)

    // Look into functiosn for profiling tracing of user scripts
    //void PyEval_SetProfile(Py_tracefunc func, PyObject *obj)
    //void PyEval_SetTrace(Py_tracefunc func, PyObject *obj)
    //PyObject* PyEval_GetCallStats(PyObject *self)

    PyRun_SimpleString("print('Initialized SPARTA PythonInterpreter')");

    // Load the "sparta" module
    PyInit_sparta();

    // Test importing stuff
    //PyObject* pName = PyUnicode_FromString("numpy");
    //sparta_assert(pName);
    //PyObject* pModule = PyImport_Import(pName);
    //sparta_assert(pModule);
    //// would put module into user namespace
    //Py_DECREF(pModule);
    //Py_DECREF(pName);
    //pName = nullptr;

    // Install Ctrl+C signal handler
    sigint_act_.sa_handler = nullptr;
    sigint_act_.sa_sigaction = sigint_handler;
    sigemptyset(&sigint_act_.sa_mask);
    sigint_act_.sa_flags = SA_RESTART | SA_SIGINFO;

    #ifndef __APPLE__
    sigint_act_.sa_restorer = nullptr;
    #endif

    if(sigaction(SIGINT, &sigint_act_, &sigint_next_) != 0){
        // Exit ctor on failure to replace sigint - prevent dtor from restoring it
        throw SpartaException("error setting signal handler for: ") << strsignal(SIGINT)
                                                                  << " errno:" << errno;
    }

    // Publish the python interpreter to the global namespace
    auto global_ns = getGlobalNS();
    try{
        //main_namespace[name.c_str()] = bp::ptr(sim);
        global_ns["sparta_pyinterp"] = WrapperCache<PythonInterpreter>().wrap(this);
    }catch(bp::error_already_set){
        PyErr_Print();
        //throw sparta::SpartaException("Could not publish Simulation");
    }
}

PythonInterpreter::~PythonInterpreter() {
    // Remove custom SIGINT handler and re-install python handler
    if(sigaction(SIGINT, &sigint_next_, (struct sigaction*)nullptr) != 0) {
        std::cerr << "Warning: Failed to restore current sigaction for " << strsignal(SIGINT)
                  << ". Restoring old action anyway" << std::endl;
    }

    if(Py_IsInitialized()){

        // Clear and destruct the boost::python objects in this map otherwise
        // python GC will destroy those first and then C++ would try to do the same
        // at the very end and Seg-Fault.
        WrapperMap::wrapper_map_.clear();
        Py_Finalize();
    }
}

std::string PythonInterpreter::getExecPrefix() const {
    std::basic_string<wchar_t> temp(Py_GetExecPrefix());
    std::basic_string<char> rstring(temp.begin(), temp.end());
    return rstring;
}

std::string PythonInterpreter::getPythonFullPath() const {
    std::basic_string<wchar_t> temp(Py_GetProgramFullPath());
    std::basic_string<char> rstring(temp.begin(), temp.end());
    return rstring;
}

std::string PythonInterpreter::getPath() const {
    std::basic_string<wchar_t> temp(Py_GetPath());
    std::basic_string<char> rstring(temp.begin(), temp.end());
    return rstring;
}

std::string PythonInterpreter::getVersion() const {
    return Py_GetVersion();
}

std::string PythonInterpreter::getPlatform() const {
    return Py_GetPlatform();
}

std::string PythonInterpreter::getCompiler() const {
    return Py_GetCompiler();
}

void PythonInterpreter::publishSimulationConfiguration(sparta::app::SimulationConfiguration * sim_config)
{
    sparta_assert(sim_config, "Cannot publish null app::SimulationConfiguration object "
                "to Python environment");
    sparta_assert(Py_IsInitialized(),
                "Attempted to publish simulation configuration object when Python was "
                "not initialized");

    auto global_ns = getGlobalNS();

    try{
        sim_config->addArchSearchPath(".");
        sim_config->addConfigSearchPath(".");
        global_ns["sim_config"] = WrapperCache<sparta::app::SimulationConfiguration>().wrap(sim_config);
        std::cout << "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
        std::cout << "* Simulation Configuration:\n";
        std::cout << "* * * You can now access the simulation configuration object 'sim_config' and\n"
            "* * * use it to amend config/arch yaml file(s), set/inspect individual parameter\n"
            "* * * values, etc.\n"
            "* * * " << std::endl;
    }catch (bp::error_already_set){
        PyErr_Print();
    }

    published_obj_names_[sim_config] = "sim_config";
}

void PythonInterpreter::publishReportConfiguration(app::ReportConfiguration * report_config)
{
    sparta_assert(report_config,
                "Cannot publish null app::ReportConfiguration "
                "object to Python environment");

    sparta_assert(Py_IsInitialized(),
                "Attempted to publish report configuration object when Python "
                "was not initialized");

    auto global_ns = getGlobalNS();

    try{
        global_ns["report_config"] = WrapperCache<app::ReportConfiguration>().wrap(report_config);
        std::cout << "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
        std::cout << "* Report Configuration:\n";
        std::cout << "* * * You can now access the report descriptors object 'report_config' and use \n"
                  << "* * * it to generate statistics reports from this simulation, optionally including \n"
                  << "* * * start/update/stop triggers.\n"
                  << "* * * " << std::endl;
    }catch(bp::error_already_set){
        PyErr_Print();
    }

    published_obj_names_[report_config] = "report_config";
}

void PythonInterpreter::publishStatisticsArchives(statistics::StatisticsArchives * archives)
{
    sparta_assert(archives,
                "Cannot publish null statistics::StatisticsArchives "
                "object to Python environment");

    sparta_assert(Py_IsInitialized(),
                "Attempted to publish statistics archives object when Python "
                "was not initialized");

    auto global_ns = getGlobalNS();

    try {
        global_ns["archives"] = WrapperCache<statistics::StatisticsArchives>().wrap(archives);
        std::cout << "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
        std::cout << "* Statistics Archives:\n";
        std::cout << "* * * You can now access the simulation's statistics values from the 'archives' object. \n"
                  << "* * * These archives are accessible for the lifetime of the simulation. \n"
                  << "* * * " << std::endl;
    } catch(bp::error_already_set) {
        PyErr_Print();
    }

    published_obj_names_[archives] = "archives";
}

void PythonInterpreter::publishStatisticsStreams(statistics::StatisticsStreams * streams)
{
    sparta_assert(streams,
                "Cannot publish null statistics::StatisticsStreams "
                "object to Python environment");

    sparta_assert(Py_IsInitialized(),
                "Attempted to publish statistics streams object when Python "
                "was not initialized");

    auto global_ns = getGlobalNS();

    try {
        //Put a hidden singleton in the Python namespace which can make
        //the connection between wrapped C++ StreamNode's and Python
        //sink objects.
        PyRun_SimpleString("import streaming");
        PyRun_SimpleString("__stream_manager = streaming.StreamManager()");

        global_ns["stream_config"] = WrapperCache<statistics::StatisticsStreams>().wrap(streams);
        std::cout << "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
                  << "* Statistics Streams:                                                    \n"
                  << "* * * You can now configure any simulation statistic(s) to stream to a   \n"
                  << "* * * Python object of your choice using the 'stream_config' object.     \n"
                  << "* * * These streams can be instantiated now, or at any time during the   \n"
                  << "* * * simulation. SPARTA will make the connection from the simulation      \n"
                  << "* * * statistics to your Python object on the fly.                       \n"
                  << "* * * " << std::endl;
    } catch (bp::error_already_set) {
        PyErr_Print();
    }

    published_obj_names_[streams] = "stream_config";
}

void PythonInterpreter::publishSimulationDatabase(simdb::ObjectManager * sim_db)
{
    sparta_assert(sim_db, "Cannot publish null simdb::ObjectManager "
                "object to Python environment");

    sparta_assert(Py_IsInitialized(),
                "Attempted to publish simulation database object when Python "
                "was not initialized");

    auto global_ns = getGlobalNS();

    try {
        global_ns["sim_db"] = WrapperCache<simdb::ObjectManager>().wrap(sim_db);
        std::cout << "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
                  << "* Simulation Database:                                                   \n"
                  << "* * * You can now access any timeseries data produced by the simulator's \n"
            "* * * statistics reporting engine using the 'sim_db' object.             \n"
                  << "* * * " << std::endl;
    } catch (bp::error_already_set) {
        PyErr_Print();
    }

    published_obj_names_[sim_db] = "sim_db";
}

void PythonInterpreter::publishDatabaseController(simdb::AsyncTaskEval * db_queue)
{
    sparta_assert(db_queue, "Cannot publish null simdb::AsyncTaskEval "
                "object to Python environment");

    sparta_assert(Py_IsInitialized(),
                "Attempted to publish database controller object when Python "
                "was not initialized");

    auto global_ns = getGlobalNS();

    try {
        global_ns["__db_queue"] = WrapperCache<simdb::AsyncTaskEval>().wrap(db_queue);
    } catch (bp::error_already_set) {
        PyErr_Print();
    }

    published_obj_names_[db_queue] = "__db_queue";
}

void PythonInterpreter::publishSimulator(app::Simulation * sim) {
    sparta_assert(sim, "Cannot publish null app::Simulator object to Python environment");
    sparta_assert(Py_IsInitialized(),
                "Attempted to publish simulator object \"" << sim->getSimName() << "\" when Python was not initialized");

    auto global_ns = getGlobalNS();

    try{
        const auto &name = sim->getSimName();
        //main_namespace[name.c_str()] = bp::ptr(sim);
        global_ns[name.c_str()] = WrapperCache<sparta::app::Simulation>().wrap(sim);
        published_obj_names_[sim] = name;
    }catch(bp::error_already_set){
        PyErr_Print();
        throw sparta::SpartaException("Could not publish Simulation");
    }
}

void PythonInterpreter::publishTree(RootTreeNode * n) {
    sparta_assert(n, "Cannot publish null RootTreeNode object to Python environment");
    sparta_assert(Py_IsInitialized(),
                "Attempted to publish tree root \"" << n->getName() << "\" when Python was not initialized");

    auto global_ns = getGlobalNS();

    try{
        const auto &name = n->getName();
        global_ns[name.c_str()] = WrapperCache<sparta::RootTreeNode>().wrap(n);
        published_obj_names_[n] = name;
    }catch(bp::error_already_set){
        PyErr_Print();
        throw sparta::SpartaException("Could not publish RootTreeNode");
    }
}

void PythonInterpreter::publishRunController(sparta::control::TemporaryRunControl* rc) {
    run_controller_ = rc;
    sparta_assert(rc, "Cannot publish null TemporaryRunControl object to Python environment");
    sparta_assert(Py_IsInitialized(),
                "Attempted to publish run controller when Python was not initialized");

    auto global_ns = getGlobalNS();

    try{
        global_ns["rc"] = WrapperCache<sparta::control::TemporaryRunControl>().wrap(rc);
    }catch(bp::error_already_set){
        PyErr_Print();
        throw sparta::SpartaException("Could not publish TemporaryRunControl");
    }

    // Publish some run commands to the global namespace (for now)
    bp::object pyrc = global_ns["rc"];
    try{
        global_ns["runc"] = pyrc.attr("runc");
        global_ns["run"] = pyrc.attr("run");
        global_ns["runi"] = pyrc.attr("runi");
        global_ns["curcycle"] = pyrc.attr("get_curcycle");
        global_ns["curtick"] = pyrc.attr("get_curtick");
        global_ns["curinst"] = pyrc.attr("get_curinst");
    }catch(bp::error_already_set){
        PyErr_Print();
        throw sparta::SpartaException("Could not publish TemporaryRunControl");
    }
    try{
        // Delete writable methods from simulationConfiguration, ParameterNode
        // and ParameterTreeNode.
        PyRun_SimpleString(
                           "del_attr = [delattr(sparta.SimulationConfiguration, attr) for attr in sim_config.__cull_methods__]\n"
                           "del_attr = [delattr(sparta.ParameterTree, attr) for attr in sim_config.arch_ptree.__cull_methods__]\n"
                           "del_attr = [delattr(sparta.Node, attr) for attr in sim_config.arch_ptree.root.__cull_methods__]\n"
                           "\n");
    }catch(bp::error_already_set){
        PyErr_Print();
        throw sparta::SpartaException("Could not perform culling of writable methods from global map.");
    }

    std::shared_ptr<statistics::StreamController> controller(
                                                             new PythonStreamController);

    run_controller_->setStreamController(controller);

    published_obj_names_[rc] = "rc";
}

void PythonInterpreter::removePublishedObject(const void * obj_this_ptr)
{
    auto iter = published_obj_names_.find(obj_this_ptr);
    if (iter != published_obj_names_.end()) {
        auto global_ns = getGlobalNS();
        sparta_assert(global_ns.contains(bp::str(iter->second)));
        global_ns[iter->second].del();
        published_obj_names_.erase(iter);
    }
}

void PythonInterpreter::interact() {
    //PyRun_InteractiveLoop(stdin, "stdin");

    // Launch IPython with current scope
    // Note: No need for isatty(fileno(stdin)) || isatty(fileno(stdout)) since
    //       ipython seems to handle that. Needs testing.
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("print(sys.version)");
    PyRun_SimpleString("import IPython.terminal");
    PyRun_SimpleString("import sparta");
    PyRun_SimpleString("import re");
    PyRun_SimpleString("import types");

    // Double-check that the expected signal handler is still installed
    // it may be OK if this is changed but the exception handling chain is
    // respected, but just ensure it matches for now and deal with other cases
    // later
    struct sigaction cursigint;
    if(sigaction(SIGINT, (struct sigaction*)nullptr, &cursigint) != 0){
        // Exit ctor on failure to replace sigint - prevent dtor from restoring it
        throw SpartaException("error reading signal handler for") << strsignal(SIGINT);
    }
    if(sigint_act_.sa_sigaction != cursigint.sa_sigaction){
        throw SpartaException("Signal handler for ") << strsignal(SIGINT)
                                                   << " changed between PythonInterpreter initialization and interaction loop";
    }

    auto global_ns = getGlobalNS();

    //sparta_assert(bp::extract<std::string>(getattr(global_ns["IPython"], "__version__")),
    //    "");

    bp::object py_ver_major = bp::eval("int(IPython.__version__.split('.')[0])",
                                       global_ns, global_ns);
    const int ver_major = bp::extract<int>(py_ver_major);
    sparta_assert(ver_major >= 2,
                "Imported IPython major version " << ver_major << " was < 2");

    // Create and run the control loop
    const char* create_interp_cmd = "IPython.terminal.embed.InteractiveShellEmbed()";
    ipython_inst_.reset(bp::incref(bp::eval(create_interp_cmd,
                                            global_ns, global_ns).ptr()));
    if(ipython_inst_ == nullptr) {
        PyErr_PrintEx(0);
    }
    sparta_assert(ipython_inst_ != nullptr);

    // Push ipython instance to global namespace for reference
    PyDict_SetItemString(global_ns.ptr(), "__ipytse", ipython_inst_.get());

    //std::cout << "IPython instance: ";
    //PyObject_Print(ipython_inst_.get(), stdout, Py_PRINT_RAW);
    //std::cout << std::endl;

    // Get the ipython instance
    bp::object ipyi(bp::handle<>(bp::borrowed(ipython_inst_.get())));

    // Between two different sessions, the IPython shell instance changes.
    // So, we need to make the Regex engine persist across all sessions.
    PyRun_SimpleString("try:\n"
                       "    __persist_rgx\n"
                       "except NameError:\n"
                       "    pass\n"
                       "else:\n"
                       "    __ipytse.strdispatchers['complete_command'].regexs =  __persist_rgx\n");

    // Install some hooks
    bp::object sparta_pyinterp = global_ns["sparta_pyinterp"];
    bp::call_method<void>(ipython_inst_.get(), "set_hook", bp::str("pre_prompt_hook"), bp::getattr(sparta_pyinterp, "_hook_pre_prompt"));
    bp::call_method<void>(ipython_inst_.get(), "set_hook", bp::str("shell_initialized"), bp::getattr(sparta_pyinterp, "_hook_shell_initialized"));


    //! \todo Consider overridding some Ipython shell members (e.g. ask_exit)

    // Call embedded interpreter this way to initialize a call stack
    PyRun_SimpleString("__ipytse.mainloop(display_banner='SPARTA Python Shell')");
    if(PyErr_Occurred()){
        PyObject *errtype, *errvalue, *traceback;
        PyErr_Fetch(&errtype, &errvalue, &traceback);
        if(errvalue != NULL){
            PyObject *s = PyObject_Str(errvalue);
            /*    Now 'PyString_AS_STRING(s)'
                  contains C string of error message
                  do something with it
            */
            std::cerr << "Error: " << PyUnicode_AsEncodedString(s, "UTF-8", "strict") << std::endl;
            Py_DECREF(s);
        }else{
            std::cerr << "Error: (unknown)" << std::endl;
        }
        Py_XDECREF(errvalue);
        Py_XDECREF(errtype);
        Py_XDECREF(traceback);

        std::cerr << "Uncaught exception in ipython main loop" << std::endl;
        exit_code_ = 1;
    }
}

void PythonInterpreter::IPyPrePrompt(PyObject*) {
    //std::cout << "IPyPrePrompt" << std::endl;
    bp::object global_ns = getGlobalNS();
    bp::object sparta_pyinterp = global_ns["sparta_pyinterp"];
    global_ns["exit"] = sparta_pyinterp.attr("exit");
}

void PythonInterpreter::IPyShellInitialized() {
    //std::cout << "IPyShellInitialized" << std::endl;

    // Publish exit command to overwrite (and indirectly invoke) IPython's
    bp::object global_ns = getGlobalNS();
    bp::object sparta_pyinterp = global_ns["sparta_pyinterp"];
    global_ns["exit"] = sparta_pyinterp.attr("exit");
}

void PythonInterpreter::asyncExit(int exit_code) {
    exit_code_ = exit_code;
    if(ipython_inst_){
        bp::object ipyi(bp::handle<>(bp::borrowed(ipython_inst_.get())));
        ipyi.attr("exit")(); // Defer to IPython's exit
    }else{
        throw SpartaException("PythonInterpreter::asyncExit was called without "
                            "an ipython instance pointer. This is a bug");
    }
}

void PythonInterpreter::handleSIGINT(siginfo_t * info, void * ucontext) {
    (void) info;
    (void) ucontext;
    if(run_controller_){
        run_controller_->asyncStop();
    }

    // Forward to next handler (probably Python's)
    if(sigint_next_.sa_sigaction){
        (*sigint_next_.sa_sigaction)(SIGINT, info, ucontext);
    }
}
} // namespace python
} // namespace sparta
