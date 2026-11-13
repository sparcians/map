// <RoiManager> -*- C++ -*-

#pragma once

#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sparta {

class RoiManager
{
public:
    explicit RoiManager(RootTreeNode* root) : root_(root) {
    }

    ~RoiManager() {
        if (registered_) {
            root_->DEREGISTER_FOR_NOTIFICATION(handleROI_, size_t, "roi_region");
        }
    }

    void onFinalizeFramework() {
        if (!roi_triggers_.empty()) {
            root_->REGISTER_FOR_NOTIFICATION(handleROI_, size_t, "roi_region");
            registered_ = true;
        }
    }

    void addRoiTrigger(size_t roi, trigger::ExpressionTrigger* trigger) {
        sparta_assert(!registered_);
        sparta_assert(roi > 0);
        auto& triggers = roi_triggers_[roi];
        if (std::find(triggers.begin(), triggers.end(), trigger) == triggers.end()) {
            triggers.push_back(trigger);
        }

        // ROI triggers are always suspended until postNotification()
        // is called to activate their ROI.
        trigger->suspend();
    }

private:
    void handleROI_(const size_t& active_roi) {
        for (auto& [roi, triggers] : roi_triggers_) {
            if (roi == active_roi) {
                for (auto trig : triggers) {
                    trig->awaken();
                }
            } else {
                for (auto trig : triggers) {
                    trig->suspend();
                }
            }
        }
    }

    RootTreeNode* root_ = nullptr;
    bool registered_ = false;
    std::unordered_map<size_t, std::vector<trigger::ExpressionTrigger*>> roi_triggers_;
};

} // namespace sparta
