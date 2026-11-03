#include "ui/popovers/PageNavigatorPanel.h"

#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/ThumbnailCache.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/PageOperations.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/BoardPanels.h"
#include "ui/popovers/PanelUtil.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace cb {

// ======================================================================================= PageGrid

PageGrid::PageGrid(const AppServices& s, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(&s.thumbnails, &ThumbnailCache::thumbnailReady, this, [this]() { update(); });
}

QSizeF PageGrid::tileSize() const
{
    const Theme& t = m_s.ui.theme;
    const qreal w = t.dp(176);
    return QSizeF(w, w * 9.0 / 16.0 + t.dp(30));
}

QSize PageGrid::sizeHint() const
{
    const Theme& t = m_s.ui.theme;
    const QSizeF tile = tileSize();
    const int rows = (m_s.doc.pageCount() + m_columns - 1) / m_columns;
    const qreal gap = t.dp(10);
    return QSize(qRound(m_columns * tile.width() + (m_columns - 1) * gap), qRound(rows * tile.height() + (rows - 1) * gap));
}

QRectF PageGrid::tileRect(int index) const
{
    const QSizeF tile = tileSize();
    const qreal gap = m_s.ui.theme.dp(10);
    return QRectF((index % m_columns) * (tile.width() + gap), (index / m_columns) * (tile.height() + gap), tile.width(),
                  tile.height());
}

int PageGrid::indexAt(const QPointF& pos) const
{
    for (int i = 0; i < m_s.doc.pageCount(); ++i)
        if (tileRect(i).contains(pos))
            return i;
    return -1;
}

