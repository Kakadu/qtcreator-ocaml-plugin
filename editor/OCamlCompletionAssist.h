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
    int activationCharSequenceLength() const override;
    bool isActivationCharSequence(const QString &sequence) const override;
};

class CompletionAssistProcessor : public TextEditor::IAssistProcessor
{
public:
    CompletionAssistProcessor();
    TextEditor::IAssistProposal *perform(const TextEditor::AssistInterface *interface) override;
private:
    int findStartOfName(int pos = -1) const;
    void findPrefixes(int pos, int &posMerlin, int &posQtC) const;
    // In C++ plugin a subclass of AssitInterface is stored
    QScopedPointer<const TextEditor::AssistInterface> m_interface;
};

}

#endif // OCAMLCOMPLETEIONASSIST_H
