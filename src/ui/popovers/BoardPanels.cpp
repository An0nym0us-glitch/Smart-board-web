#include "ui/popovers/BoardPanels.h"

#include "app/AppServices.h"
#include "app/AppSettings.h"
#include "app/LessonController.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/PageOperations.h"
#include "document/PageSize.h"
#include "math/MeasureScale.h"
#include "tools/EditOperations.h"
#include "tools/ToolController.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/NumberField.h"
#include "ui/widgets/SegmentedControl.h"
#include "ui/widgets/SwatchGrid.h"

#include <QHBoxLayout>
#include <QLineEdit>

#include <cmath>

namespace cb {

namespace {
std::function<void()> closeThen(const AppServices* sp, std::function<void()> f)
{
    return [sp, f]() {
        sp->popovers.close();
        f();
    };
}
} // namespace

// ========================================================================================= Insert

InsertPanel::InsertPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    auto withTool = [sp](ToolId tool, const char* popover) {
        return [sp, tool, popover]() {
            sp->activateTool(tool);
            sp->popovers.push(QString::fromLatin1(popover));
        };
    };
    layout->addWidget(panel::tileGrid(ui, 4,
                                      {{QStringLiteral("image"), tr("Image"), closeThen(sp, [sp]() { sp->lesson.importImages(); })},
                                       {QStringLiteral("shapes"), tr("Shape"), withTool(ToolId::Shape, "shapes")},
                                       {QStringLiteral("text"), tr("Text"), withTool(ToolId::Text, "text")},
                                       {QStringLiteral("function"), tr("Graph"), withTool(ToolId::Graph, "function")},
                                       {QStringLiteral("equation"), tr("Equation"), [sp]() { sp->popovers.push(QStringLiteral("equation")); }},
                                       {QStringLiteral("table"), tr("Table"), [sp]() { sp->popovers.push(QStringLiteral("table")); }},
                                       {QStringLiteral("pdf"), tr("PDF"), closeThen(sp, [sp]() { sp->lesson.importPdf(); })},
                                       {QStringLiteral("pptx"), tr("PowerPoint"), closeThen(sp, [sp]() { sp->lesson.importPresentation(); })}},
                                      this));
    layout->addWidget(panel::hint(ui, tr("PDF pages and PowerPoint slides become board pages with their own size, "
                                         "so nothing is cropped or stretched."),
                                  this));
}

// =========================================================================================== Edit

EditPanel::EditPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const bool selection = s.edit.hasSelection();
    const CommandStack& cmds = s.doc.commands();
    layout->addWidget(panel::tileGrid(
        ui, 4,
        {{QStringLiteral("undo"), tr("Undo"), [sp]() { sp->doc.commands().undo(); sp->popovers.refresh(); }, false, cmds.canUndo()},
         {QStringLiteral("redo"), tr("Redo"), [sp]() { sp->doc.commands().redo(); sp->popovers.refresh(); }, false, cmds.canRedo()},
         {QStringLiteral("cut"), tr("Cut"), closeThen(sp, [sp]() { sp->edit.cutSelection(); }), false, selection},
         {QStringLiteral("copy"), tr("Copy"), closeThen(sp, [sp]() { sp->edit.copySelection(); }), false, selection},
         {QStringLiteral("paste"), tr("Paste"),
          closeThen(sp,
                    [sp]() {
                        if (sp->edit.paste(sp->canvas.viewCenterInPage()))
                            sp->activateTool(ToolId::Select);
                    }),
          false, s.edit.canPaste()},
         {QStringLiteral("duplicate"), tr("Duplicate"), closeThen(sp, [sp]() { sp->edit.duplicateSelection(); }), false, selection},
         {QStringLiteral("trash"), tr("Delete"), closeThen(sp, [sp]() { sp->edit.deleteSelection(); }), false, selection},
         {QStringLiteral("select-all"), tr("Select all"),
          closeThen(sp,
                    [sp]() {
                        sp->activateTool(ToolId::Select);
                        sp->edit.selectAll();
                    })}},
        this));
    if (!selection)
        layout->addWidget(panel::hint(ui, tr("Select objects with SELECT to cut, copy, duplicate or delete them."), this));
}

// ===================================================================================== Page actions

void PageActionsPanel::confirmClearPage(const AppServices& s)
{
    const AppServices* sp = &s;
    s.popovers.close();
    Page* page = s.doc.currentPage();
    if (!page || page->objectCount() == 0) {
        s.toast.showMessage(tr("This page is already empty."), 2000);
        return;
    }
    ConfirmOverlay::ask(s.ui, s.canvas.window(), tr("Clear this page?"),
                        tr("Everything on page %1 is removed; the page itself stays. You can undo this.")
                            .arg(s.doc.currentPageIndex() + 1),
                        {{tr("Cancel"), {}, false, false},
                         {tr("Clear page"), [sp]() { pageops::clearPage(sp->doc, sp->doc.currentPageIndex()); }, true, true}});
}

