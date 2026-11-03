#include "tools/SelectTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "tools/SelectionModel.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QWidget>

#include <algorithm>

namespace cb {

SelectTool::SelectTool(ToolHost& host)
    : Tool(host)
{
}

std::vector<DocumentObject*> SelectTool::selectedObjects() const
{
    std::vector<DocumentObject*> out;
    Page* page = host().page();
    if (!page)
        return out;
    for (const ObjectId& id : host().selection().ids())
        if (DocumentObject* o = page->object(id))
            out.push_back(o);
    return out;
}

QRectF SelectTool::selectionBounds() const
{
    QRectF r;
    for (DocumentObject* o : selectedObjects())
        r = r.isNull() ? o->sceneBounds() : r.united(o->sceneBounds());
    return r;
}

SelectTool::Frame SelectTool::frame() const
{
    Frame f;
    const auto objs = selectedObjects();
    if (objs.empty())
        return f;
    const ViewTransform& view = host().view();
    const Theme& theme = host().theme();
    f.valid = true;
    if (objs.size() == 1) {
        DocumentObject* o = objs.front();
        f.single = true;
        QRectF local = o->localBounds();
        const qreal minExtent = host().viewToPageLength(theme.dp(6));
        if (local.width() < minExtent)
            local.adjust(-minExtent / 2, 0, minExtent / 2, 0);
        if (local.height() < minExtent)
            local.adjust(0, -minExtent / 2, 0, minExtent / 2);
        const QTransform t = o->transform() * view.toTransform();
        f.viewCorners << t.map(local.topLeft()) << t.map(local.topRight()) << t.map(local.bottomRight())
                      << t.map(local.bottomLeft());
        f.canResize = o->canResize();
        f.canRotate = o->canRotate();
        f.keepAspect = o->keepAspectRatio();
        for (const QPointF& cp : o->controlPoints())
            f.controlPoints << t.map(cp);
    } else {
        const QRectF v = view.pageToView(selectionBounds());
        f.viewCorners << v.topLeft() << v.topRight() << v.bottomRight() << v.bottomLeft();
        f.canResize = true;
        f.keepAspect = true;
        f.canRotate = std::any_of(objs.begin(), objs.end(), [](DocumentObject* o) { return o->canRotate(); });
    }
    const QPointF topCenter = geom::midpoint(f.viewCorners[0], f.viewCorners[1]);
    const QPointF center = geom::midpoint(f.viewCorners[0], f.viewCorners[2]);
    QPointF up = geom::normalized(topCenter - center);
    if (geom::length(up) < 0.5)
        up = QPointF(0, -1);
    f.rotateHandle = topCenter + up * theme.dp(40);
    return f;
}

QVector<QPointF> SelectTool::resizeHandles(const Frame& f) const
{
    const QPolygonF& c = f.viewCorners;
    return {c[0], geom::midpoint(c[0], c[1]), c[1], geom::midpoint(c[1], c[2]),
            c[2], geom::midpoint(c[2], c[3]), c[3], geom::midpoint(c[3], c[0])};
}

int SelectTool::hitResizeHandle(const Frame& f, const QPointF& view) const
{
    if (!f.valid || !f.canResize)
        return -1;
    const qreal hit = host().theme().scaled(ThemeMetric::HandleHitSize) / 2;
    const QVector<QPointF> handles = resizeHandles(f);
    int best = -1;
    qreal bestDist = hit;
    for (int i = 0; i < handles.size(); ++i) {
        if ((f.keepAspect || !f.single) && (i % 2 == 1))
            continue;
        const qreal d = geom::distance(view, handles[i]);
        if (d <= bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void SelectTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1 || !host().page())
        return;
    m_pointer = e.pointerId;
    m_pressPage = e.pagePos;
    m_pressView = e.viewPos;
    m_pressedObject = ObjectId();
    m_pressedWasSelected = false;
    m_mode = Mode::None;

    const Theme& theme = host().theme();
    const qreal hit = theme.scaled(ThemeMetric::HandleHitSize) / 2;
    const Frame f = frame();
    if (f.valid) {
        if (f.canRotate && geom::distance(e.viewPos, f.rotateHandle) <= hit) {
            startDrag(Mode::Rotate, e);
            return;
        }
        for (int i = 0; i < f.controlPoints.size(); ++i) {
            if (geom::distance(e.viewPos, f.controlPoints[i]) <= hit) {
                m_handle = i;
                startDrag(Mode::ControlPoint, e);
                return;
            }
        }
        const int h = hitResizeHandle(f, e.viewPos);
        // On small objects the handles crowd the middle: a press inside the frame that is closer to
        // the centre than to the handle moves the object instead of resizing it.
        const QPointF centre = geom::midpoint(f.viewCorners[0], f.viewCorners[2]);
        const bool preferMove = h >= 0 && f.viewCorners.containsPoint(e.viewPos, Qt::OddEvenFill)
            && geom::distance(e.viewPos, centre) < geom::distance(e.viewPos, resizeHandles(f)[h]);
        if (h >= 0 && !preferMove) {
            m_handle = h;
            startDrag(Mode::Resize, e);
            return;
        }
    }

    const bool additive = (e.modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) || host().settings().multiSelect();
    DocumentObject* obj = host().page()->topmostAt(e.pagePos, host().viewToPageLength(theme.dp(8)));
    if (!obj)
        obj = host().page()->topmostEnclosing(e.pagePos); // inside an unfilled rectangle, circle ...
    if (obj) {
        m_pressedObject = obj->id();
        m_pressedWasSelected = host().selection().contains(obj->id());
        if (!m_pressedWasSelected) {
            if (additive)
                host().selection().add(obj->id());
            else
                host().selection().setSingle(obj->id());
        }
        m_mode = Mode::Pending;
        return;
    }
    if (f.valid && f.viewCorners.containsPoint(e.viewPos, Qt::OddEvenFill)) {
        m_mode = Mode::Pending;
        return;
    }
    if (!additive)
        host().selection().clear();
    m_mode = host().settings().selectMode() == SelectMode::Lasso ? Mode::Lasso : Mode::RubberBand;
    m_band = QRectF(e.pagePos, e.pagePos);
    m_lasso.clear();
    m_lasso << e.pagePos;
}

void SelectTool::startDrag(Mode mode, const PointerEvent& e)
{
    m_mode = mode;
    m_originals.clear();
    for (DocumentObject* o : selectedObjects())
        m_originals.push_back({o->id(), o->clone()});
    m_startBounds = selectionBounds();
    m_pivot = m_startBounds.center();
    if (m_originals.size() == 1) {
        const DocumentObject* o = m_originals.front().object.get();
        m_pivot = o->mapToPage(o->localBounds().center());
    }
    m_startAngle = geom::angleDeg(e.pagePos - m_pivot);
}

void SelectTool::pointerMove(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    switch (m_mode) {
    case Mode::Pending:
        if (geom::distance(e.viewPos, m_pressView) > host().theme().dp(6)) {
            startDrag(Mode::Move, e);
            updateMove(e.pagePos);
        }
        break;
    case Mode::Move:
        updateMove(e.pagePos);
        break;
    case Mode::Resize:
        updateResize(e.pagePos, e.modifiers);
        break;
    case Mode::Rotate:
        updateRotate(e.pagePos);
        break;
    case Mode::ControlPoint:
        updateControlPoint(e.pagePos);
        break;
    case Mode::RubberBand:
        m_band = QRectF(m_pressPage, e.pagePos).normalized();
        host().updateOverlayAll();
        break;
    case Mode::Lasso:
        if (geom::distance(host().view().pageToView(m_lasso.last()), e.viewPos) > 3.0)
            m_lasso << e.pagePos;
        host().updateOverlayAll();
        break;
    case Mode::None:
        break;
    }
}

void SelectTool::updateMove(const QPointF& pagePos)
{
    const QPointF delta = pagePos - m_pressPage;
    Document& doc = host().document();
    const PageId pid = host().page()->id();
    for (const Original& o : m_originals) {
        ObjectPtr c = o.object->clone();
        c->setPosition(o.object->position() + delta);
        doc.replaceObject(pid, std::move(c));
    }
    host().updateOverlayAll();
}

void SelectTool::updateResize(const QPointF& pagePos, Qt::KeyboardModifiers mods)
{
    if (m_originals.empty())
        return;
    Document& doc = host().document();
    const PageId pid = host().page()->id();
    const int h = m_handle;

    if (m_originals.size() == 1) {
        const DocumentObject* orig = m_originals.front().object.get();
        const QRectF local = orig->localBounds();
        const QPointF p = orig->mapFromPage(pagePos);
        QRectF r = local;
        switch (h) {
        case 0: r.setTopLeft(p); break;
        case 1: r.setTop(p.y()); break;
        case 2: r.setTopRight(p); break;
        case 3: r.setRight(p.x()); break;
        case 4: r.setBottomRight(p); break;
        case 5: r.setBottom(p.y()); break;
        case 6: r.setBottomLeft(p); break;
        case 7: r.setLeft(p.x()); break;
        default: break;
        }
        const bool keep = orig->keepAspectRatio() || (mods & Qt::ShiftModifier);
        if (keep && h % 2 == 0 && local.width() > 0.5 && local.height() > 0.5) {
            const QPointF corners[4] = {local.topLeft(), local.topRight(), local.bottomRight(), local.bottomLeft()};
            const QPointF anchor = corners[((h / 2) + 2) % 4];
            const qreal s = std::max(std::abs(p.x() - anchor.x()) / local.width(), std::abs(p.y() - anchor.y()) / local.height());
            const qreal dirX = (h == 2 || h == 4) ? 1.0 : -1.0;
            const qreal dirY = (h == 4 || h == 6) ? 1.0 : -1.0;
            r = QRectF(anchor, anchor + QPointF(dirX * local.width() * s, dirY * local.height() * s)).normalized();
        }
        r = r.normalized();
        const ResizeHint hint = (h == 3 || h == 7) ? ResizeHint::Horizontal
            : (h == 1 || h == 5)                   ? ResizeHint::Vertical
                                                   : ResizeHint::Corner;
        ObjectPtr c = orig->clone();
        c->resizeTo(r, hint);
        doc.replaceObject(pid, std::move(c));
    } else {
        const QPointF corners[4] = {m_startBounds.topLeft(), m_startBounds.topRight(), m_startBounds.bottomRight(),
                                    m_startBounds.bottomLeft()};
        const QPointF handleStart = corners[(h / 2) % 4];
        const QPointF anchor = corners[((h / 2) + 2) % 4];
        const QPointF d0 = handleStart - anchor;
        const qreal len2 = geom::dot(d0, d0);
        if (len2 < 1e-6)
            return;
        const qreal s = std::max(0.05, geom::dot(pagePos - anchor, d0) / len2);
        for (const Original& o : m_originals) {
            ObjectPtr c = o.object->clone();
            if (o.object->canResize()) {
                const QRectF lb = o.object->localBounds();
                c->resizeTo(QRectF(lb.topLeft() * s, lb.bottomRight() * s));
            }
            c->setPosition(anchor + (o.object->position() - anchor) * s);
            doc.replaceObject(pid, std::move(c));
        }
    }
    host().updateOverlayAll();
}

void SelectTool::updateRotate(const QPointF& pagePos)
{
    if (m_originals.empty())
        return;
    qreal delta = geom::angleDifference(m_startAngle, geom::angleDeg(pagePos - m_pivot));
    if (m_originals.size() == 1) {
        const qreal base = m_originals.front().object->rotation();
        const qreal snapped = geom::snapAngle(geom::normalizeDegrees(base + delta), 15.0, 3.0);
        delta = geom::angleDifference(base, snapped);
    } else {
        delta = geom::snapAngle(delta, 15.0, 3.0);
    }
    Document& doc = host().document();
    const PageId pid = host().page()->id();
    for (const Original& o : m_originals) {
        if (!o.object->canRotate())
            continue;
        ObjectPtr c = o.object->clone();
        c->setRotation(o.object->rotation() + delta);
        c->setPosition(m_pivot + geom::rotated(o.object->position() - m_pivot, delta));
        doc.replaceObject(pid, std::move(c));
    }
    host().updateOverlayAll();
}

void SelectTool::updateControlPoint(const QPointF& pagePos)
{
    if (m_originals.size() != 1)
        return;
    ObjectPtr c = m_originals.front().object->clone();
    c->moveControlPointTo(m_handle, pagePos);
    host().document().replaceObject(host().page()->id(), std::move(c));
    host().updateOverlayAll();
}

void SelectTool::finishDrag()
{
    Page* page = host().page();
    if (!page || m_originals.empty()) {
        m_originals.clear();
        return;
    }
    QString text;
    switch (m_mode) {
    case Mode::Move: text = QObject::tr("Move"); break;
    case Mode::Resize: text = QObject::tr("Resize"); break;
    case Mode::Rotate: text = QObject::tr("Rotate"); break;
    case Mode::ControlPoint: text = QObject::tr("Edit points"); break;
    default: text = QObject::tr("Edit"); break;
    }
    auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), text);
    for (Original& o : m_originals) {
        if (DocumentObject* cur = page->object(o.id))
            cmd->add(std::move(o.object), cur->clone());
    }
    m_originals.clear();
    if (!cmd->isEmpty())
        host().document().commands().pushApplied(std::move(cmd));
}

