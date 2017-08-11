#ifndef Ruby_RubocopHighlighter_h
#define Ruby_RubocopHighlighter_h

#include <texteditor/semantichighlighter.h>

#include <utils/fileutils.h>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

QT_FORWARD_DECLARE_CLASS(QProcess)

namespace TextEditor { class TextDocument; class IAssistProcessor; class IAssistProposal; }

namespace OCamlCreator {

class Range {
public:
    uint startLine;
    uint startCol;
    uint endLine;
    uint endCol;
    uint pos;
    uint length;
    // TODO: make unsigned

    Range() : pos(0), length(0) { }
    Range(uint pos, uint length) : pos(pos), length(length) { }
    Range(uint l1, uint c1, uint l2, uint c2, uint p = 0, uint len = 0)
        : startLine(l1), startCol(c1), endLine(l2), endCol(c2)
        , pos(p), length(len)
    {}

    // Not really equal, since the length attribute is ignored.
    bool operator==(const Range &other) const {
        const uint value = other.pos;
        return value >= pos && value < (pos + length);
    }

    bool operator<(const Range &other) const {
        const uint value = other.pos;
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
    void performErrorsCheck(TextEditor::TextDocument*);

    using AsyncCompletionsAvailableHandler = std::function<void (TextEditor::IAssistProposal *proposal)>;
    ///
    /// \brief performCompletion
    /// \param doc is the document where text belongs
    /// \param prefix is the entered prefix we try to complete
    /// \param startPos
    /// \param line a line in a file (indexing starts from 0)
    /// \param pos
    /// \param handler
    ///
    void performCompletion(QTextDocument *doc, const QString& prefix, const int startPos, const int line, const int pos,
                           const AsyncCompletionsAvailableHandler &handler
                           );
    //TODO: doc may be unused

private:
    int m_startRevision;

    QElapsedTimer m_timer;

    void finishRuboCopHighlight();
    Offenses processRubocopOutput();

    int   lineColumnToPos(const int line, const int column);
    bool isReturnTrue(const QJsonArray& arr);

    void generalMsg(const QString& msg) const;
};
}

#endif