void PageActionsPanel::deleteCurrentPage(const AppServices& s)
{
    const AppServices* sp = &s;
    if (!pageops::deletePage(s.doc, s.doc.currentPageIndex())) {
        s.toast.showMessage(tr("A lesson needs at least one page. Use Clear page to empty it."), 3000);
        return;
    }
    s.toast.showActions(tr("Page deleted."), {{tr("Undo"), [sp]() { sp->doc.commands().undo(); }, true}}, 5000);
}

PageActionsPanel::PageActionsPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const int index = s.doc.currentPageIndex();
    layout->addWidget(panel::hint(ui, tr("Page %1 of %2 · %3")
                                          .arg(index + 1)
                                          .arg(s.doc.pageCount())
                                          .arg(s.doc.currentPage() ? pagesize::describe(s.doc.currentPage()->size()) : QString()),
                                  this));

    auto* grid = new QWidget(this);
    auto* g = new QGridLayout(grid);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(ui.theme.dpi(8));
    auto action = [&](int row, int col, const QString& icon, const QString& title, const QString& description,
                      std::function<void()> f, bool danger = false, bool enabled = true) {
        auto* b = new TouchButton(ui, icon, title, TouchButton::Style::Row, grid);
        b->setToolTip(description);
        b->setDanger(danger);
        b->setEnabled(enabled);
        connect(b, &QAbstractButton::clicked, this, std::move(f));
        auto* box = new QWidget(grid);
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(ui.theme.dpi(2));
        v->addWidget(b);
        v->addWidget(panel::hint(ui, description, box));
        g->addWidget(box, row, col);
        return b;
    };
    action(0, 0, QStringLiteral("page-add"), tr("New page"), tr("Adds a blank page after this one."),
           closeThen(sp, [sp]() { pageops::newPage(sp->doc, sp->doc.currentPageIndex()); }));
    action(0, 1, QStringLiteral("duplicate"), tr("Duplicate page"), tr("Adds a copy of this page."),
           closeThen(sp, [sp]() { pageops::duplicatePage(sp->doc, sp->doc.currentPageIndex()); }));
    auto* clear = action(1, 0, QStringLiteral("page-clear"), tr("Clear page"),
                         tr("Removes the contents. The page stays."), [sp]() { confirmClearPage(*sp); }, true,
                         s.doc.currentPage() && s.doc.currentPage()->objectCount() > 0);
    clear->setObjectName(QStringLiteral("clearPage"));
    auto* del = action(1, 1, QStringLiteral("page-delete"), tr("Delete page"),
                       tr("Removes the whole page from the lesson."),
                       [sp]() {
                           deleteCurrentPage(*sp);
                           sp->popovers.close();
                       },
                       true, s.doc.pageCount() > 1);
    del->setObjectName(QStringLiteral("deletePage"));
    layout->addWidget(grid);

    layout->addWidget(panel::section(ui, tr("Page setup"), this));
    auto* size = panel::pill(ui, QStringLiteral("page-size"), tr("Page size"), this,
                             [sp]() { sp->popovers.push(QStringLiteral("pagesize")); });
    auto* background = panel::pill(ui, QStringLiteral("palette"), tr("Background"), this,
                                   [sp]() { sp->popovers.push(QStringLiteral("background")); });
    auto* patterns = panel::pill(ui, QStringLiteral("template"), tr("Templates"), this,
                                 [sp]() { sp->popovers.push(QStringLiteral("templates")); });
    auto* all = panel::pill(ui, QStringLiteral("pages"), tr("All pages"), this,
                            [sp]() { sp->popovers.push(QStringLiteral("pages")); });
    layout->addWidget(panel::row(ui, {size, background, patterns, all}, this));
}

// ======================================================================================= Page size