void SelectTool::cancelDrag()
{
    Page* page = host().page();
    if (page) {
        for (const Original& o : m_originals)
            host().document().replaceObject(page->id(), o.object->clone());
    }
    m_originals.clear();
}

void SelectTool::finishBand(const PointerEvent& e)
{
    Page* page = host().page();
    if (!page)
        return;
    const bool additive = (e.modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) || host().settings().multiSelect();
    QPolygonF poly;
    if (m_mode == Mode::RubberBand) {
        if (host().view().pageToView(m_band).width() < 4 && host().view().pageToView(m_band).height() < 4)
            return;
        poly = QPolygonF(m_band);
    } else {
        if (m_lasso.size() < 3)
            return;
        poly = m_lasso;
    }
    QVector<ObjectId> ids = additive ? host().selection().ids() : QVector<ObjectId>();
    for (const auto& obj : page->objects()) {
        const bool inside = (m_mode == Mode::RubberBand && m_band.contains(obj->sceneBounds())) || obj->isInsidePolygon(poly);
        if (inside && !ids.contains(obj->id()))
            ids.push_back(obj->id());
    }
    host().selection().set(ids);
}

void SelectTool::handleTap(const PointerEvent& e)
{
    if (m_pressedObject.isNull())
        return;
    const bool additive = (e.modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) || host().settings().multiSelect();
    if (additive) {
        if (m_pressedWasSelected)
            host().selection().remove(m_pressedObject);
    } else {
        host().selection().setSingle(m_pressedObject);
    }
    const bool doubleTap = m_tapTimer.isValid() && m_tapTimer.elapsed() < 450
        && geom::distance(e.viewPos, m_lastTapView) < host().theme().dp(28) && m_lastTapObject == m_pressedObject;
    if (doubleTap) {
        m_tapTimer.invalidate();
        // The window decides what "edit" means: text/formula/graph/table editors, or the exact
        // values of measurements, lines, vectors and shapes.
        Page* page = host().page();
        if (page && page->object(m_pressedObject))
            host().requestEdit(m_pressedObject);
        return;
    }
    m_tapTimer.start();
    m_lastTapView = e.viewPos;
    m_lastTapObject = m_pressedObject;
}

void SelectTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    switch (m_mode) {
    case Mode::Pending:
        handleTap(e);
        break;
    case Mode::Move:
    case Mode::Resize:
    case Mode::Rotate:
    case Mode::ControlPoint:
        finishDrag();
        break;
    case Mode::RubberBand:
    case Mode::Lasso:
        finishBand(e);
        break;
    case Mode::None:
        break;
    }
    m_mode = Mode::None;
    m_pointer = -1;
    m_lasso.clear();
    host().updateOverlayAll();
}

void SelectTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    cancelDrag();
    m_mode = Mode::None;
    m_pointer = -1;
    m_lasso.clear();
    host().updateOverlayAll();
}

void SelectTool::hover(const PointerEvent& e)
{
    const Frame f = frame();
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (f.valid) {
        const qreal hit = host().theme().scaled(ThemeMetric::HandleHitSize) / 2;
        bool onControl = false;
        for (const QPointF& cp : f.controlPoints)
            onControl = onControl || geom::distance(e.viewPos, cp) <= hit;
        if (onControl || (f.canRotate && geom::distance(e.viewPos, f.rotateHandle) <= hit)
            || hitResizeHandle(f, e.viewPos) >= 0)
            shape = Qt::PointingHandCursor;
        else if (f.viewCorners.containsPoint(e.viewPos, Qt::OddEvenFill))
            shape = Qt::SizeAllCursor;
    }
    if (host().widget()->cursor().shape() != shape)
        host().widget()->setCursor(shape);
}

