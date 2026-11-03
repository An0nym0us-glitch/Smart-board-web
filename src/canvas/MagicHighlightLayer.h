#pragma once

#include "core/Id.h"
#include "document/StrokeObject.h"

#include <QElapsedTimer>
#include <QObject>
#include <QRectF>
#include <QTimer>

#include <vector>

class QPainter;

namespace cb {

// Timing of the Magic Highlighter; the only place these durations are defined.
/// How long a finished magic stroke stays fully visible.
constexpr int MAGIC_HIGHLIGHTER_DURATION_MS = 5000;
/// How long it then takes to fade out completely.
constexpr int MAGIC_HIGHLIGHTER_FADE_MS = 1000;
/// Repaint interval while a highlight fades (about 30 frames per second).
constexpr int MAGIC_HIGHLIGHTER_FRAME_MS = 33;

/// Temporary effect layer of the canvas for the Magic Highlighter.
///
/// Highlights live only here, in memory: they are not document objects, are never serialised,
/// exported or counted, and never touch the undo stack. The canvas paints the layer on top of
/// the page; each highlight stays for MAGIC_HIGHLIGHTER_DURATION_MS after it was finished, fades
/// out over MAGIC_HIGHLIGHTER_FADE_MS and is then dropped. The timer only runs while
/// highlights exist.
class MagicHighlightLayer : public QObject
{
    Q_OBJECT
public:
    explicit MagicHighlightLayer(QObject* parent = nullptr);

    /// Adds a finished stroke (page coordinates); its lifetime starts now.
    void add(const PageId& page, const QVector<StrokePoint>& points, const InkStyle& ink);
    /// Drops every highlight at once (page change, new lesson).
    void clear();
    int count() const { return static_cast<int>(m_items.size()); }
    int countOnPage(const PageId& page) const;

    /// Paints the highlights of a page with the painter in page coordinates.
    void paint(QPainter& painter, const PageId& page) const;
    /// The magic look, shared by the live stroke (opacity 1) and the finished highlight.
    static void paintStroke(QPainter& painter, const QVector<StrokePoint>& points, const InkStyle& ink, qreal opacity);
    /// Page rect covered by a stroke, including its glow.
    static QRectF strokeBounds(const QVector<StrokePoint>& points, const InkStyle& ink);

    /// Opacity of a highlight finished ageMs ago: 1 while visible, then a smooth fade to 0.
    static qreal opacityAt(qint64 ageMs, int visibleMs = MAGIC_HIGHLIGHTER_DURATION_MS,
                           int fadeMs = MAGIC_HIGHLIGHTER_FADE_MS);
    /// Overrides the durations (tests only; the application always uses the constants).
    void setDurations(int visibleMs, int fadeMs);

signals:
    /// A page rect whose appearance changed (highlight added, fading or removed).
    void repaintRequested(const QUuid& pageId, const QRectF& pageRect);

private:
    struct Highlight
    {
        PageId page;
        QVector<StrokePoint> points;
        InkStyle ink;
        QRectF bounds;
        qint64 finishedAt = 0;
    };

    void tick();
    void scheduleNext();
    qint64 age(const Highlight& h) const { return m_clock.elapsed() - h.finishedAt; }

    std::vector<Highlight> m_items;
    QElapsedTimer m_clock;
    QTimer m_timer;
    int m_visibleMs = MAGIC_HIGHLIGHTER_DURATION_MS;
    int m_fadeMs = MAGIC_HIGHLIGHTER_FADE_MS;
};

} // namespace cb
