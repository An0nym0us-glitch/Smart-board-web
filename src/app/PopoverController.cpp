#include "app/PopoverController.h"

#include "app/AppServices.h"
#include "ui/Popover.h"
#include "ui/popovers/BoardPanels.h"
#include "ui/popovers/CalculatorPanel.h"
#include "ui/popovers/DrawingPanels.h"
#include "ui/popovers/LessonPanels.h"
#include "ui/popovers/MathPanels.h"
#include "ui/popovers/MagicEquationPanel.h"
#include "ui/popovers/PageNavigatorPanel.h"
#include "ui/popovers/PropertiesPanel.h"
#include "document/Document.h"

namespace cb {

PopoverController::PopoverController(PopoverHost& host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
    connect(&host, &PopoverHost::opened, this, &PopoverController::opened);
    connect(&host, &PopoverHost::closed, this, &PopoverController::closed);
}

Popover* PopoverController::create(const QString& key)
{
    if (!m_services)
        return nullptr;
    const AppServices& s = *m_services;
    QString title;
    QWidget* content = nullptr;
    int width = 320;
    if (key == QLatin1String("more")) {
        title = tr("More");
        content = new MorePanel(s);
        width = 420;
    } else if (key == QLatin1String("pen")) {
        title = tr("Pen");
        content = new PenPanel(s);
        width = 330;
    } else if (key == QLatin1String("eraser")) {
        title = tr("Eraser");
        content = new EraserPanel(s);
    } else if (key == QLatin1String("select")) {
        title = tr("Select");
        content = new SelectPanel(s);
    } else if (key == QLatin1String("shapes")) {
        title = tr("Shapes");
        content = new ShapesPanel(s);
    } else if (key == QLatin1String("text")) {
        title = tr("Text");
        content = new TextPanel(s);
        width = 340;
    } else if (key == QLatin1String("color")) {
        title = tr("Colour");
        content = new ColorPanel(s);
        width = 300;
    } else if (key == QLatin1String("geometry")) {
        title = tr("Geometry");
        content = new GeometryPanel(s, GeometryPanel::Mode::Full);
        width = 460;
    } else if (key == QLatin1String("measure")) {
        title = tr("Measurements");
        content = new GeometryPanel(s, GeometryPanel::Mode::MeasureOnly);
        width = 420;
    } else if (key == QLatin1String("equation")) {
        title = tr("Equation");
        content = new EquationPanel(s);
        width = 560;
    } else if (key == QLatin1String("function")) {
        title = tr("Function / Graph");
        content = new FunctionPanel(s);
        width = 480;
    } else if (key == QLatin1String("table")) {
        title = tr("Table");
        content = new TablePanel(s);
    } else if (key == QLatin1String("insert")) {
        title = tr("Insert");
        content = new InsertPanel(s);
        width = 420;
    } else if (key == QLatin1String("edit")) {
        title = tr("Edit");
        content = new EditPanel(s);
        width = 420;
    } else if (key == QLatin1String("pageactions")) {
        title = tr("Page");
        content = new PageActionsPanel(s);
        width = 500;
    } else if (key == QLatin1String("pagesize")) {
        title = tr("Page size");
        content = new PageSetupPanel(s);
        width = 520;
    } else if (key == QLatin1String("background")) {
        title = tr("Background");
        content = new BackgroundPanel(s);
        width = 460;
    } else if (key == QLatin1String("view")) {
        title = tr("View");
        content = new ViewPanel(s);
        width = 480;
    } else if (key == QLatin1String("scale")) {
        title = tr("Scale");
        content = new ScalePanel(s);
        width = 480;
    } else if (key == QLatin1String("properties")) {
        Page* page = s.doc.currentPage();
        DocumentObject* o = page ? page->object(m_propertiesTarget) : nullptr;
        title = o ? PropertiesPanel::titleFor(*o) : tr("Properties");
        content = new PropertiesPanel(s, m_propertiesTarget);
        width = 460;
    } else if (key == QLatin1String("magic")) {
        title = tr("✨ Magic Equation Maker");
        content = new MagicEquationPanel(s, m_magicTargets);
        width = 520;
    } else if (key == QLatin1String("lesson")) {
        title = tr("File");
        content = new LessonPanel(s);
        width = 420;
    } else if (key == QLatin1String("templates")) {
        title = tr("Templates");
        content = new TemplatesPanel(s);
        width = 560;
    } else if (key == QLatin1String("import")) {
        title = tr("Import");
        content = new ImportPanel(s);
        width = 420;
    } else if (key == QLatin1String("export")) {
        title = tr("Export");
        content = new ExportPanel(s);
        width = 520;
    } else if (key == QLatin1String("pdf")) {
        title = tr("PDF");
        content = new ExportPagesPanel(s, ExportController::Format::Pdf);
        width = 400;
    } else if (key == QLatin1String("pptx")) {
        title = tr("PowerPoint");
        content = new ExportPagesPanel(s, ExportController::Format::Pptx);
        width = 400;
    } else if (key == QLatin1String("calculator")) {
        title = tr("Calculator");
        content = new CalculatorPanel(s);
        width = 470;
    } else if (key == QLatin1String("settings")) {
        title = tr("Settings");
        content = new SettingsPanel(s);
        width = 400;
    } else if (key == QLatin1String("pages")) {
        title = tr("Pages");
        content = new PageNavigatorPanel(s);
        width = 560;
    } else {
        return nullptr;
    }
    auto* popover = new Popover(s.ui, key, title, &m_host);
    popover->setMinimumContentWidth(width);
    popover->setContent(content);
    return popover;
}

void PopoverController::toggle(const QString& key, QWidget* anchor)
{
    if (m_host.isOpen() && m_host.anchorWidget() == anchor) {
        m_host.closeAll();
        return;
    }
    open(key, anchor);
}

void PopoverController::open(const QString& key, QWidget* anchor)
{
    if (key != QLatin1String("equation"))
        m_equationTarget = ObjectId();
    if (Popover* p = create(key))
        m_host.open(p, anchor);
}

void PopoverController::openAt(const QString& key, const QRect& anchorRect)
{
    if (Popover* p = create(key))
        m_host.openAt(p, anchorRect);
}

void PopoverController::push(const QString& key)
{
    if (key == QLatin1String("equation"))
        m_equationTarget = ObjectId();
    if (Popover* p = create(key))
        m_host.push(p);
}

void PopoverController::close()
{
    m_host.closeAll();
}

void PopoverController::refresh()
{
    const QString key = m_host.currentKey();
    if (key.isEmpty())
        return;
    Popover* fresh = create(key);
    if (!fresh)
        return;
    // Replace the top popover in place without animation.
    m_host.replaceTop(fresh);
}

bool PopoverController::isOpen(const QString& key) const
{
    return m_host.currentKey() == key;
}

QString PopoverController::currentKey() const
{
    return m_host.currentKey();
}

void PopoverController::editEquation(const ObjectId& id, const QRect& anchorRect)
{
    m_equationTarget = id;
    if (Popover* p = create(QStringLiteral("equation")))
        m_host.openAt(p, anchorRect);
}

void PopoverController::editProperties(const ObjectId& id, const QRect& anchorRect)
{
    m_propertiesTarget = id;
    if (Popover* p = create(QStringLiteral("properties")))
        m_host.openAt(p, anchorRect);
}

void PopoverController::openMagicEquation(const QVector<ObjectId>& strokes, const QRect& anchorRect)
{
    m_magicTargets = strokes;
    if (Popover* p = create(QStringLiteral("magic")))
        m_host.openAt(p, anchorRect);
}

} // namespace cb
