#ifndef Ruby_HoverHandler_h
#define Ruby_HoverHandler_h

#include <texteditor/basehoverhandler.h>

namespace OCamlCreator {

class HoverHandler : public TextEditor::BaseHoverHandler
{
private:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos);
};

}

#endif
