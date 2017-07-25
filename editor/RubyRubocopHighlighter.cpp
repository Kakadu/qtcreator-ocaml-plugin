#include "RubyRubocopHighlighter.h"

#include <texteditor/textdocument.h>
#include <texteditor/semantichighlighter.h>
#include <coreplugin/messagemanager.h>
#include <projectexplorer/taskhub.h>

#include <utils/asconst.h>
#include "RubyConstants.h"

#include <QDebug>
#include <QProcess>
#include <QTextDocument>
#include <QtConcurrent>
#include <QMessageBox>
#include <QTextBlock>
#include <QJsonDocument>

namespace OCamlCreator {

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
    m.insert("On",      true);
    m.insert("Default", true);
    m.insert("CodeSent", false);
    m_chart.setInitialValues(m);

    static QVector<QString> connectedStates;
    connectedStates << "Default" << "CodeSent"
//                    << "DiagnosticsAsked" << "GoToDefault"
                       ;
#if 0
    foreach (const QString& s, connectedStates) {
        m_chart.connectToState(s, [s](bool b) {
            qDebug() << qPrintable(QString("MerlinFSM in state %1 (%2)")
                                   .arg(s).arg(b));
        });
    }

    m_chart.connectToEvent("entry_to_on", [](const QScxmlEvent &event) {
        qDebug() << "MerlinFSM got event " << event.name();
    });

    connect(&m_chart, &MerlinFSM::log, [](const QString&a, const QString&b) {
       qDebug() << "MerlinFSM::log: " << a << " " << b;
    });
#endif
    m_chart.init();
    m_chart.start();
    Q_ASSERT(m_chart.isInitialized());
    qDebug() << m_chart.activeStateNames(false);
}

RubocopHighlighter::~RubocopHighlighter()
{
    m_rubocop->closeWriteChannel();
    m_rubocop->waitForFinished(3000);
    delete m_rubocop;
}

RubocopHighlighter *RubocopHighlighter::instance()
{
    static RubocopHighlighter rubocop;
    return &rubocop;
}

// return false if we are busy, true if everything is ok (or rubocop wasn't found)
bool RubocopHighlighter::run(TextEditor::TextDocument *document, const QString &fileNameTip)
{
//    if (m_busy || m_rubocop->state() == QProcess::Starting)
//        return false;
//    if (!m_rubocopFound)
//        return true;

//    m_busy = true;
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
//    QJsonArray obj {"tell","start","end", QString::fromLocal8Bit(file)};
//    QJsonDocument doc(obj);
//    QByteArray query(doc.toJson(QJsonDocument::Compact));
//    m_rubocop->write(query);
    //    sendFSMevent("sendCode");

    sendFSMevent("sendCode");
    QStringList args {"errors", "-filename", "test.ml"};
    initRubocopProcess(args);
    m_rubocop->write(file.data(), file.length());
    m_rubocop->closeWriteChannel();
//    qDebug () << args;
}

inline void RubocopHighlighter::sendFSMevent(const QString &s)
{
    qDebug() << QString("Sending event '%1' when current states are ").arg(s)
             << m_chart.activeStateNames(true);
    m_chart.submitEvent(s);
}

void RubocopHighlighter::makeMerlinAskDiagnostics()
{
    const QByteArray query = "[\"errors\"]";
//    sendFSMevent("askDiagnostics");
    m_rubocop->write(query);
    qDebug () << "query" << query;
}

