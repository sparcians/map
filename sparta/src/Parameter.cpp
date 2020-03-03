

#include "sparta/simulation/Parameter.hpp"

#include <algorithm>
#include <iomanip>
#include <ios>
#include <iterator>

#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/app/ConfigApplicators.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/Backtrace.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/app/SimulationConfiguration.hpp"

constexpr char sparta::ParameterBase::PARAMETER_NODE_TAG[];

void sparta::ParameterSet::lock(const ParameterBase * lock_param) {
    locking_params_stack_.push_back(lock_param);

    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        auto msg = sparta::log::MessageSource::getGlobalParameterTraceSource().emit("MODIFY_CALLBACK_STACK ADD    ");
        msg << lock_param->getParent()->getLocation() << ".";
        for(const ParameterBase* pb : locking_params_stack_){
            msg << pb->getName() << ">";
        };
    }
}

void sparta::ParameterSet::unlock(const ParameterBase * unlock_param) {
    sparta_assert(unlock_param == locking_params_stack_.back());
    locking_params_stack_.pop_back();

    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        auto msg = sparta::log::MessageSource::getGlobalParameterTraceSource().emit("MODIFY_CALLBACK_STACK REMOVE ");
        msg << unlock_param->getLocation() << ".";
        for(const ParameterBase* pb : locking_params_stack_){
            msg << pb->getName() << ">";
        };
    }
}

void sparta::ParameterBase::invokeModifierCB_()
{
    sparta::ParameterSet * ps = getParentAs<sparta::ParameterSet>(false);
    if(ps->isLocked()) {
        sparta_assert(ps->canParameterBeModifiedByLockingParam(this) == true,
                          "Parameter: '" << getName()
                          << "' cannot be modified by another parameter listed later in the set");
    }

    // --read-final-config disables callback overrides for parameter values.
    bool callback_allowed = !usingFinalConfig_();
    if(modifier_callback_ && callback_allowed)
    {
        ps->lock(this);
        modifier_callback_();
        ps->unlock(this);
    }
}

void sparta::ParameterBase::associateParametersForModification(std::vector<const ParameterBase *> params,
                                                               const sparta::SpartaHandler& modifier_callback)
{
    sparta::ParameterSet * ps = getParentAs<sparta::ParameterSet>(false);
    sparta_assert(ps != nullptr, "Can't associated parameter: '" << getName()
                      << "' does not have a ParameterSet associated.  Did you call __collect()?");

    sparta_assert(associated_params_.empty() == true,
                      "Associating more parameters with '" << getName()
                      << "' after already associating is not supported");

    sparta_assert(ps->hasModificationPermission(this, params) == true);

    modifier_callback_ = modifier_callback;

    // Copy the associated parameters, but no duplicates
    std::copy_if(params.begin(), params.end(), std::back_inserter(associated_params_),
                 [&] (const sparta::ParameterBase * in_param) -> bool
                 {
                     if(in_param == this) {
                         return false;
                     }

                     if(std::find(associated_params_.begin(),
                                  associated_params_.end(),
                                  in_param) == associated_params_.end())
                     {
                         return true;
                     }
                     return false;
                 });
}

void sparta::ParameterBase::logLoadedDefaultValue_() const
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "InitialDefault "
            << std::setw(15) << "---------------"
            << " -> " << std::setw(15) << getValueAsString()
            << " at:" << getLocation();

        //logCurrentBackTrace_();
    }
}

void sparta::ParameterBase::logAssignedValue_() const
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "OperatorAssigned "
            << std::setw(15) << "???????????????"
            << " -> " << std::setw(15) << getValueAsString()
            << " at:" << getLocation();
    }
}

void sparta::ParameterBase::logCurrentBackTrace_() {
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        // Print backtrace information
        std::stringstream ss;
        sparta::app::Backtrace::dumpBacktrace(ss);
        const std::string s = ss.str();
        if(s.size() > 0){
            size_t pos = 0;
            while(1){
                const size_t prev_pos = pos;
                pos = s.find('\n', prev_pos+1);
                if(pos == std::string::npos){
                    sparta::log::MessageSource::getGlobalParameterTraceSource()
                        << s.substr(prev_pos);
                        break;
                }else{
                    sparta::log::MessageSource::getGlobalParameterTraceSource()
                        << s.substr(prev_pos, pos-prev_pos);
                }

            }
        }
    }
}

