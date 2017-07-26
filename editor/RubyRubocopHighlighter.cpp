#include "RubyRubocopHighlighter.h"

#include <texteditor/textdocument.h>
#include <texteditor/semantichighlighter.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/taskhub.h>

#include <utils/asconst.h>
#include <utils/qtcassert.h>
#include "RubyConstants.h"
#include "MerlinFSM.h"

#include <QDebug>
#include <QProcess>
#include <QTextDocument>
#include <QtConcurrent>
#include <QMessageBox>
#include <QTextBlock>
#include <QJsonDocument>

namespace OCamlCreator {

class RubocopFuture : public QFutureInterface<TextEditor::HighlightingResult>, public QObject
{
public:
    RubocopFuture(const Offenses &offenses)
    {
        reportResults(offenses);
    }
};

class RubocopHighlighterPrivate
{
    RubocopHighlighter *q_ptr;
    Q_DECLARE_PUBLIC(RubocopHighlighter)
public:
    TextEditor::TextDocument *m_document;
    QHash<Utils::FileName, Diagnostics> m_diagnostics;
    QHash<int, QTextCharFormat> m_extraFormats;
    MerlinFSM* m_chart;
    QProcess *m_rubocop;
    bool m_rubocopFound;
    bool m_busy;
    QString m_outputBuffer;

public:
    RubocopHighlighterPrivate(RubocopHighlighter *q) : q_ptr(q)
      , m_document(nullptr), m_extraFormats()
      , m_chart(nullptr), m_rubocop(nullptr)
      , m_rubocopFound(), m_busy(), m_outputBuffer()
    {
        QTextCharFormat format;
        format.setUnderlineColor(Qt::darkYellow);
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        m_extraFormats[0] = format;
        m_extraFormats[1] = format;
        m_extraFormats[1].setUnderlineColor(Qt::darkGreen);
        m_extraFormats[2] = format;
        m_extraFormats[2].setUnderlineColor(Qt::red);

        m_chart = new MerlinFSM(q);
        QVariantMap m;
        m.insert("On",      true);
        m.insert("Default", true);
        m.insert("CodeSent", false);
        m_chart->setInitialValues(m);

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
        m_chart->init();
        m_chart->start();
        Q_ASSERT(m_chart->isInitialized());
        qDebug() << m_chart->activeStateNames(false);
    }
    ~RubocopHighlighterPrivate() {
        if (m_rubocop) {
            m_rubocop->closeWriteChannel();
            m_rubocop->waitForFinished(3000);
            delete m_rubocop;
        }
    }

    TextEditor::TextDocument *document() { return m_document; }
    TextEditor::TextDocument *doc() { return document(); }
    QHash<Utils::FileName, Diagnostics> diags() { return m_diagnostics; }
    MerlinFSM* chart() { return m_chart; }
    MerlinFSM* fsm()   { return chart(); }
    QProcess*  proc()  { return m_rubocop; }
    const QString& outBuf() const { return m_outputBuffer; }
    void clearBuf() { m_outputBuffer.clear(); }
    void setBusy(bool v) { m_busy = v; }
    bool isBusy() const { return m_busy; }

    void parseOccurencesJson(const QJsonValue& v) {
        QTC_CHECK(v.isArray());

        foreach (const QJsonValue& v, v.toArray()) {
            const QJsonObject& start = v.toObject().value("start").toObject();
            const QJsonObject& end = v.toObject().value("end").toObject();
            const int line1 = start.value("line").toInt();
            const int col1  = start.value("col").toInt();
            const int line2 = end.value("line").toInt();
            const int col2  = end.value("col").toInt();
            const int p1 = lineColumnToPos(line1, col1);
            const int p2 = lineColumnToPos(line2, col2);
            const Range r(line1, col1 + 1,
                    line2, col2 + 1,
                    p1, p2-p1);
            qDebug() << r;
        }
    }