int PageGrid::dropIndex(const QPointF& pos) const
{
    int best = 0;
    qreal bestDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i < m_s.doc.pageCount(); ++i) {
        const qreal d = geom::distance(tileRect(i).center(), pos);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void PageGrid::paintEvent(QPaintEvent*)
{
    const Theme& t = m_s.ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int count = m_s.doc.pageCount();
    const int current = m_s.doc.currentPageIndex();
    const int target = m_dragging ? dropIndex(m_dragPos) : -1;
    auto drawTile = [&](int i, const QRectF& r, qreal opacity) {
        const Page* page = m_s.doc.page(i);
        if (!page)
            return;
        p.setOpacity(opacity);
        const QRectF box(r.left(), r.top(), r.width(), r.width() * 9.0 / 16.0);
        // Fit the page (whatever its size) into the 16:9 tile without cropping or stretching.
        QSizeF fitted = page->size();
        fitted.scale(box.size(), Qt::KeepAspectRatio);
        const QRectF thumb(box.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted);
        const QImage img = m_s.thumbnails.thumbnail(page->id());
        QPainterPath clip;
        clip.addRoundedRect(thumb, t.dp(8), t.dp(8));
        p.save();
        p.setClipPath(clip);
        if (img.isNull())
            p.fillRect(thumb, page->background().background);
        else
            p.drawImage(thumb, img);
        p.restore();
        const bool isCurrent = i == current;
        p.setPen(QPen(isCurrent ? t.color(ThemeColor::Accent) : t.color(ThemeColor::PopoverBorder), t.dp(isCurrent ? 3 : 1)));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(thumb, t.dp(8), t.dp(8));
        p.setPen(isCurrent ? t.color(ThemeColor::Accent) : t.color(ThemeColor::Text));
        p.setFont(t.font(t.metric(ThemeMetric::SmallFontSize), isCurrent));
        QString label = QString::number(i + 1);
        if (!page->name().isEmpty())
            label += QStringLiteral("  ") + page->name();
        p.drawText(QRectF(r.left(), thumb.bottom() + t.dp(4), r.width(), t.dp(22)), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(label, Qt::ElideRight, int(r.width())));
        p.setOpacity(1.0);
    };
    for (int i = 0; i < count; ++i) {
        if (m_dragging && i == m_pressIndex) {
            // Leave a dashed placeholder where the dragged page was.
            p.setPen(QPen(t.color(ThemeColor::PopoverBorder), t.dp(1.5), Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            const QRectF r = tileRect(i);
            p.drawRoundedRect(QRectF(r.left(), r.top(), r.width(), r.width() * 9.0 / 16.0), t.dp(8), t.dp(8));
            continue;
        }
        drawTile(i, tileRect(i), 1.0);
    }
    if (m_dragging && target >= 0) {
        const QRectF r = tileRect(target);
        const qreal x = target > m_pressIndex ? r.right() + t.dp(5) : r.left() - t.dp(5);
        p.setPen(QPen(t.color(ThemeColor::Accent), t.dp(4), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.top() + r.width() * 9.0 / 16.0));
        const QRectF moving = tileRect(m_pressIndex).translated(m_dragPos - m_pressPos);
        drawTile(m_pressIndex, moving, 0.85);
    }
}

void PageGrid::mousePressEvent(QMouseEvent* e)
{
    m_pressIndex = indexAt(e->localPos());
    m_pressPos = e->localPos();
    m_dragPos = m_pressPos;
    m_dragging = false;
}

void PageGrid::mouseMoveEvent(QMouseEvent* e)
{
    if (m_pressIndex < 0)
        return;
    m_dragPos = e->localPos();
    if (!m_dragging && geom::distance(m_dragPos, m_pressPos) > m_s.ui.theme.dp(12) && m_s.doc.pageCount() > 1)
        m_dragging = true;
    if (m_dragging)
        update();
}

void PageGrid::mouseReleaseEvent(QMouseEvent* e)
{
    const int pressed = m_pressIndex;
    m_pressIndex = -1;
    if (pressed < 0)
        return;
    if (m_dragging) {
        m_dragging = false;
        const int to = dropIndex(e->localPos());
        update();
        if (to != pressed)
            emit pageMoved(pressed, to);
        return;
    }
    if (indexAt(e->localPos()) == pressed)
        emit pageActivated(pressed);
}

// =============================================================================== PageNavigatorPanel

PageNavigatorPanel::PageNavigatorPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);

    Document& doc = s.doc;
    auto* add = panel::pill(ui, QStringLiteral("page-add"), tr("New page"), this, [sp]() {
        pageops::newPage(sp->doc, sp->doc.currentPageIndex());
        sp->popovers.refresh();
    }, true);
    auto* duplicate = panel::pill(ui, QStringLiteral("duplicate"), tr("Duplicate"), this, [sp]() {
        pageops::duplicatePage(sp->doc, sp->doc.currentPageIndex());
        sp->popovers.refresh();
    });
    // Clear keeps the page and removes its content; Delete removes the page itself.
    auto* clear = panel::pill(ui, QStringLiteral("page-clear"), tr("Clear page"), this,
                              [sp]() { PageActionsPanel::confirmClearPage(*sp); });
    clear->setToolTip(tr("Remove everything on this page, keep the page"));
    clear->setEnabled(doc.currentPage() && doc.currentPage()->objectCount() > 0);
    auto* remove = panel::pill(ui, QStringLiteral("page-delete"), tr("Delete page"), this, [sp]() {
        PageActionsPanel::deleteCurrentPage(*sp);
        sp->popovers.refresh();
    });
    remove->setToolTip(tr("Remove this page from the lesson"));
    remove->setDanger(true);
    remove->setEnabled(doc.pageCount() > 1);
    layout->addWidget(panel::row(ui, {add, duplicate, clear, remove}, this));

    auto* left = panel::pill(ui, QStringLiteral("chevron-left"), QString(), this, [sp]() {
        Document& d = sp->doc;
        const int i = d.currentPageIndex();
        if (i > 0)
            d.commands().push(std::make_unique<MovePageCommand>(i, i - 1));
        sp->popovers.refresh();
    });
    left->setToolTip(tr("Move page earlier"));
    left->setEnabled(doc.currentPageIndex() > 0);
    auto* right = panel::pill(ui, QStringLiteral("chevron-right"), QString(), this, [sp]() {
        Document& d = sp->doc;
        const int i = d.currentPageIndex();
        if (i + 1 < d.pageCount())
            d.commands().push(std::make_unique<MovePageCommand>(i, i + 1));
        sp->popovers.refresh();
    });
    right->setToolTip(tr("Move page later"));
    right->setEnabled(doc.currentPageIndex() + 1 < doc.pageCount());
    auto* size = panel::pill(ui, QStringLiteral("page-size"), tr("Page size"), this,
                             [sp]() { sp->popovers.push(QStringLiteral("pagesize")); });
    auto* background = panel::pill(ui, QStringLiteral("palette"), tr("Background"), this,
                                   [sp]() { sp->popovers.push(QStringLiteral("background")); });
    auto* templates = panel::pill(ui, QStringLiteral("template"), tr("Templates"), this,
                                  [sp]() { sp->popovers.push(QStringLiteral("templates")); });
    layout->addWidget(panel::row(ui, {left, right, size, background, templates}, this));

    // Rename the current page inline.
    auto* name = new QLineEdit(doc.currentPage() ? doc.currentPage()->name() : QString(), this);
    name->setPlaceholderText(tr("Name of page %1 (optional)").arg(doc.currentPageIndex() + 1));
    name->setMinimumHeight(ui.theme.dpi(44));
    connect(name, &QLineEdit::editingFinished, this, [sp, name]() {
        Page* page = sp->doc.currentPage();
        if (!page || page->name() == name->text())
            return;
        sp->doc.commands().push(std::make_unique<ModifyPageCommand>(page->id(), name->text(), page->background(), tr("Rename page")));
    });
    layout->addWidget(name);

    auto* grid = new PageGrid(s, this);
    layout->addWidget(grid);
    connect(grid, &PageGrid::pageActivated, this, [sp](int index) {
        sp->doc.setCurrentPageIndex(index);
        sp->popovers.close();
    });
    connect(grid, &PageGrid::pageMoved, this, [sp](int from, int to) {
        sp->doc.commands().push(std::make_unique<MovePageCommand>(from, to));
        sp->popovers.refresh();
    });
}

} // namespace cb
