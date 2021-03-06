#include "OCamlCompletionAssist.cpp"
#include "RubyRubocopHighlighter.h"

#include <texteditor/refactoroverlay.h>
#include <texteditor/textdocument.h>
#include <texteditor/semantichighlighter.h>
#include <texteditor/codeassist/genericproposal.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/iassistproposal.h>
#include <texteditor/codeassist/assistproposalitem.h>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/find/searchresultwindow.h>
#include <projectexplorer/taskhub.h>

#include <utils/asconst.h>
#include <utils/qtcassert.h>
#include "RubyConstants.h"
#include "MerlinFSM.h"

#include <QtCore/QQueue>
#include <QDebug>
#include <QProcess>
#include <QTextDocument>
#include <QtConcurrent>
#include <QMessageBox>
#include <QTextBlock>
#include <QJsonDocument>

namespace OCamlCreator
{
void jsonParseStartEnd(const QJsonObject& o, int& line1, int& col1, int& line2, int& col2);
typedef TextEditor::IAssistProcessor::AsyncCompletionsAvailableHandler CompletionsHandler;

struct MerlinRequestBase {
public:
    MerlinRequestBase(const QStringList& _args) : args(_args) {}
    virtual ~MerlinRequestBase() {
        //qDebug() << "merlin request destroyed";
    }
    virtual const QString fsmEvent() const = 0;
    virtual const QString expectedState() const = 0;
    virtual bool isValid() const = 0;
    virtual const QString text() const = 0;

    // TODO: arguments should be constructed inside every message class
    QStringList args;
};

struct MerlinRequestQTextDoc : public MerlinRequestBase {
    MerlinRequestQTextDoc(const QStringList& _args, QTextDocument *_doc)
        : MerlinRequestBase(_args), doc(_doc), m_startRevision(doc->revision())
    {}
    virtual bool isValid() const { return doc!=nullptr; }
    virtual const QString text() const {
        return doc->toPlainText();
    };

private:
    QTextDocument *doc;
    int m_startRevision;
};

struct MerlinRequestComplete : public MerlinRequestQTextDoc
{
    MerlinRequestComplete( const QStringList& _args, QTextDocument *_doc
                         , int pos, const CompletionsHandler& h)
        : MerlinRequestQTextDoc(_args,_doc)
        , m_asyncCompletionsAvailableHandler(h), m_oldStartPos(pos)
    {}
    const QString fsmEvent() const Q_DECL_OVERRIDE { return "completionsAsked"; }
    const QString expectedState() const Q_DECL_OVERRIDE { return "completionsReceived"; }

    CompletionsHandler m_asyncCompletionsAvailableHandler;
    int m_oldStartPos;
};

struct MerlinRequestQTCDoc : public MerlinRequestBase
{
    MerlinRequestQTCDoc(const QStringList& _args, TextEditor::TextDocument *_doc)
        : MerlinRequestBase(_args), qtcDoc(_doc)
    {}
    virtual bool isValid() const override { return qtcDoc != nullptr; }
    virtual const QString text() const override {
        return qtcDoc->plainText();
    };
    TextEditor::TextDocument *document() { return qtcDoc; }
private:
    TextEditor::TextDocument *qtcDoc;
};

struct MerlinRequestErrors : public MerlinRequestQTCDoc {
public:
    MerlinRequestErrors(const QStringList& _args, TextEditor::TextDocument *_doc)
        : MerlinRequestQTCDoc(_args, _doc)
    {
        //qDebug() << "Request for asking erros on file" << _doc->filePath();
    }
    virtual ~MerlinRequestErrors() {
    }

    const QString fsmEvent() const Q_DECL_OVERRIDE { return "sendCode"; }
    const QString expectedState() const Q_DECL_OVERRIDE { return "codeSent"; }
};

struct MerlinRequestUsages : public MerlinRequestQTCDoc {
public:
    MerlinRequestUsages(const QStringList& _args, TextEditor::TextDocument *_doc)
        : MerlinRequestQTCDoc(_args, _doc)
    {}
    const QString fsmEvent() const Q_DECL_OVERRIDE { return "occurencesAsked"; }
    const QString expectedState() const Q_DECL_OVERRIDE { return "occurencesSent"; }
};

struct MerlinRequestGTD : public MerlinRequestQTCDoc {
public:
    MerlinRequestGTD(const QStringList& _args, TextEditor::TextDocument *_doc)
        : MerlinRequestQTCDoc(_args, _doc)
    {}
    const QString fsmEvent() const Q_DECL_OVERRIDE { return "goToDefAsked"; }
    const QString expectedState() const Q_DECL_OVERRIDE { return "GoToDefSent"; }
};


typedef ProjectExplorer::Task::TaskType TaskType;

struct SingleDiagnostic {
    QString message;
    TaskType typ;

