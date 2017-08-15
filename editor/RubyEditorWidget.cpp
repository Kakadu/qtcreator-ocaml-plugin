#include "RubyEditorWidget.h"

#include "RubyAmbiguousMethodAssistProvider.h"
#include "RubyAutoCompleter.h"
#include "RubyCodeModel.h"
#include "../RubyConstants.h"
#include "RubyHighlighter.h"
#include "RubyIndenter.h"
#include "RubyRubocopHighlighter.h"

#include <texteditor/textdocument.h>
#include <texteditor/convenience.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>


#include <QtCore/QDebug>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtWidgets/QMenu>

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

//    m_updateCodeModelTimer.setSingleShot(true);
//    m_updateCodeModelTimer.setInterval(CODEMODEL_UPDATE_INTERVAL);
//    connect(&m_updateCodeModelTimer, &QTimer::timeout, this, [this] {
//        if (m_codeModelUpdatePending)
//            updateCodeModel();
//    });

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

void EditorWidget::openLinkUnderCursor()
{
    // There we added virtual declaration to the QtC sources to be able to override
    const bool openInNextSplit = alwaysOpenLinksInNextSplit();
    QString text = textCursor().block().text();
    if (text.isEmpty())
        return ;

    const QTextBlock block = textCursor().block();
    const int line = block.blockNumber() + 1;
    const int column = textCursor().position() - block.position();
    //TODO: pass openInNextSplit
    Q_UNUSED(openInNextSplit);
    RubocopHighlighter::instance()->performGoToDefinition(textDocument(), line, column);
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

void EditorWidget::findUsages()
{
    // TODO: use TextEditor::Concenience::convertPosition
    auto cursor = textCursor();
    QString text = cursor.block().text();
    if (text.isEmpty())
        return;

    int line, col;
    TextEditor::Convenience::convertPosition(this->document(), cursor.position(), &line, &col);
    RubocopHighlighter::instance()->performFindUsages(textDocument(), line, col);
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

void EditorWidget::contextMenuEvent(QContextMenuEvent *e)
{
    QPointer<QMenu> menu(new QMenu(this));
    qDebug() << Q_FUNC_INFO;
    Core::ActionContainer *mcontext = Core::ActionManager::actionContainer(Constants::M_CONTEXT);
    QMenu *contextMenu = mcontext->menu();

//    QMenu *quickFixMenu = new QMenu(tr("&Refactor"), menu);
//    quickFixMenu->addAction(ActionManager::command(Constants::RENAME_SYMBOL_UNDER_CURSOR)->action());

//    if (isSemanticInfoValidExceptLocalUses()) {
//        d->m_useSelectionsUpdater.update(CppUseSelectionsUpdater::Synchronous);
//        AssistInterface *interface = createAssistInterface(QuickFix, ExplicitlyInvoked);
//        if (interface) {
//            QScopedPointer<IAssistProcessor> processor(
//                        CppEditorPlugin::instance()->quickFixProvider()->createProcessor());
//            QScopedPointer<IAssistProposal> proposal(processor->perform(interface));
//            if (!proposal.isNull()) {
//                auto model = static_cast<GenericProposalModel *>(proposal->model());
//                for (int index = 0; index < model->size(); ++index) {
//                    auto item = static_cast<AssistProposalItem *>(model->proposalItem(index));
//                    QuickFixOperation::Ptr op = item->data().value<QuickFixOperation::Ptr>();
//                    QAction *action = quickFixMenu->addAction(op->description());
//                    connect(action, &QAction::triggered, this, [op] { op->perform(); });
//                }
//                delete model;
//            }
//        }
//    }

    foreach (QAction *action, contextMenu->actions()) {
        qDebug() << "adding smth";
        menu->addAction(action);
//        if (action->objectName() == QLatin1String(Constants::M_REFACTORING_MENU_INSERTION_POINT))
//            menu->addMenu(quickFixMenu);
    }

    appendStandardContextMenuActions(menu);

    menu->exec(e->globalPos());
    if (!menu)
        return;
    delete menu;
}
void EditorWidget::finalizeInitialization()
{
    // TODO: we probably do not need code model update in presence of merlin
    connect(document(), &QTextDocument::contentsChanged, this, &EditorWidget::scheduleCodeModelUpdate);
    connect(document(), &QTextDocument::contentsChanged, this, &EditorWidget::scheduleRubocopUpdate);
}

}
