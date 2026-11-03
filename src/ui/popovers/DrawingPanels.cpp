#include "ui/popovers/DrawingPanels.h"

#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/TextObject.h"
#include "tools/EditOperations.h"
#include "tools/SelectionModel.h"
#include "tools/TextTool.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/BoardPanels.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/SegmentedControl.h"
#include "ui/widgets/SwatchGrid.h"
#include "ui/widgets/ToggleRow.h"
#include "ui/widgets/TouchSlider.h"

#include <QGridLayout>
#include <QPainter>

#include <iterator>

namespace cb {

// ============================================================================================ Pen

namespace {
// Segments of the pen style control. The Magic Highlighter sits next to the highlighter; it is
// a mode of the pen, not a stroke style that is ever stored.
constexpr int kMagicSegment = 2;
const StrokeStyle kStyleOfSegment[] = {StrokeStyle::Pen, StrokeStyle::Highlighter, StrokeStyle::Pen /* magic */,
                                       StrokeStyle::Dashed, StrokeStyle::Dotted};

int penSegment(const ToolSettings& t)
{
    if (t.magicHighlighter())
        return kMagicSegment;
    switch (t.penStyle()) {
    case StrokeStyle::Pen: return 0;
    case StrokeStyle::Highlighter: return 1;
    case StrokeStyle::Dashed: return 3;
    case StrokeStyle::Dotted: return 4;
    }
    return 0;
}
} // namespace

PenPanel::PenPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    ToolSettings& tools = s.tools;
    auto* layout = panel::makeLayout(ui, this);

    layout->addWidget(panel::section(ui, tr("Style"), this));
    auto* style = new SegmentedControl(ui, {{QStringLiteral("pen"), tr("Pen")},
                                            {QStringLiteral("highlighter"), tr("Highlighter")},
                                            {QStringLiteral("magic-highlighter"), tr("Magic Highlighter")},
                                            {QStringLiteral("line-dashed"), tr("Dashed")},
                                            {QStringLiteral("line-dotted"), tr("Dotted")}},
                                       this);
    style->setObjectName(QStringLiteral("penStyle"));
    style->setCurrentIndex(penSegment(tools));
    layout->addWidget(style);
    auto* magicHint = panel::hint(ui, tr("Magic Highlighter marks disappear by themselves after a few seconds. "
                                         "They are not saved and do not change the lesson."),
                                  this);
    magicHint->setVisible(tools.magicHighlighter());
    layout->addWidget(magicHint);

    auto* thickness = new TouchSlider(ui, tr("Thickness"), this);
    thickness->setRange(1, tools.usesHighlighterWidth() ? 80 : 40);
    thickness->setStep(0.5);
    thickness->setValue(tools.penWidth());
    thickness->setFormatter([](double v) { return QString::number(v, 'f', v < 10 ? 1 : 0) + QStringLiteral(" px"); });
    ToolSettings* tp = &tools;
    thickness->setPreview([tp](QPainter& p, const QRectF& box, double v) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(StrokeObject::effectiveColor(tp->ink()));
        const qreal r = std::min(box.height() / 2, std::max(1.5, v / 2));
        p.drawEllipse(box.center(), r, r);
    });
    layout->addWidget(thickness);

    layout->addWidget(panel::section(ui, tr("Colour"), this));
    auto* swatches = new SwatchGrid(ui, 6, this);
    swatches->setColors(tools.palette());
    swatches->setCurrent(tools.penColor());
    layout->addWidget(swatches);

    auto* pressure = new ToggleRow(ui, tr("Pressure sensitivity"), this);
    pressure->setDescription(tr("Line width follows stylus pressure"));
    pressure->setChecked(tools.pressureEnabled());
    layout->addWidget(pressure);
    auto* recognition = new ToggleRow(ui, tr("Shape recognition"), this);
    recognition->setDescription(tr("Turns drawn lines, circles and polygons into clean shapes"));
    recognition->setChecked(tools.shapeRecognition());
    layout->addWidget(recognition);

    connect(style, &SegmentedControl::currentChanged, this, [tp, thickness, magicHint](int i) {
        if (i < 0 || i >= int(std::size(kStyleOfSegment)))
            return;
        if (i == kMagicSegment)
            tp->setMagicHighlighter(true); // the pen style underneath is kept for later
        else
            tp->setPenStyle(kStyleOfSegment[i]); // also leaves the Magic Highlighter
        magicHint->setVisible(tp->magicHighlighter());
        thickness->setRange(1, tp->usesHighlighterWidth() ? 80 : 40);
        thickness->setValue(tp->penWidth());
    });
    connect(thickness, &TouchSlider::valueChanged, this, [tp](double v) { tp->setPenWidth(v); });
    connect(swatches, &SwatchGrid::colorPicked, this, [tp, thickness](const QColor& c) {
        tp->setPenColor(c);
        thickness->update();
    });
    connect(pressure, &ToggleRow::toggled, this, [tp](bool on) { tp->setPressureEnabled(on); });
    connect(recognition, &ToggleRow::toggled, this, [tp](bool on) { tp->setShapeRecognition(on); });
}

