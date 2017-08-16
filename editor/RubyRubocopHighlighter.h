#ifndef Ruby_RubocopHighlighter_h
#define Ruby_RubocopHighlighter_h

#include <texteditor/semantichighlighter.h>
#include <texteditor/quickfix.h> // can't use forward declarations because of typedef's and inner classes

#include <utils/fileutils.h>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

QT_FORWARD_DECLARE_CLASS(QProcess)

namespace TextEditor {
    class TextDocument; class IAssistProcessor; class IAssistProposal;
    class AssistInterface;
}

namespace OCamlCreator {

class Range {
public:
    int startLine;
    int startCol;
    int endLine;
    int endCol;
    int pos;
    int length;
    // TODO: make unsigned

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

struct MerlinQuickFix {
    int line1;
    int col1;
    int line2;
    int col2;
    int startPos;
    int endPos;
    QStringList new_values;

    MerlinQuickFix(int _line, int _col, const QStringList& _ss, const QString& _old)
        : line1(_line), col1(_col), startPos(), endPos()
        , new_values(_ss)
    {
    }
    MerlinQuickFix() {}
    MerlinQuickFix(const MerlinQuickFix& );

    operator QString() const {
        return
          QString("from %1:%2 to %3:%4").arg(line1).arg(col1).arg(line2).arg(col2) +
          QString(" new vals: %1").arg(new_values.join(" "));
    }

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
    void performErrorsCheck(TextEditor::TextDocument*);

    using AsyncCompletionsAvailableHandler = std::function<void (TextEditor::IAssistProposal *)>;
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

    void enumerateQuickFixes(const TextEditor::QuickFixInterface &iface,
                             const std::function<void(const MerlinQuickFix&)> &hook);
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