    SingleDiagnostic() : message(), typ() {}
    SingleDiagnostic(const QString& msg, ProjectExplorer::Task::TaskType t) : message(msg), typ(t)
    {}

    operator QString() {
        QString typStr;
        switch (typ) {
        case ProjectExplorer::Task::Error:   typStr = "Error";   break;
        case ProjectExplorer::Task::Warning: typStr = "Warning"; break;
        case ProjectExplorer::Task::Unknown: typStr = "Unknown"; break;
        }
        QTC_CHECK(!typStr.isEmpty());
        return QString("%1: %2").arg(typStr).arg(message);
    }

    TextEditor::HighlightingResult toHighlightResult(const Range &r) const {
        int kind = -1;
        Q_UNUSED(kind);
        TextEditor::TextStyles style;
        style.mixinStyles.initializeElements();
        switch (typ) {
        case ProjectExplorer::Task::Error:
            kind = 2;
            style.mainStyle = TextEditor::C_ERROR;
            break;
        case ProjectExplorer::Task::Warning:
            kind = 0;
            style.mainStyle = TextEditor::C_WARNING;
            break;
        case ProjectExplorer::Task::Unknown:
            return TextEditor::HighlightingResult(r.startLine, r.startCol, r.length, 1);
        }

        // return TextEditor::HighlightingResult(r.startLine, r.startCol, r.length, kind);
        return TextEditor::HighlightingResult(r.startLine, r.startCol, r.length, style);
    }
};

struct Diagnostics {
    Diagnostics() : m_isValid(false) {}
    QMap<Range, SingleDiagnostic> messages;
    operator int() const {
        return m_isValid;
    }
    void setValid(bool b) { m_isValid = b; }
    void setValid() { setValid(true); }
    void setInvalid() { m_isValid = false; }
private:
    bool m_isValid;
};

class RubocopFuture : public QFutureInterface<TextEditor::HighlightingResult>, public QObject
{
public:
    RubocopFuture(const Offenses &offenses)
    {
        reportResults(offenses);
    }
};

typedef  QSharedPointer<MerlinRequestBase> MySharedPtr;


MerlinQuickFix::MerlinQuickFix(const MerlinQuickFix &z)
    : line1(z.line1), col1(z.col1)
    , line2(z.line2), col2(z.col2)
    , startPos(z.startPos), endPos(z.startPos)
    , new_values(z.new_values)
{
}

struct WholeDiags {
    Diagnostics diags;
    QVector<MerlinQuickFix> quickFixes;
    TextEditor::RefactorMarkers  markers;
    explicit WholeDiags() {}
};

class RubocopHighlighterPrivate
{
    RubocopHighlighter *q_ptr;
    Q_DECLARE_PUBLIC(RubocopHighlighter)
public:
    QHash<Utils::FileName, WholeDiags> m_diagsHash;
    QHash<int, QTextCharFormat> m_extraFormats;
    MerlinFSM* m_chart;
    QProcess *m_rubocop;
    bool m_rubocopFound;
    bool m_busy;

    QQueue<QSharedPointer<MerlinRequestBase> > m_msgQueue;
    QString m_outputBuffer;

public:
    RubocopHighlighterPrivate(RubocopHighlighter *q) : q_ptr(q)
      , m_diagsHash()
      , m_extraFormats()
      , m_chart(nullptr), m_rubocop(nullptr)
      , m_rubocopFound(), m_busy()
      , m_msgQueue(), m_outputBuffer()
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
        m.insert("completionsSent", false);
        m_chart->setInitialValues(m);

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

    QHash<Utils::FileName, WholeDiags> diags() { return m_diagsHash; }
    const WholeDiags diagsForFile(const Utils::FileName &fn) const {
        auto it = m_diagsHash.find(fn);
        if (it == m_diagsHash.end())
            return WholeDiags();
        return *it;
    }
    MerlinFSM* chart() { return m_chart; }
    MerlinFSM* fsm()   { return chart(); }
    QProcess*  proc()  { return m_rubocop; }
    const QString& outBuf() const { return m_outputBuffer; }
    void clearBuf() { m_outputBuffer.clear(); }
    void setBusy(bool v) { m_busy = v; }
    bool isBusy() const { return m_busy; }

