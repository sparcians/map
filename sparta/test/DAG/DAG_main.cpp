
#include <inttypes.h>
#include <iostream>
#include <fstream>

#include "sparta/sparta.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Vertex.hpp"

TEST_INIT;

using namespace sparta;
using sparta::DAG;
using sparta::Port;
using sparta::DataInPort;
using sparta::DataOutPort;
using namespace std;

//____________________________________________________________
// OBSERVER
class Observer
{
public:
    Observer(const string& name):
        name_(name),
        activations_(0)
    {}


    void activate()
    {
        cout << "Observer(" << name_ << ")::activate()" << endl;
        ++activations_;
    }


    template<class DataType>
    void activate(const DataType& dat)
    {
        cout << "Observer(" << name_ << ")::activate<>(" << dat << ")" << endl;
        ++activations_;
    }

    uint32_t getActivations() const
    {
        return activations_;
    }

private:
    string name_;
    uint32_t    activations_;
};


//____________________________________________________________
// OBSERVER
class Bar : public Scheduleable
{
public:
    Bar(Scheduler * scheduler) :
    Scheduleable(sparta::SpartaHandler("test"), 0, SchedulingPhase::Trigger)
    {
        setScheduler(scheduler);
    }
    bool operator==(const Bar & other) const {
        return Scheduleable::scheduler_.equals(other.scheduler_);
    }
};

