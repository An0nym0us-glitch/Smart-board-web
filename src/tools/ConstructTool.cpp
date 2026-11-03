#include "tools/ConstructTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/CoordinateResolver.h"
#include "document/Document.h"
#include "geometry/GeometryObject.h"
#include "tools/Snap.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QPainter>

namespace cb {

namespace {
const QColor kConstructColor(129, 212, 250);
const QColor kVectorColor(255, 171, 64);

QColor colorFor(ConstructKind kind)
{
    return kind == ConstructKind::Vector ? kVectorColor : kConstructColor;
}
} // namespace

ConstructTool::ConstructTool(ToolHost& host)
    : Tool(host)
{
}

void ConstructTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1 || !host().page())
        return;
    m_pointer = e.pointerId;
    m_start = snapPoint(host(), e.pagePos, &m_snapped);
    m_current = m_start;
    host().updateOverlayAll();
}

void ConstructTool::pointerMove(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_current = snapPoint(host(), e.pagePos, &m_snapped);
    host().updateOverlayAll();
}

void ConstructTool::hover(const PointerEvent& e)
{
    m_current = snapPoint(host(), e.pagePos, &m_snapped);
    m_hovering = true;
    host().updateOverlayAll();
}

void ConstructTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    m_current = snapPoint(host(), e.pagePos, &m_snapped);
    Page* page = host().page();
    if (!page)
        return;
    const ConstructKind kind = host().settings().constructKind();
    const bool showLabel = host().settings().showCoordinates();
    std::unique_ptr<GeometryObject> obj;
    if (kind == ConstructKind::Point) {
        obj = GeometryObject::create(kind, {m_current}, colorFor(kind), showLabel);
        // Name points A, B, C ... like on a paper worksheet.
        obj->setName(QString(QChar('A' + (m_nameCounter++ % 26))));
    } else if (geom::distance(m_start, m_current) >= host().viewToPageLength(host().theme().dp(12))) {
        obj = GeometryObject::create(kind, {m_start, m_current}, colorFor(kind), showLabel);
    }
    if (obj) {
        std::vector<ObjectPtr> objects;
        objects.push_back(std::move(obj));
        host().document().commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects),
                                                                              QObject::tr("Construct")));
    }
    host().updateOverlayAll();
}

void ConstructTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId == m_pointer)
        m_pointer = -1;
    host().updateOverlayAll();
}

void ConstructTool::pageChanged()
{
    m_pointer = -1;
    m_nameCounter = 0;
}

void ConstructTool::paintOverlay(QPainter& painter) const
{
    if (m_pointer < 0)
        return;
    const ConstructKind kind = host().settings().constructKind();
    QVector<QPointF> pts = kind == ConstructKind::Point ? QVector<QPointF>{m_current} : QVector<QPointF>{m_start, m_current};
    const CoordinateSystem global = host().document().coordinatesFor(host().page());
    const CoordinateSystem* cs = resolveCoordinateSystem(host().page(), &global, pts);
    GeometryObject::paintPreview(painter, kind, pts, colorFor(kind), cs ? *cs : global,
                                 host().settings().showCoordinates());
}

void ConstructTool::paintViewOverlay(QPainter& painter) const
{
    if (!m_snapped || (m_pointer < 0 && !m_hovering))
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
