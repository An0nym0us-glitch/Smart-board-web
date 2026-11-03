#include "ui/popovers/MathPanels.h"

#include "ai/Recognition.h"
#include "app/AppServices.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/StrokeObject.h"
#include "document/TemplateLibrary.h"
#include "geometry/InstrumentLayer.h"
#include "graph/GraphObject.h"
#include "graph/TableObject.h"
#include "math/equation/EquationObject.h"
#include "tools/GraphTool.h"
#include "tools/MagicEquation.h"
#include "tools/SelectionModel.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/ToggleRow.h"
#include "ui/widgets/TouchSlider.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QThread>

namespace cb {

namespace {

/// Selected object of a given type (single selection).
DocumentObject* selectedOfType(const AppServices& s, ObjectType type)
{
    Page* page = s.doc.currentPage();
    const auto& ids = s.canvas.selectionModel().ids();
    if (!page || ids.size() != 1)
        return nullptr;
    DocumentObject* o = page->object(ids.first());
    return o && o->type() == type ? o : nullptr;
}

QLineEdit* makeLineEdit(const UiContext& ui, const QString& text, QWidget* parent)
{
    auto* e = new QLineEdit(text, parent);
    e->setMinimumHeight(ui.theme.dpi(46));
    e->setFont(ui.theme.font(ui.theme.metric(ThemeMetric::FontSize) + 1));
    return e;
}

} // namespace

// ======================================================================================= Geometry

GeometryPanel::GeometryPanel(const AppServices& s, Mode mode, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);

    if (mode == Mode::Full) {
        layout->addWidget(panel::section(ui, tr("Instruments"), this));
        auto instrument = [sp](Instrument::Kind kind) {
            return [sp, kind]() {
                sp->canvas.instruments().toggle(kind, sp->canvas.viewCenterInPage());
                sp->popovers.close();
            };
        };
        InstrumentLayer& layer = s.canvas.instruments();
        layout->addWidget(panel::tileGrid(ui, 4,
                                          {{QStringLiteral("ruler"), tr("Ruler"), instrument(Instrument::Kind::Ruler),
                                            layer.isShown(Instrument::Kind::Ruler)},
                                           {QStringLiteral("protractor"), tr("Protractor"), instrument(Instrument::Kind::Protractor),
                                            layer.isShown(Instrument::Kind::Protractor)},
                                           {QStringLiteral("set-square"), tr("Set square"), instrument(Instrument::Kind::SetSquare),
                                            layer.isShown(Instrument::Kind::SetSquare)},
                                           {QStringLiteral("compass"), tr("Compass"), instrument(Instrument::Kind::Compass),
                                            layer.isShown(Instrument::Kind::Compass)}},
                                          this));
    }

    layout->addWidget(panel::section(ui, tr("Measure"), this));
    auto measure = [sp](MeasureKind kind, const QString& tip) {
        return [sp, kind, tip]() {
            sp->tools.setMeasureKind(kind);
            sp->activateTool(ToolId::Measure);
            sp->popovers.close();
            sp->toast.showMessage(tip, 3500);
        };
    };
    const bool measuring = s.canvas.tools().activeToolId() == ToolId::Measure;
    const MeasureKind mk = s.tools.measureKind();
    layout->addWidget(panel::tileGrid(
        ui, 4,
        {{QStringLiteral("measure-distance"), tr("Distance"), measure(MeasureKind::Distance, tr("Drag from one point to another.")),
          measuring && mk == MeasureKind::Distance},
         {QStringLiteral("measure-angle"), tr("Angle"),
          measure(MeasureKind::Angle, tr("Drag from the vertex along the first arm, then tap the second arm.")),
          measuring && mk == MeasureKind::Angle},
         {QStringLiteral("measure-slope"), tr("Slope"), measure(MeasureKind::Slope, tr("Drag along the line to measure its slope.")),
          measuring && mk == MeasureKind::Slope},
         {QStringLiteral("measure-area"), tr("Area"),
          measure(MeasureKind::Area, tr("Tap the corners, then tap the first corner again.")),
          measuring && mk == MeasureKind::Area}},
        this));
    {
        // Drawing scale and exact values: shared by measurements, vectors and shapes.
        const MeasureScale scale = s.doc.coordinates().scale();
        auto* scaleButton = panel::pill(ui, QStringLiteral("scale"),
                                        scale.isIdentity() ? tr("Scale: none") : tr("Scale: %1").arg(scale.text()), this,
                                        [sp]() { sp->popovers.push(QStringLiteral("scale")); });
        layout->addWidget(panel::row(ui, {scaleButton}, this));
        layout->addWidget(panel::hint(ui, tr("Exact values: select a measurement, line, vector or shape and tap the "
                                             "precision button (or double-tap it)."),
                                      this));
    }

