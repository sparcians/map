#include <iostream>
#include <cstring>

#include "sparta/sparta.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/parsers/ConfigParserYAML.hpp"

#include "sparta/simulation/TreeNode.hpp"

using sparta::ParameterTree;

TEST_INIT

/*
 * Tests virtual parameter tree construction and extraction.
 *
 * This is a tree containing all command line configuration file parameters which have not
 * necessarily been applied to the actual sparta device (TreeNode) tree.
 */
int main ()
{
    // Instantiation

    // Specific Parameter Set
    ParameterTree pt;


    // Test various constructors in ParameterTree
    ParameterTree pt2;
    pt2 = pt;
    ParameterTree pt3(pt2);

    //EXPECT_EQUAL(pt, pt2);
    //EXPECT_EQUAL(pt, pt3); // Commutativity

    // Apply "command-line" parameters

    pt.set("top.foo.bar", "1", true, "origin #1");
    auto tfb = pt.create("top.foo.buz");
    EXPECT_EQUAL(tfb->getPath(), "top.foo.buz");
    tfb->setValue("topfoobuz");
    EXPECT_EQUAL(tfb->getValue(), "topfoobuz");
    EXPECT_EQUAL(pt.get("top.foo.buz").getValue(), "topfoobuz");
    //pt"top.foo.biz"] = "0x2";
    //pt["top"]["foo"]["baz"] = "03";

    pt.recursePrint(std::cout);

    // Read some values
    EXPECT_EQUAL(pt.get("top.foo.bar"), "1");
    //EXPECT_EQUAL(pt["top.foo.bar"], "1");
    //EXPECT_EQUAL(pt["top"]["foo"]["bar"], "1");
    EXPECT_THROW(pt["top"]["nope"]);
    EXPECT_THROW(pt["nope"]);
    EXPECT_NOTHROW(pt[""]);
    EXPECT_NOTHROW(pt.get(""));
    EXPECT_EQUAL(pt.get("").getName(), "");
    EXPECT_NOTHROW(pt.get("top")["foo"]);

    EXPECT_EQUAL(pt.get("top.foo.bar").getAs<char[]>(), "1");
    EXPECT_EQUAL(pt.get("top.foo.bar").getAs<uint32_t>(), 1);
    EXPECT_EQUAL(pt.get("top.foo.bar").getOrigin(), "origin #1");

    // Test wildcard access
    pt.set("top.foo.*", "2", true, "origin #2");
    std::cout << "A:" << std::endl;
    pt.recursePrint(std::cout);
    EXPECT_EQUAL(pt.get("top.foo.bar"), "2");
    EXPECT_EQUAL(pt.get("top.foo.bar").getOrigin(), "origin #2");
    EXPECT_EQUAL(pt.get("top.foo.something_else"), "2");

    pt.set("top.foo.biz", "3", true);
    std::cout << "B:" << std::endl;
    pt.recursePrint(std::cout);
    EXPECT_EQUAL(pt.get("top.foo.bar"), "2");
    EXPECT_EQUAL(pt.get("top.foo.biz"), "3");
    EXPECT_EQUAL(pt.get("top.foo.something_else"), "2");
    EXPECT_EQUAL(pt.get("top.foo.something_else").getAs<uint32_t>(), 2);

    pt.set("top.*.biz", "4", true);
    std::cout << "C:" << std::endl;
    pt.recursePrint(std::cout);
    EXPECT_EQUAL(pt.get("top.foo.bar"), "2");
    EXPECT_EQUAL(pt.get("top.foo.biz"), "4");
    EXPECT_EQUAL(pt.get("top.foo.something_else"), "2");
    EXPECT_EQUAL(pt.get("top.something_else.biz"), "4");

    pt.set("top.foo+.biz", "5", true);
    std::cout << "D:" << std::endl;
    pt.recursePrint(std::cout);
    EXPECT_EQUAL(pt.get("top.foo.bar"), "2");
    EXPECT_EQUAL(pt.get("top.foo.biz"), "4");
    EXPECT_EQUAL(pt.get("top.foo.something_else"), "2");
    EXPECT_EQUAL(pt.get("top.something_else.biz"), "4");
    EXPECT_EQUAL(pt.get("top.fooze.biz"), "5");
    EXPECT_EQUAL(pt.get("top.fooze.biz").getAs<uint32_t>(), 5);

    // For now parent (..) access when setting a parameter, changes NOTHING
    EXPECT_EQUAL(pt.set("top.foo+..", "6", true), false);
    std::cout << "E:" << std::endl;
    pt.recursePrint(std::cout);
    EXPECT_EQUAL(pt.get("top.foo.bar"), "2");
    EXPECT_EQUAL(pt.get("top.foo.biz"), "4");
    EXPECT_EQUAL(pt.get("top.foo.something_else"), "2");
    EXPECT_EQUAL(pt.get("top.something_else.biz"), "4");
    EXPECT_EQUAL(pt.get("top.fooze.biz"), "5");
    EXPECT_EQUAL(pt.get("top.fooze.biz").getAs<uint32_t>(), 5);

    // Creating a node does not require it. Setting a value somewhere in it does (if that value is required)
    auto tfbfb1 = pt.create("top.foo.bar.fiz.bin1");
    auto tfbfb2 = pt.create("top.foo.bar.fiz.bin2");
    auto tfbfbpat = pt.create("top.foo.bar.fiz.bin*");
    tfbfb1->setValue("NUMBER ONE", true); // Required (typical behavior)
    tfbfb2->setValue("NUMBER TWO", true); // Required (typical behavior)
    tfbfbpat->setValue("NUMBER THREE", true); // Required (typical behavior)
    EXPECT_TRUE(tfbfb1->isRequired());
    EXPECT_TRUE(tfbfb2->isRequired());
    tfbfbpat->unrequire(); // Supporting deprecated parameters, ignoring a param if missing from model, etc.
    EXPECT_FALSE(tfbfb1->isRequired()); // Hits "top.foo.bar.fiz.bin*" node first
    EXPECT_FALSE(tfbfb2->isRequired()); // Hits "top.foo.bar.fiz.bin*" node first
    EXPECT_FALSE(tfbfbpat->isRequired());

    std::vector<const sparta::ParameterTree::Node*> unreads;
    pt.getUnreadValueNodes(&unreads);
    std::cout << "Unreads: " << unreads << std::endl;

    std::cout << "After all nodes" << std::endl;
    pt.recursePrint(std::cout);

    // Test unrequire at the path level
    pt.set("top.foo.bar.fiz.bin1", "blah", true);
    pt.set("top.foo.bar.fiz.bin2", "blee", true);
    EXPECT_TRUE(pt.isRequired("top.foo.bar.fiz.bin1"));
    EXPECT_TRUE(tfbfb2->isRequired());
    pt.unrequire("top.foo.bar.fiz");
    EXPECT_FALSE(pt.isRequired("top.foo.bar.fiz.bin1"));
    EXPECT_FALSE(tfbfb2->isRequired());

    // Parse YAML file reading
    sparta::ConfigParser::YAML param_file("input.yaml", {});
    param_file.allowMissingNodes(true);
    sparta::TreeNode tmp("top", "dummy top");
    sparta::TreeNode tmp2(&tmp, "foo", "dummy top.foo");
    param_file.consumeParameters(&tmp, false); // verbose?
    //ParameterTree ypt = param_file.getParameterTree(); // Copy
    std::cout << "ParameterTree from config file" << std::endl;
    param_file.getParameterTree().recursePrint(std::cout);
    EXPECT_EQUAL(param_file.getParameterTree().get("top.foo.bar"), "0x001");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.foo.biz"), "0x2");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.foo.baz"), "03");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.foo.a.b.c"), "abc_value");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.fiz.bin"), "top.fiz.bin");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.something_else.pez"), "top.*.pez");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.foo.poz"), "0");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.fiz.piz").getValue(), "[1,2,3]");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.fiz.paz").getValue(), "[[1,2,3],[4,5,6],[],[7,8,9]]");
    EXPECT_EQUAL(param_file.getParameterTree().get("top.fiz.puz").getValue(), "[a,b,c,\"\"]");

    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo.eiohewfoewhjfoihefwo9hwe"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.something_else.efoejhwfiojn390ewjfofief"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo.baro9jkdfoijdfoindf"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo890hiw8nhfedf.bar"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo.a.b.c.d.e.f.g"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo"));

    EXPECT_TRUE(param_file.getParameterTree().isRead("top.foo.bar"));
    EXPECT_TRUE(param_file.getParameterTree().isRead("top.foo.biz"));
    EXPECT_TRUE(param_file.getParameterTree().isRead("top.foo.baz"));
    EXPECT_TRUE(param_file.getParameterTree().isRead("top.foo.a.b.c"));
    EXPECT_TRUE(param_file.getParameterTree().isRead("top.fiz.bin"));
    EXPECT_TRUE(param_file.getParameterTree().isRead("top.something_else.pez"));
    EXPECT_FALSE(param_file.getParameterTree().isRead("top.foo.bar.fiz.bin"));

    // Test PT assignment
    pt2 = pt;
    ParameterTree pt4;
    pt4.set("top.biz.buz", "pt4", true);
    pt4 = param_file.getParameterTree();
    std::cout << "After cloning yaml file output tree to pt4" << std::endl;
    pt4.recursePrint(std::cout);
    EXPECT_THROW(pt4.get("top.biz.buz")); // Cleared as part of pt3 = ...
    EXPECT_EQUAL(pt4.get("top.foo.a.b.c"), "abc_value");
    EXPECT_EQUAL(pt4.get("top.fiz.bin"), "top.fiz.bin");
    EXPECT_EQUAL(pt4.get("top.something_else.pez"), "top.*.pez");

    // Test PT comparison
    //EXPECT_EQUAL(pt, pt2);

    pt2.set("top.foo.bar", "nothing will possilbly match this value!!!", true);
    //EXPECT_NOTEQUAL(pt, pt2);

    pt2.clear();
    std::cout << "After clearing pt2" << std::endl;
    pt2.recursePrint(std::cout);

    // Walk to apply to a device tree

    // Test Value (get, access, invalid, copy-construct, assign, lexical cast access)

    //Value v = pt.get("top.foo");
    //Value v2;
    //v2 = v;
    //Value v3(v2);
    //EXPECT_EQUAL(v, v2);
    //EXPECT_EQUAL(v, v3); // Commutativity

    // Test Node (get, access, copy-construct)

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