void sparta::ParameterBase::addToSet_(ParameterSet* ps)
{
    sparta_assert(ps);
    ps->addParameter_(this);
}

uint32_t sparta::ParameterSet::readVolatileParamValuesFromVirtualTree()
{
    sparta_assert(getPhase() <= TREE_CONFIGURING,
                "Tree must be in the configuring phase or earlier");

    const ParameterTree* pt = getSimParameterTree_();
    if(!pt){
        return 0;
    }

    uint32_t updates = 0;
    for(auto* p : params_){
        if(p->isVolatile()){
            // Write new value if available regardless of write count
            updates += (uint32_t)readValueFromVirtualTree_(nullptr, pt, p);
        }
    }
    return updates;
}

uint32_t sparta::ParameterSet::readAllParamValuesFromVirtualTree()
{
    sparta_assert(getPhase() <= TREE_CONFIGURING,
                "Tree must be in the configuring phase or earlier");

    const ParameterTree* pt = getSimParameterTree_();
    if(!pt){
        return 0;
    }

    uint32_t updates = 0;
    for(auto* p : params_){
        // Write new value if available regardless of write count
        updates += (uint32_t)readValueFromVirtualTree_(nullptr, pt, p);
    }
    return updates;
}

void sparta::ParameterSet::addParameter_(sparta::ParameterBase* p)
{
    sparta_assert_context(p, "Cannot call ParameterSet::addParameter with null parameter");

    // Check for duplicates. If someone called this then another subclass
    // constructed more params, this could happen and is illegal
    if(hasParameter(p->getName())){
        ParameterException e("Attempted to add duplicate parameter named \"");
        e << p->getName() << "\" to set " << getLocation();
        throw e;
    }

    params_.push_back(p);
    keys_[p->getName()] = p;
    addChild(p); // Add to tree

    // Attempt to get value from user configuration if available now
    const ParameterTree* pt = getSimParameterTree_();
    const ParameterTree* apt = getSimArchParameterTree_();
    sparta_assert((pt==nullptr) == (apt==nullptr),
                "The parameter tree and arch parameter tree must both be null or non-null when "
                "being retrieved");

    if(pt && apt){
        // Only populate from virtual tree if it has not been written (double-check just in case)
        sparta_assert(p->getWriteCount() == 0,
                    "Somehow a newly-added parameter " << p->getLocation()
                    << " had a non-zero write count");

        // Node population Case A: read from virtual parameter trees at
        // addition/construction (assumes construction)
        readValueFromVirtualTree_(apt, pt, p);

        // Because this node can access a parameter tree from the simulator now,
        // flag it as having populated from the virtual tree. Future calls to
        // this function will continue to always try and populate NEWLY-ADDED
        // parameters from the virtual tree regardless of this flag, which stops
        // only onAddeAsChild_ from pulling new parameter values
        populated_from_virtual_ = true;
    }

    // Explicitly inherit phase from parent after being added.
    // In the ctor for this class, the parameterset does not inherit phase
    // immediately at construction so that its parameters can be pre-populated.
    // This happens part way through construction.
    if(getParent() && getParent()->getPhase() > getPhase()){
        setPhase_(getParent()->getPhase());
    }
}

void sparta::ParameterSet::onAddingChild_(TreeNode* child)
{
    ParameterBase* ctr = dynamic_cast<ParameterBase*>(child);
    if(nullptr == ctr){
        throw SpartaException("Cannot add TreeNode child at ")
            << child << " which is not a ParameterBase to ParameterSet "
            << getLocation();
    }
}

void sparta::ParameterSet::onAddedAsChild_() noexcept
{
    // Checking for finalization after adding tree. phase propagates on
    // attachment in most cases
    if(isFinalized()){
        return; // Do not modify if finalized
    }

    // Detect if the parameter set had no access to root at the time it
    // was created and children attached. Then at this point, if the
    // parameterset is able, read values from the virtual tree.
    if(not populated_from_virtual_){
        // Node population Case B: read from virtual parameter trees at
        // attachment (assumes construction)

        readValuesFromVirtualTree_();
    }
}