//____________________________________________________________
// MAIN
int main()
{
    {
        Scheduler s1, s2;
        Bar b1(&s1), b2(&s2);   // Different schedulers: b1!=b2
        EXPECT_FALSE(b1 == b2);

        Bar b3(&s1);            // Same scheduler as b1: b1==b3
        EXPECT_TRUE(b1 == b3);
    }
    sparta::Scheduler sched;
    Scheduler *s1 = &sched;

    DAG *dag[3] = {new DAG(s1, true), new DAG(s1, true), new DAG(s1, true)};

    dag[0]->enableEarlyCycleDetect();
    dag[1]->enableEarlyCycleDetect();
    dag[2]->enableEarlyCycleDetect();

    // Test ability to find a cycle in the DAG
    Vertex* f[6] = { dag[0]->newFactoryVertex("a0",s1),
                     dag[0]->newFactoryVertex("b1",s1),
                     dag[0]->newFactoryVertex("c2",s1),
                     dag[0]->newFactoryVertex("d3",s1),
                     dag[0]->newFactoryVertex("e4",s1),
                     dag[0]->newFactoryVertex("f5",s1) };

    dag[0]->link(f[0], f[2]);
    dag[0]->link(f[1], f[2]);
    dag[0]->link(f[2], f[3]);
    dag[0]->link(f[2], f[4]);
    dag[0]->link(f[3], f[4]);

    cout << dag[0];
    dag[0]->dumpToCSV(std::cout, std::cout);
    {
        fstream fs_vert("dag0_vertices.csv", fstream::out);
        fstream fs_edge("dag0_edges.csv", fstream::out);
        dag[0]->dumpToCSV(fs_vert, fs_edge);
    }


    bool did_throw = false;
    try {
        dag[0]->link(f[4], f[0]);
    } catch (DAG::CycleException & e) {
        // cout << dag[0] << endl;
        // fstream fs;
        // fs.open("dag_cycle1.dot", fstream::out);
        e.writeCycleAsDOT(cout);
        e.writeCycleAsText(cout);
        did_throw = true;
    }
    EXPECT_TRUE(did_throw);

    try {
        EXPECT_FALSE(dag[0]->sort());
    } catch (DAG::CycleException & e) {
        cout << "Cycle(s) found during sort..." << endl;
        dag[0]->printCycles(cout);
        //fstream fs;
        //fs.open("dag_cycle2.dot", fstream::out);
        e.writeCycleAsDOT(cout);
        e.writeCycleAsText(cout);
    }
    cout << endl;

    // Remove the cycle and re-try
    dag[0]->unlink(f[4], f[0]);

    try {
        EXPECT_TRUE(dag[0]->sort());
    } catch (DAG::CycleException & e) {
        cout << "Cycle(s) found during sort..." << endl;
        dag[0]->printCycles(cout);
        // fstream fs;
        // fs.open("dag_cycle3.dot", fstream::out);
        e.writeCycleAsDOT(cout);
        e.writeCycleAsText(cout);
        //fs.close();
    }
    cout << "______________________" << endl;
    cout << "SORTED DAG[0]" << endl;
    cout << "______________________" << endl;
    cout << dag[0];

    EXPECT_EQUAL(f[0]->getGroupID(), 1);
    EXPECT_EQUAL(f[1]->getGroupID(), 1); // Already set correctly from the attempt with cycles
    EXPECT_EQUAL(f[2]->getGroupID(), 2);
    EXPECT_EQUAL(f[3]->getGroupID(), 3);
    EXPECT_EQUAL(f[4]->getGroupID(), 4);

    // Test the "whiteboard" configuration
    Vertex p[8] = { Vertex("p",s1), Vertex("q",s1), Vertex("r",s1),
                 Vertex("s",s1), Vertex("t",s1), Vertex("u",s1),
                 Vertex("v",s1), Vertex("w",s1) };

    dag[1]->link(&p[0], &p[2]);
    dag[1]->link(&p[1], &p[3]);
    dag[1]->link(&p[2], &p[4]);
    dag[1]->link(&p[2], &p[5]);
    dag[1]->link(&p[3], &p[4]);
    dag[1]->link(&p[3], &p[5]);
    dag[1]->link(&p[2], &p[3]);
    dag[1]->link(&p[4], &p[6]);
    dag[1]->link(&p[5], &p[7]);
    dag[1]->link(&p[4], &p[5]);
    cout << dag[1] << endl;
    try {
        EXPECT_TRUE(dag[1]->sort());
    } catch (DAG::CycleException &) {
        cout << "Cycle(s) found during sort..." << endl;
        dag[1]->printCycles(cout);
    }
    cout << endl;

    EXPECT_EQUAL(p[0].getGroupID(), 1);
    EXPECT_EQUAL(p[1].getGroupID(), 1);
    EXPECT_EQUAL(p[2].getGroupID(), 2);
    EXPECT_EQUAL(p[3].getGroupID(), 3);
    EXPECT_EQUAL(p[4].getGroupID(), 4);
    EXPECT_EQUAL(p[5].getGroupID(), 5);
    EXPECT_EQUAL(p[6].getGroupID(), 5);
    EXPECT_EQUAL(p[7].getGroupID(), 6);

    // Test a 5x5 grid DAG
    const uint32_t rows = 5;
    const uint32_t cols = 5;
    Vertex* g[rows][cols];
    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < cols; ++j) {
            std::stringstream ss;
            ss << i << "," << j;
            g[i][j] = new Vertex(ss.str().c_str(),s1);
        }
    }

    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < cols; ++j) {
            if (j < cols - 1) {
                dag[2]->link(g[i][j], g[i][j+1]);
            }
            if (i < rows - 1) {
                dag[2]->link(g[i][j], g[i+1][j]);
            }
        }
    }

    try {
        EXPECT_TRUE(dag[2]->sort());
    } catch (DAG::CycleException &) {
        cout << "Cycle(s) found during sort..." << endl;
        dag[2]->printCycles(cout);
    }

    cout << "______________________" << endl;
    cout << "SORTED DAG[2] (5x5 grid)" << endl;
    cout << "______________________" << endl;
    cout << dag[2];

    for(DAG* dg : dag){
        delete dg;
    }

    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < cols; ++j) {
            // Group IDs begin at 1, not zero, therefore the +1 in the comparison
            EXPECT_EQUAL(g[i][j]->getGroupID(), i + j + 1);
        }
    }

    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t j = 0; j < cols; ++j) {
            delete g[i][j];
        }
    }
    sparta::RootTreeNode rtn;
    sparta::Clock clk("clock", &sched);
    rtn.setClock(&clk);
    sparta::EventSet es(&rtn);


    // Test event operations
    typedef PayloadEvent<int> InType;
    typedef Event<> OutType;
    OutType *outp[5];
    InType  *inp[5];
    Observer          obs("Listener");

    for (uint32_t p = 0; p < 5; ++p) {
        std::stringstream sin;

        sin << "in_" << p;
        inp[p] = new InType(&es, sin.str(),
                            CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, int));

        std::stringstream sout;
        sout << "out_" << p;

        outp[p] = new OutType(&es, sout.str(), SpartaHandler("dummy"));

        *outp[p] >> *inp[0];
        if (p > 0) {
            *inp[p] >> *outp[p-1];
        }
    }

    // Chained precedence operations
    OutType *chain_outp[3];
    InType *chain_inp[3];
    for (uint32_t p = 0; p < 3; ++p) {
        std::stringstream sin;

        sin << "chain_in_" << p;
        chain_inp[p] = new InType(&es, sin.str(),
                                  CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, int));

        std::stringstream sout;
        sout << "chain_out_" << p;

        chain_outp[p] = new OutType(&es, sout.str(), SpartaHandler("dummy"));
    }

    // Set up event factory
    Event<>     e_proto(&es, "e_proto", CREATE_SPARTA_HANDLER_WITH_OBJ(Observer, &obs, activate));


    *chain_inp[0] >> *chain_outp[0] >>
        *chain_inp[1] >> *chain_outp[1] >>
        *chain_inp[2] >> *chain_outp[2];

    // Finalize
    try {
        sched.getDAG()->finalize();
        cout << "______________________" << endl;
        cout << "CHAINED PRECEDENCE DAG" << endl;
        cout << "______________________" << endl;
        cout << sched.getDAG() << endl;
    } catch (DAG::CycleException &) {
        EXPECT_TRUE(false);
        cout << "Cycle(s) found during sort..." << endl;
    }

    /*
      DAVE TO FIX -- get rid of the GOP that is...

        1        2        3         4        5          6         7        8
      TrGop -> RUGop -> PUGop ->  FlGop -> ColGop ->  TiGop -> PostTick
                                                        i1  ->    o0---.
                                                        i2  ->    o1   |
                                                        i3  ->    o2   |
                                                        i4  ->    o3   |
                                                                       +-> i0
                                                        o4  -----------'

     All of the inp's and oup's (i's and o's) precede the TickGOP in
     the dummy phase tree.
    */

    const uint32_t base_grp = 11;   // Account for the addition of 7 "global" PhasedPayloadEvent in sparta::Scheduler
    EXPECT_EQUAL(inp[0]->getScheduleable().getGroupID(),  base_grp + 2);
    EXPECT_EQUAL(inp[1]->getScheduleable().getGroupID(),  base_grp);
    EXPECT_EQUAL(inp[2]->getScheduleable().getGroupID(),  base_grp);
    EXPECT_EQUAL(inp[3]->getScheduleable().getGroupID(),  base_grp);
    EXPECT_EQUAL(inp[4]->getScheduleable().getGroupID(),  base_grp);

    EXPECT_EQUAL(outp[0]->getGroupID(), base_grp + 1);
    EXPECT_EQUAL(outp[1]->getGroupID(), base_grp + 1);
    EXPECT_EQUAL(outp[2]->getGroupID(), base_grp + 1);
    EXPECT_EQUAL(outp[3]->getGroupID(), base_grp + 1);
    EXPECT_EQUAL(outp[4]->getGroupID(), base_grp);
    rtn.enterTeardown();

    for (uint32_t p = 0; p < 5; ++p) {
        delete inp[p];
        delete outp[p];
    }

    /*
       6      7       8      9     10     11     12
      TGop -> ci0 -> co0 -> ci1 -> co1 -> ci2 -> co2
     */
    EXPECT_EQUAL(chain_inp[0]->getScheduleable().getGroupID(),  base_grp);
    EXPECT_EQUAL(chain_inp[1]->getScheduleable().getGroupID(),  base_grp + 2);
    EXPECT_EQUAL(chain_inp[2]->getScheduleable().getGroupID(),  base_grp + 4);
    EXPECT_EQUAL(chain_outp[0]->getGroupID(), base_grp + 1);
    EXPECT_EQUAL(chain_outp[1]->getGroupID(), base_grp + 3);
    EXPECT_EQUAL(chain_outp[2]->getGroupID(), base_grp + 5);

    //EXPECT_EQUAL(sched.getDAG()->numGroups(), 13);
    // Account for the addition of 7 "global" PhasedPayloadEvent in sparta::Scheduler
    EXPECT_EQUAL(sched.getDAG()->numGroups(), 19);

    for (uint32_t p = 0; p < 3; ++p) {
        delete chain_inp[p];
        delete chain_outp[p];
    }

    REPORT_ERROR;

    return ERROR_CODE;

}
