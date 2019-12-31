// <sparta> -*- C++ -*-


/*!
 * \file sparta.cpp
 * \brief Instantiation of globals and static members from all various sparta
 * headers which don't have enough code to warrant their own source files.
 *
 * Additionally, anything requiring a strict static initialization order must
 * exist here.
 */

#include "sparta/sparta.hpp"

#include <cstdint>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "sparta/utils/StaticInit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/Tag.hpp"
#include "sparta/utils/KeyValue.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/functional/DataView.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/log/categories/CategoryManager.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/pevents/PEventHelper.hpp"
#include "sparta/report/format/Text.hpp"
#include "sparta/pairs/PairCollectorTreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/utils/Colors.hpp"
#include "sparta/parsers/ConfigParser.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/simulation/ResourceContainer.hpp"
#include "sparta/kernel/SleeperThreadBase.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/serialization/checkpoint/Checkpoint.hpp"

namespace sparta {

    static int static_init_counter = 0;

    SpartaStaticInitializer::SpartaStaticInitializer()
    {
        if(0 == static_init_counter++){
            // initialize statics
            // ORDER IS CRITICAL
            // These must be uninitialized in reverse order in ~SpartaStaticInitializer
            //std::cout << "SpartaStaticInitializer CONSTRUCTING" << std::endl;
            color::ColorScheme::_GBL_COLOR_SCHEME = new color::ColorScheme();
            StringManager::_GBL_string_manager = new StringManager();
            ArchData::all_archdatas_ = new (std::remove_pointer<decltype(ArchData::all_archdatas_)>::type)();
            TreeNode::statics_ = new TreeNode::TreeNodeStatics();
        }
    }

    SpartaStaticInitializer::~SpartaStaticInitializer()
    {
        if(0 == --static_init_counter){
            // uninit statics (in reverse order)
            //std::cout << "SpartaStaticInitializer DESTROYING" << std::endl;
            delete TreeNode::statics_;
            delete ArchData::all_archdatas_;
            delete StringManager::_GBL_string_manager;
            delete color::ColorScheme::_GBL_COLOR_SCHEME;
        }
    }
    namespace color {
        sparta::color::ColorScheme* ColorScheme::_GBL_COLOR_SCHEME;
    }
    sparta::StringManager* StringManager::_GBL_string_manager;

    namespace log {
        // Category Strings
        constexpr char categories::WARN_STR[];
        constexpr char categories::DEBUG_STR[];
        constexpr char categories::PARAMETERS_STR[];
        const std::string* const categories::WARN  = StringManager::getStringManager().internString(WARN_STR);
        const std::string* const categories::DEBUG = StringManager::getStringManager().internString(DEBUG_STR);
        const std::string* const categories::PARAMETERS = StringManager::getStringManager().internString(PARAMETERS_STR);
        const std::string* const categories::NONE  = StringManager::getStringManager().EMPTY;
    }
}


namespace sparta {
    // ResouceContainer
    std::string ResourceContainer::getResourceTypeRaw() const {
        return typeid(*resource_).name();
    }

    std::string ResourceContainer::getResourceType() const {
        return getResourceTypeName_();
    }

    std::string ResourceContainer::getResourceTypeName_() const {
        return demangle(typeid(*resource_).name());
    }
}

SPARTA_PARAMETER_BODY;
SPARTA_CLOCK_BODY;
SPARTA_TAG_BODY;
SPARTA_KVPAIR_BODY;
SPARTA_REGISTER_BODY;

namespace sparta {
    // ArchData
    constexpr char sparta::ArchData::Line::QUICK_CHECKPOINT_PREFIX[];
    std::vector<const sparta::ArchData*>* sparta::ArchData::all_archdatas_ = nullptr;
}

SPARTA_DATAVIEW_BODY;
SPARTA_GLOBAL_TREENODE_BODY;
SPARTA_CHECKPOINT_BODY;

SPARTA_UNIT_BODY;

namespace sparta {
    // TreeNode
    TreeNode::TreeNodeStatics* TreeNode::statics_ = nullptr;
    //std::vector<TreeNode::WeakPtr> TreeNode::parentless_nodes_;
    //std::vector<TreeNode::WeakPtr> TreeNode::all_nodes_;
    TreeNode::node_uid_type TreeNode::next_node_uid_ = 0;
    TreeNode::TagsMap TreeNode::global_tags_map_;
    uint32_t TreeNode::teardown_errors_ = 0;
    const TreeNode::node_uid_type TreeNode::MAX_NODE_UID = 0xffffffffffff;
    constexpr char TreeNode::GROUP_NAME_BUILTIN[];
    constexpr char TreeNode::GROUP_NAME_NONE[];
    constexpr char TreeNode::NODE_NAME_VIRTUAL_GLOBAL[];
    constexpr char TreeNode::LOCATION_NODE_SEPARATOR_ATTACHED;
    constexpr char TreeNode::LOCATION_NODE_SEPARATOR_EXPECTING;
    constexpr char TreeNode::LOCATION_NODE_SEPARATOR_UNATTACHED;
    constexpr char TreeNode::NODE_NAME_NONE[];
    const std::string TreeNode::DEBUG_DUMP_SECTION_DIVIDER = \
        "================================================================================\n";
    const std::vector<std::pair<const char*, std::function<void (std::string&)>>> TreeNode::TREE_NODE_PATTERN_SUBS =
    {
        // Escape original parens
        {"(", [](std::string& s){replaceSubstring(s, "(", "\\(");}},
        {")", [](std::string& s){replaceSubstring(s, ")", "\\)");}},

        // Escape original brackets
        {"[", [](std::string& s){replaceSubstring(s, "[", "\\[");}},
        {"]", [](std::string& s){replaceSubstring(s, "]", "\\]");}},

        // Replace glob-like wildcards with captured-regex replacements
        {"*", [](std::string& s){replaceSubstring(s, "*", "(.*)");}},
        {"?", [](std::string& s){replaceSubstring(s, "?", "(.?)");}},
        {"+", [](std::string& s){replaceSubstring(s, "+", "(.+)");}}

        // Disabled because supporting capture will require more complext expression
        // parsing
        //{"[!", [](std::string& s){replaceSubstring(patexp, "[!", "[^")
    };

    constexpr char Scheduler::NODE_NAME[];
    const Scheduler::Tick Scheduler::INDEFINITE = 0xFFFFFFFFFFFFFFFFull;
    std::unique_ptr<sparta::SleeperThreadBase> SleeperThread::sleeper_thread_;
    uint32_t Vertex::global_id_ = 0;

    // KeyPairs
    constexpr char sparta::PairCollectorTreeNode::COLLECTABLE_DESCRIPTION[];

    namespace pevents {
        //pevents
        //PEventHelper.h
        std::array<std::string, 2> pevents::PEventProtection::PEventProtectedAttrs {{"ev", "cyc"}};
    }

    namespace report {
        namespace format {
            // Text
            const char Text::DEFAULT_REPORT_PREFIX[] = "Report ";
        }
    }

    namespace ConfigParser {
        constexpr char ConfigParser::OPTIONAL_PARAMETER_KEYWORD[];
    }

}