void SelectTool::nudge(const QPointF& delta)
{
    Page* page = host().page();
    if (!page)
        return;
    auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), QObject::tr("Move"));
    for (DocumentObject* o : selectedObjects()) {
        ObjectPtr after = o->clone();
        after->setPosition(o->position() + delta);
        cmd->add(o->clone(), std::move(after));
    }
    if (!cmd->isEmpty())
        host().document().commands().push(std::move(cmd));
}

bool SelectTool::keyPress(QKeyEvent* event)
{
    const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 20.0 : 2.0;
    switch (event->key()) {
    case Qt::Key_Left: nudge(QPointF(-step, 0)); return true;
    case Qt::Key_Right: nudge(QPointF(step, 0)); return true;
    case Qt::Key_Up: nudge(QPointF(0, -step)); return true;
    case Qt::Key_Down: nudge(QPointF(0, step)); return true;
    case Qt::Key_Escape:
        host().selection().clear();
        return true;
    default:
        return false;
    }
}

void SelectTool::deactivate()
{
    if (m_mode == Mode::Move || m_mode == Mode::Resize || m_mode == Mode::Rotate || m_mode == Mode::ControlPoint)
        finishDrag();
    m_mode = Mode::None;
    m_pointer = -1;
}

void SelectTool::pageChanged()
{
    m_originals.clear();
    m_mode = Mode::None;
    m_pointer = -1;
    m_lasso.clear();
}

