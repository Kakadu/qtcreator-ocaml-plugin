#ifndef Ruby_RubocopHighlighter_h
#define Ruby_RubocopHighlighter_h

#include <texteditor/semantichighlighter.h>

#include <utils/fileutils.h>

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QTemporaryFile>

#include <QtScxml/QScxmlStateMachine>
#include "MerlinFSM.h"

#include <functional>

QT_FORWARD_DECLARE_CLASS(QProcess)

namespace TextEditor { class TextDocument; }

namespace OCamlCreator {

class Range {
public:
    int startLine;
    int startCol;
    int endLine;
    int endCol;
    int pos;
    int length;

    Range() : pos(0), length(0) { }
    Range(int pos, int length) : pos(pos), length(length) { }
    Range(int l1, int c1, int l2, int c2, int p = 0, int len = 0)
        : startLine(l1), startCol(c1), endLine(l2), endCol(c2)
        , pos(p), length(len)
    {}

    // Not really equal, since the length attribute is ignored.
    bool operator==(const Range &other) const {
        const int value = other.pos;
        return value >= pos && value < (pos + length);
    }

    bool operator<(const Range &other) const {
        const int value = other.pos;
        return pos < value && (pos + length) < value;
    }
    inline QString toString() const {
        return QString("from {%1;%2} to {%3;%4} (pos=%5, len=%6)")
                .arg(startLine)
                .arg(startCol)
                .arg(endLine)
                .arg(endCol)
                .arg(pos)
                .arg(length);
    }
};

QDebug operator<< (QDebug &d, const Range &r);

typedef TextEditor::HighlightingResult Offense;
typedef QVector<TextEditor::HighlightingResult> Offenses;

struct Diagnostics {
    Diagnostics() : m_isValid(false) {}
    QMap<Range, QString> messages;
    operator int() const {
        return m_isValid;
    }
    void setValid(bool b) { m_isValid = b; }
    void setValid() { setValid(true); }
    void setInvalid() { m_isValid = false; }
private:
    bool m_isValid;
};

class RubocopHighlighter : public QObject {
    Q_OBJECT
public:
    RubocopHighlighter();
    ~RubocopHighlighter();

    static RubocopHighlighter *instance();

    bool run(TextEditor::TextDocument *document, const QString &fileNameTip);
    QString diagnosticAt(const Utils::FileName &file, int pos);
private:
    Diagnostics processMerlinErrors(const QJsonValue& v);
    void makeMerlinAnalyzeBuffer(const QByteArray&);
    inline void sendFSMevent(const QString&);
    void makeMerlinAskDiagnostics();
    bool m_rubocopFound;
    bool m_busy;
    QProcess *m_rubocop;// m_rubocop - указатель на открываемый процесс
    QTemporaryFile m_rubocopScript;//временный файл
    QString m_outputBuffer;

    int m_startRevision;
    TextEditor::TextDocument *m_document;
    QHash<int, QTextCharFormat> m_extraFormats;

    MerlinFSM m_chart;
    QHash<Utils::FileName, Diagnostics> m_diagnostics;

    QElapsedTimer m_timer;
    void parseDiagnosticsJson(const QJsonValue& resp);

    void initRubocopProcess(const QStringList &args);
    void finishRuboCopHighlight();
    Offenses processRubocopOutput();

    Range lineColumnLengthToRange(int line, int column, int length);
    int   lineColumnToPos(const int line, const int column);
    bool isReturnTrue(const QJsonArray& arr);
};
}

#endif
