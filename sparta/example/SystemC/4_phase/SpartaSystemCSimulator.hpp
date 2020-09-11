
#include "sparta/app/Simulation.hpp"
#include "sparta/utils/SysCSpartaSchedulerAdapter.hpp"

#include "systemc_example_top.hpp"

namespace sparta {
    class Scheduler;
    class TreeNode;
}

namespace sparta_sim
{

    class SpartaSystemCSimulator : public sparta::app::Simulation
    {
    public:
        SpartaSystemCSimulator(sparta::Scheduler * sched);

        ~SpartaSystemCSimulator();

    private:

        void buildTree_() override;
        void configureTree_() override;
        void bindTree_() override;

        void runRaw_(uint64_t run_time) override final {
            sysc_sched_runner_.run(run_time);
        }

        sparta::SysCSpartaSchedulerAdapter sysc_sched_runner_;
        std::vector<std::unique_ptr<sparta::TreeNode>> tns_to_delete_;

        systemc_example::systemc_example_top systemc_example_top_;

    };
}