PageSetupPanel::PageSetupPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    using namespace pagesize;
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const QSizeF current = s.doc.currentPage() ? s.doc.currentPage()->size() : s.doc.defaultPageSize();
    layout->addWidget(panel::hint(ui, tr("This page: %1").arg(describe(current)), this));

    // State shared by the controls (owned by this panel).
    struct State
    {
        Preset preset = Preset::Board16x9;
        Orientation orientation = Orientation::Landscape;
        QSizeF customCm{30.0, 20.0};
        int scope = 0;
    };
    auto state = std::make_shared<State>();
    state->preset = presetOf(current);
    state->orientation = orientationOf(current);
    if (state->preset == Preset::Custom)
        state->customCm = toCentimetres(current);

    layout->addWidget(panel::section(ui, tr("Size"), this));
    QStringList labels;
    for (const Info& info : presets())
        labels << info.label;
    labels << tr("Custom");
    auto* customRow = new QWidget(this);
    auto* presetsWidget = panel::choices(ui, labels, static_cast<int>(state->preset), this, [state, customRow](int i) {
        state->preset = static_cast<Preset>(i);
        customRow->setVisible(state->preset == Preset::Custom);
    }, 5);
    layout->addWidget(presetsWidget);

    auto* orientation = new SegmentedControl(ui, {{QString(), tr("Landscape")}, {QString(), tr("Portrait")}}, this);
    orientation->setCurrentIndex(state->orientation == Orientation::Portrait ? 1 : 0);
    connect(orientation, &SegmentedControl::currentChanged, this,
            [state](int i) { state->orientation = i == 1 ? Orientation::Portrait : Orientation::Landscape; });
    layout->addWidget(orientation);

    auto* cr = new QVBoxLayout(customRow);
    cr->setContentsMargins(0, 0, 0, 0);
    auto* width = new NumberField(ui, tr("Width"), QStringLiteral("cm"), customRow);
    auto* height = new NumberField(ui, tr("Height"), QStringLiteral("cm"), customRow);
    for (NumberField* f : {width, height}) {
        f->setRange(5, 500);
        f->setDecimals(1);
    }
    width->setValue(state->customCm.width());
    height->setValue(state->customCm.height());
    connect(width, &NumberField::valueEdited, this, [state](double v) { state->customCm.setWidth(v); });
    connect(height, &NumberField::valueEdited, this, [state](double v) { state->customCm.setHeight(v); });
    cr->addWidget(width);
    cr->addWidget(height);
    customRow->setVisible(state->preset == Preset::Custom);
    layout->addWidget(customRow);

    layout->addWidget(panel::section(ui, tr("Apply to"), this));
    auto* scope = new SegmentedControl(ui, {{QString(), tr("This page")}, {QString(), tr("All pages")}, {QString(), tr("New pages")}}, this);
    connect(scope, &SegmentedControl::currentChanged, this, [state](int i) { state->scope = i; });
    layout->addWidget(scope);

    auto* apply = panel::pill(ui, QStringLiteral("check"), tr("Apply page size"), this, [sp, state]() {
        const QSizeF size = state->preset == Preset::Custom
            ? fromCentimetres(state->orientation == Orientation::Portrait && state->customCm.width() > state->customCm.height()
                                  ? state->customCm.transposed()
                                  : state->customCm)
            : sizeFor(state->preset, state->orientation);
        Document& doc = sp->doc;
        switch (state->scope) {
        case 0:
            pageops::setPageSize(doc, {doc.currentPageIndex()}, size);
            break;
        case 1:
            pageops::setPageSize(doc, {}, size);
            doc.setDefaultPageSize(size);
            break;
        default:
            doc.setDefaultPageSize(size);
            sp->toast.showMessage(tr("New pages will be %1.").arg(describe(size)), 2500);
            break;
        }
        sp->popovers.close();
        sp->canvas.fitPage();
    }, true);
    layout->addWidget(panel::row(ui, {apply}, this));
    layout->addWidget(panel::hint(ui, tr("Sizes are logical (1 cm on the page is 1 cm in measurements) and do not "
                                         "depend on the screen. Exports use the full page."),
                                  this));
}

// ===================================================================================== Background

