#include "RubyEditorDocument.h"
#include "../RubyConstants.h"
#include "../RubyPlugin.h"

namespace OCamlCreator
{

EditorDocument::EditorDocument()
{
    setId(Constants::OCaml::EditorId);
}

TextEditor::QuickFixAssistProvider *EditorDocument::quickFixAssistProvider() const
{
    return Plugin::instance()->quickFixProvider();
}

}
