#include "canvas/ThumbnailCache.h"

#include "canvas/PageRenderer.h"
#include "document/Document.h"

namespace cb {

ThumbnailCache::ThumbnailCache(Document& doc, const QSize& size, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
    , m_size(size)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &ThumbnailCache::processQueue);
    connect(&m_doc, &Document::documentReset, this, [this]() {
        m_entries.clear();
        m_queue.clear();
    });
}

QImage ThumbnailCache::thumbnail(const PageId& id)
{
    const Page* page = m_doc.pageById(id);
    if (!page)
        return QImage();
    auto it = m_entries.find(id);
    const bool stale = it == m_entries.end() || it->revision != page->revision() || it->image.isNull();
    if (stale && !m_queue.contains(id)) {
        m_queue.push_back(id);
        m_timer.start();
    }
    return it == m_entries.end() ? QImage() : it->image;
}

void ThumbnailCache::processQueue()
{
    if (m_queue.isEmpty())
        return;
    const PageId id = m_queue.takeFirst();
    if (const Page* page = m_doc.pageById(id)) {
        Entry e;
        e.revision = page->revision();
        // Keep the page's aspect ratio (A4 pages are not squeezed into a 16:9 box).
        QSize size = page->frameRect().size().toSize();
        size.scale(m_size, Qt::KeepAspectRatio);
        e.image = PageRenderer::renderToImage(*page, page->frameRect(), size.expandedTo(QSize(1, 1)), m_doc.images(),
                                              m_doc.coordinates());
        m_entries.insert(id, e);
        emit thumbnailReady(id);
    }
    if (!m_queue.isEmpty())
        m_timer.start();
}

} // namespace cb
