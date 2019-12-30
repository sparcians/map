
#include <inttypes.h>
#include <iostream>
#include <memory>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/app/Simulation.hpp"

/*!
 * \file HierarchicalBuilding_test.cpp
 * \brief Test for building a hierarchical tree
 *
 * \verbatim
 * Builds a tree:
 *
 *             top
 *            / | \
 *       ____/  |  \___________________________
 *      /        \                   \         \
 *     /          \                   \         \
 * cluster0       cluster1             mem      board_cfg
 *   /   \           |    \______                      \
 * core0  core1      core0       core1                params
 *   |        \         \             \
 * subcomp0   subcomp0  subcomp0     subcomp0
 *
 *
 * Controlled by parameters:
 *   top.boardconfig.params.num_clusters 2 // Don't really want to put params on "top" object.
 *   top.cluster0.params.num_cores 2
 *   top.cluster0.params.core_type x
 *   top.cluster1.params.num_cores 2
 *   top.cluster1.params.core_type y
 *
 * \endverbatim
 */

TEST_INIT;

class BaseCore;
class XCore;
class YCore;
class Subcomponent;
class Mem;

using sparta::utils::uint32_to_str;

/*!
 * \brief Memory unit
 */
class Mem : public sparta::Resource
{
public:

    static constexpr const char* name="mem";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {;}

        // No parameters
    };

    /*!
     * \brief Resource-constructor
     */
    Mem(sparta::TreeNode* node, const ParameterSet*) :
        sparta::Resource(node)
    {
        std::cout << "Constructed a Mem object " << getContainer() << std::endl;
    }
};

/*!
 * \brief Simple subcomponent object which is not a resource and is created
 * directly by its parent
 */
class Subcomponent : public sparta::TreeNode
{
public:

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="subcomponent";

    // Parameter Set
    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {;}

        PARAMETER (std::string, foo, "subcomponent foo parameter", "Example parameter")

    };

    /*!
     * \brief Constructed using parent and index but is own node and creates
     * own parameter set. Self-assigns a name/group
     */
    Subcomponent(sparta::TreeNode* parent, group_idx_type idx)
      : sparta::TreeNode(parent,
                       (std::string("subcomp") + uint32_to_str(idx)),
                       "subcomp",
                       idx,
                       "A subcomponent"),
        params_(this)
    {
        std::cout << "Constructed a subcomponent " << *this << " with param foo = " << params_.foo() << std::endl;

        // Create stuff! Ports, statsets, events, etc....
    }

protected:

    // Callback for binding before top-level simulation has a chance
    void onBindTreeEarly_() override {

    }

    // Callback for binding after top-level simulation has a chance
    void onBindTreeLate_() override {

    }

private:

    ParameterSet params_; //!< Parameter Set for this object
};

/*!
 * \brief Parameter set for board configuration
 */
class BoardConfigParameterSet : public sparta::ParameterSet
{
public:
    BoardConfigParameterSet(sparta::TreeNode* parent) :
        sparta::ParameterSet(parent)
    {;}

    PARAMETER (uint32_t, num_clusters, 2, "Number of clusters to create")

};


//! Core base class
class BaseCore {
public:
    // Things common to all types of core when convenient
};

//! Simple core resource type "x"
class XCore : public sparta::Resource, public BaseCore
{
public:

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="xcore";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {;}

        // No parameters
    };

    /*!
     * \brief Resource-constructor
     */
    XCore(sparta::TreeNode* node, const ParameterSet*) :
        sparta::Resource(node),
        subcomp_(node, 0) // Create subcomponent as a simple treenode given this a parent and index. It names itself
    {
        std::cout << "Constructed an X core " << getContainer() << std::endl;
    }

protected:

    // Callback for binding before top-level simulation has a chance
    void onBindTreeEarly_() override {
        // Bind children!
    }

    // Callback for binding after top-level simulation has a chance
    void onBindTreeLate_() override {

    }

private:

    Subcomponent subcomp_; //!< Some subcomponent for this core
};

//! Simple core resource type "y"
class YCore : public sparta::Resource, public BaseCore
{
public:

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="ycore";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {;}

        // No parameters
        PARAMETER (std::string, y_core_exclusive_param, "This is a Y core", "Parameter that exists in YCore but not XCore")
    };

    /*!
     * \brief Resource-constructor
     */
    YCore(sparta::TreeNode* node, const ParameterSet* params) :
        sparta::Resource(node),
        params_(params)
    {
        std::cout << "Constructed a Y core " << getContainer() << " with paramter y_core_exclusive_param = " << params_->y_core_exclusive_param() << std::endl;
    }

protected:

    // Callback for binding before top-level simulation has a chance
    void onBindTreeEarly_() override {

    }

    // Callback for binding after top-level simulation has a chance
    void onBindTreeLate_() override {

    }

private:

    const ParameterSet* const params_; // Pointer to parameter set for this core
};

//! Simple cluster resource which defines its own parameter set object.
class Cluster : public sparta::Resource
{
public:

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="cluster";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {;}

