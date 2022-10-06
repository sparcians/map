

#include <iostream>
#include <inttypes.h>
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "cache/preload/PreloadableIF.hpp"
#include "cache/preload/PreloadableNode.hpp"
#include "cache/preload/PreloadPkt.hpp"
#include "cache/preload/FlatPreloadPkt.hpp"
#include "cache/preload/PreloaderIF.hpp"
#include "cache/SimpleCache2.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "cache/preload/PreloadEmitter.hpp"

TEST_INIT
using namespace std::placeholders;
class MyPreloadableCache : public sparta::TreeNode
{
    struct Line : public sparta::cache::LineData {
        Line(const uint64_t linesize) :
            sparta::cache::LineData(linesize)
        {}
        std::string a;
        uint32_t b = 0;
        bool c = false;
    };
public:
    MyPreloadableCache(sparta::TreeNode* parent,
                       const std::string& name) :
        sparta::TreeNode(parent, name, "some descriptions"),
        cache(1024, 256, 256, Line(256),
              sparta::cache::TreePLRUReplacement(1/*ways*/)),
        name_(name),
        preloadable_(this,
                     std::bind(&MyPreloadableCache::preloadPkt_,
                               this, _1),
                     std::bind(&MyPreloadableCache::preloadDump_,
                               this, _1))
    {}

    sparta::cache::SimpleCache2<Line> cache;

private:
    bool preloadPkt_(sparta::cache::PreloadPkt& data)
    {
        std::cout << "preloadPkt_ ";
        data.print(std::cout);
        std::cout << std::endl;
        // cache three has a different yaml structure.
        // we want to check that it exists properly.
        if (name_ == "cache3")
        {
            // Since I don't actually load the data for cache3
            // into any cache, I just want to make sure it atleast
            // got the preload call and that the following expect
            // statements were executed.
            EXPECT_REACHED();
            std::cout << "preloading cache3" << std::endl;
            EXPECT_EQUAL(data.getScalar<std::string>("a"), "a");
            EXPECT_EQUAL(data.getScalar<std::string>("c"), "c");
            EXPECT_EQUAL(data.hasKey("c"), true);
            EXPECT_EQUAL(data.hasKey("nested_data"), true);
            sparta::cache::PreloadPkt::NodeHandle nested_data;
            nested_data = data.getMap("nested_data");
            EXPECT_TRUE(nested_data->hasKey("z"));
            EXPECT_EQUAL(nested_data->getScalar<uint32_t>("z"), 100);
            sparta::cache::PreloadPkt::NodeList a_list;
            uint32_t len = nested_data->getList("deep_list", a_list);
            EXPECT_EQUAL(len, 3);
            EXPECT_EQUAL(a_list[0]->getScalar<int>("a"), 0);
            EXPECT_EQUAL(a_list[1]->getScalar<int>("a"), 1);
            EXPECT_EQUAL(a_list[2]->getScalar<int>("a"), 2);

            return true;
        }
        else if (name_ == "cache_simple")
        {
            // Since I don't actually load the data for cache_simple
            // into any cache, I just want to make sure it atleast
            // got the preload call and that the following expect
            // statements were executed.
            EXPECT_REACHED();
            std::cout << "preloading cache_simple" << std::endl;
            sparta::cache::PreloadPkt::NodeList line_list;
            uint32_t len = data.getList(line_list);
            EXPECT_EQUAL(len, 3);
            EXPECT_EQUAL(line_list[0]->getScalar<uint64_t>("va"), 0x1000);
            EXPECT_EQUAL(line_list[0]->getScalar<uint64_t>("val"), 0xfffff);
            EXPECT_EQUAL(line_list[1]->getScalar<uint64_t>("va"), 0x2000);
            EXPECT_EQUAL(line_list[1]->getScalar<uint64_t>("val"), 0xfffff);
            EXPECT_EQUAL(line_list[2]->getScalar<uint64_t>("va"), 0x3000);
            EXPECT_EQUAL(line_list[2]->getScalar<uint64_t>("val"), 0xaaaaa);
            return true;
        }

        sparta::cache::PreloadPkt::NodeList list;
        data.getList("lines", list);
        for (auto& node : list)
        {
            uint64_t va = node->getScalar<uint64_t>("va");
            Line& line = cache.getLineForReplacement(va);
            line.a = node->getScalar<std::string>("a");
            line.b = node->getScalar<uint32_t>("b");
            line.c = node->getScalar<bool>("c");
            std::cout << *this  << "preloaded line. VA: 0x" << std::hex << va
                      << std::dec << " a: " << line.a
                      << " b: " << line.b << " c: " << line.c
                      << std::endl;
            cache.allocateWithMRUUpdate(line, va);
            // make sure the cache itself didn't fail.
            sparta_assert(cache.getLine(va) != nullptr);
        }
        return true;
    }

    void preloadDump_(sparta::cache::PreloadEmitter& emitter) const
    {
        emitter << sparta::cache::PreloadEmitter::BeginMap;
        emitter << sparta::cache::PreloadEmitter::Key << "lines";
        emitter << sparta::cache::PreloadEmitter::Value;
        emitter << sparta::cache::PreloadEmitter::BeginSeq;
        for (auto set_it = cache.begin();
             set_it != cache.end(); ++set_it)
        {
            for (auto line_it = set_it->begin();
                 line_it != set_it->end(); ++line_it)
            {
                if(line_it->isValid())
                {
                    std::map<std::string, std::string> map;
                    map["a"] = line_it->a;
                    map["b"] = std::to_string(line_it->b);
                    emitter << map;
                }
            }
        }
        emitter << sparta::cache::PreloadEmitter::EndSeq;
        emitter << sparta::cache::PreloadEmitter::EndMap;

    }

