#include "tools/MeasureTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/CoordinateResolver.h"
#include "document/Document.h"
#include "geometry/MeasurementObject.h"
#include "tools/Snap.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QPainter>

namespace cb {

namespace {
const QColor kMeasureColor(255, 213, 79);
}

MeasureTool::MeasureTool(ToolHost& host)
    : Tool(host)
{
}

void MeasureTool::reset()
{
    m_points.clear();
    m_pointer = -1;
    m_dragging = false;
    host().updateOverlayAll();
}

void MeasureTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1 || !host().page())
        return;
    m_pointer = e.pointerId;
    m_current = snapPoint(host(), e.pagePos, &m_currentSnapped);
    const MeasureKind kind = host().settings().measureKind();
    if (m_points.isEmpty() || kind == MeasureKind::Distance || kind == MeasureKind::Slope) {
        if (kind != MeasureKind::Area || m_points.isEmpty()) {
            m_points = {m_current};
            m_dragging = true;
        }
    }
    host().updateOverlayAll();
}

void MeasureTool::pointerMove(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_current = snapPoint(host(), e.pagePos, &m_currentSnapped);
    host().updateOverlayAll();
}

void MeasureTool::hover(const PointerEvent& e)
{
    if (m_points.isEmpty())
        return;
    m_current = snapPoint(host(), e.pagePos, &m_currentSnapped);
    host().updateOverlayAll();
}

void MeasureTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    m_current = snapPoint(host(), e.pagePos, &m_currentSnapped);
    const MeasureKind kind = host().settings().measureKind();
    const qreal minLen = host().viewToPageLength(host().theme().dp(10));
    switch (kind) {
    case MeasureKind::Distance:
    case MeasureKind::Slope:
        if (!m_points.isEmpty() && geom::distance(m_points.first(), m_current) >= minLen) {
            m_points = {m_points.first(), m_current};
            commit();
        } else {
            reset();
        }
        break;
    case MeasureKind::Angle:
        if (m_points.size() == 1) {
            if (geom::distance(m_points.first(), m_current) >= minLen)
                m_points.push_back(m_current); // vertex, first arm
            else
                reset();
        } else if (m_points.size() == 2) {
            if (geom::distance(m_points.first(), m_current) >= minLen) {
                // Stored as arm end, vertex, arm end.
                m_points = {m_points[1], m_points[0], m_current};
                commit();
            }
        }
        break;
    case MeasureKind::Area: {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool doubleTap = now - m_lastTapMs < 400;
        m_lastTapMs = now;
        if (m_points.size() >= 3
            && (geom::distance(m_current, m_points.first()) < host().viewToPageLength(host().theme().dp(22)) || doubleTap)) {
            commit();
        } else if (m_points.isEmpty() || geom::distance(m_points.last(), m_current) >= minLen) {
            m_points.push_back(m_current);
        }
        break;
    }
    }
    m_dragging = false;
    host().updateOverlayAll();
}

void MeasureTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId == m_pointer)
        reset();
}

void MeasureTool::commit()
{
    Page* page = host().page();
    const MeasureKind kind = host().settings().measureKind();
    if (page && m_points.size() >= 2) {
        std::vector<ObjectPtr> objects;
        objects.push_back(MeasurementObject::create(kind, m_points, kMeasureColor));
        host().document().commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects),
                                                                              QObject::tr("Measure")));
    }
    reset();
}

bool MeasureTool::keyPress(QKeyEvent* event)
{
    if (m_points.isEmpty())
        return false;
    if (event->key() == Qt::Key_Escape) {
        reset();
        return true;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && host().settings().measureKind() == MeasureKind::Area && m_points.size() >= 3) {
        commit();
        return true;
    }
    return false;
}

void MeasureTool::deactivate()
{
    if (host().settings().measureKind() == MeasureKind::Area && m_points.size() >= 3)
        commit();
    reset();
}

void MeasureTool::pageChanged()
{
    m_points.clear();
    m_pointer = -1;
}

QVector<QPointF> MeasureTool::previewPoints() const
{
    const MeasureKind kind = host().settings().measureKind();
    QVector<QPointF> pts = m_points;
    if (pts.isEmpty())
        return pts;
    switch (kind) {
    case MeasureKind::Distance:
    case MeasureKind::Slope:
        return {pts.first(), m_current};
    case MeasureKind::Angle:
        if (pts.size() == 1)
            return {m_current, pts[0]};
        return {pts[1], pts[0], m_current};
    case MeasureKind::Area:
        pts.push_back(m_current);
        return pts;
    }
    return pts;
}

void MeasureTool::paintOverlay(QPainter& painter) const
{
    const QVector<QPointF> pts = previewPoints();
    if (pts.size() < 2)
        return;
    const CoordinateSystem global = host().document().coordinatesFor(host().page());
    const CoordinateSystem* cs = resolveCoordinateSystem(host().page(), &global, pts);
    MeasurementObject::paintPreview(painter, host().settings().measureKind(), pts, kMeasureColor,
                                    cs ? *cs : global, host().view().zoom());
}

void MeasureTool::paintViewOverlay(QPainter& painter) const
{
    if (m_points.isEmpty() && m_pointer < 0)
        return;
    if (!m_currentSnapped)
        return;
    const QPointF v = host().view().pageToView(m_current);
    const qreal r = host().theme().dp(10);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(host().theme().color(ThemeColor::Accent), host().theme().dp(2)));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(v, r, r);
    painter.restore();
}

} // namespace cb
