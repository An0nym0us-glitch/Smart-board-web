#include "tools/PenTool.h"

#include "ai/ShapeRecognizer.h"
#include "canvas/MagicHighlightLayer.h"
#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "geometry/InstrumentLayer.h"
#include "geometry/Label.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace cb {

PenTool::PenTool(ToolHost& host)
    : Tool(host)
{
}

QCursor PenTool::cursor() const
{
    // Small dot cursor in the current colour, for mouse users.
    const qreal dpr = 2.0;
    const int size = 16;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 0, 0, 160), 1.2));
    p.setBrush(host().settings().penColor());
    p.drawEllipse(QPointF(size / 2.0, size / 2.0), 3.5, 3.5);
    p.end();
    return QCursor(pm, size / 2, size / 2);
}

void PenTool::pointerDown(const PointerEvent& e)
{
    if (!host().page())
        return;
    LiveStroke s;
    s.ink = host().settings().ink();
    // Pressure only when the device reports it; mouse and touch give uniform ink.
    s.ink.pressure = s.ink.pressure && e.hasPressure;
    s.magic = host().settings().magicHighlighter();
    s.start = e.pagePos;
    const qreal tol = host().viewToPageLength(host().theme().dp(26));
    host().instruments().edgeConstraint(e.pagePos, tol, &s.constraint);
    const QPointF p = s.constraint.isValid() ? s.constraint.project(e.pagePos) : e.pagePos;
    s.start = p;
    s.smoothed = p;
    StrokePoint sp;
    sp.pos = p;
    sp.pressure = static_cast<float>(e.hasPressure ? std::max(0.05, e.pressure) : 1.0);
    s.points.push_back(sp);
    m_live.insert(e.pointerId, s);
    host().updateOverlay(geom::inflated(QRectF(p, QSizeF(1, 1)), s.ink.width + 2));
}

void PenTool::addPoint(LiveStroke& s, const QPointF& raw, qreal pressure, bool force)
{
    QPointF p = raw;
    if (s.constraint.isValid()) {
        p = s.constraint.project(raw);
    } else {
        const qreal smoothing = host().settings().smoothing();
        const qreal alpha = 1.0 - smoothing * 0.72;
        s.smoothed += (raw - s.smoothed) * alpha;
        p = force ? raw : s.smoothed;
    }
    const qreal minDist = host().viewToPageLength(0.9);
    if (!force && !s.points.isEmpty() && geom::distance(s.points.last().pos, p) < minDist)
        return;
    StrokePoint sp;
    sp.pos = p;
    sp.pressure = static_cast<float>(pressure);
    if (s.constraint.isValid() && s.constraint.kind == EdgeConstraint::Kind::Line && s.points.size() >= 2) {
        // A straight line along a ruler only needs its end points.
        s.points.last() = sp;
        return;
    }
    s.points.push_back(sp);
}

QRectF PenTool::strokeRect(const LiveStroke& s, int fromIndex) const
{
    QRectF r;
    for (int i = std::max(0, fromIndex); i < s.points.size(); ++i) {
        const QRectF pr(s.points[i].pos, QSizeF(0.01, 0.01));
        r = r.isNull() ? pr : r.united(pr);
    }
    return geom::inflated(r, s.ink.width + host().viewToPageLength(60));
}

void PenTool::pointerMove(const PointerEvent& e)
{
    auto it = m_live.find(e.pointerId);
    if (it == m_live.end())
        return;
    const int before = std::max(0, it->points.size() - 2);
    const qreal pressure = e.hasPressure ? std::max(0.05, e.pressure) : 1.0;
    addPoint(*it, e.pagePos, pressure, false);
    if (it->constraint.isValid())
        host().updateOverlay(strokeRect(*it, 0).united(geom::inflated(QRectF(e.pagePos, QSizeF(1, 1)), 80)));
    else
        host().updateOverlay(strokeRect(*it, before));
}