    if (mode == Mode::Full) {
        layout->addWidget(panel::section(ui, tr("Construct"), this));
        auto construct = [sp](ConstructKind kind) {
            return [sp, kind]() {
                sp->tools.setConstructKind(kind);
                sp->activateTool(ToolId::Construct);
                sp->popovers.close();
            };
        };
        const bool constructing = s.canvas.tools().activeToolId() == ToolId::Construct;
        const ConstructKind ck = s.tools.constructKind();
        layout->addWidget(panel::tileGrid(ui, 5,
                                          {{QStringLiteral("point"), tr("Point"), construct(ConstructKind::Point),
                                            constructing && ck == ConstructKind::Point},
                                           {QStringLiteral("segment"), tr("Segment"), construct(ConstructKind::Segment),
                                            constructing && ck == ConstructKind::Segment},
                                           {QStringLiteral("line"), tr("Line"), construct(ConstructKind::Line),
                                            constructing && ck == ConstructKind::Line},
                                           {QStringLiteral("ray"), tr("Ray"), construct(ConstructKind::Ray),
                                            constructing && ck == ConstructKind::Ray},
                                           {QStringLiteral("vector"), tr("Vector"), construct(ConstructKind::Vector),
                                            constructing && ck == ConstructKind::Vector}},
                                          this));

        layout->addWidget(panel::section(ui, tr("Coordinates"), this));
        auto* plane = panel::pill(ui, QStringLiteral("coordinates"), tr("Coordinate plane background"), this, [sp]() {
            Page* page = sp->doc.currentPage();
            if (!page)
                return;
            TemplateSpec spec = sp->templates.find(QStringLiteral("coordinate"));
            sp->doc.commands().push(std::make_unique<ModifyPageCommand>(page->id(), page->name(), spec, tr("Coordinate plane")));
            sp->popovers.close();
        });
        layout->addWidget(panel::row(ui, {plane}, this));
    }

    auto* labels = new ToggleRow(ui, tr("Show coordinates and lengths"), this);
    labels->setChecked(s.tools.showCoordinates());
    auto* snap = new ToggleRow(ui, tr("Snap to grid and points"), this);
    snap->setChecked(s.tools.snapToGrid());
    layout->addWidget(labels);
    layout->addWidget(snap);
    ToolSettings* tp = &s.tools;
    connect(labels, &ToggleRow::toggled, this, [tp](bool on) { tp->setShowCoordinates(on); });
    connect(snap, &ToggleRow::toggled, this, [tp](bool on) { tp->setSnapToGrid(on); });
}

// ======================================================================================= Equation