BackgroundPanel::BackgroundPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    auto scopeAll = std::make_shared<bool>(false);
    auto applyColor = [sp, scopeAll](const QColor& c) {
        Document& doc = sp->doc;
        const QVector<int> pages = *scopeAll ? QVector<int>() : QVector<int>{doc.currentPageIndex()};
        pageops::setBackgroundColor(doc, pages, c);
    };

    layout->addWidget(panel::section(ui, tr("Apply to"), this));
    auto* scope = new SegmentedControl(ui, {{QString(), tr("This page")}, {QString(), tr("All pages")}}, this);
    connect(scope, &SegmentedControl::currentChanged, this, [scopeAll](int i) { *scopeAll = i == 1; });
    layout->addWidget(scope);

    layout->addWidget(panel::section(ui, tr("Colour"), this));
    struct Named
    {
        QString label;
        QColor color;
    };
    const QVector<Named> named = {{tr("White"), QColor(255, 255, 255)},
                                  {tr("Black"), QColor(0, 0, 0)},
                                  {tr("Light grey"), QColor(0xe6, 0xe6, 0xe6)},
                                  {tr("Blackboard"), QColor(0x1f, 0x2b, 0x26)}};
    QVector<QWidget*> quick;
    for (const Named& n : named) {
        auto* b = panel::pill(ui, QString(), n.label, this, [applyColor, c = n.color]() { applyColor(c); });
        b->setIndicatorColor(n.color);
        quick.push_back(b);
    }
    layout->addWidget(panel::row(ui, quick, this));

    auto* swatches = new SwatchGrid(ui, 8, this);
    swatches->setColors({QColor(0xff, 0xff, 0xff), QColor(0xf5, 0xf0, 0xe1), QColor(0xe8, 0xf4, 0xfd), QColor(0xe8, 0xf5, 0xe9),
                         QColor(0xff, 0xf8, 0xe1), QColor(0xfc, 0xe4, 0xec), QColor(0xd9, 0xd9, 0xd9), QColor(0x9e, 0x9e, 0x9e),
                         QColor(0x00, 0x00, 0x00), QColor(0x26, 0x32, 0x38), QColor(0x0d, 0x1b, 0x2a), QColor(0x1b, 0x3a, 0x2f),
                         QColor(0x1f, 0x2b, 0x26), QColor(0x3e, 0x27, 0x23), QColor(0x1a, 0x23, 0x7e), QColor(0x4a, 0x14, 0x8c)});
    if (s.doc.currentPage())
        swatches->setCurrent(s.doc.currentPage()->background().background);
    connect(swatches, &SwatchGrid::colorPicked, this, applyColor);
    layout->addWidget(swatches);

    layout->addWidget(panel::section(ui, tr("Custom colour"), this));
    auto* hex = new QLineEdit(s.doc.currentPage() ? s.doc.currentPage()->background().background.name() : QString(), this);
    hex->setPlaceholderText(QStringLiteral("#RRGGBB"));
    hex->setMinimumHeight(ui.theme.dpi(44));
    auto* applyHex = panel::pill(ui, QStringLiteral("check"), tr("Apply"), this, [sp, hex, applyColor]() {
        const QColor c(hex->text().trimmed());
        if (!c.isValid()) {
            sp->toast.showMessage(tr("Enter a colour like #20304A."), 2500);
            return;
        }
        applyColor(c);
    });
    auto* hexRow = new QWidget(this);
    auto* hr = new QHBoxLayout(hexRow);
    hr->setContentsMargins(0, 0, 0, 0);
    hr->addWidget(hex, 1);
    hr->addWidget(applyHex);
    layout->addWidget(hexRow);
    layout->addWidget(panel::hint(ui, tr("The background belongs to the page: it is saved, exported and always stays "
                                         "behind the content. Grid and line patterns are kept."),
                                  this));
}

// =========================================================================================== View

ViewPanel::ViewPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const qreal zoom = s.canvas.zoom();
    layout->addWidget(panel::section(ui, tr("Zoom %1 %").arg(qRound(zoom * 100)), this));
    QStringList labels;
    int current = -1;
    const auto& presets = CanvasWidget::zoomPresets();
    for (int i = 0; i < presets.size(); ++i) {
        labels << QStringLiteral("%1 %").arg(qRound(presets[i] * 100));
        if (std::abs(presets[i] - zoom) < 1e-3)
            current = i;
    }
    layout->addWidget(panel::choices(ui, labels, current, this, [sp](int i) {
        sp->canvas.setZoom(CanvasWidget::zoomPresets().value(i, 1.0));
        sp->popovers.refresh();
    }, 5));
    auto* out = panel::pill(ui, QStringLiteral("zoom-out"), tr("Zoom out"), this, [sp]() {
        sp->canvas.zoomStep(-1);
        sp->popovers.refresh();
    });
    auto* in = panel::pill(ui, QStringLiteral("zoom-in"), tr("Zoom in"), this, [sp]() {
        sp->canvas.zoomStep(1);
        sp->popovers.refresh();
    });
    auto* reset = panel::pill(ui, QString(), tr("Reset 100 %"), this, [sp]() {
        sp->canvas.setZoom(1.0);
        sp->popovers.refresh();
    });
    layout->addWidget(panel::row(ui, {out, in, reset}, this));
    auto* fitPage = panel::pill(ui, QStringLiteral("fit-page"), tr("Fit page"), this, closeThen(sp, [sp]() { sp->canvas.fitPage(); }));
    auto* fitWidth = panel::pill(ui, QStringLiteral("fit-width"), tr("Fit width"), this, closeThen(sp, [sp]() { sp->canvas.fitWidth(); }));
    auto* full = panel::pill(ui, QStringLiteral("fullscreen"), tr("Full screen"), this, closeThen(sp, [sp]() {
        QWidget* w = sp->canvas.window();
        if (w->isFullScreen())
            w->showMaximized();
        else
            w->showFullScreen();
    }));
    layout->addWidget(panel::row(ui, {fitPage, fitWidth, full}, this));
    layout->addWidget(panel::hint(ui, tr("100 % is the lesson's own scale. Zoom changes only the view: sizes, "
                                         "measurements and exports stay the same."),
                                  this));
}