    void initRubocopProcess(const QStringList &args);
    void sendFSMevent(const QString&);
    void parseDiagnosticsJson(const QJsonValue& resp, MerlinRequestErrors *);
    void parseDefinitionsJson(const QJsonValue& resp, MerlinRequestGTD *);
    void parseCompletionsJson(const QJsonValue& resp, MerlinRequestComplete* req);
    int lineColumnToPos(QTextDocument *doc, const int line, const int column);

    void enqueMsg(MerlinRequestBase *msg);
    void processingFinished();
    void sendTopMessage();

    /* ********************************  search related stuff *****************/

    void parseOccurencesJson(const QJsonValue& v, MerlinRequestUsages* req);

};

int RubocopHighlighterPrivate::lineColumnToPos(QTextDocument *doc, const int line, const int column) {
    QTC_CHECK(doc);
    QTextBlock block = doc->findBlockByLineNumber(line - 1);
    return block.position() + column;
}

void RubocopHighlighterPrivate::enqueMsg(MerlinRequestBase *msg)
{
    m_msgQueue.enqueue( QSharedPointer<MerlinRequestBase>(msg) );

    if (! msg->isValid()) {
        qWarning() << "we scheduled a merlin request but it is not valid. Skipping";
        return;
    }

    if (m_busy)
        return;

    // run request immediately
    sendTopMessage();
}

void RubocopHighlighterPrivate::processingFinished()
{
    if (m_msgQueue.isEmpty())
        return;

    sendTopMessage();
}

void RubocopHighlighterPrivate::sendTopMessage()
{
    QTC_CHECK(!m_msgQueue.isEmpty());

    auto msg = m_msgQueue.head();
    initRubocopProcess(msg->args);

    const QByteArray& file = msg->text().toLocal8Bit();
    proc()->write(file.data(), file.length());
    setBusy(true);
    proc()->closeWriteChannel();
    sendFSMevent(msg->fsmEvent());
}

void RubocopHighlighterPrivate::parseOccurencesJson(const QJsonValue &v, MerlinRequestUsages *req)
{
    QTC_CHECK(req);
    QJsonArray arr;
    if (v.isArray())
        arr = v.toArray();
    else
        arr.push_back(v);


    qDebug() << arr;

    using namespace Core;
    SearchResult *search = SearchResultWindow::instance()->startNewSearch
            (q_ptr->tr("OCaml usages:"),
             QString(""),
             "searchTerm",
             SearchResultWindow::SearchOnly,
             SearchResultWindow::PreserveCaseEnabled,
             QLatin1String("OCamlEditor") );

    SearchResultWindow::instance()->popup(IOutputPane::ModeSwitch | IOutputPane::WithFocus);

    foreach (const QJsonValue& v, arr) {
        const QJsonObject& start = v.toObject().value("start").toObject();
        const QJsonObject& end = v.toObject().value("end").toObject();
        const int line1 = start.value("line").toInt();
        const int col1  = start.value("col").toInt();
        const int line2 = end.value("line").toInt();
        const int col2  = end.value("col").toInt();

        auto pos1 = Search::TextPosition(line1, col1);
        auto pos2 = Search::TextPosition(line2, col2);
        // Maybe this check is wrong
        QTC_CHECK(line1 == line2);

        auto fileName = req->document()->filePath().toString();

        auto line = req->document()->document()->findBlockByLineNumber(pos1.line-1).text();
        search->addResult(fileName, line, Search::TextRange(pos1, pos2) );
    }
}

void RubocopHighlighterPrivate::parseDefinitionsJson(const QJsonValue& resp, MerlinRequestGTD*)
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

void RubocopHighlighterPrivate::parseCompletionsJson(const QJsonValue& resp, MerlinRequestComplete* req)
{
    QTC_CHECK(req);

    auto root = resp.toObject();
    QTC_CHECK(root.value("entries").isArray());

    QList<TextEditor::AssistProposalItemInterface *> items;
    foreach (auto j, root.value("entries").toArray()) {
        auto obj = j.toObject();
        auto name = obj.value("name").toString();
        auto item = new TextEditor::AssistProposalItem();
        item->setText(name);
        QString hint = "";
        auto descStr = obj.value("desc").toString();
        if (!descStr.isEmpty())
            hint = descStr;

        // info is commented out because tooltips doesn't support multiline strings
#if 0
        auto infoStr = obj.value("info").toString();
        if (!infoStr.isEmpty()) {
            hint += QChar::LineSeparator;
            hint += infoStr;
        }
#endif
        item->setDetail(hint);
        items << item;
    }

    if (items.size() == 0) {
        qDebug() << "Got 0 completions.";
//        qDebug() << root;
        return;
    }
    auto prop = new TextEditor::GenericProposal(req->m_oldStartPos, items);

    if (req->m_asyncCompletionsAvailableHandler) {
        // call and make empty
        // qDebug() << "calling handler with items.size() = " << items.size();
        req->m_asyncCompletionsAvailableHandler(prop);
        req->m_asyncCompletionsAvailableHandler = [=](TextEditor::IAssistProposal*) { };
    }
}


QTextCursor cursorAtLastPositionOfLine(QTextDocument *textDocument, int lineNumber)
{
    const QTextBlock textBlock = textDocument->findBlockByNumber(lineNumber - 1);
    QTC_ASSERT(textBlock.isValid(), return QTextCursor());

    const int lastPositionOfLine = textBlock.position() + textBlock.length() - 1;

    QTextCursor textCursor(textDocument);
    textCursor.setPosition(lastPositionOfLine);

    return textCursor;
}

void RubocopHighlighterPrivate::parseDiagnosticsJson(const QJsonValue& resp, MerlinRequestErrors* req)
{
    Diagnostics diags;
    Offenses offenses;
    QTC_CHECK(req);
    auto document = req->document();
    Q_Q(RubocopHighlighter);
    Q_UNUSED(q);

    auto error = []() {
        qWarning() << Q_FUNC_INFO;
        qWarning() << "can't parse JSON";
    };

    if (Core::EditorManager::instance()->currentDocument() != req->document()) {
        qDebug() << "the editor is for another file";
        return;
    }

    QJsonArray errorsArr;
    QJsonArray qfArr;
    if (resp.isArray()) {
        errorsArr = resp.toArray();
    } else if (resp.isObject()) {
        auto o = resp.toObject();
        QTC_CHECK(o.value("errors").isArray());
        errorsArr = o.value("errors").toArray();
        qfArr = o.value("quickfixes").toArray();
    } else {
        qWarning() << "bad format" << resp;
        error();
        return;
    }

    auto curInfo = m_diagsHash[document->filePath()];
    {
        QVector<MerlinQuickFix> curQf;
        curInfo.markers.clear();
        foreach (auto v, qfArr) {
            auto vo = v.toObject();
            auto qf = MerlinQuickFix();
            jsonParseStartEnd(vo, qf.line1, qf.col1, qf.line2, qf.col2);

            QTC_CHECK(vo.value("suggs").isArray() );
            foreach (auto s, vo.value("suggs").toArray()) {
                qf.new_values << s.toString();
            }

            qf.startPos = lineColumnToPos(req->document()->document(), qf.line1, qf.col1);
            qf.endPos   = lineColumnToPos(req->document()->document(), qf.line2, qf.col2);
            curQf.append(qf);

            auto marker = TextEditor::RefactorMarker();
            marker.tooltip = qf.new_values.join(" ");
            auto textDocument = req->document()->document();
            marker.cursor = cursorAtLastPositionOfLine(textDocument, qf.line1);

            curInfo.markers.append(marker);
        }
        curInfo.quickFixes = curQf;

    }

    diags = curInfo.diags = Diagnostics();
    diags.setValid();
    foreach (const QJsonValue& v, errorsArr) {
        const auto vo = v.toObject();
        Range r;
        jsonParseStartEnd(vo, r.startLine, r.startCol, r.endLine, r.endCol);
        r.startCol++;
        r.endCol++;  // because QtC enumerates colums from 1
        r.pos = lineColumnToPos(document->document(), r.startLine, r.startCol);
        const int pos2 = lineColumnToPos(document->document(), r.endLine, r.endCol);
        r.length = pos2 - r.pos;

        const QString& msg = vo.value("message").toString();
        const QString& typStr = vo.value("type").toString();
        TaskType taskTyp = ProjectExplorer::Task::Unknown;
        if (typStr == "error")
            taskTyp = ProjectExplorer::Task::Error;
        else if (typStr == "warning")
            taskTyp = ProjectExplorer::Task::Warning;
        else {
            QTC_ASSERT(false, {qDebug() << "Unsupported info from merlin";});
        }
        diags.messages[r] = SingleDiagnostic(msg, taskTyp);


        // TODO: response contains filed valid: bool
        // but it seems not important
    }

    // now we convert parsed diagnostics to editor's highlighting regions
    // and add tasks to the TaskHub (Issues bottom pane)
    ProjectExplorer::TaskHub::clearTasks(Constants::TASK_CATEGORY_MERLIN_COMPILE);
    for (const Range& r : diags.messages.keys()) {
        auto taskInfo = diags.messages.value(r);
        offenses << taskInfo.toHighlightResult(r);
        using namespace ProjectExplorer;
        TaskHub::instance()->addTask(taskInfo.typ, taskInfo.message,
                                     Constants::TASK_CATEGORY_MERLIN_COMPILE,
                                     document->filePath(),
                                     r.startLine
                                     );
        // tasks doesn't support specifying column. Maybe create a PR?
    }
    {
    RubocopFuture rubocopFuture(offenses);

    TextEditor::SemanticHighlighter::clearExtraAdditionalFormatsUntilEnd(document->syntaxHighlighter(),
                                                                         rubocopFuture.future());
    TextEditor::SemanticHighlighter::incrementalApplyExtraAdditionalFormats(document->syntaxHighlighter(),
                                                                            rubocopFuture.future(), 0,
                                                                            offenses.count(), m_extraFormats);
//    q->generalMsg(QString("Got %1 offenses").arg(offenses.length()));
    emit q->codeWarningsUpdated(req->document()->filePath(),
                                req->document()->document()->revision(),
                                curInfo.markers );
    }
}

void RubocopHighlighterPrivate::sendFSMevent(const QString &s)
{
//    qDebug() << QString("Sending event '%1' when current states are ").arg(s)
//             << chart()->activeStateNames(true);
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
//    if (m_busy || m_rubocop->state() == QProcess::Starting)
//        return false;
//    if (!m_rubocopFound)
//        return true;

    Q_UNUSED(fileNameTip);
//    m_busy = true;
    m_startRevision = document->document()->revision();

    m_timer.start();
//    d->m_document = document;

    /*QJsonArray obj {"tell","start","end","let f x = x let () = ()"};
    QJsonDocument doc(obj);
    QByteArray query(doc.toJson(QJsonDocument::Compact));
    m_rubocop->write(query);
    qDebug () << "query" << obj;*/

    performErrorsCheck( document );
    return true;
}

QString RubocopHighlighter::diagnosticAt(const Utils::FileName &file, int pos)
{
    Q_D(RubocopHighlighter);
    auto it = d->diags().find(file);
    if (it == d->diags().end())
        return QString();

    if (it->diags.messages.keys().count()>0)
        return it->diags.messages[Range(pos + 1, 0)];
    else
        return "";
}

void RubocopHighlighter::performGoToDefinition(TextEditor::TextDocument *document, const int line, const int column)
{
    if (!document)
        return;

    Q_D(RubocopHighlighter);
    const QString& pos = QString("%1:%2").arg(line).arg(column);
    auto path = document->filePath().toString();
    QStringList args { "locate", "-position", pos, "-filename", path };
    d->enqueMsg(new MerlinRequestGTD(args, document) );
}

void RubocopHighlighter::performFindUsages(TextEditor::TextDocument *document, const int line, const int column)
{
    if (!document)
        return;

    Q_D(RubocopHighlighter);
    const QString& pos = QString("%1:%2").arg(line).arg(column);
    auto path = document->filePath().toString();
    QStringList args { "occurrences", "-identifier-at", pos/*, "-filename", path*/ };
    d->enqueMsg(new MerlinRequestUsages(args, document) );
}

void RubocopHighlighter::performErrorsCheck(TextEditor::TextDocument *doc)
{
    Q_D(RubocopHighlighter);
    QTC_CHECK(doc);
    const QString filePath = doc->filePath().isEmpty()
            ? "*buffer*"
            : doc->filePath().toString();

//    auto path = Core::EditorManager::instance()->currentDocument()->filePath().toString();
    QStringList args {"errors" , "-filename", filePath };
    d->enqueMsg(new MerlinRequestErrors(args, doc) );
}

void RubocopHighlighter::performCompletion(QTextDocument *doc, const QString& prefix, const int startPos,
           const int line, const int column, const AsyncCompletionsAvailableHandler &handler)
{
    if (!Core::EditorManager::instance()->currentDocument())
        return;

    const QString& pos = QString("%1:%2").arg(line+1).arg(column);
    QStringList args { "complete-prefix"
                     , "-position", pos
                     , "-prefix", prefix
                     , "-doc", "true"
                     };
    Q_D(RubocopHighlighter);
    d->enqueMsg(new MerlinRequestComplete(args, doc, startPos, handler) );
}

void OCamlCreator::RubocopHighlighter::enumerateQuickFixes(const TextEditor::QuickFixInterface &iface,
              const std::function<void(const MerlinQuickFix&)> &hook)
{
    Q_D(RubocopHighlighter);
    // we should find all quickfixes such that current cursor position is between
    // begin and end of the quickFixItem

    const int curPos = iface->position();
    auto path = iface->fileName();
    // iface->fileName() actually contains full path

    foreach (auto qf, d->diags()[Utils::FileName::fromString(path)].quickFixes) {
        auto qfStartPos = d->lineColumnToPos(iface->textDocument(), qf.line1, qf.col1);
        // TODO: Dirty hack. Support multiline idents
        auto qfEndPos   = qf.col2 - qf.col1 + qfStartPos;
        if (curPos >= qfStartPos && curPos <= qfEndPos)
            hook(qf);
    }
}

void RubocopHighlighterPrivate::initRubocopProcess(const QStringList& args)
{
//    qDebug() << "init process for a single message";

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
//        qDebug() << "started";
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
    //TODO: We should start `opam config env` on startup and get env vars from there.
    static const QString opamPath =
//            "/home/kakadu/.opam/4.04.0+fp+flambda/bin/";
            "/home/kakadu/.opam/4.02.2+multicore+moreplugins/bin/"
//            ""
            ;
    m_rubocop->processEnvironment().insert("MERLIN_LOG"," /tmp/merlin.qtcreator.log");
    setBusy(true);
    m_rubocop->start(opamPath + "ocamlmerlin", new_args);
    qDebug() << QString("starting (PID=%1)").arg(m_rubocop->pid()) << "with args" << qPrintable(m_rubocop->arguments().join(' '));
}

void RubocopHighlighter::finishRuboCopHighlight()
{
    Q_D(RubocopHighlighter);

    //TODO: check the revision
//    if (m_startRevision != d->document()->document()->revision()) {
//        d->setBusy(false);
//        qDebug() << "Document changed, skipping merlin info";
//        return;
//    }

    // https://github.com/ocaml/merlin/blob/master/doc/dev/PROTOCOL.md
    // for protocol details

    QJsonDocument jsonResponse = QJsonDocument::fromJson(d->outBuf().toUtf8());
    d->clearBuf();

//#if 0
//    qDebug() << "Queue size before poping: " << d->m_msgQueue.size();
//    for (auto it = d->m_msgQueue.begin(); it != d->m_msgQueue.end(); ++it) {
//        qDebug() << (*it)->expectedState();
//    }
//#endif
    auto lastRequest  = d->m_msgQueue.dequeue();
//#if 0
//    qDebug() << "lastRequest " << lastRequest->expectedState();
//    qDebug() << "Queue size after poping: " << d->m_msgQueue.size();
//#endif

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
//            qDebug() << "Got a result in a state codeSent";
            d->sendFSMevent("diagnosticsReceived");
//            qDebug() << "json is   " << root;
            d->parseDiagnosticsJson(root.value("value"),
                                    dynamic_cast<MerlinRequestErrors*>(lastRequest.data()) );
        } else if (d->fsm()->isActive("GoToDefSent")) {
            d->sendFSMevent("definitionsReceived");
            d->parseDefinitionsJson(root.value("value"),
                                    dynamic_cast<MerlinRequestGTD*>(lastRequest.data()) );
        } else if (d->fsm()->isActive("occurencesSent")) {
            d->sendFSMevent("occurencesReceived");
            d->parseOccurencesJson(root.value("value"),
                                   dynamic_cast<MerlinRequestUsages*>(lastRequest.data()));
        } else if (d->fsm()->isActive("completionsSent")) {
            d->sendFSMevent("completionsReceived");
            d->parseCompletionsJson(root.value("value"),
                                    dynamic_cast<MerlinRequestComplete*>(lastRequest.data()) );
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
    d->processingFinished();
}

void RubocopHighlighter::generalMsg(const QString &msg) const {
    Core::MessageManager::instance()->write(msg);
}

void jsonParseStartEnd(const QJsonObject& o, int& line1, int& col1, int& line2, int&col2) {
    const QJsonObject& start = o.value("start").toObject();
    const QJsonObject& end   = o.value("end").toObject();
    line1 = start.value("line").toInt();
    col1  = start.value("col").toInt();
    line2 = end.value("line").toInt();
    col2  = end.value("col").toInt();
}

}
