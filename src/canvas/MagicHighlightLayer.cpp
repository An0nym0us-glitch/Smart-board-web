#include "canvas/MagicHighlightLayer.h"

#include "core/Geometry.h"

#include <QPainter>

#include <algorithm>

namespace cb {

MagicHighlightLayer::MagicHighlightLayer(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &MagicHighlightLayer::tick);
}

void MagicHighlightLayer::add(const PageId& page, const QVector<StrokePoint>& points, const InkStyle& ink)
{
    if (points.isEmpty())
        return;
    Highlight h;
    h.page = page;
    h.points = points;
    h.ink = ink;
    h.bounds = strokeBounds(points, ink);
    h.finishedAt = m_clock.elapsed();
    m_items.push_back(std::move(h));
    emit repaintRequested(page, m_items.back().bounds);
    scheduleNext();
}

void MagicHighlightLayer::clear()
{
    std::vector<Highlight> items;
    items.swap(m_items);
    m_timer.stop();
    for (const Highlight& h : items)
        emit repaintRequested(h.page, h.bounds);
}

int MagicHighlightLayer::countOnPage(const PageId& page) const
{
    return static_cast<int>(std::count_if(m_items.begin(), m_items.end(), [&](const Highlight& h) { return h.page == page; }));
}

void MagicHighlightLayer::setDurations(int visibleMs, int fadeMs)
{
    m_visibleMs = std::max(0, visibleMs);
    m_fadeMs = std::max(1, fadeMs);
    scheduleNext();
}

qreal MagicHighlightLayer::opacityAt(qint64 ageMs, int visibleMs, int fadeMs)
{
    if (ageMs <= visibleMs)
        return 1.0;
    const qreal t = std::clamp(qreal(ageMs - visibleMs) / std::max(1, fadeMs), 0.0, 1.0);
    // Smoothstep: the fade starts and ends gently.
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

void MagicHighlightLayer::tick()
{
    // Drop what has faded out, repaint what is fading.
    auto it = m_items.begin();
    while (it != m_items.end()) {
        const qint64 a = age(*it);
        if (a >= m_visibleMs + m_fadeMs) {
            const PageId page = it->page;
            const QRectF bounds = it->bounds;
            it = m_items.erase(it);
            emit repaintRequested(page, bounds);
            continue;
        }
        if (a > m_visibleMs)
            emit repaintRequested(it->page, it->bounds);
        ++it;
    }
    scheduleNext();
}

void MagicHighlightLayer::scheduleNext()
{
    if (m_items.empty()) {
        m_timer.stop();
        return;
    }
    // Sleep until the next highlight starts fading; animate at frame rate while one fades.
    qint64 wait = m_visibleMs + m_fadeMs;
    for (const Highlight& h : m_items) {
        const qint64 a = age(h);
        if (a >= m_visibleMs) {
            wait = MAGIC_HIGHLIGHTER_FRAME_MS;
            break;
        }
        wait = std::min(wait, m_visibleMs - a + 1);
    }
    m_timer.start(static_cast<int>(std::max<qint64>(1, wait)));
}

void MagicHighlightLayer::paint(QPainter& painter, const PageId& page) const
{
    for (const Highlight& h : m_items) {
        if (h.page != page)
            continue;
        const qreal opacity = opacityAt(age(h), m_visibleMs, m_fadeMs);
        if (opacity > 0.0)
            paintStroke(painter, h.points, h.ink, opacity);
    }
}

QRectF MagicHighlightLayer::strokeBounds(const QVector<StrokePoint>& points, const InkStyle& ink)
{
    QRectF r;
    for (const StrokePoint& p : points) {
        const QRectF pr(p.pos, QSizeF(0.01, 0.01));
        r = r.isNull() ? pr : r.united(pr);
    }
    return geom::inflated(r, ink.width * 1.0 + 4.0);
}

void MagicHighlightLayer::paintStroke(QPainter& painter, const QVector<StrokePoint>& points, const InkStyle& ink, qreal opacity)
{
    if (points.isEmpty() || opacity <= 0.0)
        return;
    painter.save();
    painter.setOpacity(painter.opacity() * opacity);
    // Soft glow around the band ...
    InkStyle glow = ink;
    glow.style = StrokeStyle::Pen;
    glow.pressure = false;
    glow.width = ink.width * 1.6;
    glow.color.setAlphaF(0.16);
    StrokeObject::paintPoints(painter, points, glow);
    // ... the translucent highlighter band itself ...
    InkStyle band = ink;
    band.style = StrokeStyle::Highlighter;
    band.pressure = false;
    StrokeObject::paintPoints(painter, points, band);
    // ... and a thin bright core that makes it read as "light", not as ink.
    InkStyle core = band;
    core.style = StrokeStyle::Pen;
    core.width = std::max(1.0, ink.width * 0.16);
    core.color = QColor(255, 255, 255, 150);
    StrokeObject::paintPoints(painter, points, core);
    painter.restore();
}

} // namespace cb
