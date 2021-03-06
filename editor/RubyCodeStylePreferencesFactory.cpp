#include "RubyCodeStylePreferencesFactory.h"
#include "../RubyConstants.h"
#include "RubyIndenter.h"

#include <coreplugin/id.h>
#include <texteditor/simplecodestylepreferences.h>
#include <texteditor/snippets/snippetprovider.h>

#include <extensionsystem/pluginmanager.h>

#include <QLabel>

namespace OCamlCreator {

Core::Id CodeStylePreferencesFactory::languageId()
{
    return Constants::OCaml::SettingsId;
}

QString CodeStylePreferencesFactory::displayName()
{
    return QStringLiteral("Ruby");
}

TextEditor::ICodeStylePreferences *CodeStylePreferencesFactory::createCodeStyle() const
{
    return new TextEditor::SimpleCodeStylePreferences();
}

QWidget *CodeStylePreferencesFactory::createEditor(TextEditor::ICodeStylePreferences*, QWidget *parent) const
{
    return new QLabel(tr("There's no configuration widget yet, sorry."), parent);
}

TextEditor::Indenter *CodeStylePreferencesFactory::createIndenter() const
{
    return new Indenter;
}

TextEditor::SnippetProvider *CodeStylePreferencesFactory::snippetProvider() const
{
    return ExtensionSystem::PluginManager::getObject<TextEditor::SnippetProvider>(
        [](TextEditor::SnippetProvider *provider) {
            return provider->groupId() == Constants::OCaml::SnippetGroupId;
    });
}

QString CodeStylePreferencesFactory::previewText() const
{
    return QStringLiteral(
            "module Foo\n"
            "  class Bar\n"
            "    attr_reader :something\n"
            "\n"
            "    def foo(arg1, arg2)\n"
            "      arg1.each do |i|\n"
            "        bar(i + 2) if arg2 =~ /^fo+/\n"
            "      end\n"
            "    end\n"
            "  end\n"
            "end\n");
}

}