namespace {
class FormulaPreview : public QWidget
{
public:
    FormulaPreview(const UiContext& ui, QWidget* parent)
        : QWidget(parent)
        , m_ui(ui)
    {
        setMinimumHeight(ui.theme.dpi(130));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setLatex(const QString& latex)
    {
        m_latex = latex;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Theme& t = m_ui.theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1f, 0x2b, 0x26));
        p.drawRoundedRect(QRectF(rect()), t.dp(12), t.dp(12));
        if (m_latex.trimmed().isEmpty()) {
            p.setPen(t.color(ThemeColor::TextMuted));
            p.setFont(t.font());
            p.drawText(rect(), Qt::AlignCenter, tr("Type a formula or tap the symbols below"));
            return;
        }
        EquationObject::paintFormula(p, m_latex, QRectF(rect()).adjusted(t.dp(12), t.dp(8), -t.dp(12), -t.dp(8)), t.dp(40),
                                     QColor(245, 245, 240));
    }

private:
    static QString tr(const char* s) { return QCoreApplication::translate("EquationPanel", s); }
    const UiContext& m_ui;
    QString m_latex;
};
} // namespace

EquationPanel::EquationPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
{
    const UiContext& ui = s.ui;
    auto* layout = panel::makeLayout(ui, this);
    auto* preview = new FormulaPreview(ui, this);
    m_preview = preview;
    layout->addWidget(preview);

    QString initial = s.popovers.takeEquationPrefill();
    const ObjectId target = s.popovers.equationTarget();
    if (Page* page = s.doc.currentPage()) {
        if (DocumentObject* o = page->object(target); o && o->type() == ObjectType::Equation) {
            initial = static_cast<EquationObject*>(o)->latex();
            m_size = static_cast<EquationObject*>(o)->pixelSize();
        }
    }
    m_input = makeLineEdit(ui, initial, this);
    m_input->setPlaceholderText(QStringLiteral("\\frac{a}{b},  x^{2},  \\sqrt{x},  \\int_0^1 f(x)\\,dx"));
    layout->addWidget(m_input);
    preview->setLatex(initial);
    connect(m_input, &QLineEdit::textChanged, this, [preview](const QString& t) { preview->setLatex(t); });
    connect(m_input, &QLineEdit::returnPressed, this, &EquationPanel::commit);

    struct Key
    {
        const char* label;
        const char* snippet;
    };
    static const Key keys[] = {
        {"x²", "^{2}"},           {"xₙ", "_{}"},          {"a⁄b", "\\frac{}{}"},     {"√x", "\\sqrt{}"},
        {"ⁿ√x", "\\sqrt[n]{}"},   {"∫", "\\int_{a}^{b} "}, {"∑", "\\sum_{i=1}^{n} "}, {"lim", "\\lim_{x \\to 0} "},
        {"( )", "\\left( \\right)"}, {"vec", "\\vec{}"},  {"[ ]", "\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}"},
        {"{", "\\begin{cases} x & x \\geq 0 \\\\ -x & x < 0 \\end{cases}"},
        {"π", "\\pi "},           {"θ", "\\theta "},      {"α", "\\alpha "},          {"β", "\\beta "},
        {"Δ", "\\Delta "},        {"∞", "\\infty "},      {"±", "\\pm "},             {"×", "\\times "},
        {"÷", "\\div "},          {"·", "\\cdot "},       {"≤", "\\leq "},            {"≥", "\\geq "},
        {"≠", "\\neq "},          {"≈", "\\approx "},     {"→", "\\to "},             {"°", "^{\\circ}"},
    };
    auto* keysWidget = new QWidget(this);
    auto* grid = new QGridLayout(keysWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(ui.theme.dpi(6));
    int i = 0;
    for (const Key& k : keys) {
        auto* b = new TouchButton(ui, QString(), QString::fromUtf8(k.label), TouchButton::Style::Pill, keysWidget);
        b->setMinimumWidth(ui.theme.dpi(58));
        const QString snippet = QString::fromUtf8(k.snippet);
        connect(b, &QAbstractButton::clicked, this, [this, snippet]() { insertSnippet(snippet); });
        grid->addWidget(b, i / 8, i % 8);
        ++i;
    }
    layout->addWidget(keysWidget);

    auto* size = new TouchSlider(ui, tr("Size"), this);
    size->setRange(24, 200);
    size->setStep(2);
    size->setValue(m_size);
    size->setFormatter([](double v) { return QString::number(qRound(v)) + QStringLiteral(" px"); });
    connect(size, &TouchSlider::valueChanged, this, [this](double v) { m_size = v; });
    layout->addWidget(size);

    const bool editing = !initial.isEmpty() || !target.isNull();
    auto* insert = panel::pill(ui, QStringLiteral("check"), editing ? tr("Update formula") : tr("Insert formula"), this,
                               [this]() { commit(); }, true);
    QVector<QWidget*> buttons{insert};
    if (!editing) {
        // The optional Magic Equation Maker: only for handwriting the teacher selected explicitly.
        const AppServices* sp = &s;
        auto* magicButton = panel::pill(ui, QStringLiteral("magic"), tr("✨ Magic Equation Maker"), this, [sp]() {
            Page* page = sp->doc.currentPage();
            const QVector<ObjectId> ids = sp->canvas.selectionModel().ids();
            if (!page || !magic::isHandwriting(*page, ids)) {
                sp->toast.showMessage(tr("Write the equation, select the handwriting with SELECT, then tap ✨."), 4000);
                return;
            }
            QRectF bounds;
            for (const ObjectId& id : ids)
                bounds = bounds.isNull() ? page->object(id)->sceneBounds() : bounds.united(page->object(id)->sceneBounds());
            sp->popovers.openMagicEquation(ids, sp->canvas.pageRectToWidget(bounds));
        });
        buttons.push_back(magicButton);
    }
    layout->addWidget(panel::row(ui, buttons, this));
    m_input->setFocus();
}

void EquationPanel::insertSnippet(const QString& snippet)
{
    const int pos = m_input->cursorPosition();
    QString text = m_input->text();
    text.insert(pos, snippet);
    m_input->setText(text);
    const int hole = snippet.indexOf(QStringLiteral("{}"));
    m_input->setCursorPosition(hole >= 0 ? pos + hole + 1 : pos + snippet.size());
    m_input->setFocus();
}

void EquationPanel::commit()
{
    const QString latex = m_input->text().trimmed();
    Page* page = m_s.doc.currentPage();
    if (!page)
        return;
    const ObjectId target = m_s.popovers.equationTarget();
    DocumentObject* existing = page->object(target);
    if (existing && existing->type() == ObjectType::Equation) {
        if (latex.isEmpty()) {
            m_s.doc.commands().push(std::make_unique<RemoveObjectsCommand>(page->id(), std::vector<ObjectId>{target}, tr("Delete formula")));
        } else {
            auto after = existing->clone();
            static_cast<EquationObject*>(after.get())->setLatex(latex);
            static_cast<EquationObject*>(after.get())->setPixelSize(m_size);
            auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), tr("Edit formula"));
            cmd->add(existing->clone(), std::move(after));
            m_s.doc.commands().push(std::move(cmd));
        }
    } else if (!latex.isEmpty()) {
        auto eq = EquationObject::create(latex, m_s.canvas.viewCenterInPage(), m_size, m_s.tools.penColor());
        const ObjectId id = eq->id();
        std::vector<ObjectPtr> objects;
        objects.push_back(std::move(eq));
        m_s.doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), tr("Insert formula")));
        m_s.activateTool(ToolId::Select);
        m_s.canvas.selectionModel().setSingle(id);
    }
    m_s.popovers.clearEquationTarget();
    m_s.popovers.close();
}