// ========================================================================================== Scale

ScalePanel::ScalePanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const MeasureScale currentScale = s.doc.coordinates().scale();
    auto state = std::make_shared<MeasureScale>(currentScale);
    auto apply = [sp](const MeasureScale& scale) {
        if (!scale.isValid()) {
            sp->toast.showMessage(tr("Both distances must be greater than zero."), 2500);
            return;
        }
        if (scale == sp->doc.coordinates().scale())
            return;
        sp->doc.commands().push(std::make_unique<SetScaleCommand>(scale, tr("Change scale")));
        sp->popovers.refresh();
    };
    layout->addWidget(panel::hint(ui, currentScale.isIdentity()
                                          ? tr("No scale: measurements show board lengths.")
                                          : tr("Current scale: %1").arg(currentScale.text()),
                                  this));

    layout->addWidget(panel::section(ui, tr("Quick scales"), this));
    struct Preset
    {
        QString label;
        MeasureScale scale;
    };
    auto make = [](double b, const char* bu, double r, const char* ru) {
        MeasureScale m;
        m.boardValue = b;
        m.boardUnit = QString::fromLatin1(bu);
        m.realValue = r;
        m.realUnit = QString::fromLatin1(ru);
        return m;
    };
    const QVector<Preset> presets = {{tr("None (1 : 1)"), MeasureScale()},
                                     {QStringLiteral("10 cm = 1 m"), make(10, "cm", 1, "m")},
                                     {QStringLiteral("1 cm = 10 m"), make(1, "cm", 10, "m")},
                                     {QStringLiteral("10 cm = 1 km"), make(10, "cm", 1, "km")},
                                     {QStringLiteral("5 cm = 1 km"), make(5, "cm", 1, "km")},
                                     {QStringLiteral("1 cm = 1 km"), make(1, "cm", 1, "km")}};
    QStringList labels;
    int current = -1;
    for (int i = 0; i < presets.size(); ++i) {
        labels << presets[i].label;
        if (presets[i].scale == currentScale || (i == 0 && currentScale.isIdentity()))
            current = i;
    }
    layout->addWidget(panel::choices(ui, labels, current, this, [presets, apply](int i) { apply(presets[i].scale); }, 3));

    layout->addWidget(panel::section(ui, tr("Custom scale"), this));
    const QStringList unitList = units::lengthUnits();
    auto* board = new NumberField(ui, tr("On the board"), QString(), this);
    board->setRange(1e-9, 1e9);
    board->setValue(state->boardValue);
    connect(board, &NumberField::valueEdited, this, [state](double v) { state->boardValue = v; });
    layout->addWidget(board);
    layout->addWidget(panel::choices(ui, {QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"), QStringLiteral("in")},
                                     QStringList({QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"), QStringLiteral("in")})
                                         .indexOf(state->boardUnit),
                                     this,
                                     [state](int i) {
                                         static const QStringList u = {QStringLiteral("mm"), QStringLiteral("cm"),
                                                                       QStringLiteral("m"), QStringLiteral("in")};
                                         state->boardUnit = u.value(i, QStringLiteral("cm"));
                                     }));
    auto* real = new NumberField(ui, tr("In reality"), QString(), this);
    real->setRange(1e-9, 1e12);
    real->setValue(state->realValue);
    connect(real, &NumberField::valueEdited, this, [state](double v) { state->realValue = v; });
    layout->addWidget(real);
    layout->addWidget(panel::choices(ui, unitList, unitList.indexOf(state->realUnit), this,
                                     [state, unitList](int i) { state->realUnit = unitList.value(i, QStringLiteral("cm")); }));
    auto* applyButton = panel::pill(ui, QStringLiteral("check"), tr("Apply scale"), this, [state, apply]() { apply(*state); }, true);
    layout->addWidget(panel::row(ui, {applyButton}, this));
    layout->addWidget(panel::hint(ui, tr("Lengths, areas, perimeters and vectors are shown in real units. Board "
                                         "distances are lesson units, independent of zoom and screen."),
                                  this));
}

} // namespace cb