void RubocopHighlighter::initRubocopProcess(const QStringList& args)
{
    qDebug() << "init process for a single message";

    m_rubocop = new QProcess;
    void (QProcess::*signal)(int) = &QProcess::finished;
    QObject::connect(m_rubocop, signal, [&](int status) {
        if (status) {
            QMessageBox::critical(0, QLatin1String("Rubocop"),
                                  QString::fromUtf8(m_rubocop->readAllStandardError().trimmed()));
            m_rubocopFound = false;
        }
    });
    QObject::connect(m_rubocop, &QProcess::started, [&]() {
        qDebug() << "started";
    });
    QObject::connect(m_rubocop, &QProcess::errorOccurred, [&](QProcess::ProcessError e) {
        qDebug() << "error "   << e;
    });

    QObject::connect(m_rubocop, &QProcess::readyReadStandardOutput, [&]() {
//        qDebug() << "on readyReadStandardOutput";
        m_outputBuffer.append(QString::fromUtf8(m_rubocop->readAllStandardOutput()));
//        qDebug() << qPrintable(m_outputBuffer) << endl;

        if (m_outputBuffer.endsWith(QLatin1String("\n")))
            finishRuboCopHighlight();
    });

    //TODO: detect opam and remove hardcoded path
    // http://stackoverflow.com/questions/19409940/how-to-get-output-system-command-in-qt
    auto new_args = args;
    new_args.push_front("single");
//    qDebug() << "starting";
    static const QString opamPath = "/home/kakadu/.opam/4.04.0+fp+flambda/bin/";
    m_rubocop->start(opamPath + "ocamlmerlin", new_args);
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
///
/// \brief RubocopHighlighter::parseDiagnosticsJson
/// \param resp is an array of diagnostics represented in JSON
///
void RubocopHighlighter::parseDiagnosticsJson(const QJsonValue& resp) {
    Diagnostics diags;
    Offenses offenses;

    if (!resp.isArray()) {
        qWarning() << "not an array " << resp;
        goto BAD_JSON;
    }

    diags = m_diagnostics[m_document->filePath()] = Diagnostics();
    diags.setValid();

    foreach (const QJsonValue& v, resp.toArray()) {
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
        diags.messages[r] = msg;

        // TODO: response contains filed valid: bool
        // but it seems not important
    }
    qDebug() << "got " << diags.messages.count() << "diagnostics";
    // now we convert parsed diagnostics to editor's highlighting regions
    // and add tasks to the TaskHub (Issues bottom pane)
    ProjectExplorer::TaskHub::clearTasks(Constants::TASK_CATEGORY_MERLIN_COMPILE);
    for (const Range& r : diags.messages.keys()) {
        offenses << TextEditor::HighlightingResult(r.startLine, r.startCol, r.length, 2);
        using namespace ProjectExplorer;
        TaskHub::instance()->addTask(Task::Error, diags.messages.value(r, "?"),
                                     Constants::TASK_CATEGORY_MERLIN_COMPILE);
    }
    {
    RubocopFuture rubocopFuture(offenses);
    TextEditor::SemanticHighlighter::clearExtraAdditionalFormatsUntilEnd(m_document->syntaxHighlighter(),
                                                                         rubocopFuture.future());
    TextEditor::SemanticHighlighter::incrementalApplyExtraAdditionalFormats(m_document->syntaxHighlighter(),
                                                                            rubocopFuture.future(), 0,
                                                                            offenses.count(), m_extraFormats);
    Core::MessageManager::instance()->write(QString("Got %1 offenses").arg(offenses.length()));
    }
    return;
BAD_JSON:
    qWarning() << Q_FUNC_INFO;
    qWarning() << "can't parse JSON";
}

void RubocopHighlighter::finishRuboCopHighlight()
{
    if (m_startRevision != m_document->document()->revision()) {
        m_busy = false;
        return;
    }
    // https://github.com/ocaml/merlin/blob/master/doc/dev/PROTOCOL.md
    // for protocol details

    QJsonDocument jsonResponse = QJsonDocument::fromJson(m_outputBuffer.toUtf8());
    m_outputBuffer.clear();

    if (jsonResponse.isEmpty())
        return;

    if (!jsonResponse.isObject()) {
        qCritical() << "JSON response is not an object";
        qCritical() << jsonResponse;
        return;
    }
    auto root = jsonResponse.object();
    Q_ASSERT(root.value("class") != "");

    QString clas = root.value("class").toString();
    if (clas == "return") {
        if (m_chart.isActive("codeSent")) {
            qDebug() << "Got a result in a state codeSent";
            sendFSMevent("diagnosticsReceived");
            qDebug() << "json is " << root;
            parseDiagnosticsJson(root.value("value"));
        } else {
            sendFSMevent("errorHappend");
            qWarning() << "Got a result but the state is not detectable";
        }
    } else if (clas == "exception") {
        qWarning() << "merlin exception";
        qWarning() << root;
    } else if (clas == "failure") {
        qWarning() << "merlin failure";
        qWarning() << root;
    } else if (clas == "error") {
        qWarning() << "merlin error";
        qWarning() << root;
    } else {
        qWarning() << "Unknown merlin response class";
    }
    m_busy = false;
}

Diagnostics RubocopHighlighter::processMerlinErrors(const QJsonValue& v) {
    if (!v.isArray()) {
        qDebug() << "not an array" << v;
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

        // TODO: response contains filed valid: bool
        // Undestand why it is important
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

OCamlCreator::Range RubocopHighlighter::lineColumnLengthToRange(int line, int column, int length)
{
    int pos = lineColumnToPos(line, column);
    return OCamlCreator::Range(pos, length);
}

}
