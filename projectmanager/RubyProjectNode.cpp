#include "RubyProjectNode.h"

using namespace ProjectExplorer;

namespace OCamlCreator {

bool ProjectNode::supportsAction(ProjectAction action, Node *) const
{
    switch (action) {
    case AddNewFile:
    case EraseFile:
    case Rename:
    case DuplicateFile:
        return true;
    default:
        return false;
    }
}

}
