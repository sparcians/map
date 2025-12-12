
#include "sparta/app/Simulation.hpp"
#include "sparta/utils/SysCSpartaSchedulerAdapter.hpp"

#include "systemc_example_top.hpp"

namespace sparta {
    class Scheduler;
    class TreeNode;
}

namespace sparta_sim
{
    //! \class SpartaSystemCSimulator
    //! \brief Top level constructor for the SystemC example TLM simulator
    class SpartaSystemCSimulator : public sparta::app::Simulation
    {
    public:
        /**
         * \brief Construction of the SpartaSystemCSimulator
         *
         * This class creates the following components for a simple
         * simulation:
         *
         * - `systemc_example_top` that contains the `tlm::SimpleBusAT` and the two `tlm::initiator_top` instances
         * - `sparta::SpartaTLMTargetGasket` that binds to SystemC on one side and Sparta on the other
         * - `sparta::SpartaMemory` which is the receiver of traffic from the initiators
         * - `sparta::SysCSpartaSchedulerAdapter` which will run the simulator including the SystemC kernel
         *
         * Then, when asked to run, will call run on the
         * SysCSpartaSchedulerAdapter which runs both kernels
         */
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
