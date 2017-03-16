#include "RubyRubocopHighlighter.h"

#include <texteditor/textdocument.h>
#include <texteditor/semantichighlighter.h>

#include <utils/asconst.h>

#include <QDebug>
#include <QProcess>
#include <QTextDocument>
#include <QtConcurrent>
#include <QMessageBox>
#include <QTextBlock>
#include <QJsonDocument>

namespace Ruby {

QDebug operator<< (QDebug& d, const Range &r) {
    d << r.toString();
    return d;
}

class RubocopFuture : public QFutureInterface<TextEditor::HighlightingResult>, public QObject
{
public:
    RubocopFuture(const Offenses &offenses)
    {
        reportResults(offenses);
    }
};

RubocopHighlighter::RubocopHighlighter()
    : m_rubocopFound(true)
    , m_busy(false)
    , m_rubocop(nullptr)
    , m_startRevision(0)
    , m_document(nullptr)
    , m_chart()
{
    QTextCharFormat format;
    format.setUnderlineColor(Qt::darkYellow);
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    m_extraFormats[0] = format;
    m_extraFormats[1] = format;
    m_extraFormats[1].setUnderlineColor(Qt::darkGreen);
    m_extraFormats[2] = format;
    m_extraFormats[2].setUnderlineColor(Qt::red);

    QVariantMap m;
    m.insert("wrapper", true);
    m.insert("On",      true);
    m.insert("Default", true);
    m.insert("CodeSent", false);
    m.insert("DiagnosticsAsked", false);
    m_chart.setInitialValues(m);

    m_chart.connectToState("CodeSent", [](bool) {
        qDebug() << "MerlinFSM in state CodeSent";
    });
    m_chart.connectToEvent("entry_to_on", [](const QScxmlEvent &event) {
        qDebug() << "MerlinFSM got event " << event.name();
    });

    connect(&m_chart, &MerlinFSM::log, [](const QString&a, const QString&b) {
       qDebug() << "MerlinFSM::log: " << a << " " << b;
    });

    m_chart.init();
    m_chart.start();
    Q_ASSERT(m_chart.isInitialized());
    qDebug() << m_chart.activeStateNames(false);
    initRubocopProcess();
}

RubocopHighlighter::~RubocopHighlighter()
{
    m_rubocop->closeWriteChannel();
    m_rubocop->waitForFinished(3000);
    delete m_rubocop;
}

RubocopHighlighter *RubocopHighlighter::instance()//pattern singleton
{
    static RubocopHighlighter rubocop;
    return &rubocop;
}

// return false if we are busy, true if everything is ok (or rubocop wasn't found)
bool RubocopHighlighter::run(TextEditor::TextDocument *document, const QString &fileNameTip)
{
    if (m_busy || m_rubocop->state() == QProcess::Starting)
        return false;
    if (!m_rubocopFound)
        return true;

    m_busy = true;
    m_startRevision = document->document()->revision();

    m_timer.start();
    m_document = document;

    const QString filePath = document->filePath().isEmpty()
            ? fileNameTip
            : document->filePath().toString();
    /*QJsonArray obj {"tell","start","end","let f x = x let () = ()"};
    QJsonDocument doc(obj);
    QByteArray query(doc.toJson(QJsonDocument::Compact));
    m_rubocop->write(query);
    qDebug () << "query" << obj;*/

    makeMerlinAnalyzeBuffer( document->contents() );
    return true;
}

QString RubocopHighlighter::diagnosticAt(const Utils::FileName &file, int pos)
{
    auto it = m_diagnostics.find(file);
    if (it == m_diagnostics.end())
        return QString();

    return it->messages[Range(pos + 1, 0)];
}

void RubocopHighlighter::makeMerlinAnalyzeBuffer(const QByteArray &file)
{
    QJsonArray obj {"tell","start","end", QString::fromLocal8Bit(file)};
    QJsonDocument doc(obj);
    QByteArray query(doc.toJson(QJsonDocument::Compact));
    m_chart.submitEvent("sendCode");
    m_rubocop->write(query);
    qDebug () << "query" << obj;
}

void RubocopHighlighter::makeMerlinAskDiagnostics()
{
    const QByteArray query = "[\"errors\"]";
    m_chart.submitEvent("askDiagnostics");
    m_rubocop->write(query);
    qDebug () << "query" << query;
}
void RubocopHighlighter::initRubocopProcess()
{
    qDebug() << "initProcess";
   /* if (m_rubocopScript.open()) {
        QFile script(QStringLiteral("/home/andrew/.opam/4.02.3/bin/ocamlmerlin"));

        script.open(QFile::ReadOnly);
        m_rubocopScript.write(script.readAll());
        m_rubocopScript.close();
    }*/

    m_rubocop = new QProcess;
    void (QProcess::*signal)(int) = &QProcess::finished;
    QObject::connect(m_rubocop, signal, [&](int status) {
        if (status) {
            QMessageBox::critical(0, QLatin1String("Rubocop"),
                                  QString::fromUtf8(m_rubocop->readAllStandardError().trimmed()));
            m_rubocopFound = false;
        }
    });


    QObject::connect(m_rubocop, &QProcess::readyReadStandardOutput, [&]() {
        m_outputBuffer.append(QString::fromUtf8(m_rubocop->readAllStandardOutput()));
        qDebug() << qPrintable(m_outputBuffer) << endl;

        if (m_outputBuffer.endsWith(QLatin1String("\n")))
            finishRuboCopHighlight();
    });

    //TODO: detect opam and remove hardcoded path
    // http://stackoverflow.com/questions/19409940/how-to-get-output-system-command-in-qt
    qDebug() << "starting";
    m_rubocop->start(QStringLiteral("/home/kakadu/.opam/4.04.0+fp+flambda/bin/ocamlmerlin"));
}

bool RubocopHighlighter::isReturnTrue(const QJsonArray& arr) {
    return (arr.count() == 2) &&
           (arr.at(0) == "return") &&
           (arr.at(1) == true);
}

bool isReturnFalse(const QJsonArray& arr) {
    return (arr.count() == 2) &&
           (arr.at(0) == "return") &&
           (arr.at(1) == false);
}


void RubocopHighlighter::finishRuboCopHighlight()
{
    if (m_startRevision != m_document->document()->revision()) {
        m_busy = false;
        return;
    }
    Diagnostics diags;

    //qDebug() << "m_outputBuffer.toUtf8() = " << m_outputBuffer.toUtf8();
    QJsonDocument jsonResponse = QJsonDocument::fromJson(m_outputBuffer.toUtf8());
    m_outputBuffer.clear();
    const QJsonArray& arr = jsonResponse.array();

    //qDebug() << "arr = " << arr;
    if (isReturnTrue(arr)) {
        if (m_chart.activeStateNames() == QStringList("CodeSent"))
            makeMerlinAskDiagnostics();
    } else if (isReturnFalse(arr)) {
        m_chart.submitEvent("errorHappend");
    } else if ( (diags = processMerlinErrors(arr.at(1)))) {
        qDebug() << diags.messages;
        Offenses offenses;

        for (const Range& r : diags.messages.keys()) {

            offenses << TextEditor::HighlightingResult(r.startLine, r.startCol, r.length, 2);

        }
        RubocopFuture rubocopFuture(offenses);
        TextEditor::SemanticHighlighter::incrementalApplyExtraAdditionalFormats(m_document->syntaxHighlighter(),
                                                                                rubocopFuture.future(), 0,
                                                                                offenses.count(), m_extraFormats);
        TextEditor::SemanticHighlighter::clearExtraAdditionalFormatsUntilEnd(m_document->syntaxHighlighter(),
                                                                             rubocopFuture.future());
        //result << TextEditor::HighlightingResult(lineN, column, length, kind);
    } else {
        m_chart.submitEvent("errorHappend");
    }


    m_busy = false;

    qDebug() << "rubocop in" << m_timer.elapsed() << "ms,"
//             << offenses.count() << "offenses found."
                ;
}

Diagnostics RubocopHighlighter::processMerlinErrors(const QJsonValue& v) {
    //qDebug() << Q_FUNC_INFO;
    if (!v.isArray()) {
        qDebug() << "not an array";
        return Diagnostics();
    }
    Diagnostics &d = m_diagnostics[m_document->filePath()] = Diagnostics();
    d.setValid();

    QJsonArray arr = v.toArray();
    foreach (const QJsonValue& v, arr) {
        const QJsonObject& start = v.toObject().take("start").toObject();
        const QJsonObject& end = v.toObject().take("end").toObject();
        const int line1 = start.value("line").toInt();
        const int col1 = start.value("col").toInt();
        const int p1 = lineColumnToPos(line1, col1);
        const int p2 = lineColumnToPos(end.value("line").toInt(),
                                       end.value("col").toInt() );
        Range r(start.value("line").toInt(),
                start.value("col").toInt() + 1,
                end.value("line").toInt(),
                end.value("col").toInt() + 1,
                p1, p2-p1);

        const QString& msg = v.toObject().value("message").toString();
        d.messages[r] = msg;
    }

    return d;
}

static int kindOfSeverity(const QStringRef &severity)
{
    switch (severity.at(0).toLatin1()) {
    case 'W': return 0; // yellow
    case 'C': return 1; // green
    default:  return 2; // red
    }
}

Offenses RubocopHighlighter::processRubocopOutput()
{
    Offenses result;
    Diagnostics &diag = m_diagnostics[m_document->filePath()] = Diagnostics();

    const QVector<QStringRef> lines = m_outputBuffer.splitRef('\n');
    for (const QStringRef &line : lines) {
        if (line == "--")
            break;
        QVector<QStringRef> fields = line.split(':');
        if (fields.size() < 5)
            continue;
        int kind = kindOfSeverity(fields[0]);
        int lineN = fields[1].toInt();
        int column = fields[2].toInt();
        int length = fields[3].toInt();
        result << TextEditor::HighlightingResult(uint(lineN), uint(column), uint(length), kind);

        int messagePos = fields[4].position();
        QStringRef message(line.string(), messagePos, line.position() + line.length() - messagePos);
        diag.messages[lineColumnLengthToRange(lineN, column, length)] = message.toString();
    }
    m_outputBuffer.clear();

    return result;
}

int RubocopHighlighter::lineColumnToPos(const int line, const int column) {
    QTextBlock block = m_document->document()->findBlockByLineNumber(line - 1);
    return block.position() + column;
}

Ruby::Range RubocopHighlighter::lineColumnLengthToRange(int line, int column, int length)
{
    int pos = lineColumnToPos(line, column);
    return Ruby::Range(pos, length);
}

}
