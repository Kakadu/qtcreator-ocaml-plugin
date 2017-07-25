#include "RubyEditorWidget.h"

#include "RubyAmbiguousMethodAssistProvider.h"
#include "RubyAutoCompleter.h"
#include "RubyCodeModel.h"
#include "../RubyConstants.h"
#include "RubyHighlighter.h"
#include "RubyIndenter.h"
#include "RubyRubocopHighlighter.h"

#include <texteditor/textdocument.h>

#include <QtCore/QDebug>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>

namespace OCamlCreator {

const int CODEMODEL_UPDATE_INTERVAL = 150;
const int RUBOCOP_UPDATE_INTERVAL = 300;

EditorWidget::EditorWidget()
    : m_wordRegex("[\\w!\\?]+")
    , m_codeModelUpdatePending(false)
    , m_rubocopUpdatePending(false)
    , m_ambigousMethodAssistProvider(new AmbigousMethodAssistProvider)
{
    setLanguageSettingsId(Constants::OCaml::SettingsId);

    m_commentDefinition.multiLineStart.clear();
    m_commentDefinition.multiLineEnd.clear();
    m_commentDefinition.singleLine = '#';

    m_updateCodeModelTimer.setSingleShot(true);
    m_updateCodeModelTimer.setInterval(CODEMODEL_UPDATE_INTERVAL);
    connect(&m_updateCodeModelTimer, &QTimer::timeout, this, [this] {
        if (m_codeModelUpdatePending)
            updateCodeModel();
    });

    m_updateRubocopTimer.setSingleShot(true);
    m_updateRubocopTimer.setInterval(RUBOCOP_UPDATE_INTERVAL);
    connect(&m_updateRubocopTimer, &QTimer::timeout, this, [this] {
        if (m_rubocopUpdatePending)
            updateRubocop();
    });

    CodeModel::instance();
}

EditorWidget::~EditorWidget()
{
    delete m_ambigousMethodAssistProvider;
}

TextEditor::TextEditorWidget::Link EditorWidget::findLinkAt(const QTextCursor &cursor, bool, bool inNextSplit)
{
    Q_UNUSED(inNextSplit);
    /* Hack here. QtCreator API doesn't support asynchornous finding links (yet).
     * See TextEditorWidget::openLinkUnderCursor implementation
     * So, we should return empty link here but schedule request to merlin.
     * The problem will be that all calls to findLinkAt will cause triggering of
     * go-to-definition request.
     */
    QString text = cursor.block().text();
    if (text.isEmpty())
        return Link();

    const QTextBlock block = cursor.block();
    const int line = block.blockNumber() + 1;
    const int column = cursor.position() - block.position();
    RubocopHighlighter::instance()->performGoToDefinition(textDocument(), line, column);
    return Link();
}

void EditorWidget::unCommentSelection()
{
    Utils::unCommentSelection(this, m_commentDefinition);
}

void EditorWidget::aboutToOpen(const QString &fileName, const QString &realFileName)
{
    Q_UNUSED(fileName);
    m_filePathDueToMaybeABug = realFileName;
}

void EditorWidget::scheduleCodeModelUpdate()
{
    m_codeModelUpdatePending = m_updateCodeModelTimer.isActive();
    if (m_codeModelUpdatePending)
        return;

    m_codeModelUpdatePending = false;
    updateCodeModel();
    m_updateCodeModelTimer.start();
}

void EditorWidget::updateCodeModel()
{
    const QString textData = textDocument()->plainText();
    CodeModel::instance()->updateFile(textDocument()->filePath().toString(), textData);
}

void EditorWidget::scheduleRubocopUpdate()
{
    m_rubocopUpdatePending = m_updateRubocopTimer.isActive();
    if (m_rubocopUpdatePending)
        return;

    m_rubocopUpdatePending = false;
    updateRubocop();
    m_updateRubocopTimer.start();
}

void EditorWidget::updateRubocop()
{
    if (!RubocopHighlighter::instance()->run(textDocument(), m_filePathDueToMaybeABug)) {
        m_rubocopUpdatePending = true;
        m_updateRubocopTimer.start();
    }
}

void EditorWidget::finalizeInitialization()
{
    connect(document(), &QTextDocument::contentsChanged, this, &EditorWidget::scheduleCodeModelUpdate);
    connect(document(), &QTextDocument::contentsChanged, this, &EditorWidget::scheduleRubocopUpdate);
}

}
