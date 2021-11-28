
#include "sparta/ports/ExportedPort.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/DynamicResourceTreeNode.hpp"

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SyncPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/ExportedPort.hpp"
#include "sparta/events/StartupEvent.hpp"

#include <iostream>

class Unit1 : public sparta::Unit
{
public:
    Unit1(sparta::TreeNode * my_node) :
        sparta::Unit(my_node),
        sub_unit_params_(my_node),
        dyn_rtn_(my_node, "subunit", "Subunit in Unit1", &sub_unit_params_),
        exported_port_(getPortSet(), "a_signal_out_port",
                       my_node,      "a_deep_signal_out_port")
    { }

private:

    class SubUnit1 : public sparta::Unit
    {
    public:
        SubUnit1(sparta::TreeNode * my_node, const sparta::ParameterSet *) :
            sparta::Unit(my_node)
        {
            sparta::StartupEvent(my_node, CREATE_SPARTA_HANDLER(SubUnit1, driveSignal_));
        }
    private:
        void driveSignal_() { a_signal_out_port_.send(); }

        sparta::SignalOutPort a_signal_out_port_{getPortSet(), "a_deep_signal_out_port"};
    };
    sparta::ParameterSet sub_unit_params_;
    sparta::DynamicResourceTreeNode<SubUnit1, sparta::ParameterSet> dyn_rtn_;

    sparta::ExportedPort exported_port_;
};

class Unit2 : public sparta::Unit
{
public:
    Unit2(sparta::TreeNode * my_node):
        sparta::Unit(my_node)
    {
        a_signal_in_port.registerConsumerHandler(CREATE_SPARTA_HANDLER(Unit2, receivedSignal));
    }

    void receivedSignal() {
        std::cout << __PRETTY_FUNCTION__ << ": received signal" << std::endl;
    }

private:
    //sparta::ExportedPort a_signal_in_port_{getPortSet(), "a_signal_in_port"};
    sparta::SignalInPort a_signal_in_port{getPortSet(), "a_signal_in_port"};
};


int main()
{
    sparta::Scheduler     sched;
    sparta::RootTreeNode  root;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&root, "root_clk");
    cm.normalize();
    root.setClock(root_clk.get());

    sparta::TreeNode unit1_tn(&root, "unit1", "unit 1");
    sparta::TreeNode unit2_tn(&root, "unit2", "unit 2");

    Unit1 unit1(&unit1_tn);
    Unit2 unit2(&unit2_tn);

    root.enterConfiguring();
    root.enterFinalized();
    std::cout << root.renderSubtree() << std::endl;

    sparta::bind(unit1.getPortSet()->getChildAs<sparta::Port>("a_signal_out_port"),
                 unit2.getPortSet()->getChildAs<sparta::Port>("a_signal_in_port"));

    sched.finalize();

    sched.run(1);

    root.enterTeardown();

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
