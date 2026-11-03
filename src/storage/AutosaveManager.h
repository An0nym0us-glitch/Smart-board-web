#pragma once

#include <QDateTime>
#include <QLockFile>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <memory>

namespace cb {

class Document;
struct DocumentContents;

/// Information about an unsaved session left behind by a crash or power loss.
struct RecoveryInfo
{
    QString sessionId;
    QString dataPath;
    QString originalPath;
    QString title;
    QDateTime savedAt;
    int pageCount = 0;
};

/// Periodically writes a recovery copy of the lesson while it has unsaved changes.
///
/// * Serialisation snapshots the document on the GUI thread (cheap copy) and writes on a worker
///   thread, so autosave never freezes the board.
/// * Writes are atomic (temporary file + rename): a crash during autosave cannot corrupt the
///   previous recovery copy.
/// * Each running session holds a lock file; recovery files of sessions whose lock is stale
///   (process gone) are offered for restoration on the next start.
class AutosaveManager : public QObject
{
    Q_OBJECT
public:
    explicit AutosaveManager(Document& doc, QObject* parent = nullptr);
    ~AutosaveManager() override;

    void setIntervalSeconds(int seconds);
    void start();
    /// Writes a recovery copy immediately (blocking; used before risky operations and in tests).
    bool saveNow();
    /// Removes this session's recovery copy (after a successful save or a clean exit).
    void discard();

    QString sessionId() const { return m_sessionId; }
    static QString recoveryDirectory();

    /// Recovery copies of other (crashed) sessions.
    static QVector<RecoveryInfo> findRecoverable();
    static bool restore(const RecoveryInfo& info, DocumentContents* out, QString* error);
    static void discardRecovery(const RecoveryInfo& info);

signals:
    void saved(const QDateTime& when);
    void failed(const QString& error);

private:
    void tick();
    QString dataPath() const;
    QString metaPath() const;
    QByteArray metaJson() const;

    Document& m_doc;
    QString m_sessionId;
    std::unique_ptr<QLockFile> m_lock;
    QTimer m_timer;
    quint64 m_savedRevision = 0;
    bool m_busy = false;
    bool m_hasCopy = false;
};

} // namespace cb
