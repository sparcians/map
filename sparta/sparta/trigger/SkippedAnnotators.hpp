
#ifndef __SKIPPED_ANNOTATORS_H__
#define __SKIPPED_ANNOTATORS_H__

#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/kernel/Scheduler.hpp"

namespace sparta {
namespace trigger {

/*!
 * \brief Base for helper classes used to print out strings to the stats
 * report in place of skipped updates
 */
class SkippedAnnotatorBase {
public:
    virtual ~SkippedAnnotatorBase() {}
    virtual std::string currentAnnotation() const = 0;

    void skip() {
        ++current_skip_count_;
        skip_();
    }
    void reset() {
        current_skip_count_ = 0;
        reset_();
    }
    size_t currentSkipCount() const {
        return current_skip_count_;
    }

protected:
    explicit SkippedAnnotatorBase(const Clock * clk) :
        clk_(clk)
    {}
    const Clock * getClock_() const {
        sparta_assert(clk_);
        return clk_;
    }
    const Scheduler * getScheduler_() const {
        sparta_assert(clk_ && clk_->getScheduler());
        return clk_->getScheduler();
    }

private:
    virtual void skip_() {}
    virtual void reset_() {}
    size_t current_skip_count_ = 0;
    const Clock *const clk_;
};

class UpdateCountSkippedAnnotator : public SkippedAnnotatorBase
{
public:
    explicit UpdateCountSkippedAnnotator(const sparta::CounterBase * ctr) :
        SkippedAnnotatorBase(nullptr),
        ctr_(ctr)
    {
        reset_();
    }

private:
    virtual std::string currentAnnotation() const override {
        std::ostringstream oss;
        oss << std::string("#") << (ctr_->get() - initial_);
        return oss.str();
    }
    virtual void reset_() override {
        initial_ = ctr_->get();
    }

    const sparta::CounterBase * ctr_ = nullptr;
    uint64_t initial_ = 0;
};

class UpdateCyclesSkippedAnnotator : public SkippedAnnotatorBase
{
public:
    explicit UpdateCyclesSkippedAnnotator(const sparta::Clock * clk) :
        SkippedAnnotatorBase(clk)
    {
        reset_();
    }

private:
    virtual std::string currentAnnotation() const override {
        auto clk = getClock_();
        std::ostringstream oss;
        auto curr_cycle = clk->getCycle(getScheduler_()->getElapsedTicks());
        oss << std::string("#") << (curr_cycle - initial_);
        return oss.str();
    }
    virtual void reset_() override {
        initial_ = getClock_()->getCycle(getScheduler_()->getElapsedTicks());
    }

    uint64_t initial_ = 0;
};

class UpdateTimeSkippedAnnotator : public SkippedAnnotatorBase
{
public:
    UpdateTimeSkippedAnnotator(const Clock * clk) :
        SkippedAnnotatorBase(clk)
    {
        reset_();
    }

private:
    virtual std::string currentAnnotation() const override {
        auto curr_pico_seconds = getScheduler_()->getElapsedTicks();
        std::ostringstream oss;
        oss << std::string("#") << (curr_pico_seconds - initial_);
        return oss.str();
    }
    virtual void reset_() override {
        initial_ = getScheduler_()->getElapsedTicks();
    }

    uint64_t initial_ = 0;
};

}
}

#endif
