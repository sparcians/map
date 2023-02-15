
#include "Preloader.hpp"
#include "sparta/app/Simulation.hpp"
#include "cache/preload/PreloadableIF.hpp"

namespace core_example
{
    void Preloader::preload()
    {
        if (filepath_ != "")
        {
            std::cout << "[Preloading caches]: " << filepath_ << std::endl;
            sparta::cache::PreloaderIF::parseYaml_(filepath_);
        }
    }

    void Preloader::preloadPacket_(const std::string& treenode,
                                   sparta::cache::PreloadPkt& pkt)
    {
        // This is a very dump preloader and just sends the pkt directly to
        // the Preloadable node specified in the yaml. Other preloaders
        // may be more verbose.
        sparta::RootTreeNode* root = getContainer()->getSimulation()->getRoot();
        std::vector<sparta::TreeNode*> nodes;
        root->getSearchScope()->findChildren(treenode, nodes);
        bool preloaded_atleast_one = false;
        for (auto& node : nodes)
        {
            sparta::cache::PreloadableIF* cache = dynamic_cast<sparta::cache::PreloadableIF*>(node);
            if (cache)
            {
                bool success = cache->preloadPkt(pkt);
                preloaded_atleast_one |= success;
            }
        }
        sparta_assert(preloaded_atleast_one, "Failed to preload the packet to any cache");
    }
} // namespace core_example
