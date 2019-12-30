// <Completer> -*- C++ -*-


#include "python/sparta_support/Completer.hpp"
#include "python/sparta_support/TreePathCompleter.hpp"


namespace sparta{

/*!
 * \brief Update the IPython regex engine with the address of the tree
 * and the new concrete path that has been created.
 */
void updateCompleter(const std::string& path, sparta::ParameterTree* tree){
    sparta::Completer<sparta::TreePathCompleter>::getCompleter().getTargetCompleter().
        updateCompleter(path, tree);
}

/*!
 * \brief Build the IPython regex engine from a .yaml file.
 */
void buildCompleter(const sparta::ParameterTree& tree, sparta::ParameterTree* add_tree,
                    const std::string& tree_type, const std::string& pattern)
{
    sparta::Completer<sparta::TreePathCompleter>::getCompleter().getTargetCompleter().
        buildCompleter(tree, add_tree, tree_type, pattern);
}

}
