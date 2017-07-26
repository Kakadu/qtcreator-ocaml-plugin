#ifndef Ruby_RubocopHighlighter_h
#define Ruby_RubocopHighlighter_h

#include <texteditor/semantichighlighter.h>

#include <utils/fileutils.h>

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QTemporaryFile>

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
    operator QString() const {
        return QString("from {%1;%2} to {%3;%4} (pos=%5, len=%6)")
                .arg(startLine)
                .arg(startCol)
                .arg(endLine)
                .arg(endCol)
                .arg(pos)
                .arg(length);
    }
};

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

class RubocopHighlighterPrivate;

class RubocopHighlighter : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(RubocopHighlighter)
    RubocopHighlighterPrivate *d_ptr;
public:
    RubocopHighlighter();
    ~RubocopHighlighter();

    static RubocopHighlighter *instance();

    bool run(TextEditor::TextDocument *document, const QString &fileNameTip);
    QString diagnosticAt(const Utils::FileName &file, int pos);
    void performGoToDefinition(TextEditor::TextDocument *document, const int line, const int column);
    void performFindUsages(TextEditor::TextDocument *document, const int line, const int column);
    void performErrorsCheck(const QByteArray&);

private:
    int m_startRevision;

    QElapsedTimer m_timer;

    void finishRuboCopHighlight();
    Offenses processRubocopOutput();

    Range lineColumnLengthToRange(int line, int column, int length);
    int   lineColumnToPos(const int line, const int column);
    bool isReturnTrue(const QJsonArray& arr);

    void generalMsg(const QString& msg) const;
};
}

#endif
