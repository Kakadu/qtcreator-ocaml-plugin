#include "RubyAutoCompleter.h"
#include "RubyCompletionAssist.h"
#include "RubyEditorDocument.h"
#include "RubyEditor.h"
#include "RubyEditorFactory.h"
#include "RubyHighlighter.h"
#include "RubyHoverHandler.h"
#include "RubyIndenter.h"

#include "../RubyConstants.h"
#include "RubyEditorWidget.h"

#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorsettings.h>

#include <QCoreApplication>

namespace OCamlCreator {

EditorFactory::EditorFactory()
{
    setId(Constants::OCaml::EditorId);
    setDisplayName(qApp->translate("OpenWith::Editors", Constants::OCaml::EditorDisplayName));
//    addMimeType(Constants::MimeType);
    addMimeType(Constants::OCaml::MimeType);
    addMimeType(Constants::OCaml::ProjectMimeType);

    setDocumentCreator([]() { return new EditorDocument; });
    setIndenterCreator([]() { return new Indenter; });
    setEditorWidgetCreator([]() { return new EditorWidget; });
    setEditorCreator([]() { return new Editor; });
    setAutoCompleterCreator([]() { return new AutoCompleter; });
    setCompletionAssistProvider(new CompletionAssistProvider);
    //setSyntaxHighlighterCreator([]() { return new Highlighter; });
    setUseGenericHighlighter(true);
    setCommentDefinition(Utils::CommentDefinition::HashStyle);
    setParenthesesMatchingEnabled(true);
    setCodeFoldingSupported(true);
    setMarksVisible(true);
    addHoverHandler(new HoverHandler);

    setEditorActionHandlers(TextEditor::TextEditorActionHandler::Format
                          | TextEditor::TextEditorActionHandler::UnCommentSelection
                          | TextEditor::TextEditorActionHandler::UnCollapseAll
                          | TextEditor::TextEditorActionHandler::FollowSymbolUnderCursor);
}

void EditorFactory::decorateEditor(TextEditor::TextEditorWidget *editor)
{
    if (TextEditor::TextDocument *document = editor->textDocument()) {
        document->setSyntaxHighlighter(new Highlighter);
        document->setIndenter(new Indenter);
    }
}

}
