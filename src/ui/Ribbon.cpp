#include "ui/Ribbon.h"

#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QHBoxLayout>
#include <QPainter>

#include <algorithm>

namespace cb {

namespace {
class Separator : public QWidget
{
public:
    Separator(const UiContext& ui, QWidget* parent)
        : QWidget(parent)
        , m_ui(ui)
    {
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const Theme& t = m_ui.theme;
        p.setPen(QPen(t.color(ThemeColor::Separator), t.dp(1)));
        const int x = width() / 2;
        p.drawLine(x, t.dpi(16), x, height() - t.dpi(16));
    }

private:
    const UiContext& m_ui;
};
} // namespace

Ribbon::Ribbon(const UiContext& base, qreal scale, QWidget* parent)
    : QWidget(parent)
    , m_baseScale(base.theme.uiScale())
    , m_scale(std::clamp(scale, kMinScale, kMaxScale))
{
    m_ui.theme = base.theme;
    m_ui.theme.setUiScale(m_baseScale * m_scale);
    const UiContext& ui = m_ui;
    auto* layout = new QHBoxLayout(this);
    auto separator = [this, &ui]() {
        m_separators.append(new Separator(ui, this));
        return m_separators.last();
    };

    m_more = addButton(QStringLiteral("more"), tr("More"));
    m_more->setCheckable(true);
    layout->addWidget(m_more);
    layout->addWidget(separator());

    m_pen = addButton(QStringLiteral("pen"), tr("Pen"));
    m_eraser = addButton(QStringLiteral("eraser"), tr("Eraser"));
    m_select = addButton(QStringLiteral("select"), tr("Select"));
    m_toolChip = addButton(QString(), QString());
    for (TouchButton* b : {m_pen, m_eraser, m_select, m_toolChip}) {
        b->setCheckable(true);
        b->setHasOptions(true);
        layout->addWidget(b);
    }
    m_toolChip->setVisible(false);
    layout->addWidget(separator());

    m_undo = addButton(QStringLiteral("undo"), tr("Undo"));
    m_redo = addButton(QStringLiteral("redo"), tr("Redo"));
    layout->addWidget(m_undo);
    layout->addWidget(m_redo);

    // File / Insert / Edit groups (popovers).
    m_groupSeparator = separator();
    layout->addWidget(m_groupSeparator);
    m_file = addButton(QStringLiteral("file"), tr("File"));
    m_insert = addButton(QStringLiteral("insert"), tr("Insert"));
    m_edit = addButton(QStringLiteral("edit"), tr("Edit"));
    m_file->setToolTip(tr("New, open, save, import and export"));
    m_insert->setToolTip(tr("Insert image, shape, text, graph, equation, PDF or PowerPoint"));
    m_edit->setToolTip(tr("Cut, copy, paste, delete"));
    for (TouchButton* b : {m_file, m_insert, m_edit}) {
        b->setHasOptions(true);
        layout->addWidget(b);
    }

    layout->addStretch(1);

    // Page group: navigation, new page and the page actions menu.
    m_prev = new TouchButton(ui, QStringLiteral("chevron-left"), tr("Previous page"), TouchButton::Style::Icon, this);
    m_page = new TouchButton(ui, QString(), QStringLiteral("1 / 1"), TouchButton::Style::Pill, this);
    m_page->setToolTip(tr("Pages"));
    m_next = new TouchButton(ui, QStringLiteral("chevron-right"), tr("Next page"), TouchButton::Style::Icon, this);
    m_newPage = addButton(QStringLiteral("page-add"), tr("New page"));
    m_newPage->setToolTip(tr("Add a blank page after this one"));
    m_pageMenu = addButton(QStringLiteral("pages"), tr("Page"));
    m_pageMenu->setHasOptions(true);
    m_pageMenu->setToolTip(tr("New, duplicate, clear or delete page; page size and background"));
    layout->addWidget(m_prev);
    layout->addWidget(m_page);
    layout->addWidget(m_next);
    layout->addWidget(m_newPage);
    layout->addWidget(m_pageMenu);

    layout->addStretch(1);

    // View group: zoom out, zoom level (opens the View popover), zoom in, fit, full screen.
    m_zoomOut = new TouchButton(ui, QStringLiteral("zoom-out"), tr("Zoom out"), TouchButton::Style::Icon, this);
    m_zoom = new TouchButton(ui, QString(), QStringLiteral("100%"), TouchButton::Style::Pill, this);
    m_zoom->setToolTip(tr("Zoom and view: 25 % to 250 %, fit page, fit width"));
    m_zoomIn = new TouchButton(ui, QStringLiteral("zoom-in"), tr("Zoom in"), TouchButton::Style::Icon, this);
    m_fit = new TouchButton(ui, QStringLiteral("fit-page"), tr("Fit page"), TouchButton::Style::Icon, this);
    m_fullScreen = new TouchButton(ui, QStringLiteral("fullscreen"), tr("Full screen"), TouchButton::Style::Icon, this);
    layout->addWidget(m_zoomOut);
    layout->addWidget(m_zoom);
    layout->addWidget(m_zoomIn);
    layout->addWidget(m_fit);
    layout->addWidget(m_fullScreen);

    applyMetrics();

    m_optional = {{m_fit}, {m_zoomOut, m_zoomIn}, {m_edit}, {m_insert}, {m_file, m_groupSeparator}, {m_prev, m_next}};

    connect(m_more, &QAbstractButton::clicked, this, &Ribbon::moreClicked);
    connect(m_pen, &QAbstractButton::clicked, this, &Ribbon::penClicked);
    connect(m_eraser, &QAbstractButton::clicked, this, &Ribbon::eraserClicked);
    connect(m_select, &QAbstractButton::clicked, this, &Ribbon::selectClicked);
    connect(m_toolChip, &QAbstractButton::clicked, this, &Ribbon::toolChipClicked);
    connect(m_undo, &QAbstractButton::clicked, this, &Ribbon::undoClicked);
    connect(m_redo, &QAbstractButton::clicked, this, &Ribbon::redoClicked);
    connect(m_file, &QAbstractButton::clicked, this, &Ribbon::fileClicked);
    connect(m_insert, &QAbstractButton::clicked, this, &Ribbon::insertClicked);
    connect(m_edit, &QAbstractButton::clicked, this, &Ribbon::editClicked);
    connect(m_prev, &QAbstractButton::clicked, this, &Ribbon::previousPageClicked);
    connect(m_next, &QAbstractButton::clicked, this, &Ribbon::nextPageClicked);
    connect(m_page, &QAbstractButton::clicked, this, &Ribbon::pageIndicatorClicked);
    connect(m_newPage, &QAbstractButton::clicked, this, &Ribbon::newPageClicked);
    connect(m_pageMenu, &QAbstractButton::clicked, this, &Ribbon::pageMenuClicked);
    connect(m_zoom, &QAbstractButton::clicked, this, &Ribbon::zoomClicked);
    connect(m_zoomIn, &QAbstractButton::clicked, this, &Ribbon::zoomInClicked);
    connect(m_zoomOut, &QAbstractButton::clicked, this, &Ribbon::zoomOutClicked);
    connect(m_fit, &QAbstractButton::clicked, this, &Ribbon::fitClicked);
    connect(m_fullScreen, &QAbstractButton::clicked, this, &Ribbon::fullScreenClicked);
}

void Ribbon::setScale(qreal scale)
{
    scale = std::clamp(scale, kMinScale, kMaxScale);
    if (qFuzzyCompare(scale, m_scale))
        return;
    m_scale = scale;
    m_ui.theme.setUiScale(m_baseScale * m_scale);
    applyMetrics();
    updateFit();
    update();
}

void Ribbon::applyMetrics()
{
    const Theme& t = m_ui.theme;
    setFixedHeight(t.scaledInt(ThemeMetric::RibbonHeight));
    layout()->setContentsMargins(t.dpi(8), t.dpi(6), t.dpi(8), t.dpi(6));
    layout()->setSpacing(t.dpi(4));
    for (QWidget* s : m_separators)
        s->setFixedWidth(t.dpi(13));
    m_page->setMinimumWidth(t.dpi(120));
    m_zoom->setMinimumWidth(t.dpi(84));
    for (TouchButton* b : findChildren<TouchButton*>()) {
        b->updateGeometry();
        b->update();
    }
    updateGeometry();
}

void Ribbon::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateFit();
}

