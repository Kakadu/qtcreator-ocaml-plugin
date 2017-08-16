#include "RubyQuickFixes.h"

#include <QTextDocument>
#include <texteditor/codeassist/assistinterface.h>
#include <algorithm>

#include "RubyScanner.h"
#include "RubyRubocopHighlighter.h"

namespace OCamlCreator {
void registerQuickFixes(ExtensionSystem::IPlugin *plugIn)
{
    plugIn->addAutoReleasedObject(new SwitchStringQuotes);
    plugIn->addAutoReleasedObject(new RenameHintFixFactory);
}

void SwitchStringQuotes::matchingOperations(const TextEditor::QuickFixInterface &interface, TextEditor::QuickFixOperations &result)
{
    QTextBlock block = interface->textDocument()->findBlock(interface->position());
    QString line = block.text();
    int userCursorPosition = interface->position();
    int position = userCursorPosition - block.position();
    Token token = Scanner::tokenAt(&line, position);

    if (token.kind != OCamlCreator::Token::String)
        return;

    SwitchStringQuotesOp* operation = new SwitchStringQuotesOp(block, token, userCursorPosition);
    QString description;
    if (line[token.position] == '"')
        description = "Convert to single quotes";
    else
        description = "Convert to double quotes";
    operation->setDescription(description);

    result << operation;
}

SwitchStringQuotesOp::SwitchStringQuotesOp(QTextBlock &block, const Token &token, int userCursorPosition)
    : m_block(block), m_token(token), m_userCursorPosition(userCursorPosition)
{
}

void SwitchStringQuotesOp::perform()
{
    qDebug() << Q_FUNC_INFO;
    QString string = m_block.text().mid(m_token.position, m_token.length);

    QString oldQuote = "\"";
    QString newQuote = "'";
    if (string[0] != '"')
        std::swap(oldQuote, newQuote);

    string.replace("\\" + oldQuote, oldQuote);
    string.replace(newQuote, "\\" + newQuote);
    string[0] = newQuote[0];
    string[string.length() - 1] = newQuote[0];

    QTextCursor cursor(m_block);
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, m_token.position);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, m_token.length);
    cursor.removeSelectedText();
    cursor.insertText(string);
    cursor.endEditBlock();
}

void RenameHintFixFactory::matchingOperations(const TextEditor::QuickFixInterface &iface,
                                              TextEditor::QuickFixOperations & rez)
{
    auto hook = [&rez, &iface](const MerlinQuickFix& qf) {
        //qDebug() << "Got a QF with " << qf.new_values.size() << "elems";
        QTextBlock block = iface->textDocument()->findBlock(iface->position());
        foreach (auto s, qf.new_values)
            rez << QSharedPointer<RenameHintFixOp>::create(block, qf, s);
    };

    RubocopHighlighter::instance()->enumerateQuickFixes(iface, hook);
}

RenameHintFixOp::RenameHintFixOp(QTextBlock &block, const MerlinQuickFix &qf, const QString &s)
    : m_block(block), m_qf(qf), m_newVal(s)
{

}

void RenameHintFixOp::perform()
{
    const int bp = m_block.position();
    QTextCursor cursor(m_block);
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, m_qf.startPos - bp);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, m_qf.col2 - m_qf.col1 );
    cursor.removeSelectedText();
    cursor.insertText(m_newVal);
    cursor.endEditBlock();
}

}
