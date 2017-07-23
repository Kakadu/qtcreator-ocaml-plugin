#ifndef RubyEditorFactory_h
#define RubyEditorFactory_h

#include <texteditor/texteditor.h>

namespace OCamlCreator {

class EditorFactory : public TextEditor::TextEditorFactory
{
    Q_OBJECT

public:
    EditorFactory();
    static void decorateEditor(TextEditor::TextEditorWidget *editor);
};

}

#endif
