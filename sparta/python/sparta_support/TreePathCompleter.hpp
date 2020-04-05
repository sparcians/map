// <TreePathCompleter> -*- C++ -*-


/*!
 * \file TreePathCompleter.h
 * \brief Argument Tab-completer for methods accepting tree-paths.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <sstream>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <cctype>

#include <boost/python.hpp>
#include "sparta/simulation/ParameterTree.hpp"
#include "python/sparta_support/module_sparta.hpp"

namespace sparta{

    /*!
     * \brief Argument completer class for arch-tree and config-tree.
     *  This class is capable of providing tab-completed paths in methods
     *  of SimulationConfiguration, ParameterTree and ParameterTreeNode
     *  during interactive python sessions.
     */
    class TreePathCompleter{
    public:
        /*!
         * \brief Default constructible.
         */
        TreePathCompleter() = default;
        /*!
         * \brief Not copy-contructible
         */
        TreePathCompleter(const TreePathCompleter&) = delete;

        /*!
         * \brief Not move-contructible
         */
        TreePathCompleter(TreePathCompleter&&) = delete;
        ~TreePathCompleter() = default;

        /*!
         * \brief Build the initial regex-engine
         * \param tree The actual sparta tree to build from
         * \param tree_addr_of Address of this tree (do not really need it)
         * \param tree_type Tree type refers to arch or config tree
         * \param pattern For processing parameters, a pattern is supplied
         */
        void buildCompleter(const sparta::ParameterTree& tree,
                            sparta::ParameterTree* tree_addr_of,
                            const std::string& tree_type,
                            const std::string& pattern = ""){
            sparta_assert(tree_addr_of, "The parameter tree address cannot be a nullptr");

            // This method is called everytime a new parameter is added
            // from processParameter API but the tree-completer is set up
            // only during the first time. The rest of the time, the update
            // method is called.
            if(__builtin_expect(checkNewTreeId_(tree_addr_of), 0)){
                setTreeId_(tree_addr_of, tree_type);
                populateTreeUtil_(tree.getRoot(), tree_addr_of);
                buildRegexDef_(tree_addr_of);
                buildRegexHook_(tree_addr_of);
                updatePythonMap_();
                invokeRegexCompleter_();
                return;
            }
            updateCompleter_(pattern, tree_addr_of);
        }

        // This method updates the regex-engine if a new parameter is added
        // or if there is a new path in the arch tree.
        void updateCompleter(const std::string& path, sparta::ParameterTree* tree_addr_of){
            return updateCompleter_(path, tree_addr_of);
        }

    private:

        // This method checks if the tree we are building the regex-engine from
        // is already built or not.
        bool checkNewTreeId_(sparta::ParameterTree* tree_addr_of) const{
            return tree_type_id_.find(tree_addr_of) == tree_type_id_.end();
        }

        // This method takes the new path from the tree and updates the
        // regex-engine from it.
        void updateCompleter_(const std::string& path, sparta::ParameterTree* tree_addr_of){

            // Cases when users are rebuilding the same path again and again is rare.
            if(__builtin_expect(actual_tree_map_[tree_addr_of].find(path) ==
                                actual_tree_map_[tree_addr_of].end(), 1)){
                return addNewPath_(path, tree_addr_of);
            }
        }

        // We store a string name for every tree pointer which is useful for
        // storing a map in python.
        void setTreeId_(sparta::ParameterTree* tree_addr_of, const std::string& tree_type){
            tree_type_id_[tree_addr_of] = tree_type;
            reverse_lookup_id_[tree_type] = tree_addr_of;
        }

        // This method takes a dot separated tree path and makes it double underscore separated.
        // This is done because python method names cannot have dots in them and we autogenerate
        // methods in python from tree paths.
        void replaceDotsInPath_(const std::string& path, sparta::ParameterTree* tree_addr_of){
            std::size_t count = std::count(path.begin(), path.end(), '.');
            char * c = new char[path.size() + count + 1];
            c[path.size() + count] = '\n';
            std::size_t j {0};
            for(std::size_t i {0u}; i < path.size(); ++i){
                if(path[i] != '.'){
                    c[j++] = path[i];
                }
                else{
                    c[j++] = '_';
                    c[j++] = '_';
                }
            }
            std::string new_path(c, path.size() + count);
            delete [] c;
            name_map_[tree_addr_of].insert({path, new_path});
        }

        // This method takes a sparta tree and builds a parent -> children map from it.
        void populateTreeUtil_(const sparta::ParameterTree::Node* root, sparta::ParameterTree* tree_addr_of){
            if(__builtin_expect(!root->hasValue(), 1)){
                std::string path {root->getPath()};
                actual_tree_map_[tree_addr_of].insert({ path, std::vector<std::string>() });
                replaceDotsInPath_(path, tree_addr_of);
                tree_map_[tree_addr_of].insert({ name_map_[tree_addr_of][path], std::vector<std::string>() });
                for(auto children : root->getChildren()){
                    tree_map_[tree_addr_of][name_map_[tree_addr_of][path]].emplace_back(children->getName());
                    actual_tree_map_[tree_addr_of][path].emplace_back(children->getName());
                    populateTreeUtil_(children, tree_addr_of);
                }
            }
        }

        // The internal regex map in IPython StrDispatchers cannot be overwritten.
        // So, when new nodes are added to a parent, we need to remove the function
        // hook for that parent, update the function to return the new child, and
        // push it back in the regex map. This method removes that function hook.
        void removeFunctionHooks_(const std::string& rgx_key){
            def_hook_str_ += "__re_key = " +  rgx_key + "\n"
                          +  "__regex_map = get_ipython().strdispatchers['complete_command'].regexs\n"
                          +  "if __re_str_map.has_key(__re_key) == True:\n"
                          +  "    if __regex_map.has_key(__re_str_map[__re_key]) == True:\n"
                          +  "        __regex_map.pop(__re_str_map[__re_key])\n";
        }

        // This method takes a regex string which is the user readline and
        // build the function to be called when tab-completed. We store this
        // function in the regex map.
        void buildFunctionHooks_(const std::string& rgx_string, const std::string& rgx_key){
            // This autogenerated should be an instancemethod of the interpreter.
            def_hook_str_ += "__f__" + rgx_string + "__completer = types.MethodType(__" + rgx_string +  "__completer, get_ipython())\n"
                          // Store the regex key.
                          +  "__re_key = " + rgx_key + "\n"
                          // Compile the regex_key and store it in regex_map.
                          +  "__re_str_map[__re_key] = re.compile(__re_key)\n"
                          // Call the IPython complete_command dict.
                          +  "__sdp = get_ipython().strdispatchers.get('complete_command', IPython.utils.strdispatch.StrDispatch())\n"
                          // Add your own regex instance in the map.
                          +  "__sdp.add_re(__re_str_map[__re_key], __f__" + rgx_string + "__completer, 50)\n"
                          +  "get_ipython().strdispatchers['complete_command'] = __sdp\n"
                          +  "__dp = getattr(get_ipython().hooks, 'complete_command', None)\n"
                          +  "if 'complete_command' in IPython.core.hooks.__all__:\n"
                          +  "    print 'Warning! You are customizing an existing hook.'\n"
                          +  "if False and ('complete_command' in IPython.core.hooks.deprecated):\n"
                          +  "    alternative = IPython.core.hooks.deprecated['complete_command']\n"
                          +  "    warn('Hook {} is deprecated. Use {} instead.'.format('complete_command', alternative))\n"
                          +  "if not __dp:\n"
                          +  "    __dp = IPython.core.hooks.CommandChainDispatcher()\n"
                          +  "try:\n"
                          +  "    __dp.add(__f__" +  rgx_string + "__completer, 50)\n"
                          +  "except AttributeError:\n"
                          +  "    __dp = __f__" + rgx_string + "__completer\n"
                          +  "setattr(get_ipython().hooks, 'complete_command', __dp)\n";
        }

        // Tab completion for nodes is different from tab completion for trees
        // because tab completion for trees always starts with root. node tab
        // completion can start from anywhere and this method figures that out.
        void buildNodeHooks_(){
            std::string regex_key = "";
            // These must be the key-word APIs that the node tab-completer works on.
            std::string suffix_regex_key = R"((setChild|createChild|addChild|getChild)\(\'\s?)";
            // The regex string could start with anything, but must contain a dot[API](', followed
            // by one or more characters and dot, or just the end of the line.
            regex_key += R"('.*\.)" + suffix_regex_key + R"((.+\..$)|(\s?.$)')";
            // First remove the function hook, if it exists.
            removeFunctionHooks_(regex_key);
            def_hook_str_ += std::string("")
                          +  "def __node__completer(self, event):\n"
                          // Strip out whitespace from user-readline to make things simple.
                          +  "    event.line = event.line.replace(' ', '')\n"
                          // The accpeted node level APIs are these.
                          +  "    patterns = ['.setChild', '.createChild', '.addChild', '.getChild']\n"
                          +  "    max_ind = 0\n"
                          // We need to look for the last occurence of API key-word.
                          // This is because we need to have multiple tab-completions in a
                          // single user-readline. This is because users can call multiple
                          // tab-completed APIs in a single line.
                          +  "    for pattern in patterns:\n"
                          +  "        try:\n"
                          +  "            index = event.line.rfind(re.findall(pattern, event.line)[-1])\n"
                          +  "        except IndexError:\n"
                          +  "            pass\n"
                          +  "        else:\n"
                          +  "            if index > max_ind:\n"
                          +  "                max_ind = index\n"
                          // Store whatever is to the left of the last API.
                          +  "    prefix = event.line[:max_ind]\n"
                          // Store whatever is to the right of the last API.
                          +  "    suffix = event.line[max_ind + 1:]\n"
                          // We need to store the partially completed path from within quotes.
                          // Because we need to analyse what the user has already provided, build the
                          // concrete path by prepending the calling objects own path and then retrieve
                          // the children from the map.
                          +  "    index = suffix.rfind(re.findall(\"'\", suffix)[-1])\n"
                          // Get the partial query path.
                          +  "    g_path = suffix[index + 1:]\n"
                          +  "    var = ";
            // All these keywords are line splitters. We want to identify the object which called this API.
            regex_key = std::string("")
                      +  R"(re.split('~|`|!|@|#|$|=|==|%|&|,|<|>|\+|\/|\(|\)|\*|\^|\-)"
                      +  R"(|\'|\"|\n|if|is|in|and|or|not|as|False|None|True|assert|break|class|)"
                      +  R"(continue|def|del|elif|else|except|finally|for|from|global|import|)"
                      +  R"(lambda|nonlocal|pass|raise|return|try|while|with|yield', prefix))";
            def_hook_str_ += regex_key
                          +  "\n";
            def_hook_str_ += std::string("")
                          // Check if the calling object is a node of arch tree.
                          +  "    if __ipytse.__dict__['user_ns'].has_key(var[-1]):\n"
                          +  "        if __ipytse.__dict__['user_ns'][var[-1]].owner is sim_config.arch_ptree:\n"
                          // Build complete tree path.
                          +  "            full_path = __ipytse.__dict__['user_ns'][var[-1]].path + '.' + g_path\n"
                          // If path ends with a dot, remove it and lookup.
                          +  "            if full_path[-1] == '.':\n"
                          // If path does not exists, return empty list else return children list.
                          +  "                if __actual_tree_map['architecture'].has_key(full_path[:-1]):\n"
                          +  "                    return __actual_tree_map['architecture'][full_path[:-1]]\n"
                          +  "                else:\n"
                          +  "                    return ['']\n"
                          +  "            else:\n"
                          +  "                if __actual_tree_map['architecture'].has_key(full_path):\n"
                          +  "                    return __actual_tree_map['architecture'][full_path]\n"
                          +  "                else:\n"
                          +  "                    return ['']\n"
                          // Check if the calling object is a node of config tree.
                          +  "        if __ipytse.__dict__['user_ns'][var[-1]].owner is sim_config.config_ptree:\n"
                          // Build complete tree path.
                          +  "            full_path = __ipytse.__dict__['user_ns'][var[-1]].path + '.' + g_path\n"
                          // If path ends with a dot, remove it and lookup.
                          +  "            if full_path[-1] == '.':\n"
                          // If path does not exists, return empty list else return children list.
                          +  "                if __actual_tree_map['parameter'].has_key(full_path[:-1]):\n"
                          +  "                    return __actual_tree_map['parameter'][full_path[:-1]]\n"
                          +  "                else:\n"
                          +  "                    return ['']\n"
                          +  "            else:\n"
                          +  "                if __actual_tree_map['parameter'].has_key(full_path):\n"
                          +  "                    return __actual_tree_map['parameter'][full_path]\n"
                          +  "                else:\n"
                          +  "                    return ['']\n";
        }

        // This method builds an argument completer for a parent-children set.
        // Given a string parent and a vector of string children and a tree address,
        // this method builds the actual python def which is called on tab-completion.
        void buildArgsCompleter_(const std::string& parent,
                                 const std::vector<std::string>& children,
                                 sparta::ParameterTree* tree_addr_of){
            def_hook_str_ += "def __"
                          +  parent
                          +  "__completer(self, event):\n"
                          // We can strip away the whitespace to make things simple.
                          +  "    event.line = event.line.replace(' ', '')\n"
                          // We need to look out for these key-word APIs.
                          +  "    patterns = ['.getNode', '.create', '.hasValue', '.isRead', "
                          +  "'.isRequired', '.exists', '.set', '.unrequire', '.processParameter']\n"
                          +  "    max_ind = 0\n"
                          // We need to look for the last occurence of API key-word.
                          // This is because we need to have multiple tab-completions in a
                          // single user-readline. This is because users can call multiple
                          // tab-completed APIs in a single line.
                          +  "    for pattern in patterns:\n"
                          +  "        try:\n"
                          +  "            index = event.line.rfind(re.findall(pattern, event.line)[-1])\n"
                          +  "        except IndexError:\n"
                          +  "            pass\n"
                          +  "        else:\n"
                          +  "            if index > max_ind:\n"
                          +  "                max_ind = index\n"
                          // Store whatever is to the left of the last API.
                          +  "    prefix = event.line[:max_ind]\n"
                          +  "    var = ";
            // All these keywords are line splitters. We want to identify the object which called this API.
            std::string regex_key = std::string("")
                                  + R"(re.split('~|`|!|@|#|$|=|==|%|&|,|<|>|\+|\/|\(|\)|\*|\^|\-)"
                                  + R"(|\'|\"|\n|if|is|in|and|or|not|as|False|None|True|assert|break|class|)"
                                  + R"(continue|def|del|elif|else|except|finally|for|from|global|import|)"
                                  + R"(lambda|nonlocal|pass|raise|return|try|while|with|yield', prefix))";
            def_hook_str_ += regex_key
                          +  "\n";
            // We need to know whether this is a arch tree or config tree.
            // Depending on this, we will insert the appropriate children in the map.
            if(tree_type_id_[tree_addr_of] == "architecture"){
                def_hook_str_ += std::string("")
                              // var[-1] contains the calling object of the API.
                              // If it is a arch_ptree or any other alias of it, we return the children list
                              // of the appropriate tree.
                              +  "    if (var[-1] == 'sim_config.arch_ptree') or "
                              +  "((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is "
                              +  "sim_config.arch_ptree)):\n"
                              +  "        return[";
                // Add all the children.
                for(const auto& items : children){
                    def_hook_str_ += "'" + items + "',";
                }
                def_hook_str_ += std::string("")
                              +  "'']\n"
                              // var[-1] contains the calling object of the API.
                              // If it is a config_ptree or any other alias of it, we return the children list
                              // of the appropriate tree.
                              +  "    elif ((var[-1] == 'sim_config.config_ptree') or "
                              +  "(var[-1] == 'sim_config')) or "
                              +  "(((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is "
                              +  "sim_config.config_ptree)) or "
                              +  "((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is sim_config))):\n"
                              +  "        return[";
                // We add all the children of this type of tree after confirming it is a valid path.
                if(reverse_lookup_id_.find("parameter") != reverse_lookup_id_.end()){
                    if(tree_map_[reverse_lookup_id_["parameter"]].find(parent) !=
                        tree_map_[reverse_lookup_id_["parameter"]].end()){
                        for(const auto& items : tree_map_[reverse_lookup_id_["parameter"]][parent]){
                            def_hook_str_ += "'" + items + "',";
                        }
                        def_hook_str_ += "'',";
                    }
                    else{
                        def_hook_str_ += "'',";
                    }
                }
                else{
                    def_hook_str_ += "'',";
                }
            }
            // We do the exact same processing for the other type of tree.
            else if(tree_type_id_[tree_addr_of] == "parameter"){
                def_hook_str_ += std::string("")
                              // var[-1] contains the calling object of the API.
                              // If it is a config_ptree or any other alias of it, we return the children list
                              // of the appropriate tree.
                              +  "    if ((var[-1] == 'sim_config.config_ptree') or "
                              +  "(var[-1] == 'sim_config')) or "
                              +  "(((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is "
                              +  "sim_config.config_ptree)) or "
                              +  "((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is sim_config))):\n"
                              +  "        return[";
                for(const auto& items : children){
                    def_hook_str_ += "'" + items + "',";
                }
                def_hook_str_ += std::string("")
                              +  "'']\n"
                              // var[-1] contains the calling object of the API.
                              // If it is a arch_ptree or any other alias of it, we return the children list
                              // of the appropriate tree.
                              +  "    elif (var[-1] == 'sim_config.arch_ptree') or "
                              +  "((__ipytse.__dict__['user_ns'].has_key(var[-1]) == True) and "
                              +  "(__ipytse.__dict__['user_ns'][var[-1]] is "
                              +  "sim_config.arch_ptree)):\n"
                              +  "        return[";
                if(reverse_lookup_id_.find("architecture") != reverse_lookup_id_.end()){
                    if(tree_map_[reverse_lookup_id_["architecture"]].find(parent) !=
                        tree_map_[reverse_lookup_id_["architecture"]].end()){
                        for(const auto& items : tree_map_[reverse_lookup_id_["architecture"]][parent]){
                            def_hook_str_ += "'" + items + "',";
                        }
                        def_hook_str_ += "'',";
                    }
                    else{
                        def_hook_str_ += "'',";
                    }
                }
                else{
                    def_hook_str_ += "'',";
                }
            }
            def_hook_str_ += "'']\n";
        }

        // This method autogenerates python functions from tree path names.
        void buildRegexDef_(sparta::ParameterTree* tree_addr_of){
            // If this is the first attempt to autogenerate a python def, then this variable
            // re_str_map will not exist. So, we do a check if it exists or not and if it does not, we
            // build one.
            def_hook_str_ = std::string("\n")
                          + "try:\n"
                          + "    __re_str_map\n"
                          + "except NameError:\n"
                          + "    __re_str_map = {}\n";

            // We loop through the entire tree map.
            for(const auto& it : tree_map_[tree_addr_of]){
                std::string regex_key = "";
                // This regex should match double underscores with a negative lookahead underscore.
                // Also, the match should not happen at the end of the string.
                std::regex target("__(?!_)(?!$)");
                // The regex string must contain one of these APIs for parameter tree tab completions.
                std::string prefix_regex_key = std::string("")
                                             + R"((getNode|create|exists|hasValue|isRead|isRequired|)"
                                             + R"(set|unrequire|processParameter)\(\'\s?)";
                // The final regex pattern could start with anything but must contain .[API]('.
                regex_key += R"('.*\.)"
                          +  prefix_regex_key
                          +  std::regex_replace(it.first,
                                                target,
                                                R"(\.\s?)",
                                                std::regex_constants::match_not_bow);
                // Chances that the API argument string is empty happens only at the root of the tree.
                // Thus it is rare compared to other cases.
                if(__builtin_expect(it.first == "", 0)){
                    // In this case, a trailing dot is not needed.
                    regex_key += R"(.$')";
                }
                else{
                    // In all other cases, a trailing dot is needed.
                    regex_key += R"(\..$')";
                }
                // We remove the function hook for this path, if exists.
                removeFunctionHooks_(regex_key);
                // We autogenerate the actual python def for this path.
                buildArgsCompleter_(it.first, it.second, tree_addr_of);
            }
            // We build the parameter node function hooks.
            buildNodeHooks_();
        }

        // This method autogenerates function hooks from tree paths.
        void buildRegexHook_(sparta::ParameterTree* tree_addr_of){
            std::string regex_key = "";
            // The regex string must contain one of these APIs for parameter node tab completions.
            std::string prefix_regex_key = R"((setChild|createChild|addChild|getChild)\(\'\s?)";
            // The final regex pattern could start with anything but must contain .[API]('.
            regex_key += R"('.*\.)"
                      +  prefix_regex_key
                      +  R"(.*')";
            // We autogenerate the python def for this node.
            buildFunctionHooks_("node", regex_key);
            // This regex should match double underscores with a negative lookahead underscore.
            // Also, the match should not happen at the end of the string.
            std::regex target("__(?!_)(?!$)");
            for(const auto& it : tree_map_[tree_addr_of]){
                regex_key = "";
                // The regex string must contain one of these APIs for parameter tree tab completions.
                prefix_regex_key = std::string("")
                                 +  R"((getNode|create|exists|hasValue|isRead|isRequired|)"
                                 +  R"(set|unrequire|processParameter)\(\'\s?)";
                // The final regex pattern could start with anything but must contain .[API]('.
                regex_key += R"('.*\.)"
                          +  prefix_regex_key
                          // We need to replace each matched dunderscore with a dot and 0/1 space.
                          +  std::regex_replace(it.first,
                                                target,
                                                R"(\.\s?)",
                                                std::regex_constants::match_not_bow);
                // Chances that the API argument string is empty happens only at the root of the tree.
                // Thus it is rare compared to other cases.
                if(__builtin_expect(it.first == "", 0)){
                    // In this case, a trailing dot is not needed.
                    regex_key += R"(.$')";
                }
                else{
                    // In all other cases, a trailing dot is needed.
                    regex_key += R"(\..$')";
                }
                // we call the function to actually autogenerate the python def for this path.
                buildFunctionHooks_(it.first, regex_key);
            }
            // The whole Regex-Engine map that is stored in the current ipython instance must
            // be loaded and stored in the next ipython instance, so that users can access
            // tab-completion in the next subsequent session.
            def_hook_str_ += "__persist_rgx = get_ipython().strdispatchers['complete_command'].regexs";
            def_hook_str_ += "\n";
        }

        // This method updates the internal python dict. which holds parent-child strings.
        // We absolutely need to have a parent-child map of the trees in python shell.
        // This is because, when tab-completing node apis, we need to know the current path
        // of the calling node and retrieve the children of that node from some kind of a map.
        // This map must exist in python and we must avoid transferring this result into C++
        // and then lookup and send back the children list to python again.
        void updatePythonMap_(){
            // This is a simple method which builds a python dict from C++ map.
            // We must be careful in writing the python dict properly, with correct syntax.
            def_hook_str_ += "__actual_tree_map = {}\n";
            for(const auto& keys : reverse_lookup_id_){
                def_hook_str_ += "__actual_tree_map['"
                              +  keys.first
                              +  "'] = {";
                for(const auto& it : actual_tree_map_[reverse_lookup_id_[keys.first]]){
                    def_hook_str_ += "'"
                                  +  it.first
                                  +  "':[";
                    for(const auto& values : it.second){
                        def_hook_str_ += "'"
                                      +  values
                                      +  "', ";
                    }
                    def_hook_str_ += "''],";
                }
                def_hook_str_ += "'':['']}\n";
            }
        }

        // This method runs the string directly in the interpreter.
        void invokeRegexCompleter_(){
            PyRun_SimpleString(def_hook_str_.c_str());
        }

        // This method takes a new tree path and tries to add it to the regex-engine.
        void addNewPath_(const std::string& path, sparta::ParameterTree* tree_addr_of){
            std::string non_const_path {path};
            std::vector<std::string> aux_nodes;
            std::vector<std::string> non_const_nodes;
            // The new tree path could contain n number of new nodes.
            while(true){
                // We split the string, starting from the last dot.
                std::size_t pos = non_const_path.find_last_of('.');
                std::string new_node = non_const_path.substr(pos + 1);
                aux_nodes.emplace_back(new_node);
                non_const_path.erase(pos);
                non_const_nodes.emplace_back(non_const_path);
                // If the remaining prefix string is an existing tree path, we stop the parsing.
                // Else, there are multiple new nodes still left in the string.
                if(actual_tree_map_[tree_addr_of].find(non_const_nodes.back()) !=
                    actual_tree_map_[tree_addr_of].end()){
                    break;
                }
            }
            // We repopulate the regex-engine with all the new nodes.
            repopulateTreeUtil_(non_const_nodes, aux_nodes, tree_addr_of);
        }

        // This method figures out how to modify the existing regex-engine
        // to account for the new children some parent might now have.
        void repopulateTreeUtil_(std::vector<std::string>& non_const_nodes,
                                 std::vector<std::string>& aux_nodes,
                                 sparta::ParameterTree* tree_addr_of){

            // Start out with an empty string. This string will gradually expand
            // to contain the entire python autogenerated code.
            def_hook_str_ = "";

            // Users could potentially create multiple new nodes with a single command.
            // This method picks apart the individual new nodes, one by one, and add its
            // concrete path and list of children in the regex map.
            while(!non_const_nodes.empty() && !aux_nodes.empty()){

                // We update the children list of this node.
                actual_tree_map_[tree_addr_of][non_const_nodes.back()].emplace_back(aux_nodes.back());

                // We try to look if this node is an existing parent.
                if(name_map_[tree_addr_of].find(non_const_nodes.back()) == name_map_[tree_addr_of].end()){

                    // If this is a brand new node, we replace the dots in path with underscores.
                    // Because we will autogenerate a python method returning the tab completion for this node.
                    replaceDotsInPath_(non_const_nodes.back(), tree_addr_of);
                }

                // We call the build method for this node which autogenerates the python function.
                buildArgsCompleter_(name_map_[tree_addr_of][non_const_nodes.back()],
                                    actual_tree_map_[tree_addr_of][non_const_nodes.back()],
                                    tree_addr_of);

                // This regex should match double underscores with a negative lookahead underscore.
                // Also, the match should not happen at the end of the string.
                std::regex target("__(?!_)(?!$)");

                // The regex string must contain one of these APIs for parameter tree tab completions.
                std::string prefix_regex_key = std::string("")
                                             + R"((getNode|create|exists|hasValue|isRead|isRequired|)"
                                             + R"(set|unrequire|processParameter)\(\'\s?)";

                // The final regex pattern could start with anything but must contain .[API]('.
                std::string regex_key = R"('.*\.)"
                                      + prefix_regex_key
                                      // We need to replace each matched dunderscore with a dot and 0/1 space.
                                      + std::regex_replace(name_map_[tree_addr_of][non_const_nodes.back()],
                                                           target,
                                                           R"(\.\s?)",
                                                           std::regex_constants::match_not_bow);

                // Chances that the API argument string is empty happens only at the root of the tree.
                // Thus it is rare compared to other cases.
                if(__builtin_expect(name_map_[tree_addr_of][non_const_nodes.back()] == "", 0)){
                    // In this case, a trailing dot is not needed.
                    regex_key += R"(.$')";
                }
                else{
                    // In all other cases, a trailing dot is needed.
                    regex_key += R"(\..$')";
                }
                // If a tab-completer for such a concrete path exists, we must remove it.
                removeFunctionHooks_(regex_key);
                // Build updated tab-completer for this concrete path returning new child.
                buildFunctionHooks_(name_map_[tree_addr_of][non_const_nodes.back()], regex_key);
                // We are done with this node. Pop this node from container and process next.
                non_const_nodes.pop_back();
                aux_nodes.pop_back();
            }
            // The whole Regex-Engine map that is stored in the current ipython instance must
            // be loaded and stored in the next ipython instance, so that users can access
            // tab-completion in the next subsequent session.
            def_hook_str_ += "__persist_rgx = get_ipython().strdispatchers['complete_command'].regexs";
            def_hook_str_ += "\n";
            updatePythonMap_();
            invokeRegexCompleter_();
        }

        //! The final autogenerated python code to be executed in the interpreter.
        std::string def_hook_str_;
        //! Map from tree address to tree string name.
        std::unordered_map<sparta::ParameterTree*, std::string> tree_type_id_;
        //! Map from tree string name to tree address.
        std::unordered_map<std::string, sparta::ParameterTree*> reverse_lookup_id_;
        //! Map with tree address as key and underscore separated parent - child path as value.
        std::unordered_map<sparta::ParameterTree*, std::unordered_map<std::string,
                                                 std::vector<std::string>>> tree_map_;
        //! Map with tree address as key and dot separated parent - child path as value.
        std::unordered_map<sparta::ParameterTree*, std::unordered_map<std::string,
                                                 std::vector<std::string>>> actual_tree_map_;
        //! Map with tree address as key and single node name - concrete path as value.
        std::unordered_map<sparta::ParameterTree*, std::unordered_map<std::string,
                                                 std::string>> name_map_;
    };
} // namespace sparta