    void initRubocopProcess(const QStringList &args);
    void sendFSMevent(const QString&);
    void parseDiagnosticsJson(const QJsonValue& resp);
    void parseDefinitionsJson(const QJsonValue& resp);
    int lineColumnToPos(const int line, const int column);
};

int RubocopHighlighterPrivate::lineColumnToPos(const int line, const int column) {
    QTextBlock block = m_document->document()->findBlockByLineNumber(line - 1);
    return block.position() + column;
}

void RubocopHighlighterPrivate::parseDefinitionsJson(const QJsonValue& resp)
{
    Q_Q(RubocopHighlighter);
    qDebug() << resp;
    using namespace ::Utils;
    // {"file":"test.ml","pos":{"col":4,"line":35}})
    if (resp.isString()) {
        q->generalMsg( resp.toString() );
        return;
    }

    QTC_CHECK(resp.isObject());
    auto root = resp.toObject();
    QTC_CHECK(root.value("pos") != "");
    auto posJson = root.value("pos").toObject();
    QTC_CHECK(posJson.value("col") != "");
    auto col = posJson.value("col").toInt();
    QTC_CHECK(posJson.value("line") != "");
    auto line = posJson.value("line").toInt();

    if (root.value("file").isUndefined()) {
        qWarning() << "got answer with underfined path";
        q->generalMsg( QString("got answer with undefined path %1:%2").arg(__FILE__).arg(__LINE__) );
    } else  {
        auto path = root.value("file").toString();
        QTC_CHECK(!path.isEmpty());
        //qDebug() << "relocating editor to " << path << line << ":" << col;
        Core::EditorManager::instance()->openEditorAt(path, line, col);
    }
}

void RubocopHighlighterPrivate::parseDiagnosticsJson(const QJsonValue& resp)
{
    Diagnostics diags;
    Offenses offenses;
    Q_Q(RubocopHighlighter);

    if (!resp.isArray()) {
        qWarning() << "not an array " << resp;
        goto BAD_JSON;
    }
    diags = m_diagnostics[doc()->filePath()] = Diagnostics();
    diags.setValid();

    foreach (const QJsonValue& v, resp.toArray()) {
        const QJsonObject& start = v.toObject().value("start").toObject();
        const QJsonObject& end = v.toObject().value("end").toObject();
        const int line1 = start.value("line").toInt();
        const int col1  = start.value("col").toInt();
        const int line2 = end.value("line").toInt();
        const int col2  = end.value("col").toInt();
        const int p1 = lineColumnToPos(line1, col1);
        const int p2 = lineColumnToPos(line2, col2);
        Range r(line1, col1 + 1,
                line2, col2 + 1,
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

    TextEditor::SemanticHighlighter::clearExtraAdditionalFormatsUntilEnd(document()->syntaxHighlighter(),
                                                                         rubocopFuture.future());
    TextEditor::SemanticHighlighter::incrementalApplyExtraAdditionalFormats(document()->syntaxHighlighter(),
                                                                            rubocopFuture.future(), 0,
                                                                            offenses.count(), m_extraFormats);
    q->generalMsg(QString("Got %1 offenses").arg(offenses.length()));
    }
    return;
BAD_JSON:
    qWarning() << Q_FUNC_INFO;
    qWarning() << "can't parse JSON";
}

void RubocopHighlighterPrivate::sendFSMevent(const QString &s)
{
    qDebug() << QString("Sending event '%1' when current states are ").arg(s)
             << chart()->activeStateNames(true);
    chart()->submitEvent(s);
}

RubocopHighlighter::RubocopHighlighter()
    : d_ptr(new RubocopHighlighterPrivate(this))
    , m_startRevision(0)
{
}

RubocopHighlighter::~RubocopHighlighter()
{
}

RubocopHighlighter *RubocopHighlighter::instance()
{
    static RubocopHighlighter rubocop;
    return &rubocop;
}

bool RubocopHighlighter::run(TextEditor::TextDocument *document, const QString &fileNameTip)
{
    Q_D(RubocopHighlighter);
//    if (m_busy || m_rubocop->state() == QProcess::Starting)
//        return false;
//    if (!m_rubocopFound)
//        return true;

//    m_busy = true;
    m_startRevision = document->document()->revision();

    m_timer.start();
    d->m_document = document;

    const QString filePath = document->filePath().isEmpty()
            ? fileNameTip
            : document->filePath().toString();
    /*QJsonArray obj {"tell","start","end","let f x = x let () = ()"};
    QJsonDocument doc(obj);
    QByteArray query(doc.toJson(QJsonDocument::Compact));
    m_rubocop->write(query);
    qDebug () << "query" << obj;*/

    performErrorsCheck( document->contents() );
    return true;
}

QString RubocopHighlighter::diagnosticAt(const Utils::FileName &file, int pos)
{
    Q_D(RubocopHighlighter);
    auto it = d->diags().find(file);
    if (it == d->diags().end())
        return QString();

    return it->messages[Range(pos + 1, 0)];
}

void RubocopHighlighter::performGoToDefinition(TextEditor::TextDocument *document, const int line, const int column)
{
    if (!document)
        return;

    Q_D(RubocopHighlighter);
    d->sendFSMevent("goToDefAsked");
    const QString& pos = QString("%1:%2").arg(line).arg(column);
    auto path = document->filePath().toString();
    QStringList args { "locate", "-position", pos, "-filename", path };
//    qDebug() << "locating at" << pos;
    d->initRubocopProcess(args);
    const QByteArray& file = document->contents();
    d->proc()->write(file.data(), file.length());
    d->proc()->closeWriteChannel();
}

void RubocopHighlighter::performFindUsages(TextEditor::TextDocument *document, const int line, const int column)
{
    if (!document)
        return;

    Q_D(RubocopHighlighter);
    d->sendFSMevent("occurencesAsked");
    const QString& pos = QString("%1:%2").arg(line).arg(column);
    auto path = document->filePath().toString();
    QStringList args { "locate", "-identifier-at", pos/*, "-filename", path*/ };
//    qDebug() << "locating at" << pos;
    d->initRubocopProcess(args);
    const QByteArray& file = document->contents();
    d->proc()->write(file.data(), file.length());
    d->proc()->closeWriteChannel();
}

void RubocopHighlighter::performErrorsCheck(const QByteArray &file)
{
    if (!Core::EditorManager::instance()->currentDocument())
        return;

    Q_D(RubocopHighlighter);
    d->sendFSMevent("sendCode");
    auto path = Core::EditorManager::instance()->currentDocument()->filePath().toString();
    QStringList args {"errors", "-filename", path };
    d->initRubocopProcess(args);
    d->proc()->write(file.data(), file.length());
    d->proc()->closeWriteChannel();
}

void RubocopHighlighterPrivate::initRubocopProcess(const QStringList& args)
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

        Q_Q(RubocopHighlighter);
        if (m_outputBuffer.endsWith(QLatin1String("\n")))
            q->finishRuboCopHighlight();
    });

    //TODO: detect opam and remove hardcoded path
    // http://stackoverflow.com/questions/19409940/how-to-get-output-system-command-in-qt
    auto new_args = args;
    new_args.push_front("single");
    static const QString opamPath = "/home/kakadu/.opam/4.04.0+fp+flambda/bin/";
    m_rubocop->processEnvironment().insert("MERLIN_LOG"," /tmp/merlin.qtcreator.log");
    m_rubocop->start(opamPath + "ocamlmerlin", new_args);
    qDebug() << QString("starting (PID=%1)").arg(m_rubocop->pid()) << "with args" << qPrintable(m_rubocop->arguments().join(' '));
}

