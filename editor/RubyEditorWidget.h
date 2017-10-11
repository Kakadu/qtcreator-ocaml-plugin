#ifndef RubyEditorWidget_h
#define RubyEditorWidget_h

#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>

#include <texteditor/texteditor.h>
#include <utils/uncommentselection.h>

namespace Utils {class FileName; }

namespace OCamlCreator {

class AmbigousMethodAssistProvider;

class EditorWidget : public TextEditor::TextEditorWidget
{
    Q_OBJECT

public:
    EditorWidget();
    ~EditorWidget();

    void unCommentSelection() override;

    void aboutToOpen(const QString &fileName, const QString &realFileName) override;
    void findUsages();

    virtual void openLinkUnderCursor() Q_DECL_OVERRIDE;
protected:
    void finalizeInitialization() override;
    void contextMenuEvent(QContextMenuEvent *) override;

private slots:
    void scheduleCodeModelUpdate();

    void scheduleRubocopUpdate();
    void updateCodeModel();
    void updateRubocop();

    void onCodeWarningsUpdated(const Utils::FileName& name,
                               unsigned revision,
                               const TextEditor::RefactorMarkers &refactorMarkers);

private:
    QRegularExpression m_wordRegex;
    Utils::CommentDefinition m_commentDefinition;
    QTimer m_updateCodeModelTimer;
    bool m_codeModelUpdatePending;

    QTimer m_updateRubocopTimer;
    bool m_rubocopUpdatePending;

    QString m_filePathDueToMaybeABug;

    AmbigousMethodAssistProvider *m_ambigousMethodAssistProvider;
};

}

#endif