void SelectTool::paintViewOverlay(QPainter& p) const
{
    const Theme& t = host().theme();
    const QColor sel = t.color(ThemeColor::Selection);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_mode == Mode::RubberBand) {
        QPen pen(sel, t.dp(1.5), Qt::DashLine);
        p.setPen(pen);
        p.setBrush(t.color(ThemeColor::SelectionFill));
        p.drawRect(host().view().pageToView(m_band));
    } else if (m_mode == Mode::Lasso && m_lasso.size() > 1) {
        QPolygonF v;
        for (const QPointF& pt : m_lasso)
            v << host().view().pageToView(pt);
        p.setPen(QPen(sel, t.dp(1.5), Qt::DashLine));
        p.setBrush(t.color(ThemeColor::SelectionFill));
        p.drawPolygon(v);
    }

    const Frame f = frame();
    if (f.valid) {
        const auto objs = selectedObjects();
        if (objs.size() > 1) {
            p.setPen(QPen(QColor(sel.red(), sel.green(), sel.blue(), 110), t.dp(1)));
            p.setBrush(Qt::NoBrush);
            for (DocumentObject* o : objs)
                p.drawRect(host().view().pageToView(o->sceneBounds()));
        }
        p.setPen(QPen(sel, t.dp(1.6)));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(f.viewCorners);

        const bool dragging = m_mode == Mode::Move;
        if (!dragging) {
            const qreal hs = t.scaled(ThemeMetric::HandleSize) / 2;
            if (f.canRotate) {
                const QPointF topCenter = geom::midpoint(f.viewCorners[0], f.viewCorners[1]);
                p.setPen(QPen(sel, t.dp(1.5)));
                p.drawLine(topCenter, f.rotateHandle);
                p.setBrush(t.color(ThemeColor::Handle));
                p.drawEllipse(f.rotateHandle, hs * 1.3, hs * 1.3);
                p.setPen(QPen(sel, t.dp(1.8), Qt::SolidLine, Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                const qreal ar = hs * 0.65;
                p.drawArc(QRectF(f.rotateHandle.x() - ar, f.rotateHandle.y() - ar, 2 * ar, 2 * ar), 40 * 16, 280 * 16);
            }
            if (f.canResize) {
                const QVector<QPointF> handles = resizeHandles(f);
                p.setPen(QPen(sel, t.dp(1.6)));
                p.setBrush(t.color(ThemeColor::Handle));
                for (int i = 0; i < handles.size(); ++i) {
                    if ((f.keepAspect || !f.single) && i % 2 == 1)
                        continue;
                    p.drawRoundedRect(QRectF(handles[i].x() - hs, handles[i].y() - hs, 2 * hs, 2 * hs), hs * 0.35, hs * 0.35);
                }
            }
            p.setPen(QPen(t.color(ThemeColor::Handle), t.dp(2)));
            p.setBrush(sel);
            for (const QPointF& cp : f.controlPoints)
                p.drawEllipse(cp, hs * 1.15, hs * 1.15);
        }
    }
    p.restore();
}

} // namespace cb
