#include "RubyEditor.h"

#include "../RubyConstants.h"

namespace OCamlCreator {

Editor::Editor()
{
//    addContext(Constants::LangRuby);
    addContext(Constants::OCaml::EditorId);
    addContext(Constants::OCaml::LangOCaml);
    addContext(Constants::M_TOOLS_OCAML);
}

}