// ========================================================================================= Eraser

EraserPanel::EraserPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    ToolSettings* tp = &s.tools;
    auto* layout = panel::makeLayout(ui, this);
    layout->addWidget(panel::section(ui, tr("Mode"), this));
    auto* mode = new SegmentedControl(ui, {{QStringLiteral("eraser-stroke"), tr("Stroke")},
                                           {QStringLiteral("eraser-object"), tr("Object")},
                                           {QStringLiteral("eraser"), tr("Area")}},
                                      this);
    mode->setCurrentIndex(static_cast<int>(tp->eraserMode()));
    layout->addWidget(mode);
    auto* description = panel::hint(ui, QString(), this);
    auto describe = [description](int m) {
        switch (static_cast<EraserMode>(m)) {
        case EraserMode::Stroke: description->setText(tr("Removes every ink stroke you touch.")); break;
        case EraserMode::Object: description->setText(tr("Removes any object you touch: ink, shapes, text, images.")); break;
        case EraserMode::Area: description->setText(tr("Rubs out exactly what is under the eraser: ink and shapes, even part of a shape.")); break;
        }
    };
    describe(mode->currentIndex());
    layout->addWidget(description);

    auto* size = new TouchSlider(ui, tr("Size"), this);
    size->setRange(16, 200);
    size->setStep(2);
    size->setValue(tp->eraserSize());
    size->setFormatter([](double v) { return QString::number(qRound(v)) + QStringLiteral(" px"); });
    size->setPreview([](QPainter& p, const QRectF& box, double v) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
        p.setBrush(QColor(255, 255, 255, 30));
        const qreal r = std::min(box.height() / 2, v / 8.0 + 4);
        p.drawEllipse(box.center(), r, r);
    });
    layout->addWidget(size);
    layout->addWidget(panel::hint(ui, tr("Tip: wipe with three fingers held together or the flat of your hand to erase anytime."), this));

    const AppServices* sp = &s;
    // The one Clear Page flow (also in the Page menu).
    auto* clear = panel::pill(ui, QStringLiteral("page-clear"), tr("Clear page"), this,
                              [sp]() { PageActionsPanel::confirmClearPage(*sp); });
    clear->setDanger(true);
    layout->addWidget(panel::row(ui, {clear}, this));

    connect(mode, &SegmentedControl::currentChanged, this, [tp, describe](int i) {
        tp->setEraserMode(static_cast<EraserMode>(i));
        describe(i);
    });
    connect(size, &TouchSlider::valueChanged, this, [tp](double v) { tp->setEraserSize(v); });
}

// ========================================================================================= Select

SelectPanel::SelectPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    ToolSettings* tp = &s.tools;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    layout->addWidget(panel::section(ui, tr("Selection"), this));
    auto* mode = new SegmentedControl(ui, {{QStringLiteral("select-rect"), tr("Rectangle")},
                                           {QStringLiteral("lasso"), tr("Lasso")}},
                                      this);
    mode->setCurrentIndex(static_cast<int>(tp->selectMode()));
    layout->addWidget(mode);
    auto* multi = new ToggleRow(ui, tr("Multi-select"), this);
    multi->setDescription(tr("Each tap adds to the selection"));
    multi->setChecked(tp->multiSelect());
    layout->addWidget(multi);

    auto* all = panel::pill(ui, QStringLiteral("select-all"), tr("Select all"), this, [sp]() {
        sp->edit.selectAll();
        sp->popovers.close();
    });
    auto* paste = panel::pill(ui, QStringLiteral("paste"), tr("Paste"), this, [sp]() {
        if (!sp->edit.paste(sp->canvas.viewCenterInPage()))
            sp->toast.showMessage(tr("The clipboard is empty."), 2000);
        sp->popovers.close();
    });
    paste->setEnabled(s.edit.canPaste());
    layout->addWidget(panel::row(ui, {all, paste}, this));
    layout->addWidget(panel::hint(ui, tr("Drag handles to resize, the round handle to rotate. Double tap text, "
                                        "formulas, graphs and tables to edit them."),
                                  this));

    connect(mode, &SegmentedControl::currentChanged, this, [tp](int i) { tp->setSelectMode(static_cast<SelectMode>(i)); });
    connect(multi, &ToggleRow::toggled, this, [tp](bool on) { tp->setMultiSelect(on); });
}

