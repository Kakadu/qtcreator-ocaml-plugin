#ifndef Ruby_EditorDocument_h
#define Ruby_EditorDocument_h

#include <texteditor/textdocument.h>

namespace OCamlCreator {

class EditorDocument : public TextEditor::TextDocument
{
public:
    explicit EditorDocument();

    TextEditor::QuickFixAssistProvider *quickFixAssistProvider() const override;
};

}

#endif