        PARAMETER(uint32_t, num_cores, 2, "Number of cores to create")
        PARAMETER(std::string, core_type, "x", "Type of core this cluster will contain. Choices:{x,y}. Default:x")
    };

    /*!
     * \brief Resource-constructor
     */
    Cluster(sparta::TreeNode* node, const ParameterSet* params)
      : sparta::Resource(node),
        params_(params)
    {
        // Should move core construction into a CoreBuilder class!?

        // Choose factory based on core type
        sparta::ResourceFactoryBase* core_fact = nullptr;
        if(params_->core_type == "x"){
            core_fact = &xcore_fact_;
        }else if(params_->core_type == "y"){
            core_fact = &ycore_fact_;
        }
        sparta_assert_context(core_fact,
                            "Unable to find a factory for creating cores base on selected type \""
                            << params_->core_type() << "\"");

        for(uint32_t i=0; i<params->num_cores; ++i){
            auto core_rtn = new sparta::ResourceTreeNode(getContainer(), // Node for this Cluster resource
                                                       std::string("core") + uint32_to_str(i),
                                                       "core",
                                                       i,
                                                       "A Core of the chosen type",
                                                       core_fact);
            to_free_.emplace_back(core_rtn);
            core_rtn->finalize(); // Construct core resource here and now

            // Grab core interface for easy reference
            auto core = dynamic_cast<BaseCore*>(core_rtn->getResource());
            cores_.push_back(core);

            // Could assign a clock here too (otherwise core gets clock from ancestors)
        }
    }

protected:

    // Callback for binding before top-level simulation has a chance
    void onBindTreeEarly_() override {
        // Bind cores and their children based on topology & parameters!
    }

    // Callback for binding after top-level simulation has a chance
    void onBindTreeLate_() override {

    }

private:

    // Factories for each type core that can exist in this cluster.
    // These are unregistered and exist as long as this core. Eventually, they
    // will be registered globally. These must be destructed AFTER child
    // ResourceTreeNodes
    sparta::ResourceFactory<XCore> xcore_fact_;
    sparta::ResourceFactory<YCore> ycore_fact_;

    const ParameterSet* const params_; // Pointer to parameter set for this cluster

    /*!
     * \brief Cores in the cluster
     */
    std::vector<std::unique_ptr<sparta::TreeNode>> to_free_;

    std::vector<BaseCore*> cores_; // Borrowed pointers
};

//sparta::ResourceFactory<XCore> Cluster::xcore_fact_;
//sparta::ResourceFactory<YCore> Cluster::ycore_fact_;

/*!
 * \brief Simulator example
 */
class MySimulator : public sparta::app::Simulation
{
public:

    MySimulator(const std::string& name, sparta::Scheduler * scheduler)
      : sparta::app::Simulation(name, scheduler)
    {
        // Register resources for access by name. This is going to be more
        // useful with python-based topology later on.
        getResourceSet()->addResourceFactory<sparta::ResourceFactory<Cluster, Cluster::ParameterSet>>();
        getResourceSet()->addResourceFactory<sparta::ResourceFactory<Mem, Mem::ParameterSet>>();
    }

    ~MySimulator()
    {
        getRoot()->enterTeardown(); // Allow deletion of nodes without error now
    }

    void buildTree_() override
    {
        // Create board config params (place for board-level configuration)
        BoardConfigParameterSet* board_params = nullptr;
        {
            auto board_cfg_node = new sparta::TreeNode(getRoot(),
                                                     "board_cfg",
                                                     "Board configuration");

            to_free_.emplace_back(board_cfg_node);
            board_params = new BoardConfigParameterSet(board_cfg_node);
            to_free_.emplace_back(board_params);
        }

        sparta::Clock::Handle master_clock = getClockManager().getRoot();

        // Should move into a ClusterBuilder or array of ClusteBuilder depending on the implementation!
        {
            for(uint32_t i=0; i<board_params->num_clusters; ++i){
                clusters_.emplace_back(new sparta::ResourceTreeNode(getRoot(),
                                                                  std::string("cluster") + uint32_to_str(i),
                                                                  "cluster",
                                                                  i,
                                                                  "A Cluster!",
                                                                  getResourceSet()->getResourceFactory("cluster")));
                clusters_.back()->setClock(master_clock.get());
            }
        }

        // Set up a placeholder memory node
        {
            clusters_.emplace_back(new sparta::ResourceTreeNode(getRoot(),
                                                              "mem",
                                                              sparta::TreeNode::GROUP_NAME_NONE,
                                                              sparta::TreeNode::GROUP_IDX_NONE,
                                                              "A Mem object!",
                                                              getResourceSet()->getResourceFactory("mem")));
            clusters_.back()->setClock(master_clock.get());
        }
    }

    void configureTree_() override
    {
        // Nothing needed. maybe compute and override some parameters
    }

    void bindTree_() override
    {
        // At this point, everything is built. onBindTreeEarly_ will have been called for everything as well

        std::cout << "The tree from the top (with builtin groups): " << std::endl << getRoot()->renderSubtree(-1, true) << std::endl;
        std::cout << "Nodes: " <<  getRoot()->getRecursiveNodeCount<sparta::ParameterBase>() << std::endl;
    }

    // Do nothing for this dummy simulator
    void runControlLoop_(uint64_t run_time) override
    {
        (void) run_time;
    }

private:

    std::vector<std::unique_ptr<sparta::TreeNode>> to_free_; // Miscellaneous nodes to free at destruction
    std::vector<std::unique_ptr<sparta::TreeNode>> clusters_; // Nodes to free at destruction
};


int main(int argc, char** argv)
{
     sparta::app::CommandLineSimulator cls("Usage string"); // Omitted default command-line setup

    // Parse command line options and configure simulator
    int err_code = 0;
    if(!cls.parse(argc, argv, err_code)){
        return err_code; // Any errors already printed to cerr
    }

    // Create the simulator
    try{
        sparta::Scheduler scheduler;
        MySimulator sim("mysim", &scheduler);
        cls.populateSimulation(&sim);
        cls.runSimulator(&sim);
        cls.postProcess(&sim);
    }catch(...){
        // We could still handle or log the exception here
        throw;
    }

    REPORT_ERROR;

    return ERROR_CODE;
}