void Ribbon::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateFit();
}

void Ribbon::updateFit()
{
    if (m_fitting || !layout())
        return;
    m_fitting = true;
    for (const auto& group : m_optional)
        for (QWidget* w : group)
            w->setVisible(true);
    auto required = [this]() {
        const QMargins m = layout()->contentsMargins();
        int total = m.left() + m.right();
        int count = 0;
        for (int i = 0; i < layout()->count(); ++i) {
            QWidget* w = layout()->itemAt(i)->widget();
            if (!w || w->isHidden())
                continue;
            total += std::max(w->sizeHint().width(), w->minimumWidth());
            ++count;
        }
        return total + std::max(0, count - 1) * layout()->spacing();
    };
    for (const auto& group : m_optional) {
        if (required() <= width())
            break;
        for (QWidget* w : group)
            w->setVisible(false);
    }
    m_fitting = false;
}

TouchButton* Ribbon::addButton(const QString& icon, const QString& text)
{
    return new TouchButton(m_ui, icon, text, TouchButton::Style::Ribbon, this);
}

QSize Ribbon::sizeHint() const
{
    return QSize(800, m_ui.theme.scaledInt(ThemeMetric::RibbonHeight));
}

void Ribbon::setActiveTool(ToolId id)
{
    // Checkable buttons are driven by the tool state, not by clicks.
    m_pen->setChecked(id == ToolId::Pen);
    m_eraser->setChecked(id == ToolId::Eraser);
    m_select->setChecked(id == ToolId::Select);
    m_toolChip->setChecked(m_toolChip->isVisible() && id != ToolId::Pen && id != ToolId::Eraser && id != ToolId::Select);
}

