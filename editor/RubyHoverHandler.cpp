#include "RubyHoverHandler.h"

#include "RubyRubocopHighlighter.h"

#include <QDebug>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

namespace OCamlCreator {

void HoverHandler::identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos)
{
    RubocopHighlighter *rubocop = RubocopHighlighter::instance();

    QString diagnostic = rubocop->diagnosticAt(editorWidget->textDocument()->filePath(), pos);
    setToolTip(diagnostic);
}

}