    std::string name_;
    sparta::cache::PreloadableNode preloadable_;
};

class SamplePreloader : public sparta::cache::PreloaderIF
{
public:
    SamplePreloader(sparta::RootTreeNode* root) :
        root_(root)
    {
        sparta::cache::PreloaderIF::parseYaml_("samplepreload1.yaml");
    }

    void dumpAndVerify()
    {
        std::cout << std::endl << " --------- PRELOAD DUMP -------------" << std::endl;
        std::stringstream ss;
        dumpPreloadTree_(root_, ss);
        std::cout << ss.str() << std::endl;
        std::cout << "-------------------------------------" << std::endl;
        // compare the string.
        std::ifstream expected("expected_dump.yaml");
        std::stringstream expected_buffer;
        expected_buffer << expected.rdbuf();
        EXPECT_EQUAL(ss.str(), expected_buffer.str());

    }
private:
    virtual void preloadPacket_(const std::string& treenode,
                                sparta::cache::PreloadPkt& pkt) override
    {
        std::vector<sparta::TreeNode*> nodes;
        root_->getSearchScope()->findChildren(treenode, nodes);
        for (auto& node : nodes)
        {
            sparta::cache::PreloadableIF* cache = dynamic_cast<sparta::cache::PreloadableIF*>(node);
            if (cache)
            {
                cache->preloadPkt(pkt);
            }

        }
    }
    sparta::RootTreeNode* root_ = nullptr;
};


//____________________________________________________________
// MAIN
int main()
{
    sparta::RootTreeNode rtn;
    sparta::TreeNode core0(&rtn, "core0", "core0 node");
    MyPreloadableCache cache1(&core0, "cache1");
    MyPreloadableCache cache2(&core0, "cache2");
    MyPreloadableCache cache3(&rtn, "cache3");
    MyPreloadableCache simple_cache(&rtn, "cache_simple");
    std::cout << rtn.renderSubtree() << std::endl;
    SamplePreloader sample(&rtn);
    // Make sure the caches have the correct data.
    {
        // cache 1, spot check some values.
        sparta_assert(cache1.cache.getLine(0x1000) != nullptr);
        EXPECT_EQUAL(cache1.cache.getLine(0x1000)->a, "test");
        EXPECT_EQUAL(cache1.cache.getLine(0x4000)->c, true);
        EXPECT_EQUAL(cache1.cache.getLine(0xffff)->b, 0x3333);
        // cache 2 spot check some values.
        EXPECT_EQUAL(cache2.cache.getLine(0x4000)->a, "hello2world");
        EXPECT_EQUAL(cache2.cache.getLine(0xffff)->c, false);
        EXPECT_EQUAL(cache2.cache.getLine(0x2000)->c, true);

    }

    // Test the FlatPreloadPkt()
    {
        sparta::cache::FlatPreloadPkt pkt;
        pkt.addValue("a", "a");
        pkt.addValue("b", "0x300");
        pkt.addValue("test", "hello");
        EXPECT_EQUAL(pkt.getScalar<std::string>("a"), "a");
        EXPECT_EQUAL(pkt.getScalar<uint32_t>("b"), 0x300);
        EXPECT_EQUAL(pkt.getScalar<std::string>("test"), "hello");
        EXPECT_THROW(pkt.getScalar<uint32_t>("NOKEY"));
        EXPECT_THROW(pkt.getMap("test"));
        sparta::cache::PreloadPkt::NodeList list;
        EXPECT_THROW(pkt.getList("test", list));

    }

    // Test the preload emitter.
    {
        using namespace sparta::cache;
        PreloadEmitter simple;
        simple << PreloadEmitter::BeginMap;
        simple << PreloadEmitter::Key << "key";
        simple << PreloadEmitter::Value << "test";
        simple << PreloadEmitter::EndMap;
        std::cout << simple << std::endl;
        PreloadEmitter em;
        em << PreloadEmitter::BeginMap;
        em << PreloadEmitter::Key;
        em << "lines";
        em << PreloadEmitter::Value;
        em << PreloadEmitter::BeginSeq;
        em << PreloadEmitter::BeginMap;
        em << PreloadEmitter::Key << "a" << PreloadEmitter::Value << 0x200;
        em << PreloadEmitter::Key << "b" << PreloadEmitter::Value << 0x300;
        em << PreloadEmitter::EndMap;
        em << PreloadEmitter::BeginMap;
        em << PreloadEmitter::Key << "va" << PreloadEmitter::Value << 0x400;
        em << PreloadEmitter::Key << "pa" << PreloadEmitter::Value << 0x500;
        em << PreloadEmitter::EndMap;
        em << PreloadEmitter::EndSeq;
        em << PreloadEmitter::EndMap;
        std::cout << em;
        std::stringstream stream;
        stream << em;
        YamlPreloadPkt pkt(stream);
        PreloadPkt::NodeList list;
        pkt.getList("lines", list);
        auto& node = list[0];
        EXPECT_EQUAL(node->getScalar<uint32_t>("a"), 0x200);

    }

    sample.dumpAndVerify();

    rtn.enterTeardown();
    ENSURE_ALL_REACHED(1);
    REPORT_ERROR;
    return ERROR_CODE;
}