// ========================================================================================= Shapes

namespace {
class ShapeButton : public QAbstractButton
{
public:
    ShapeButton(const UiContext& ui, ShapeKind kind, QWidget* parent)
        : QAbstractButton(parent)
        , m_ui(ui)
        , m_kind(kind)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(shapeKindLabel(kind));
        setFixedSize(ui.theme.dpi(62), ui.theme.dpi(62));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Theme& t = m_ui.theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(t.dp(2), t.dp(2), -t.dp(2), -t.dp(2));
        p.setPen(isChecked() ? QPen(t.color(ThemeColor::Accent), t.dp(1.5)) : Qt::NoPen);
        p.setBrush(isChecked() ? t.color(ThemeColor::ButtonChecked)
                               : (isDown() ? t.color(ThemeColor::ButtonPressed) : t.color(ThemeColor::ButtonHover)));
        p.drawRoundedRect(r, t.dp(10), t.dp(10));
        const QColor fg = isChecked() ? t.color(ThemeColor::Accent) : t.color(ThemeColor::Text);
        const qreal s = r.width() * 0.52;
        QPen pen(fg, t.dp(2.2), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.translate(r.center());
        if (isLineShape(m_kind)) {
            ShapeObject::paintArrow(p, QPointF(-s / 2, s / 3), QPointF(s / 2, -s / 3), pen, m_kind == ShapeKind::DoubleArrow,
                                    m_kind != ShapeKind::Line);
            return;
        }
        QSizeF size(s, s * 0.78);
        if (m_kind == ShapeKind::Circle || m_kind == ShapeKind::Hexagon || m_kind == ShapeKind::RegularPolygon)
            size = QSizeF(s, s);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(ShapeObject::outlineFor(m_kind, size, 5));
    }

private:
    const UiContext& m_ui;
    ShapeKind m_kind;
};
} // namespace

ShapesPanel::ShapesPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    ToolSettings* tp = &s.tools;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);

    auto* gridWidget = new QWidget(this);
    auto* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(ui.theme.dpi(6));
    const ShapeKind kinds[] = {ShapeKind::Line,        ShapeKind::Arrow,         ShapeKind::DoubleArrow,
                               ShapeKind::Rectangle,   ShapeKind::RoundedRect,   ShapeKind::Circle,
                               ShapeKind::Ellipse,     ShapeKind::Triangle,      ShapeKind::RightTriangle,
                               ShapeKind::Diamond,     ShapeKind::Parallelogram, ShapeKind::Hexagon,
                               ShapeKind::RegularPolygon, ShapeKind::FreePolygon};
    int i = 0;
    for (ShapeKind kind : kinds) {
        auto* b = new ShapeButton(ui, kind, gridWidget);
        b->setChecked(tp->shapeKind() == kind && s.canvas.tools().activeToolId() == ToolId::Shape);
        connect(b, &QAbstractButton::clicked, this, [sp, tp, kind]() {
            tp->setShapeKind(kind);
            sp->activateTool(ToolId::Shape);
            sp->popovers.close();
            if (kind == ShapeKind::FreePolygon)
                sp->toast.showMessage(tr("Tap the corners. Tap the first corner again to finish."), 3500);
        });
        grid->addWidget(b, i / 5, i % 5);
        ++i;
    }
    layout->addWidget(gridWidget);

    layout->addWidget(panel::section(ui, tr("Fill"), this));
    auto* fill = new SegmentedControl(ui, {{QString(), tr("None")}, {QString(), tr("Tint")}, {QString(), tr("Solid")}}, this);
    fill->setCurrentIndex(static_cast<int>(tp->shapeFill()));
    layout->addWidget(fill);
    auto* sides = new TouchSlider(ui, tr("Polygon sides"), this);
    sides->setRange(3, 12);
    sides->setStep(1);
    sides->setValue(tp->polygonSides());
    sides->setFormatter([](double v) { return QString::number(qRound(v)); });
    layout->addWidget(sides);
    layout->addWidget(panel::hint(ui, tr("Shapes use the pen colour and thickness. Hold Shift for squares and 45° lines."), this));

    connect(fill, &SegmentedControl::currentChanged, this, [tp](int v) { tp->setShapeFill(static_cast<ShapeFill>(v)); });
    connect(sides, &TouchSlider::valueChanged, this, [tp](double v) { tp->setPolygonSides(qRound(v)); });
}

