#pragma once

#include "core/Id.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QTimer>

namespace cb {

class Document;

/// Small page previews for the page navigator. Pages are re-rendered lazily (one per event loop
/// turn) only when their revision changes, so the GUI never blocks on many pages.
class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    ThumbnailCache(Document& doc, const QSize& size, QObject* parent = nullptr);

    /// Returns the cached thumbnail (possibly stale or null) and schedules a refresh if needed.
    QImage thumbnail(const PageId& page);
    QSize size() const { return m_size; }

signals:
    void thumbnailReady(const QUuid& pageId);

private:
    void processQueue();

    struct Entry
    {
        QImage image;
        quint64 revision = 0;
    };
    Document& m_doc;
    QSize m_size;
    QHash<PageId, Entry> m_entries;
    QList<PageId> m_queue;
    QTimer m_timer;
};

} // namespace cb