void Ribbon::setToolChip(const QString& icon, const QString& label)
{
    m_toolChip->setIconName(icon);
    m_toolChip->setText(label);
    m_toolChip->setToolTip(label);
    m_toolChip->setVisible(!icon.isEmpty());
    m_toolChip->setChecked(!icon.isEmpty());
    m_toolChip->updateGeometry();
}

void Ribbon::setPenColor(const QColor& color)
{
    m_pen->setIndicatorColor(color);
}

void Ribbon::setPageInfo(int currentIndex, int count)
{
    m_page->setText(QStringLiteral("%1 / %2").arg(currentIndex + 1).arg(count));
    m_prev->setEnabled(currentIndex > 0);
    m_next->setEnabled(currentIndex + 1 < count);
    m_page->updateGeometry();
}

void Ribbon::setUndoRedo(bool canUndo, bool canRedo, const QString& undoText, const QString& redoText)
{
    m_undo->setEnabled(canUndo);
    m_redo->setEnabled(canRedo);
    m_undo->setToolTip(canUndo ? tr("Undo %1").arg(undoText) : tr("Undo"));
    m_redo->setToolTip(canRedo ? tr("Redo %1").arg(redoText) : tr("Redo"));
}

void Ribbon::setZoomPercent(int percent)
{
    m_zoom->setText(QStringLiteral("%1%").arg(percent));
}

void Ribbon::setFullScreen(bool on)
{
    m_fullScreen->setIconName(on ? QStringLiteral("fullscreen-exit") : QStringLiteral("fullscreen"));
}

void Ribbon::setMoreOpen(bool open)
{
    m_more->setChecked(open);
}

void Ribbon::paintEvent(QPaintEvent*)
{
    const Theme& t = m_ui.theme;
    QPainter p(this);
    p.fillRect(rect(), t.color(ThemeColor::Ribbon));
    p.setPen(QPen(t.color(ThemeColor::RibbonBorder), t.dp(1)));
    p.drawLine(0, 0, width(), 0);
}

} // namespace cb