// ======================================================================================= Function

GraphObject* FunctionPanel::graph() const
{
    Page* page = m_s.doc.currentPage();
    DocumentObject* o = page ? page->object(m_graphId) : nullptr;
    return o && o->type() == ObjectType::Graph ? static_cast<GraphObject*>(o) : nullptr;
}

void FunctionPanel::modify(const QString& text, const std::function<void(GraphObject&)>& change)
{
    GraphTool::modifyGraph(m_s.canvas, m_graphId, text, change);
}

void FunctionPanel::rebuild()
{
    m_s.popovers.refresh();
}

FunctionPanel::FunctionPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
    , m_s(s)
{
    const UiContext& ui = s.ui;
    auto* layout = panel::makeLayout(ui, this);
    if (DocumentObject* o = selectedOfType(s, ObjectType::Graph))
        m_graphId = o->id();
    GraphObject* g = graph();

    if (!g) {
        layout->addWidget(panel::hint(ui, tr("Insert a graph, then type functions such as  2x + 1,  a·sin(bx),  x² − 4."), this));
        auto* insert = panel::pill(ui, QStringLiteral("function"), tr("Insert graph"), this, [this]() {
            Page* page = m_s.doc.currentPage();
            if (!page)
                return;
            auto graph = GraphObject::create(m_s.canvas.viewCenterInPage(), QSizeF(760, 560));
            graph->addFunction(QStringLiteral("a*x^2 + b*x + c"), QColor());
            const ObjectId id = graph->id();
            std::vector<ObjectPtr> objects;
            objects.push_back(std::move(graph));
            m_s.doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), tr("Insert graph")));
            m_s.canvas.selectionModel().setSingle(id);
            m_s.activateTool(ToolId::Graph);
            rebuild();
        }, true);
        layout->addWidget(panel::row(ui, {insert}, this));
        return;
    }

    // Functions.
    layout->addWidget(panel::section(ui, tr("Functions"), this));
    const char* names = "fghpqrs";
    for (int i = 0; i < static_cast<int>(g->functions().size()); ++i) {
        const GraphFunction& f = g->functions()[static_cast<size_t>(i)];
        auto* rowWidget = new QWidget(this);
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(ui.theme.dpi(6));
        auto* visible = new TouchButton(ui, f.visible ? QStringLiteral("eye") : QStringLiteral("eye-off"),
                                        tr("Show / hide"), TouchButton::Style::Icon, rowWidget);
        visible->setIndicatorColor(f.visible ? f.color : QColor());
        auto* label = new QLabel(QStringLiteral("%1(x) =").arg(QLatin1Char(i < 7 ? names[i] : 'f')), rowWidget);
        label->setFont(ui.theme.font(ui.theme.metric(ThemeMetric::FontSize) + 1, true));
        label->setStyleSheet(QStringLiteral("color: %1;").arg(f.color.name()));
        auto* edit = makeLineEdit(ui, math::Expression::stripDefinition(f.expression), rowWidget);
        edit->setMinimumWidth(ui.theme.dpi(260));
        auto* remove = new TouchButton(ui, QStringLiteral("trash"), tr("Remove"), TouchButton::Style::Icon, rowWidget);
        row->addWidget(visible);
        row->addWidget(label);
        row->addWidget(edit, 1);
        row->addWidget(remove);
        layout->addWidget(rowWidget);
        const QString err = g->functionError(i);
        if (!err.isEmpty()) {
            auto* errLabel = panel::hint(ui, err, this);
            errLabel->setStyleSheet(QStringLiteral("color: %1;").arg(ui.theme.color(ThemeColor::Danger).name()));
            layout->addWidget(errLabel);
        }
        connect(edit, &QLineEdit::editingFinished, this, [this, i, edit]() {
            GraphObject* gr = graph();
            if (!gr || i >= static_cast<int>(gr->functions().size())
                || gr->functions()[static_cast<size_t>(i)].expression == edit->text())
                return;
            modify(tr("Edit function"), [i, text = edit->text()](GraphObject& go) { go.setFunctionExpression(i, text); });
            // Parameters and error messages depend on the expression: rebuild the panel.
            rebuild();
        });
        connect(visible, &QAbstractButton::clicked, this, [this, i]() {
            modify(tr("Show function"), [i](GraphObject& go) { go.setFunctionVisible(i, !go.functions()[static_cast<size_t>(i)].visible); });
            rebuild();
        });
        connect(remove, &QAbstractButton::clicked, this, [this, i]() {
            modify(tr("Remove function"), [i](GraphObject& go) { go.removeFunction(i); });
            rebuild();
        });
    }
    auto* add = panel::pill(ui, QStringLiteral("plus"), tr("Add function"), this, [this]() {
        modify(tr("Add function"), [](GraphObject& go) { go.addFunction(QString(), QColor()); });
        rebuild();
    });
    layout->addWidget(panel::row(ui, {add}, this));

    // Parameters.
    if (!g->parameters().empty()) {
        layout->addWidget(panel::section(ui, tr("Parameters"), this));
        for (const GraphParameter& prm : g->parameters()) {
            auto* slider = new TouchSlider(ui, prm.name, this);
            slider->setRange(prm.min, prm.max);
            slider->setStep(0.1);
            slider->setValue(prm.value);
            slider->setFormatter([](double v) { return geom::formatNumber(v, 1); });
            const QString name = prm.name;
            connect(slider, &TouchSlider::valueChanged, this, [this, name](double v) {
                Page* page = m_s.doc.currentPage();
                GraphObject* gr = graph();
                if (!page || !gr)
                    return;
                if (!m_sliderBefore)
                    m_sliderBefore = gr->clone();
                ObjectPtr c = gr->clone();
                static_cast<GraphObject*>(c.get())->setParameterValue(name, v);
                m_s.doc.replaceObject(page->id(), std::move(c));
            });
            connect(slider, &TouchSlider::sliderReleased, this, [this, name]() {
                Page* page = m_s.doc.currentPage();
                GraphObject* gr = graph();
                if (!page || !gr || !m_sliderBefore)
                    return;
                auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), tr("Change %1").arg(name));
                cmd->add(std::move(m_sliderBefore), gr->clone());
                m_s.doc.commands().pushApplied(std::move(cmd));
            });
            layout->addWidget(slider);
        }
    }

    // View.
    layout->addWidget(panel::section(ui, tr("View"), this));
    auto zoom = [this](qreal factor) {
        return [this, factor]() {
            modify(tr("Zoom graph"), [factor](GraphObject& go) { go.zoomAtLocal(go.mathToLocal(go.window().center()), factor); });
        };
    };
    auto* zoomIn = panel::pill(ui, QStringLiteral("zoom-in"), QString(), this, zoom(1.5));
    auto* zoomOut = panel::pill(ui, QStringLiteral("zoom-out"), QString(), this, zoom(1.0 / 1.5));
    auto* reset = panel::pill(ui, QStringLiteral("reset"), tr("Reset"), this, [this]() {
        modify(tr("Reset graph view"), [](GraphObject& go) { go.resetView(); });
    });
    auto* grid = panel::pill(ui, QStringLiteral("grid"), tr("Grid"), this, [this]() {
        modify(tr("Toggle grid"), [](GraphObject& go) { go.setShowGrid(!go.showGrid()); });
    });
    layout->addWidget(panel::row(ui, {zoomIn, zoomOut, reset, grid}, this));

    // Points and vectors.
    layout->addWidget(panel::section(ui, tr("Points and vectors"), this));
    auto* pointMode = new ToggleRow(ui, tr("Tap to place points"), this);
    pointMode->setDescription(tr("Points snap onto curves; tap a point again to remove it"));
    auto* graphTool = static_cast<GraphTool*>(s.canvas.tools().tool(ToolId::Graph));
    pointMode->setChecked(graphTool && graphTool->pointMode());
    connect(pointMode, &ToggleRow::toggled, this, [this, graphTool](bool on) {
        if (graphTool)
            graphTool->setPointMode(on);
        m_s.activateTool(ToolId::Graph);
    });
    layout->addWidget(pointMode);
    auto* vectorEdit = makeLineEdit(ui, QString(), this);
    vectorEdit->setPlaceholderText(tr("Vector: 3, 2  or  1, 1, 4, 3"));
    auto* addVector = panel::pill(ui, QStringLiteral("vector"), tr("Add"), this, [this, vectorEdit]() {
        static const QRegularExpression num(QStringLiteral("-?\\d+(?:[.,]\\d+)?"));
        QVector<double> values;
        auto it = num.globalMatch(vectorEdit->text().replace(QChar(0x2212), QLatin1Char('-')));
        while (it.hasNext())
            values.push_back(it.next().captured().replace(QLatin1Char(','), QLatin1Char('.')).toDouble());
        if (values.size() != 2 && values.size() != 4) {
            m_s.toast.showMessage(tr("Enter 2 numbers (components) or 4 numbers (start and end)."), 3000);
            return;
        }
        const QPointF from = values.size() == 4 ? QPointF(values[0], values[1]) : QPointF(0, 0);
        const QPointF to = values.size() == 4 ? QPointF(values[2], values[3]) : QPointF(values[0], values[1]);
        modify(tr("Add vector"), [from, to](GraphObject& go) { go.addVector(from, to); });
        vectorEdit->clear();
    });
    auto* clearPoints = panel::pill(ui, QStringLiteral("trash"), tr("Clear points"), this, [this]() {
        modify(tr("Clear points"), [](GraphObject& go) { go.clearPointsAndVectors(); });
    });
    auto* vectorRow = new QWidget(this);
    auto* vr = new QHBoxLayout(vectorRow);
    vr->setContentsMargins(0, 0, 0, 0);
    vr->setSpacing(ui.theme.dpi(6));
    vr->addWidget(vectorEdit, 1);
    vr->addWidget(addVector);
    vr->addWidget(clearPoints);
    layout->addWidget(vectorRow);

    // Value table.
    layout->addWidget(panel::section(ui, tr("Value table"), this));
    auto* from = makeLineEdit(ui, QStringLiteral("-3"), this);
    auto* to = makeLineEdit(ui, QStringLiteral("3"), this);
    auto* step = makeLineEdit(ui, QStringLiteral("1"), this);
    for (QLineEdit* e : {from, to, step})
        e->setMaximumWidth(ui.theme.dpi(80));
    auto* makeTable = panel::pill(ui, QStringLiteral("table"), tr("Create"), this, [this, from, to, step]() {
        GraphObject* gr = graph();
        Page* page = m_s.doc.currentPage();
        if (!gr || !page)
            return;
        const double x0 = from->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        const double x1 = to->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        const double dx = std::abs(step->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble());
        if (dx <= 0 || x1 < x0 || (x1 - x0) / dx > 40) {
            m_s.toast.showMessage(tr("Choose a range with at most 40 rows."), 3000);
            return;
        }
        QVector<int> columns;
        for (int i = 0; i < static_cast<int>(gr->functions().size()); ++i)
            if (gr->functions()[static_cast<size_t>(i)].visible && gr->functionError(i).isEmpty()
                && !gr->functions()[static_cast<size_t>(i)].expression.trimmed().isEmpty())
                columns.push_back(i);
        const int rows = static_cast<int>(std::floor((x1 - x0) / dx + 1e-9)) + 2;
        const QRectF gb = gr->sceneBounds();
        auto table = TableObject::create(rows, columns.size() + 1, QPointF());
        const char* names = "fghpqrs";
        table->setCell(0, 0, QStringLiteral("x"));
        for (int c = 0; c < columns.size(); ++c)
            table->setCell(0, c + 1, QStringLiteral("%1(x)").arg(QLatin1Char(columns[c] < 7 ? names[columns[c]] : 'f')));
        for (int r = 1; r < rows; ++r) {
            const double x = x0 + (r - 1) * dx;
            table->setCell(r, 0, geom::formatNumber(x, 3));
            for (int c = 0; c < columns.size(); ++c)
                table->setCell(r, c + 1, geom::formatNumber(gr->evaluate(columns[c], x), 3));
        }
        table->setPosition(QPointF(gb.right() + table->localBounds().width() / 2 + 30, gb.top() + table->localBounds().height() / 2));
        const QRectF tb = table->sceneBounds();
        std::vector<ObjectPtr> objects;
        objects.push_back(std::move(table));
        m_s.doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), tr("Value table")));
        m_s.canvas.ensureVisible(tb);
    });
    auto* tableRow = new QWidget(this);
    auto* tr2 = new QHBoxLayout(tableRow);
    tr2->setContentsMargins(0, 0, 0, 0);
    tr2->setSpacing(ui.theme.dpi(6));
    tr2->addWidget(new QLabel(tr("x from"), tableRow));
    tr2->addWidget(from);
    tr2->addWidget(new QLabel(tr("to"), tableRow));
    tr2->addWidget(to);
    tr2->addWidget(new QLabel(tr("step"), tableRow));
    tr2->addWidget(step);
    tr2->addStretch(1);
    tr2->addWidget(makeTable);
    layout->addWidget(tableRow);
    layout->addWidget(panel::hint(ui, tr("Drag inside the graph to pan, pinch to zoom."), this));
}

