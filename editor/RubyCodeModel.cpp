#include "RubyCodeModel.h"
#include "RubySimpleScanner.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace Ruby {

Symbol::Symbol(const QString& name, int line, int column)
    : name(name), line(line), column(column)
{
}

CodeModel::CodeModel()
{
}

CodeModel::~CodeModel()
{
}

CodeModel* CodeModel::instance()
{
    static CodeModel model;
    return &model;
}

void CodeModel::updateModels(const QStringList& files)
{
    for (const QString& file : files)
        updateModel(file);
}

void CodeModel::updateModel(const QString& file)
{
    QElapsedTimer timer;
    timer.start();

    QFileInfo info(file);
    SymbolGroup& group = m_symbols[file];

    if (!group.lastUpdate.isNull() && group.lastUpdate > info.lastModified())
        return;

    QFile f(file);
    if (!f.open(QFile::ReadOnly))
        return;

    SimpleScanner scanner(&f);

    Symbol symbol;
    group.symbols.clear();
    while (!(symbol = scanner.nextSymbol()).name.isNull()) {
        symbol.file = file;
        group.symbols << symbol;
    }

    group.lastUpdate = QDateTime::currentDateTime();
    qDebug() << "Code model updated in" << timer.elapsed() << "ms " << file;
}

QList<Symbol> CodeModel::methodsIn(const QString& file) const
{
    return m_symbols[file].symbols;
}

QList<Symbol> CodeModel::allMethods() const
{
    // TODO: cache this!?
    QList<Symbol> result;
    for (const SymbolGroup& group : m_symbols)
        result << group.symbols;
    return result;
}

QList<Symbol> CodeModel::allMethodsNamed(const QString& name) const
{
    QList<Symbol> result;
    // FIXME: Replace this linear brute force approach
    for (const SymbolGroup& group : m_symbols) {
        for (const Symbol& symbol : group.symbols) {
            if (symbol.name == name)
                result << symbol;
        }
    }
    return result;
}

}

#include "RubyCodeModel.moc"