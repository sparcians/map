
#include "sparta/simulation/Resource.hpp"

#include <iomanip>
#include <map>
#include <memory>
#include <utility>

#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {

Resource::Resource(TreeNode* rc) :
    res_container_(notNull(rc)),
    name_(rc->getName()),
    clk_(rc->getClock())
{
    res_container_->setResource_(this);
}

Resource::Resource(TreeNode* rc,
                   const std::string& name) :
    res_container_(notNull(rc)),
    name_(name),
    clk_(rc->getClock())
{
    res_container_->setResource_(this);
}

Resource::Resource(const std::string & name,
                   const Clock * clk) :
    res_container_(nullptr),
    name_(name),
    clk_(clk)
{ }

Resource::~Resource()
{
    if(res_container_ != nullptr){
        res_container_->unsetResource_();
    }
}

const Clock * Resource::getClock() const {
    return clk_;
}

Scheduler * Resource::getScheduler(const bool must_exist) const {
    auto clk = getClock();
    sparta_assert(clk || !must_exist);
    if (!clk) {
        return nullptr;
    }

    auto scheduler = clk->getScheduler();
    sparta_assert(scheduler || !must_exist);
    return scheduler;
}

std::string Resource::getName() const {
    return name_;
}

TreeNode* Resource::getContainer() {
    return res_container_;
}

const TreeNode* Resource::getContainer() const {
    return res_container_;
}

ResourceContainer* Resource::getResourceContainer() {
    return res_container_;
}

const ResourceContainer* Resource::getResourceContainer() const {
    return res_container_;
}

void Resource::simulationTerminating_() { }

void Resource::validatePostRun_(const PostRunValidationInfo& info) const {
    (void) info;
}

void Resource::dumpDebugContent_(std::ostream& output) const {
    (void) output;
}

void Resource::onStartingTeardown_() {
}

sparta::ResourceFactoryBase* ResourceSet::getResourceFactory(const std::string& name) {
    auto itr = factories_.find(name);
    if(itr != factories_.end()){
        return itr->second.get();
    }
    sparta::SpartaException ex("No resource factory named \"");
    ex << name << "\" found in the list of resource factory. Options are\n";
    for(const auto& p : factories_){
        ex << p.first << ", ";
    }
    throw ex;
}

bool ResourceSet::hasResource(const std::string& name) const noexcept {
    auto itr = factories_.find(name);
    return itr != factories_.end();
}

std::string ResourceSet::renderResources(bool one_per_line) {
    std::stringstream ss;
    for(auto& p : factories_){
        if(false == one_per_line){
            if(p.first != factories_.begin()->first){
                ss << ", ";
            }
        }else{
            ss << std::setw(max_res_name_len_);
        }

        ss << p.first;

        if(one_per_line){
            ss << '\n';
        }
    }
    return ss.str();
}

} // namespace sparta
