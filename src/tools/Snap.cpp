#include "tools/Snap.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/CoordinateResolver.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "tools/Tool.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <cmath>

namespace cb {

QPointF snapPoint(ToolHost& host, const QPointF& pagePos, bool* snapped, const DocumentObject* exclude)
{
    if (snapped)
        *snapped = false;
    Page* page = host.page();
    if (!page)
        return pagePos;
    const qreal tol = host.viewToPageLength(host.theme().dp(16));

    // 1. Existing construction points.
    qreal best = tol;
    QPointF result = pagePos;
    bool found = false;
    for (DocumentObject* o : page->objectsIntersecting(geom::inflated(QRectF(pagePos, QSizeF(0, 0)), tol * 2 + 80))) {
        if (o == exclude)
            continue;
        const bool construction = o->type() == ObjectType::Geometry || o->type() == ObjectType::Measurement
            || (o->type() == ObjectType::Shape && !o->controlPoints().isEmpty());
        if (!construction)
            continue;
        for (const QPointF& local : o->controlPoints()) {
            const QPointF p = o->mapToPage(local);
            const qreal d = geom::distance(p, pagePos);
            if (d <= best) {
                best = d;
                result = p;
                found = true;
            }
        }
    }
    if (found) {
        if (snapped)
            *snapped = true;
        return result;
    }

    // 2. Whole coordinates on grid-like backgrounds or inside graphs.
    if (!host.settings().snapToGrid())
        return pagePos;
    const CoordinateSystem pageGlobal = host.document().coordinatesFor(page);
    const CoordinateSystem* global = &pageGlobal;
    const CoordinateSystem* cs = resolveCoordinateSystem(page, global, {pagePos}, exclude);
    const TemplateKind kind = page->background().kind;
    const bool gridBackground = kind == TemplateKind::Grid || kind == TemplateKind::GraphPaper
        || kind == TemplateKind::CoordinatePlane || kind == TemplateKind::Dots;
    if (!cs || (cs == global && !gridBackground))
        return pagePos;
    const QPointF m = cs->toMath(pagePos);
    // Snap to integers, or to halves when zoomed in far enough to distinguish them.
    const qreal unitPx = geom::distance(cs->toPage(QPointF(0, 0)), cs->toPage(QPointF(1, 0))) * host.view().zoom();
    const double step = unitPx > 120 ? 0.5 : 1.0;
    const QPointF rounded(std::round(m.x() / step) * step, std::round(m.y() / step) * step);
    const QPointF p = cs->toPage(rounded);
    if (geom::distance(p, pagePos) <= tol) {
        if (snapped)
            *snapped = true;
        return p;
    }
    return pagePos;
}

} // namespace cb
