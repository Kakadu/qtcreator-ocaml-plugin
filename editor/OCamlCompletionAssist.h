#ifndef OCAMLCOMPLETEIONASSIST_H
#define OCAMLCOMPLETEIONASSIST_H

#include <texteditor/codeassist/completionassistprovider.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/assistproposaliteminterface.h>

namespace OCamlCreator {

class OCamlCompletionAssistProvider : public TextEditor::CompletionAssistProvider
{
public:
    OCamlCompletionAssistProvider();

    TextEditor::IAssistProcessor *createProcessor() const override;

    IAssistProvider::RunType runType() const override;
    int activationCharSequenceLength() const override { return 1; }
    bool isActivationCharSequence(const QString &sequence) const override {
        Q_UNUSED(sequence);
        return false;
    }
};

class CompletionAssistProcessor : public TextEditor::IAssistProcessor
{
public:
    CompletionAssistProcessor();
    TextEditor::IAssistProposal *perform(const TextEditor::AssistInterface *interface) override;
private:
    int findStartOfName(int pos = -1) const;
    // In C++ plugin a subclass of AssitInterface is stored
    QScopedPointer<const TextEditor::AssistInterface> m_interface;
};

class MerlinAssistProposalItem : public TextEditor::AssistProposalItemInterface
{
public:
    MerlinAssistProposalItem(const QString &text, const int prefixStartPos);

    // AssistProposalItemInterface interface
public:
    QString text() const override;
    bool implicitlyApplies() const override;
    bool prematurelyApplies(const QChar &typedCharacter) const override;
    void apply(TextEditor::TextDocumentManipulatorInterface &manipulator, int basePosition) const override;
    QIcon icon() const override;
    QString detail() const override;
    bool isSnippet() const override;
    bool isValid() const override;
    quint64 hash() const override;

private:
    const QString m_text;
    const int     m_prefixPos;
};

}

#endif // OCAMLCOMPLETEIONASSIST_H
