#include "storage/AutosaveManager.h"

#include "document/Document.h"
#include "storage/ProjectSerializer.h"
#include "core/Background.h"

#if defined(Q_OS_WASM)
#include "platform/WebPlatform.h"
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStandardPaths>
#include <QUuid>

namespace cb {

AutosaveManager::AutosaveManager(Document& doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(recoveryDirectory());
#if defined(Q_OS_WASM)
    // A browser tab cannot tell whether another tab is still running, so the web version uses no
    // lock files: copies left by earlier visits are offered for recovery (see findRecoverable()).
    activeSessionId() = m_sessionId;
#else
    m_lock = std::make_unique<QLockFile>(recoveryDirectory() + QLatin1Char('/') + m_sessionId + QStringLiteral(".lock"));
    m_lock->setStaleLockTime(0);
    m_lock->tryLock(100);
#endif
    m_timer.setInterval(60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &AutosaveManager::tick);
    // Once the lesson is saved normally the recovery copy is obsolete.
    connect(&m_doc, &Document::modifiedChanged, this, [this](bool modified) {
        if (!modified)
            discard();
    });
}

AutosaveManager::~AutosaveManager()
{
    m_timer.stop();
    if (m_lock)
        m_lock->unlock();
}

QString& AutosaveManager::activeSessionId()
{
    static QString id;
    return id;
}

QString AutosaveManager::recoveryDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/recovery");
}

QString AutosaveManager::dataPath() const
{
    return recoveryDirectory() + QLatin1Char('/') + m_sessionId + QStringLiteral(".classboard");
}

QString AutosaveManager::metaPath() const
{
    return recoveryDirectory() + QLatin1Char('/') + m_sessionId + QStringLiteral(".json");
}

void AutosaveManager::setIntervalSeconds(int seconds)
{
    m_timer.setInterval(std::max(10, seconds) * 1000);
}

void AutosaveManager::start()
{
    m_timer.start();
}

QByteArray AutosaveManager::metaJson() const
{
    QJsonObject meta;
    meta.insert(QStringLiteral("session"), m_sessionId);
    meta.insert(QStringLiteral("originalPath"), m_doc.filePath());
    meta.insert(QStringLiteral("title"), m_doc.displayName());
    meta.insert(QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    meta.insert(QStringLiteral("pages"), m_doc.pageCount());
    return QJsonDocument(meta).toJson(QJsonDocument::Compact);
}

void AutosaveManager::tick()
{
    if (m_busy || !m_doc.isModified() || m_doc.revision() == m_savedRevision)
        return;
    m_busy = true;
    const quint64 revision = m_doc.revision();
    auto contents = std::make_shared<DocumentContents>(m_doc.copyContents());
    const QString data = dataPath();
    const QString meta = metaPath();
    const QByteArray metaBytes = metaJson();
    auto result = std::make_shared<QString>();
    auto ok = std::make_shared<bool>(false);
    QPointer<AutosaveManager> self(this);
    runInBackground(this, [contents, data, meta, metaBytes, result, ok]() {
        QString error;
        *ok = ProjectSerializer::save(data, *contents, &error)
            && ProjectSerializer::writeFileAtomically(meta, metaBytes, &error);
        *result = error;
    }, [self, revision, result, ok]() {
        if (!self)
            return;
        self->m_busy = false;
        if (*ok) {
            self->m_savedRevision = revision;
            self->m_hasCopy = true;
#if defined(Q_OS_WASM)
            web::persist();
#endif
            emit self->saved(QDateTime::currentDateTime());
        } else {
            emit self->failed(*result);
        }
    }, true);
}

bool AutosaveManager::saveNow()
{
    QString error;
    const DocumentContents contents = m_doc.copyContents();
    const bool ok = ProjectSerializer::save(dataPath(), contents, &error)
        && ProjectSerializer::writeFileAtomically(metaPath(), metaJson(), &error);
    if (ok) {
        m_savedRevision = m_doc.revision();
        m_hasCopy = true;
#if defined(Q_OS_WASM)
        web::persist();
#endif
    } else {
        emit failed(error);
    }
    return ok;
}

void AutosaveManager::discard()
{
    const bool removedData = QFile::remove(dataPath());
    const bool removedMeta = QFile::remove(metaPath());
    const bool hadFiles = removedData || removedMeta;
    m_hasCopy = false;
    m_savedRevision = m_doc.revision();
#if defined(Q_OS_WASM)
    if (hadFiles)
        web::persist();
#else
    Q_UNUSED(hadFiles);
#endif
}

QVector<RecoveryInfo> AutosaveManager::findRecoverable()
{
    QVector<RecoveryInfo> result;
    const QDir dir(recoveryDirectory());
    const QStringList metas = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QString& metaName : metas) {
        const QString id = QFileInfo(metaName).completeBaseName();
#if defined(Q_OS_WASM)
        if (id == activeSessionId())
            continue;
#else
        // A lock we can acquire belongs to a session that is no longer running.
        QLockFile lock(dir.filePath(id + QStringLiteral(".lock")));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(0))
            continue;
        lock.unlock();
#endif
        QFile f(dir.filePath(metaName));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object();
        RecoveryInfo info;
        info.sessionId = id;
        info.dataPath = dir.filePath(id + QStringLiteral(".classboard"));
        if (!QFile::exists(info.dataPath))
            continue;
        info.originalPath = meta.value(QStringLiteral("originalPath")).toString();
        info.title = meta.value(QStringLiteral("title")).toString();
        info.savedAt = QDateTime::fromString(meta.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
        info.pageCount = meta.value(QStringLiteral("pages")).toInt();
        result.push_back(info);
    }
    return result;
}

bool AutosaveManager::restore(const RecoveryInfo& info, DocumentContents* out, QString* error)
{
    return ProjectSerializer::load(info.dataPath, out, error);
}

void AutosaveManager::discardRecovery(const RecoveryInfo& info)
{
    const QDir dir(recoveryDirectory());
    QFile::remove(info.dataPath);
    QFile::remove(dir.filePath(info.sessionId + QStringLiteral(".json")));
    QFile::remove(dir.filePath(info.sessionId + QStringLiteral(".lock")));
#if defined(Q_OS_WASM)
    web::persist();
#endif
}

} // namespace cb