// =========================================================================================== Text

void TextPanel::applyFormat(const AppServices& s, const TextFormat& format)
{
    s.tools.setTextFormat(format);
    auto* textTool = static_cast<TextTool*>(s.canvas.tools().tool(ToolId::Text));
    if (textTool && textTool->applyFormat(format))
        return;
    Page* page = s.doc.currentPage();
    if (!page)
        return;
    auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), tr("Format text"));
    for (const ObjectId& id : s.canvas.selectionModel().ids()) {
        DocumentObject* o = page->object(id);
        if (!o || o->type() != ObjectType::Text)
            continue;
        auto after = o->clone();
        static_cast<TextObject*>(after.get())->setFormat(format);
        cmd->add(o->clone(), std::move(after));
    }
    if (!cmd->isEmpty())
        s.doc.commands().push(std::move(cmd));
}

TextPanel::TextPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);

    auto* size = new TouchSlider(ui, tr("Size"), this);
    size->setRange(14, 160);
    size->setStep(2);
    size->setValue(s.tools.textFormat().pixelSize);
    size->setFormatter([](double v) { return QString::number(qRound(v)) + QStringLiteral(" px"); });
    layout->addWidget(size);

    auto makeToggle = [&](const QString& icon, const QString& tip, bool on) {
        auto* b = new TouchButton(ui, icon, tip, TouchButton::Style::Icon, this);
        b->setCheckable(true);
        b->setChecked(on);
        return b;
    };
    const TextFormat f = s.tools.textFormat();
    auto* bold = makeToggle(QStringLiteral("bold"), tr("Bold"), f.bold);
    auto* italic = makeToggle(QStringLiteral("italic"), tr("Italic"), f.italic);
    auto* underline = makeToggle(QStringLiteral("underline"), tr("Underline"), f.underline);
    auto* align = new SegmentedControl(ui, {{QStringLiteral("align-left"), QString()},
                                            {QStringLiteral("align-center"), QString()},
                                            {QStringLiteral("align-right"), QString()}},
                                       this);
    align->setCurrentIndex(f.alignment & Qt::AlignHCenter ? 1 : (f.alignment & Qt::AlignRight ? 2 : 0));
    align->setFixedWidth(ui.theme.dpi(190));
    layout->addWidget(panel::row(ui, {bold, italic, underline, align}, this));

    layout->addWidget(panel::section(ui, tr("Colour"), this));
    auto* swatches = new SwatchGrid(ui, 6, this);
    swatches->setColors(s.tools.palette());
    swatches->setCurrent(f.color);
    layout->addWidget(swatches);

    auto* add = panel::pill(ui, QStringLiteral("text"), tr("Add text box"), this, [sp]() {
        sp->activateTool(ToolId::Text);
        sp->popovers.close();
        sp->toast.showMessage(tr("Tap the board where the text should go."), 2500);
    }, true);
    layout->addWidget(panel::row(ui, {add}, this));

    auto update = [sp, size, bold, italic, underline, align, swatches]() {
        TextFormat format = sp->tools.textFormat();
        format.pixelSize = qRound(size->value());
        format.bold = bold->isChecked();
        format.italic = italic->isChecked();
        format.underline = underline->isChecked();
        format.alignment = align->currentIndex() == 1 ? Qt::AlignHCenter : (align->currentIndex() == 2 ? Qt::AlignRight : Qt::AlignLeft);
        format.color = swatches->current().isValid() ? swatches->current() : format.color;
        applyFormat(*sp, format);
    };
    connect(size, &TouchSlider::sliderReleased, this, update);
    for (TouchButton* b : {bold, italic, underline})
        connect(b, &QAbstractButton::toggled, this, update);
    connect(align, &SegmentedControl::currentChanged, this, update);
    connect(swatches, &SwatchGrid::colorPicked, this, update);
}

// ========================================================================================== Color

ColorPanel::ColorPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(s.ui, this);
    auto* swatches = new SwatchGrid(s.ui, 6, this);
    swatches->setColors(s.tools.palette());
    layout->addWidget(swatches);
    connect(swatches, &SwatchGrid::colorPicked, this, [sp](const QColor& c) {
        sp->edit.setSelectionColor(c);
        sp->popovers.close();
    });
}

} // namespace cb