// ========================================================================================== Table

TablePanel::TablePanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    auto* selected = static_cast<TableObject*>(selectedOfType(s, ObjectType::Table));
    auto* rows = new TouchSlider(ui, tr("Rows"), this);
    rows->setRange(1, 30);
    rows->setStep(1);
    rows->setValue(selected ? selected->rowCount() : 4);
    rows->setFormatter([](double v) { return QString::number(qRound(v)); });
    auto* cols = new TouchSlider(ui, tr("Columns"), this);
    cols->setRange(1, 12);
    cols->setStep(1);
    cols->setValue(selected ? selected->columnCount() : 3);
    cols->setFormatter([](double v) { return QString::number(qRound(v)); });
    layout->addWidget(rows);
    layout->addWidget(cols);
    auto* header = new ToggleRow(ui, tr("Header row"), this);
    header->setChecked(selected ? selected->hasHeader() : true);
    layout->addWidget(header);

    if (selected) {
        const ObjectId id = selected->id();
        auto apply = [sp, id, rows, cols, header]() {
            Page* page = sp->doc.currentPage();
            DocumentObject* o = page ? page->object(id) : nullptr;
            if (!o || o->type() != ObjectType::Table)
                return;
            auto after = o->clone();
            auto* t = static_cast<TableObject*>(after.get());
            t->setDimensions(qRound(rows->value()), qRound(cols->value()));
            t->setHeader(header->isChecked());
            auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), tr("Change table"));
            cmd->add(o->clone(), std::move(after));
            sp->doc.commands().push(std::move(cmd));
        };
        connect(rows, &TouchSlider::sliderReleased, this, apply);
        connect(cols, &TouchSlider::sliderReleased, this, apply);
        connect(header, &ToggleRow::toggled, this, apply);
    } else {
        auto* insert = panel::pill(ui, QStringLiteral("table"), tr("Insert table"), this, [sp, rows, cols, header]() {
            Page* page = sp->doc.currentPage();
            if (!page)
                return;
            auto table = TableObject::create(qRound(rows->value()), qRound(cols->value()), sp->canvas.viewCenterInPage());
            table->setHeader(header->isChecked());
            const ObjectId id = table->id();
            std::vector<ObjectPtr> objects;
            objects.push_back(std::move(table));
            sp->doc.commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects), tr("Insert table")));
            sp->activateTool(ToolId::Select);
            sp->canvas.selectionModel().setSingle(id);
            sp->popovers.close();
        }, true);
        layout->addWidget(panel::row(ui, {insert}, this));
    }
    layout->addWidget(panel::hint(ui, tr("Double tap a cell to type in it. Enter moves down, Tab moves right."), this));
}

} // namespace cb