void PenTool::pointerUp(const PointerEvent& e)
{
    auto it = m_live.find(e.pointerId);
    if (it == m_live.end())
        return;
    const qreal pressure = e.hasPressure ? std::max(0.05, e.pressure) : 1.0;
    if (geom::distance(it->points.last().pos, e.pagePos) > host().viewToPageLength(0.5))
        addPoint(*it, e.pagePos, pressure, true);
    finish(e.pointerId);
}

void PenTool::pointerCancel(const PointerEvent& e)
{
    auto it = m_live.find(e.pointerId);
    if (it == m_live.end())
        return;
    const QRectF r = strokeRect(*it, 0);
    m_live.erase(it);
    host().updateOverlay(r);
}

void PenTool::finish(int pointerId)
{
    LiveStroke s = m_live.take(pointerId);
    const QRectF dirty = strokeRect(s, 0);
    Page* page = host().page();
    if (!page || s.points.isEmpty()) {
        host().updateOverlay(dirty);
        return;
    }

    // Light simplification keeps files small without visible change.
    if (s.points.size() > 3 && !s.constraint.isValid()) {
        QVector<QPointF> pts;
        pts.reserve(s.points.size());
        for (const StrokePoint& sp : s.points)
            pts.push_back(sp.pos);
        const qreal eps = host().viewToPageLength(s.ink.pressure ? 0.15 : 0.3);
        const QVector<int> keep = geom::simplifyIndices(pts, eps);
        QVector<StrokePoint> simplified;
        simplified.reserve(keep.size());
        for (int i : keep)
            simplified.push_back(s.points[i]);
        s.points = simplified;
    }

    if (s.magic) {
        // Temporary highlight: shown by the effect layer, never a document object or undo step.
        host().magicHighlights().add(page->id(), s.points, s.ink);
        host().updateOverlay(dirty);
        return;
    }

    std::vector<ObjectPtr> objects;
    QString text = QObject::tr("Draw");
    if (host().settings().shapeRecognition() && !s.constraint.isValid() && s.points.size() >= 4) {
        QVector<QPointF> pts;
        for (const StrokePoint& sp : s.points)
            pts.push_back(sp.pos);
        ShapeRecognizer recognizer;
        if (ObjectPtr shape = recognizer.recognize(pts, s.ink, host().viewToPageLength(1.0))) {
            objects.push_back(std::move(shape));
            text = QObject::tr("Draw shape");
        }
    }
    if (objects.empty())
        objects.push_back(StrokeObject::fromPagePoints(s.points, s.ink));
    host().document().commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), text));
    host().updateOverlay(dirty);
}

void PenTool::deactivate()
{
    const QList<int> ids = m_live.keys();
    for (int id : ids)
        finish(id);
}

void PenTool::pageChanged()
{
    m_live.clear();
}

void PenTool::paintOverlay(QPainter& painter) const
{
    for (const LiveStroke& s : m_live) {
        if (s.magic)
            MagicHighlightLayer::paintStroke(painter, s.points, s.ink, 1.0);
        else
            StrokeObject::paintPoints(painter, s.points, s.ink);
    }
}

void PenTool::paintViewOverlay(QPainter& painter) const
{
    // Length read-out while drawing along a straight edge.
    for (const LiveStroke& s : m_live) {
        if (s.constraint.kind != EdgeConstraint::Kind::Line || s.points.size() < 2)
            continue;
        const CoordinateSystem cs = host().document().coordinatesFor(host().page());
        const qreal len = cs.mathDistance(s.points.first().pos, s.points.last().pos);
        const QPointF anchor = host().view().pageToView(s.points.last().pos) + QPointF(0, -host().theme().dp(40));
        QString text = geom::formatNumber(len, 1) + QLatin1Char(' ') + cs.unitLabel();
        if (cs.usesScale())
            text += QStringLiteral(" = ") + cs.formatLength(len);
        paintValueLabel(painter, anchor, text, QColor(255, 224, 130), 0.0, host().theme().dp(18));
    }
}

} // namespace cb