void RubocopHighlighter::finishRuboCopHighlight()
{
    Q_D(RubocopHighlighter);
    if (m_startRevision != d->document()->document()->revision()) {
        d->m_busy = false;
        return;
    }
    // https://github.com/ocaml/merlin/blob/master/doc/dev/PROTOCOL.md
    // for protocol details

    QJsonDocument jsonResponse = QJsonDocument::fromJson(d->outBuf().toUtf8());
    d->clearBuf();

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
        if (d->fsm()->isActive("codeSent")) {
            qDebug() << "Got a result in a state codeSent";
            d->sendFSMevent("diagnosticsReceived");
            qDebug() << "json is " << root;
            d->parseDiagnosticsJson(root.value("value"));
        } else if (d->fsm()->isActive("GoToDefSent")) {
            d->sendFSMevent("definitionsReceived");
            d->parseDefinitionsJson(root.value("value"));
        } else if (d->fsm()->isActive("occurencesSent")) {
            d->sendFSMevent("occurencesReceived");
            d->parseOccurencesJson(root.value("value"));
        } else {
            d->sendFSMevent("errorHappend");
            qWarning() << "Got a result but the state is not detectable";
        }
    } else if (clas == "exception") {
        qWarning() << "merlin exception";
        qWarning() << root;
    } else if (clas == "failure") {
        qWarning() << "merlin failure";
        qWarning() << root;
        generalMsg("Merlin failure: " + root.value("value").toString() );

    } else if (clas == "error") {
        qWarning() << "merlin error";
        qWarning() << root;
    } else {
        qWarning() << "Unknown merlin response class";
    }
    d->setBusy(false);
}

//Diagnostics RubocopHighlighter::processMerlinErrors(const QJsonValue& v)
//{
//    Q_D(RubocopHighlighter);
//    if (!v.isArray()) {
//        qDebug() << "not an array" << v;
//        return Diagnostics();
//    }
//    Diagnostics &d = m_diagnostics[m_document->filePath()] = Diagnostics();
//    d.setValid();

//    QJsonArray arr = v.toArray();
//    foreach (const QJsonValue& v, arr) {
//        const QJsonObject& start = v.toObject().take("start").toObject();
//        const QJsonObject& end = v.toObject().take("end").toObject();
//        const int line1 = start.value("line").toInt();
//        const int col1 = start.value("col").toInt();
//        const int p1 = lineColumnToPos(line1, col1);
//        const int p2 = lineColumnToPos(end.value("line").toInt(),
//                                       end.value("col").toInt() );
//        Range r(start.value("line").toInt(),
//                start.value("col").toInt() + 1,
//                end.value("line").toInt(),
//                end.value("col").toInt() + 1,
//                p1, p2-p1);

//        const QString& msg = v.toObject().value("message").toString();
//        d.messages[r] = msg;

//        // TODO: response contains filed valid: bool
//        // Undestand why it is important
//    }

//    return d;
//}

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
    Q_D(RubocopHighlighter);
    Offenses result;
    Diagnostics &diag = d->diags()[d->doc()->filePath()] = Diagnostics();

    const QVector<QStringRef> lines = d->outBuf().splitRef('\n');
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
    d->clearBuf();

    return result;
}

void RubocopHighlighter::generalMsg(const QString &msg) const {
    Core::MessageManager::instance()->write(msg);
}

OCamlCreator::Range RubocopHighlighter::lineColumnLengthToRange(int line, int column, int length)
{
    Q_D(RubocopHighlighter);
    int pos = d->lineColumnToPos(line, column);
    return OCamlCreator::Range(pos, length);
}

}
