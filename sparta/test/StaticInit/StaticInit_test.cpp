
#include <inttypes.h>
#include <iostream>
#include <memory>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaTester.hpp"


/*!
 * \file main.cpp
 * \brief Test for StatisticSet, StatisticDef, and StatisticInstance
 *
 * Under the right circumstances, this test will ensure that there are no
 * segfaults during static destruction caused by SPARTA static members being
 * destructed before other objects that depend on them (e.g. TreeNode
 * instances). Note that this may not always be a valid test, because static
 * initialization order is not defined and some compilers will
 * for gcc-4.7.0, this test originally caused a segfault in TreeNode::~TreeNode
 * which was fixed.
 */

TEST_INIT

using sparta::StatisticDef;
using sparta::StatisticInstance;
using sparta::StatisticSet;
using sparta::RootTreeNode;
using sparta::TreeNode;
using sparta::Counter;


class StaticStuff {
public:
    RootTreeNode root;
    TreeNode dummy;

    StaticStuff() :
        dummy(&root, "dummy", "A dummy node")
    {;}
};

/*!
 * \brief Instantiate this in main, and allow the static destruction to destroy
 * it.
 */
std::unique_ptr<StaticStuff> sstuff;

/*!
 * \brief Instantiate this globally and allow the static destructor to destroy
 * it
 *
 * \warning NOT YET SUPPORTED because of StringManager's statics.
 * \verbatim
 *   what():  _GBL_string_manager.is_constructed == IS_CONSTRUCTED_CONST: Attempted to access StringManager singleton before it was statically constructed.: in file: '/sarc/spa/users/nhaw/proj/sparta/test/StaticInit/../../sparta/StringManager.h', on line: 121
 * \endverbatim
 */
StaticStuff ss2;

int main()
{
    // Place into a tree
    sstuff.reset(new StaticStuff());

    StatisticSet sset(&sstuff->root);
    StatisticSet cset(&sstuff->dummy);
    Counter ctr(&cset, "a", "Counter A", Counter::COUNT_NORMAL);
    EXPECT_TRUE(sset.isAttached()); // Ensure that node constructed with parent arg is properly attached

    // Done

    // Report errors before drawing trees in case any nodes were attached which
    // should not have been.
    REPORT_ERROR;


    // Render Tree for information purposes

    std::cout << "The tree from the top with builtins: " << std::endl << sstuff->root.renderSubtree(-1, true) << std::endl;
    std::cout << "The tree from the top without builtins: " << std::endl << sstuff->root.renderSubtree() << std::endl;
    std::cout << "The tree from sset: " << std::endl << sset.renderSubtree(-1, true);

    sstuff->root.enterTeardown();
    ss2.root.enterTeardown();

    return ERROR_CODE;
}
