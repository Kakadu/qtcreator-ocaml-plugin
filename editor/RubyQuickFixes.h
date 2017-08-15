#ifndef Ruby_QuickFixes_h
#define Ruby_QuickFixes_h

#include <QTextBlock>

#include <extensionsystem/iplugin.h>
#include <texteditor/quickfix.h>

#include "RubyScanner.h"
#include "RubyRubocopHighlighter.h"

namespace OCamlCreator {

void registerQuickFixes(ExtensionSystem::IPlugin *plugIn);

class QuickFixFactory : public TextEditor::QuickFixFactory {
    Q_OBJECT
};

class SwitchStringQuotes : public QuickFixFactory {
public:
    void matchingOperations(const TextEditor::QuickFixInterface &interface, TextEditor::QuickFixOperations &result) override;
};

class SwitchStringQuotesOp : public TextEditor::QuickFixOperation {
public:
    SwitchStringQuotesOp(QTextBlock &block, const Token &token, int userCursorPosition);
    void perform() override;
private:
    QTextBlock m_block;
    Token m_token;
    int m_userCursorPosition;
};

class RenameHintFixFactory : public QuickFixFactory
{
    Q_OBJECT
public:
    void matchingOperations(const TextEditor::QuickFixInterface &, TextEditor::QuickFixOperations &) override;
};

class RenameHintFixOp : public TextEditor::QuickFixOperation {
public:
    RenameHintFixOp(QTextBlock &block, const MerlinQuickFix&, const QString &new_val);
    void perform() override;
    QString description() const Q_DECL_OVERRIDE {
        return m_newVal;
    }

private:
    QTextBlock m_block;
    MerlinQuickFix m_qf;
    QString m_newVal;
};

}

#endif
