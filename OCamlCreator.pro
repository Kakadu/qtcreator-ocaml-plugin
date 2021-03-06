CONFIG += c++14

isEmpty(QTC_SOURCE):error(QTC_SOURCE must be set)
isEmpty(QTC_BUILD):error(QTC_BUILD must be set)
IDE_BUILD_TREE=$$QTC_BUILD
QTC_PLUGIN_NAME = OCamlCreator
QTC_PLUGIN_DEPENDS = coreplugin texteditor projectexplorer
include($$QTC_SOURCE/src/qtcreatorplugin.pri)

CONFIG += qjson

QT     += scxml
STATECHARTS += MerlinFSM.scxml

SOURCES += RubyPlugin.cpp \
    editor/RubyAmbiguousMethodAssistProvider.cpp \
    editor/RubyAutoCompleter.cpp \
    editor/RubyCodeModel.cpp \
    editor/RubyCodeStylePreferencesFactory.cpp \
    editor/RubyEditor.cpp \
    editor/RubyEditorDocument.cpp \
    editor/RubyEditorFactory.cpp \
    editor/RubyEditorWidget.cpp \
    editor/RubyHighlighter.cpp \
    editor/RubyHoverHandler.cpp \
    editor/RubyIndenter.cpp \
    editor/RubyQuickFixAssistProvider.cpp \
    editor/RubyQuickFixes.cpp \
    editor/RubyRubocopHighlighter.cpp \
    editor/RubyScanner.cpp \
    editor/RubySymbolFilter.cpp \
    editor/OCamlCompletionAssist.cpp \
    projectmanager/RubyProject.cpp \
    projectmanager/RubyProjectNode.cpp \
    #editor/RubyCompletionAssist.cpp \
    #projectmanager/RubyProjectWizard.cpp


equals(TEST, 1) {
    SOURCES += editor/ScannerTest.cpp
}

HEADERS += RubyPlugin.h \
    RubyConstants.h \
    editor/RubyAmbiguousMethodAssistProvider.h \
    editor/RubyAutoCompleter.h \
    editor/RubyCodeModel.h \
    editor/RubyCodeStylePreferencesFactory.h \
    editor/RubyEditor.h \
    editor/RubyEditorDocument.h \
    editor/RubyEditorFactory.h \
    editor/RubyEditorWidget.h \
    editor/RubyHighlighter.h \
    editor/RubyHoverHandler.h \
    editor/RubyIndenter.h \
    editor/RubyQuickFixAssistProvider.h \
    editor/RubyQuickFixes.h \
    editor/RubyRubocopHighlighter.h \
    editor/RubyScanner.h \
    editor/RubySymbol.h \
    editor/RubySymbolFilter.h \
    editor/SourceCodeStream.h \
    editor/OCamlCompletionAssist.h \
    projectmanager/RubyProject.h \
    projectmanager/RubyProjectNode.h \
    #projectmanager/RubyProjectWizard.h
    #editor/RubyCompletionAssist.h \

RESOURCES += \
    Ruby.qrc

OTHER_FILES += \
    README.md Ruby.json.in
