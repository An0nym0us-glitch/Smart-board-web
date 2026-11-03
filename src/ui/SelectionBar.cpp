#include "ui/SelectionBar.h"

#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Document.h"
#include "tools/EditOperations.h"
#include "tools/SelectTool.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "ui/Popover.h"
#include "ui/UiContext.h"
#include "ui/popovers/PropertiesPanel.h"
#include "ui/widgets/TouchButton.h"

#include <QHBoxLayout>
#include <QPainter>

namespace cb {

SelectionBar::SelectionBar(const AppServices& s, QWidget* canvas)
    : QWidget(canvas)
    , m_s(s)
{
    const Theme& t = s.ui.theme;
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(t.dpi(6), t.dpi(4), t.dpi(6), t.dpi(4));
    layout->setSpacing(t.dpi(2));
    auto add = [&](const QString& icon, const QString& tip, std::function<void()> action) {
        auto* b = new TouchButton(s.ui, icon, tip, TouchButton::Style::Icon, this);
        b->setFixedSize(t.dpi(52), t.dpi(52));
        connect(b, &QAbstractButton::clicked, this, std::move(action));
        layout->addWidget(b);
        return b;
    };
    const AppServices* sp = &s;
    m_edit = add(QStringLiteral("edit"), tr("Edit"), [this]() { emit editRequested(); });
    m_precision = add(QStringLiteral("precision"), tr("Exact values (length, angle, slope, size)"), [this]() { emit editRequested(); });
    m_precision->setObjectName(QStringLiteral("precision"));
    m_magic = add(QStringLiteral("magic"), tr("✨ Magic Equation Maker: turn this handwriting into an equation"),
                  [this]() { emit magicRequested(); });
    m_magic->setObjectName(QStringLiteral("magic"));
    m_color = add(QStringLiteral("palette"), tr("Colour"), [sp, this]() { sp->popovers.open(QStringLiteral("color"), m_color); });
    add(QStringLiteral("bring-front"), tr("Bring to front"), [sp]() { sp->edit.bringToFront(); });
    add(QStringLiteral("send-back"), tr("Send to back"), [sp]() { sp->edit.sendToBack(); });
    add(QStringLiteral("duplicate"), tr("Duplicate"), [sp]() { sp->edit.duplicateSelection(); });
    add(QStringLiteral("copy"), tr("Copy"), [sp]() { sp->edit.copySelection(); });
    auto* del = add(QStringLiteral("trash"), tr("Delete"), [sp]() { sp->edit.deleteSelection(); });
    del->setDanger(true);
    hide();
}

void SelectionBar::refresh()
{
    CanvasWidget& canvas = m_s.canvas;
    const SelectionModel& sel = canvas.selectionModel();
    auto* select = static_cast<SelectTool*>(canvas.tools().tool(ToolId::Select));
    const bool active = canvas.tools().activeToolId() == ToolId::Select && !sel.isEmpty() && select
        && !canvas.tools().hasActivePointers();
    if (!active) {
        hide();
        return;
    }
    Page* page = m_s.doc.currentPage();
    bool editable = false;
    bool colorable = false;
    bool precise = false;
    bool handwriting = page != nullptr;
    if (page) {
        for (const ObjectId& id : sel.ids()) {
            DocumentObject* o = page->object(id);
            if (!o)
                continue;
            if (sel.count() == 1 && o->isEditable())
                editable = true;
            if (sel.count() == 1 && PropertiesPanel::supports(*o))
                precise = true;
            if (o->color().isValid())
                colorable = true;
            if (o->type() != ObjectType::Stroke)
                handwriting = false;
        }
    }
    m_edit->setVisible(editable);
    m_precision->setVisible(precise);
    m_magic->setVisible(handwriting);
    m_color->setVisible(colorable);
    adjustSize();

    const QRect bounds = canvas.pageRectToWidget(select->selectionBounds());
    const Theme& t = m_s.ui.theme;
    const int gap = t.dpi(56); // leave room for the rotation handle
    int x = bounds.center().x() - width() / 2;
    int y = bounds.top() - gap - height();
    if (y < t.dpi(8))
        y = bounds.bottom() + t.dpi(20);
    if (y + height() > canvas.height() - t.dpi(8))
        y = std::max(t.dpi(8), bounds.top() + t.dpi(8));
    x = std::clamp(x, t.dpi(8), std::max(t.dpi(8), canvas.width() - width() - t.dpi(8)));
    move(x, y);
    show();
    raise();
}

void SelectionBar::paintEvent(QPaintEvent*)
{
    const Theme& t = m_s.ui.theme;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.setPen(QPen(t.color(ThemeColor::PopoverBorder), t.dp(1)));
    p.setBrush(t.color(ThemeColor::Popover));
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
}

} // namespace cb
