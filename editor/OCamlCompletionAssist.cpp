#include "OCamlCompletionAssist.h"

#include <utils/qtcassert.h>

#include <texteditor/codeassist/completionassistprovider.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/iassistproposal.h>
#include <texteditor/codeassist/iassistproposalmodel.h>
#include <texteditor/codeassist/assistinterface.h>
#include <texteditor/codeassist/assistenums.h>

#include <QtGui/QTextDocument>
#include <QtGui/QTextBlock>
#include <QtGui/QIcon>

#include "RubyRubocopHighlighter.h"

namespace OCamlCreator {

OCamlCompletionAssistProvider::OCamlCompletionAssistProvider()
{

}

TextEditor::IAssistProvider::RunType OCamlCompletionAssistProvider::runType() const
{
    return TextEditor::IAssistProvider::Asynchronous;
}

TextEditor::IAssistProcessor *OCamlCompletionAssistProvider::createProcessor() const {
    return new CompletionAssistProcessor;
}

/* ************************************************************************** */

/* ************************************************************************** */
CompletionAssistProcessor::CompletionAssistProcessor()
{}

TextEditor::IAssistProposal *CompletionAssistProcessor::perform(const TextEditor::AssistInterface *interface)
{
    // There we trigger request to merlin
    m_interface.reset(interface);

    if (interface->reason() != TextEditor::ExplicitlyInvoked)
        return nullptr;

    setPerformWasApplicable(true);

    int startPosition = interface->position();
    const int startOfPrefix = findStartOfName();

    // FIXME: We should check the block status in case of multi-line tokens
    QTextBlock block = interface->textDocument()->findBlock(startPosition);
    int linePosition = startPosition - block.position();
    const QString line =
            interface->textAt(startOfPrefix, startPosition-startOfPrefix);
    qDebug() << "asked prefix is " << line
             << QString("with startPos = %1, linePos = %2").arg(startPosition).arg(linePosition);

    RubocopHighlighter::instance()->performCompletion(
                interface->textDocument(),
                line, startPosition,
                block.firstLineNumber(), linePosition,
                [this](TextEditor::IAssistProposal *proposal) {
                    qDebug() << "Setting proposal of length " << proposal->model()->size();
                    this->setAsyncProposalAvailable(proposal);
                } );

    return nullptr;
}

bool isValidAsciiIdentifierChar(const QChar &ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

bool isValidFirstIdentifierChar(const QChar &ch)
{
    return ch.isLetter() || ch == QLatin1Char('_') || ch.isHighSurrogate() || ch.isLowSurrogate();
}

bool isValidIdentifierChar(const QChar &ch)
{
    return isValidFirstIdentifierChar(ch) || ch.isNumber();
}

int CompletionAssistProcessor::findStartOfName(int pos) const
{
    if (pos == -1)
        pos = m_interface->position();
    QChar chr;

    // Skip to the start of a name
    do {
        chr = m_interface->characterAt(--pos);
    } while (isValidIdentifierChar(chr));

    return pos + 1;
}

MerlinAssistProposalItem::MerlinAssistProposalItem(const QString &text, const int prefixStartPos)
  : m_text(text), m_prefixPos(prefixStartPos)
{

}

QString MerlinAssistProposalItem::text() const
{
    return m_text;
}

bool MerlinAssistProposalItem::implicitlyApplies() const
{
    return false;
}

bool MerlinAssistProposalItem::prematurelyApplies(const QChar &typedCharacter) const
{
    Q_UNUSED(typedCharacter);
    return false;
}

void MerlinAssistProposalItem::apply(TextEditor::TextDocumentManipulatorInterface &manip, int basePosition) const
{
    qDebug() << Q_FUNC_INFO;
    QTC_CHECK(basePosition >= m_prefixPos);
    Q_UNUSED(basePosition);

    qDebug() << "text starting from passed pos-2 = " << manip.textAt(basePosition-2, 2);
    qDebug() << "text starting from passed pos =   " << manip.textAt(basePosition, 2);
    qDebug() << "prefix start pos              =   " << m_prefixPos;
    manip.replace(m_prefixPos, basePosition-m_prefixPos, m_text);
}

QIcon MerlinAssistProposalItem::icon() const
{
    return QIcon();
}

QString MerlinAssistProposalItem::detail() const
{
    return "";
}

bool MerlinAssistProposalItem::isSnippet() const
{
    return false;
}

bool MerlinAssistProposalItem::isValid() const
{
    return true;
}

quint64 MerlinAssistProposalItem::hash() const
{
    return qHash(m_text);
}

}

