#include "OCamlCompletionAssist.h"

#include <utils/qtcassert.h>

#include <texteditor/convenience.h>
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

    if (interface->reason() == TextEditor::IdleEditor)
        return nullptr;

    setPerformWasApplicable(true);

    int curPosition = interface->position();

    int askedLine, askedCol, prefixLine, prefixCol;
    TextEditor::Convenience::convertPosition(interface->textDocument(), curPosition, &askedLine, &askedCol);
    //const int startPosition = findStartOfName();
    int startPosition, qtcPos;
    findPrefixes(-1, startPosition, qtcPos);
    TextEditor::Convenience::convertPosition(interface->textDocument(), startPosition, &prefixLine, &prefixCol);
//    qDebug() << QString("prefix(Line,Col) = (%1, %2)").arg(prefixLine).arg(prefixCol);
    // FIXME: We should check the block status in case of multi-line tokens
    const QString prefix =
            interface->textAt(startPosition, curPosition-startPosition);
//    const QString qtcPrefix =
//            interface->textAt(qtcPos, curPosition-qtcPos);
//    qDebug() << QString("QtC prefix = `%1`").arg(qtcPrefix);

    RubocopHighlighter::instance()->performCompletion(
                interface->textDocument(),
                prefix,
                qtcPos,
                askedLine, askedCol,
                [this](TextEditor::IAssistProposal *proposal) {
                    QTC_CHECK(proposal);
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

void CompletionAssistProcessor::findPrefixes(int pos, int &posMerlin, int &posQtC) const
{
    // getting a merlin substring of a format : aaaaa.bbbb.prefixQtC
    if (pos == -1)
        pos = m_interface->position();
    QChar chr;
    bool stopQtC = false;

    // Skip to the start of a name
    do {
        if (!stopQtC) {
            if (chr == '.') {
                stopQtC = true;
                posQtC = pos+1;
            } else
                posQtC--;
        }
        chr = m_interface->characterAt(--pos);
    } while (isValidIdentifierChar(chr) || (chr == '.') );

    posMerlin = pos+1;
}

int CompletionAssistProcessor::findStartOfName(int pos) const
{
    if (pos == -1)
        pos = m_interface->position();
    QChar chr;

    // Skip to the start of a name
    do {
        chr = m_interface->characterAt(--pos);
    } while (isValidIdentifierChar(chr) || (chr == '.') );

    return pos + 1;
}

int OCamlCompletionAssistProvider::activationCharSequenceLength() const
{
    return 2;
}

bool OCamlCompletionAssistProvider::isActivationCharSequence(const QString &sequence) const
{
    const QChar &ch  = sequence.at(1);
    const QChar &ch2 = sequence.at(0);

    if (ch == '.' || ch2 == '.')
        return true;
    if (isValidAsciiIdentifierChar(ch) && isValidAsciiIdentifierChar(ch2)) {
        return true;
    }
    return false;
}

}