const sparta::ParameterTree* sparta::ParameterSet::getSimParameterTree_()
{
    TreeNode* n = getRoot();
    RootTreeNode* r = dynamic_cast<RootTreeNode*>(n);
    if(!r){
        return nullptr;
    }

    app::Simulation* sim = r->getSimulator();
    if(!sim){
        return nullptr;
    }

    app::SimulationConfiguration * sim_config = sim->getSimulationConfiguration();
    if(!sim_config) {
        return nullptr;
    }

    return &sim_config->getUnboundParameterTree();
}

const sparta::ParameterTree* sparta::ParameterSet::getSimArchParameterTree_()
{
    TreeNode* n = getRoot();
    RootTreeNode* r = dynamic_cast<RootTreeNode*>(n);
    if(!r){
        return nullptr;
    }

    app::Simulation* sim = r->getSimulator();
    if(!sim){
        return nullptr;
    }

    app::SimulationConfiguration * sim_config = sim->getSimulationConfiguration();
    if(!sim_config) {
        return nullptr;
    }

    return &sim_config->getArchUnboundParameterTree();
}

void sparta::ParameterSet::readValuesFromVirtualTree_()
{
    const ParameterTree* apt = getSimArchParameterTree_();
    const ParameterTree* pt = getSimParameterTree_();
    sparta_assert((pt==nullptr) == (apt==nullptr),
                "The parameter tree and arch parameter tree must both be null or non-null when "
                "being retrieved");

    if(!pt || !apt){
        return; // Unable to read values because node is not attached to a root
                // or root has no sim or sim has no virtual parameter tree
    }

    // Able to read from virtual parameter tree, so don't do this again
    populated_from_virtual_ = true;

    for(auto* p : params_){
        readValueFromVirtualTree_(apt, pt, p);
    }
}

bool sparta::ParameterSet::readValueFromVirtualTree_(const ParameterTree* arch_pt,
                                                     const ParameterTree* pt,
                                                     sparta::ParameterBase* p)
{
    sparta_assert_context(p, "Cannot call readNewValueFromVirtualTree_ with a null Parameter");

    if(arch_pt){
        auto aptn = arch_pt->tryGet(p->getLocation());
        if(aptn){
            app::ParameterDefaultApplicator pa("", aptn->getValue());
            pa.apply(p);
            p->restoreValueFromDefault(); // Override default
            p->resetWriteCount_();

        }
    }

    if(pt){
        auto ptn = pt->tryGet(p->getExpectedLocation());
        if(ptn){
            app::ParameterApplicator pa("", ptn->getValue());
            pa.apply(p);
            return true;
        }
    }

    return false;
}

void sparta::ParameterBase::restoreValueFromDefault()
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "RestoreDefault "
            << std::setw(15) << getValueAsString()
            << " -> " << std::setw(15) << getDefaultAsString()
            << " at:" << getLocation();
    }
    restoreValueFromDefaultImpl_();
}

void sparta::ParameterBase::setValueFromString(const std::string& str, bool poke)
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "SetValue "
            << std::setw(15) << getValueAsString()
            << " -> " << std::setw(15) << str
            << " at:" << getLocation();
    }
    setValueFromStringImpl_(str, poke);
}

void sparta::ParameterBase::setValueFromStringVector(const std::vector<std::string>& str, bool poke)
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "SetValueFromVector "
            << std::setw(15) << getValueAsString()
            << " -> " << std::setw(15) << str
            << " at:" << getLocation();
    }
    setValueFromStringVectorImpl_(str, poke);
}

void sparta::ParameterBase::setItemValueFromString(const std::vector<uint32_t>& indices,
                                                 const std::string& str)
{
    if(SPARTA_EXPECT_FALSE(sparta::log::MessageSource::getGlobalParameterTraceSource().observed())){
        sparta::log::MessageSource::getGlobalParameterTraceSource()
            << std::setw(19) << "SetItem "
            << std::setw(15) << getValueAsString()
            << " -> " << std::setw(15) << str
            << " at:" << getLocation() << " element:" << indices;
    }
    setItemValueFromStringImpl_(indices, str);
}

bool sparta::ParameterBase::usingFinalConfig_()
{
    if (getSimulation())
    {
        return getSimulation()->usingFinalConfig();
    }
    return false;
}
