#ifndef Ruby_QuickFixAssistProvider_h
#define Ruby_QuickFixAssistProvider_h

#include <texteditor/codeassist/quickfixassistprovider.h>

namespace OCamlCreator {

class QuickFixAssistProvider : public TextEditor::QuickFixAssistProvider
{
public:
    QuickFixAssistProvider(QObject *parent);
    IAssistProvider::RunType runType() const override;
    TextEditor::IAssistProcessor *createProcessor() const override;

    QList<TextEditor::QuickFixFactory *> quickFixFactories() const override;
};

}

#endif
