#include "RubyPlugin.h"

#include "RubyConstants.h"

#include "editor/RubyCodeModel.h"
#include "editor/RubyCodeStylePreferencesFactory.h"
#include "editor/RubyEditorFactory.h"
#include "editor/RubyHighlighter.h"
#include "editor/RubyQuickFixAssistProvider.h"
#include "editor/RubyQuickFixes.h"
#include "editor/RubySymbolFilter.h"
#include "editor/RubyCompletionAssist.h"
#include "projectmanager/RubyProject.h"

#ifdef OCAML_WIZARD_SUPPORTED
#include "projectmanager/RubyProjectWizard.h"
#endif

#include <coreplugin/icore.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/taskhub.h>

#include <texteditor/codestylepool.h>
#include <texteditor/simplecodestylepreferences.h>
#include <texteditor/snippets/snippetprovider.h>
#include <texteditor/tabsettings.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>

namespace OCamlCreator {

Plugin *Plugin::m_instance = 0;

Plugin::Plugin()
{
    m_instance = this;
}

Plugin::~Plugin()
{
    TextEditor::TextEditorSettings::unregisterCodeStyle(Constants::OCaml::SettingsId);
    TextEditor::TextEditorSettings::unregisterCodeStylePool(Constants::OCaml::SettingsId);
    TextEditor::TextEditorSettings::unregisterCodeStyleFactory(Constants::OCaml::SettingsId);

    m_instance = 0;
}

Plugin *Plugin::instance()
{
    return m_instance;
}

bool Plugin::initialize(const QStringList &, QString *errorString)
{
    Q_UNUSED(errorString);

    // removed in commit bd8f70374d43ea2c668e95d105cb019f18597572
    //Utils::MimeDatabase::addMimeTypes(QLatin1String(":/rubysupport/Ruby.mimetypes.xml"));
    //Utils::MimeDatabase::addMimeTypes(QLatin1String(":/rubysupport/OCaml.mimetypes.xml"));

    initializeToolsSettings();

    addAutoReleasedObject(new EditorFactory);

    //ProjectExplorer::ProjectManager::registerProjectType<Project>(Constants::ProjectMimeType);

    // TODO: reenable this stuff
//    addAutoReleasedObject(new SymbolFilter([](const QString &file) {
//        return CodeModel::instance()->methodsIn(file);
//    }, "Ruby Methods in Current Document", '.'));
//    addAutoReleasedObject(new SymbolFilter([](const QString &) {
//        return CodeModel::instance()->allMethods();
//    }, "Ruby methods", 'm'));
//    addAutoReleasedObject(new SymbolFilter([](const QString &) {
//        return CodeModel::instance()->allClasses();
//    }, "Ruby classes", 'c'));

    ProjectExplorer::ProjectManager::registerProjectType<Project>(Constants::OCaml::ProjectMimeType);

    // TODO: Implement Wizard for OCaml
//    Core::IWizardFactory::registerFactoryCreator([]() {
//        return QList<Core::IWizardFactory *>() << new ProjectWizard;
//    });

#ifdef OCAML_WIZARD_SUPPORTED
    addAutoReleasedObject(new ProjectWizard);
#endif

    addAutoReleasedObject(new CompletionAssistProvider);

    m_quickFixProvider = new QuickFixAssistProvider(this);
    registerQuickFixes(this);
    TextEditor::SnippetProvider::registerGroup(Constants::OCaml::SnippetGroupId,
                                               tr("OCamlCreator", "SnippetProvider"),
                                               &EditorFactory::decorateEditor);

    return true;
}

void Plugin::extensionsInitialized()
{
    ProjectExplorer::TaskHub::addCategory(Constants::TASK_CATEGORY_MERLIN_COMPILE, tr("Merlin"));
}

QuickFixAssistProvider *Plugin::quickFixProvider()
{
    return m_quickFixProvider;
}

void Plugin::initializeToolsSettings()
{
    // code style factory
    auto factory = new CodeStylePreferencesFactory;
    TextEditor::TextEditorSettings::registerCodeStyleFactory(factory);

    // code style pool
    auto pool = new TextEditor::CodeStylePool(factory, this);
    TextEditor::TextEditorSettings::registerCodeStylePool(Constants::OCaml::SettingsId, pool);

    // global code style settings
    auto globalCodeStyle = new TextEditor::SimpleCodeStylePreferences(this);
    globalCodeStyle->setDelegatingPool(pool);
    globalCodeStyle->setDisplayName(tr("Global", "Settings"));
    globalCodeStyle->setId("OCamlGlobal");
    pool->addCodeStyle(globalCodeStyle);
    TextEditor::TextEditorSettings::registerCodeStyle(Constants::OCaml::SettingsId, globalCodeStyle);

    // built-in settings
    // Ruby style
    auto rubyCodeStyle = new TextEditor::SimpleCodeStylePreferences;
    rubyCodeStyle->setId("ocaml");
    rubyCodeStyle->setDisplayName(tr("OCamlCreator"));
    rubyCodeStyle->setReadOnly(true);
    TextEditor::TabSettings tabSettings;
    tabSettings.m_tabPolicy = TextEditor::TabSettings::SpacesOnlyTabPolicy;
    tabSettings.m_tabSize = 2;
    tabSettings.m_indentSize = 2;
    tabSettings.m_continuationAlignBehavior = TextEditor::TabSettings::ContinuationAlignWithIndent;
    rubyCodeStyle->setTabSettings(tabSettings);
    pool->addCodeStyle(rubyCodeStyle);

    // default delegate for global preferences
    globalCodeStyle->setCurrentDelegate(rubyCodeStyle);

    pool->loadCustomCodeStyles();

    // load global settings (after built-in settings are added to the pool)
    globalCodeStyle->fromSettings(Constants::OCaml::SettingsId, Core::ICore::settings());

    // mimetypes to be handled
    TextEditor::TextEditorSettings::registerMimeTypeForLanguageId(Constants::OCaml::MimeType, Constants::OCaml::SettingsId);
}

}
