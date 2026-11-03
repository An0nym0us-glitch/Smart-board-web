#include "app/MainWindow.h"

#include "ai/MathInkRecognizer.h"
#include "ai/Recognition.h"
#include "app/AppServices.h"
#include "app/AppSettings.h"
#include "app/ExportController.h"
#include "app/LessonController.h"
#include "app/PopoverController.h"
#include "storage/AutosaveManager.h"
#include "canvas/CanvasWidget.h"
#include "canvas/ThumbnailCache.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/PageOperations.h"
#include "document/TemplateLibrary.h"
#include "geometry/InstrumentLayer.h"
#include "geometry/Instruments.h"
#include "graph/TableObject.h"
#include "tools/EditOperations.h"
#include "tools/EraserTool.h"
#include "tools/MagicEquation.h"
#include "tools/SelectionModel.h"
#include "tools/TextTool.h"
#include "tools/ToolController.h"
#include "tools/ToolSettings.h"
#include "ui/Popover.h"
#include "ui/Ribbon.h"
#include "ui/SelectionBar.h"
#include "ui/TableCellEditor.h"
#include "ui/Toast.h"
#include "ui/popovers/PropertiesPanel.h"
#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

namespace cb {

namespace {
bool isImageFile(const QString& path)
{
    static const QStringList suffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                         QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
                                         QStringLiteral("svg")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}
} // namespace

qreal MainWindow::automaticUiScale(const QWidget* window)
{
    const QScreen* screen = window && window->screen() ? window->screen() : QGuiApplication::primaryScreen();
    if (!screen)
        return 1.0;
    // Logical width already includes the OS scaling. A 4K board left at 100% needs larger controls.
    const qreal logicalWidth = screen->availableGeometry().width();
    return std::clamp(logicalWidth / 1920.0 * 0.85, 1.0, 2.0);
}

MainWindow::MainWindow(UiContext& ui, AppSettings& settings, QWidget* parent)
    : QMainWindow(parent)
    , m_ui(ui)
    , m_settings(settings)
    , m_tools(std::make_unique<ToolSettings>())
    , m_doc(std::make_unique<Document>())
    , m_templates(std::make_unique<TemplateLibrary>())
    , m_recognizers(std::make_unique<RecognizerRegistry>())
{
    setWindowTitle(QStringLiteral("ClassBoard"));
    setMinimumSize(800, 560);
    m_settings.loadTools(*m_tools);
    m_templates->load();
    m_recognizers->loadPlugins(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins/recognizers"));
    // Built-in offline recogniser for the Magic Equation Maker (plug-in models take precedence).
    m_recognizers->registerEquationRecognizer(std::make_unique<MathInkRecognizer>());

    const TemplateSpec defaultTemplate = m_templates->find(m_settings.defaultTemplateId());
    m_doc->resetToNew(defaultTemplate);

    // Layout: board area (canvas + overlays) above the ribbon.
    auto* central = new QWidget(this);
    central->setAutoFillBackground(true);
    QPalette pal = central->palette();
    pal.setColor(QPalette::Window, ui.theme.color(ThemeColor::Window));
    central->setPalette(pal);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_boardArea = new QWidget(central);
    auto* boardLayout = new QVBoxLayout(m_boardArea);
    boardLayout->setContentsMargins(0, 0, 0, 0);
    m_canvas = new CanvasWidget(ui, *m_doc, *m_tools, m_boardArea);
    boardLayout->addWidget(m_canvas);
    layout->addWidget(m_boardArea, 1);
    setCentralWidget(central);

    m_popoverHost = new PopoverHost(ui, m_boardArea);
    m_toast = new Toast(ui, m_boardArea);

    m_edit = std::make_unique<EditOperations>(*m_doc, m_canvas->selectionModel());
    m_thumbnails = std::make_unique<ThumbnailCache>(*m_doc, QSize(320, 180));
    m_lesson = std::make_unique<LessonController>(ui, *m_doc, m_settings, *m_templates, *m_toast, *m_canvas, this);
    m_exporter = std::make_unique<ExportController>(*m_doc, m_settings, *m_toast, this);
    m_popovers = std::make_unique<PopoverController>(*m_popoverHost);
    m_cellEditor = std::make_unique<TableCellEditor>(ui, *m_canvas, *m_doc);
    m_calculator = std::make_unique<math::Calculator>();
    m_services.reset(new AppServices{ui,
                                     *m_doc,
                                     *m_tools,
                                     m_settings,
                                     *m_canvas,
                                     *m_lesson,
                                     *m_exporter,
                                     *m_templates,
                                     *m_thumbnails,
                                     *m_edit,
                                     *m_toast,
                                     *m_popovers,
                                     *m_recognizers,
                                     *m_calculator,
                                     [this](ToolId id) { activateTool(id); },
                                     [this](qreal scale) { applyUiScale(scale); },
                                     [this](qreal scale) { m_ribbon->setScale(scale); }});
    m_popovers->setServices(m_services.get());

    m_selectionBar = new SelectionBar(*m_services, m_canvas);
    connect(m_selectionBar, &SelectionBar::editRequested, this, [this]() {
        const auto& ids = m_canvas->selectionModel().ids();
        if (ids.size() == 1)
            editObject(ids.first());
    });
    connect(m_selectionBar, &SelectionBar::magicRequested, this, &MainWindow::openMagicEquation);

    buildRibbon();
    createShortcuts();

    // Canvas and input preferences.
    m_canvas->input().setPalmEraseEnabled(m_settings.palmErase());
    m_canvas->input().setMultiUserTouch(m_settings.multiUserTouch());
    m_canvas->setShowPageFrame(m_settings.showPageFrame());
    m_canvas->installEventFilter(this);

    // Document -> UI state.
    connect(m_doc.get(), &Document::pagesChanged, this, &MainWindow::updateRibbonState);
    connect(m_doc.get(), &Document::currentPageChanged, this, &MainWindow::updateRibbonState);
    connect(&m_doc->commands(), &CommandStack::changed, this, &MainWindow::updateRibbonState);
    connect(m_doc.get(), &Document::modifiedChanged, this, &MainWindow::updateTitle);
    connect(m_doc.get(), &Document::filePathChanged, this, &MainWindow::updateTitle);
    connect(m_doc.get(), &Document::documentReset, this, &MainWindow::updateTitle);
    connect(&m_doc->commands(), &CommandStack::applied, this, [this](const QUuid& pageId) {
        // Undo/redo on another page: show that page so the change is visible.
        const int index = m_doc->indexOfPage(pageId);
        if (index >= 0 && index != m_doc->currentPageIndex())
            m_doc->setCurrentPageIndex(index);
    });
    connect(m_doc.get(), &Document::contentChanged, m_selectionBar, &SelectionBar::refresh);
    connect(&m_canvas->selectionModel(), &SelectionModel::changed, m_selectionBar, &SelectionBar::refresh);
    connect(m_canvas, &CanvasWidget::viewChanged, m_selectionBar, &SelectionBar::refresh);
    connect(m_canvas, &CanvasWidget::interactionFinished, m_selectionBar, &SelectionBar::refresh);
    connect(&m_canvas->tools(), &ToolController::activeToolChanged, this, [this]() {
        updateRibbonState();
        updateToolChip();
        m_selectionBar->refresh();
    });
    connect(m_canvas, &CanvasWidget::zoomChanged, this, [this](qreal zoom) { m_ribbon->setZoomPercent(qRound(zoom * 100)); });
    connect(m_canvas, &CanvasWidget::editRequested, this, &MainWindow::editObject);
    connect(m_canvas, &CanvasWidget::statusText, this, [this](const QString& text) {
        if (!text.isEmpty())
            m_toast->showMessage(text, 1200);
    });
    connect(m_tools.get(), &ToolSettings::changed, this, [this]() {
        m_ribbon->setPenColor(m_tools->penColor());
        updateToolChip();
        if (auto* compass = static_cast<Compass*>(m_canvas->instruments().find(Instrument::Kind::Compass)))
            compass->setInk(m_tools->ink());
    });
    connect(&m_canvas->instruments(), &InstrumentLayer::visibilityChanged, this, [this]() {
        if (auto* compass = static_cast<Compass*>(m_canvas->instruments().find(Instrument::Kind::Compass)))
            compass->setInk(m_tools->ink());
    });
    connect(m_popovers.get(), &PopoverController::opened, this, [this](const QString& key) {
        m_ribbon->setMoreOpen(key == QLatin1String("more"));
    });
    connect(m_popovers.get(), &PopoverController::closed, this, [this]() { m_ribbon->setMoreOpen(false); });

    activateTool(ToolId::Pen);
    updateRibbonState();
    updateTitle();

    const QByteArray geometry = m_settings.windowGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1440, 900);
}

MainWindow::~MainWindow()
{
    // Tear down controllers before the widgets and the document they reference.
    m_services.reset();
    m_popovers.reset();
    m_cellEditor.reset();
    m_exporter.reset();
    m_lesson.reset();
    m_thumbnails.reset();
    m_edit.reset();
    delete takeCentralWidget();
}

// ------------------------------------------------------------------------------------ ribbon

void MainWindow::buildRibbon()
{
    auto* central = centralWidget();
    auto* layout = static_cast<QVBoxLayout*>(central->layout());
    if (m_ribbon) {
        layout->removeWidget(m_ribbon);
        m_ribbon->deleteLater();
    }
    m_ribbon = new Ribbon(m_ui, m_settings.ribbonScale(), central);
    layout->addWidget(m_ribbon);

    connect(m_ribbon, &Ribbon::moreClicked, this, [this]() { m_popovers->toggle(QStringLiteral("more"), m_ribbon->moreButton()); });
    connect(m_ribbon, &Ribbon::penClicked, this, [this]() { onCoreToolClicked(ToolId::Pen, QStringLiteral("pen")); });
    connect(m_ribbon, &Ribbon::eraserClicked, this, [this]() { onCoreToolClicked(ToolId::Eraser, QStringLiteral("eraser")); });
    connect(m_ribbon, &Ribbon::selectClicked, this, [this]() { onCoreToolClicked(ToolId::Select, QStringLiteral("select")); });
    connect(m_ribbon, &Ribbon::toolChipClicked, this, [this]() {
        QString key;
        switch (m_canvas->tools().activeToolId()) {
        case ToolId::Shape: key = QStringLiteral("shapes"); break;
        case ToolId::Text: key = QStringLiteral("text"); break;
        case ToolId::Measure: key = QStringLiteral("measure"); break;
        case ToolId::Construct: key = QStringLiteral("geometry"); break;
        case ToolId::Graph: key = QStringLiteral("function"); break;
        default: break;
        }
        m_ribbon->setActiveTool(m_canvas->tools().activeToolId());
        if (!key.isEmpty())
            m_popovers->toggle(key, m_ribbon->toolChip());
    });
    connect(m_ribbon, &Ribbon::undoClicked, this, [this]() { m_doc->commands().undo(); });
    connect(m_ribbon, &Ribbon::redoClicked, this, [this]() { m_doc->commands().redo(); });
    connect(m_ribbon, &Ribbon::previousPageClicked, this, [this]() { m_doc->setCurrentPageIndex(m_doc->currentPageIndex() - 1); });
    connect(m_ribbon, &Ribbon::nextPageClicked, this, [this]() { m_doc->setCurrentPageIndex(m_doc->currentPageIndex() + 1); });
    connect(m_ribbon, &Ribbon::pageIndicatorClicked, this, [this]() { m_popovers->toggle(QStringLiteral("pages"), m_ribbon->pageButton()); });
    connect(m_ribbon, &Ribbon::newPageClicked, this, &MainWindow::addPage);
    connect(m_ribbon, &Ribbon::fileClicked, this, [this]() { m_popovers->toggle(QStringLiteral("lesson"), m_ribbon->fileButton()); });
    connect(m_ribbon, &Ribbon::insertClicked, this, [this]() { m_popovers->toggle(QStringLiteral("insert"), m_ribbon->insertButton()); });
    connect(m_ribbon, &Ribbon::editClicked, this, [this]() { m_popovers->toggle(QStringLiteral("edit"), m_ribbon->editButton()); });
    connect(m_ribbon, &Ribbon::pageMenuClicked, this,
            [this]() { m_popovers->toggle(QStringLiteral("pageactions"), m_ribbon->pageMenuButton()); });
    connect(m_ribbon, &Ribbon::zoomClicked, this, [this]() { m_popovers->toggle(QStringLiteral("view"), m_ribbon->zoomButton()); });
    connect(m_ribbon, &Ribbon::zoomInClicked, this, [this]() { m_canvas->zoomStep(1); });
    connect(m_ribbon, &Ribbon::zoomOutClicked, this, [this]() { m_canvas->zoomStep(-1); });
    connect(m_ribbon, &Ribbon::fitClicked, this, [this]() { m_canvas->fitPage(); });
    connect(m_ribbon, &Ribbon::fullScreenClicked, this, &MainWindow::toggleFullScreen);

    m_ribbon->setPenColor(m_tools->penColor());
    m_ribbon->setZoomPercent(qRound(m_canvas->zoom() * 100));
    m_ribbon->setFullScreen(isFullScreen());
    updateRibbonState();
    updateToolChip();
}

void MainWindow::onCoreToolClicked(ToolId id, const QString& popoverKey)
{
    TouchButton* anchor = id == ToolId::Pen ? m_ribbon->penButton()
        : id == ToolId::Eraser             ? m_ribbon->eraserButton()
                                           : m_ribbon->selectButton();
    if (m_canvas->tools().activeToolId() == id) {
        // Second tap on the active tool opens its options.
        m_ribbon->setActiveTool(id);
        m_popovers->toggle(popoverKey, anchor);
        return;
    }
    m_popovers->close();
    activateTool(id);
}

void MainWindow::activateTool(ToolId id)
{
    const bool keepSelection = id == ToolId::Select || id == ToolId::Graph;
    if (!keepSelection)
        m_canvas->selectionModel().clear();
    m_canvas->tools().setActiveTool(id);
    m_ribbon->setActiveTool(id);
    updateToolChip();
}

void MainWindow::updateToolChip()
{
    if (!m_ribbon)
        return;
    QString icon;
    QString label;
    switch (m_canvas->tools().activeToolId()) {
    case ToolId::Shape:
        icon = QStringLiteral("shapes");
        label = shapeKindLabel(m_tools->shapeKind());
        break;
    case ToolId::Text:
        icon = QStringLiteral("text");
        label = tr("Text");
        break;
    case ToolId::Measure:
        icon = QStringLiteral("measure-distance");
        switch (m_tools->measureKind()) {
        case MeasureKind::Distance: label = tr("Distance"); icon = QStringLiteral("measure-distance"); break;
        case MeasureKind::Angle: label = tr("Angle"); icon = QStringLiteral("measure-angle"); break;
        case MeasureKind::Slope: label = tr("Slope"); icon = QStringLiteral("measure-slope"); break;
        case MeasureKind::Area: label = tr("Area"); icon = QStringLiteral("measure-area"); break;
        }
        break;
    case ToolId::Construct:
        switch (m_tools->constructKind()) {
        case ConstructKind::Point: icon = QStringLiteral("point"); label = tr("Point"); break;
        case ConstructKind::Segment: icon = QStringLiteral("segment"); label = tr("Segment"); break;
        case ConstructKind::Line: icon = QStringLiteral("line"); label = tr("Line"); break;
        case ConstructKind::Ray: icon = QStringLiteral("ray"); label = tr("Ray"); break;
        case ConstructKind::Vector: icon = QStringLiteral("vector"); label = tr("Vector"); break;
        }
        break;
    case ToolId::Graph:
        icon = QStringLiteral("function");
        label = tr("Graph");
        break;
    default:
        break;
    }
    m_ribbon->setToolChip(icon, label);
    m_ribbon->setActiveTool(m_canvas->tools().activeToolId());
}

void MainWindow::updateRibbonState()
{
    if (!m_ribbon)
        return;
    const CommandStack& cmds = m_doc->commands();
    m_ribbon->setUndoRedo(cmds.canUndo(), cmds.canRedo(), cmds.undoText(), cmds.redoText());
    m_ribbon->setPageInfo(m_doc->currentPageIndex(), m_doc->pageCount());
    m_ribbon->setActiveTool(m_canvas->tools().activeToolId());
}

void MainWindow::updateTitle()
{
    setWindowTitle(QStringLiteral("%1%2 – ClassBoard").arg(m_doc->displayName(), m_doc->isModified() ? QStringLiteral(" •") : QString()));
}

void MainWindow::addPage()
{
    pageops::newPage(*m_doc, m_doc->currentPageIndex());
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen())
        showMaximized();
    else
        showFullScreen();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_ribbon)
        m_ribbon->setFullScreen(isFullScreen());
}

void MainWindow::applyUiScale(qreal scale)
{
    const qreal effective = scale > 0 ? scale : automaticUiScale(this);
    m_ui.theme.setUiScale(effective);
    qApp->setStyleSheet(m_ui.theme.styleSheet());
    m_popovers->close();
    buildRibbon();
    // Recreate the floating selection bar with the new metrics.
    m_selectionBar->deleteLater();
    m_selectionBar = new SelectionBar(*m_services, m_canvas);
    connect(m_selectionBar, &SelectionBar::editRequested, this, [this]() {
        const auto& ids = m_canvas->selectionModel().ids();
        if (ids.size() == 1)
            editObject(ids.first());
    });
    connect(m_selectionBar, &SelectionBar::magicRequested, this, &MainWindow::openMagicEquation);
    connect(m_doc.get(), &Document::contentChanged, m_selectionBar, &SelectionBar::refresh);
    connect(&m_canvas->selectionModel(), &SelectionModel::changed, m_selectionBar, &SelectionBar::refresh);
    connect(m_canvas, &CanvasWidget::viewChanged, m_selectionBar, &SelectionBar::refresh);
    connect(m_canvas, &CanvasWidget::interactionFinished, m_selectionBar, &SelectionBar::refresh);
    m_canvas->update();
}

// ------------------------------------------------------------------------------------ editing

void MainWindow::editObject(const ObjectId& id)
{
    Page* page = m_doc->currentPage();
    DocumentObject* o = page ? page->object(id) : nullptr;
    if (!o)
        return;
    switch (o->type()) {
    case ObjectType::Text: {
        activateTool(ToolId::Text);
        if (auto* text = static_cast<TextTool*>(m_canvas->tools().tool(ToolId::Text)))
            text->editObject(id);
        break;
    }
    case ObjectType::Equation: {
        const QRect r = m_canvas->pageRectToWidget(o->sceneBounds());
        const QPoint topLeft = m_canvas->mapTo(m_boardArea, r.topLeft());
        m_popovers->editEquation(id, QRect(topLeft, r.size()));
        break;
    }
    case ObjectType::Graph:
        activateTool(ToolId::Graph);
        m_canvas->selectionModel().setSingle(id);
        m_popovers->open(QStringLiteral("function"), m_ribbon->toolChip());
        break;
    case ObjectType::Table: {
        auto* table = static_cast<TableObject*>(o);
        int row = 0;
        int col = 0;
        if (!table->cellAt(table->mapFromPage(m_canvas->lastPressPage()), &row, &col)) {
            row = table->hasHeader() && table->rowCount() > 1 ? 1 : 0;
            col = 0;
        }
        m_cellEditor->edit(id, row, col);
        break;
    }
    default:
        if (PropertiesPanel::supports(*o)) {
            const QRect r = m_canvas->pageRectToWidget(o->sceneBounds());
            m_popovers->editProperties(id, QRect(m_canvas->mapTo(m_boardArea, r.topLeft()), r.size()));
        }
        break;
    }
}

void MainWindow::openMagicEquation()
{
    Page* page = m_doc->currentPage();
    const QVector<ObjectId> ids = m_canvas->selectionModel().ids();
    if (!page || !magic::isHandwriting(*page, ids)) {
        m_toast->showMessage(tr("Select your handwriting with SELECT, then tap ✨ Magic Equation Maker."), 3500);
        return;
    }
    QRectF bounds;
    for (const ObjectId& id : ids)
        bounds = bounds.isNull() ? page->object(id)->sceneBounds() : bounds.united(page->object(id)->sceneBounds());
    const QRect r = m_canvas->pageRectToWidget(bounds);
    m_popovers->openMagicEquation(ids, QRect(m_canvas->mapTo(m_boardArea, r.topLeft()), r.size()));
}

// ----------------------------------------------------------------------------------- shortcuts

void MainWindow::createShortcuts()
{
    auto add = [this](const QKeySequence& seq, std::function<void()> f) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, std::move(f));
    };
    add(QKeySequence::Undo, [this]() { m_doc->commands().undo(); });
    add(QKeySequence::Redo, [this]() { m_doc->commands().redo(); });
    add(QKeySequence(Qt::CTRL + Qt::Key_Y), [this]() { m_doc->commands().redo(); });
    add(QKeySequence::Save, [this]() { m_lesson->save(); });
    add(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S), [this]() { m_lesson->saveAs(); });
    add(QKeySequence::Open, [this]() { m_lesson->open(); });
    add(QKeySequence::New, [this]() { m_lesson->newLesson(); });
    add(QKeySequence::Copy, [this]() { m_edit->copySelection(); });
    add(QKeySequence::Cut, [this]() { m_edit->cutSelection(); });
    add(QKeySequence::Paste, [this]() {
        if (m_edit->paste(m_canvas->viewCenterInPage()))
            activateTool(ToolId::Select);
    });
    add(QKeySequence(Qt::CTRL + Qt::Key_D), [this]() { m_edit->duplicateSelection(); });
    add(QKeySequence::SelectAll, [this]() {
        activateTool(ToolId::Select);
        m_edit->selectAll();
    });
    add(QKeySequence::Delete, [this]() { m_edit->deleteSelection(); });
    add(QKeySequence(Qt::Key_Backspace), [this]() { m_edit->deleteSelection(); });
    add(QKeySequence(Qt::Key_PageDown), [this]() { m_doc->setCurrentPageIndex(m_doc->currentPageIndex() + 1); });
    add(QKeySequence(Qt::Key_PageUp), [this]() { m_doc->setCurrentPageIndex(m_doc->currentPageIndex() - 1); });
    add(QKeySequence(Qt::CTRL + Qt::Key_M), [this]() { addPage(); });
    add(QKeySequence(Qt::Key_F11), [this]() { toggleFullScreen(); });
    add(QKeySequence(Qt::CTRL + Qt::Key_0), [this]() { m_canvas->fitPage(); });
    add(QKeySequence::ZoomIn, [this]() { m_canvas->zoomBy(1.25); });
    add(QKeySequence(Qt::CTRL + Qt::Key_Equal), [this]() { m_canvas->zoomBy(1.25); });
    add(QKeySequence::ZoomOut, [this]() { m_canvas->zoomBy(0.8); });
    add(QKeySequence(Qt::Key_P), [this]() { activateTool(ToolId::Pen); });
    add(QKeySequence(Qt::Key_E), [this]() { activateTool(ToolId::Eraser); });
    add(QKeySequence(Qt::Key_V), [this]() { activateTool(ToolId::Select); });
    add(QKeySequence(Qt::Key_T), [this]() { activateTool(ToolId::Text); });
    add(QKeySequence(Qt::Key_Escape), [this]() {
        if (m_popoverHost->isOpen())
            m_popovers->close();
        else
            m_canvas->selectionModel().clear();
    });
}

// ---------------------------------------------------------------------------------- lifecycle

void MainWindow::startup(const QString& fileToOpen)
{
    applyUiScale(m_settings.uiScale());
    if (m_settings.fullScreen())
        showFullScreen();
    if (!fileToOpen.isEmpty() && QFileInfo::exists(fileToOpen)) {
        m_lesson->openFile(fileToOpen);
    } else {
        QTimer::singleShot(700, this, [this]() { m_lesson->checkRecovery(); });
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_canvas) {
        if (event->type() == QEvent::DragEnter) {
            auto* e = static_cast<QDragEnterEvent*>(event);
            if (e->mimeData()->hasUrls() || e->mimeData()->hasImage()) {
                e->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* e = static_cast<QDropEvent*>(event);
            const QPointF pagePos = m_canvas->view().viewToPage(e->posF());
            QStringList images;
            QString lesson;
            QString pdf;
            QString presentation;
            for (const QUrl& url : e->mimeData()->urls()) {
                const QString path = url.toLocalFile();
                const QString suffix = QFileInfo(path).suffix().toLower();
                if (suffix == QLatin1String("classboard"))
                    lesson = path;
                else if (suffix == QLatin1String("pdf"))
                    pdf = path;
                else if (suffix == QLatin1String("pptx") || suffix == QLatin1String("ppt") || suffix == QLatin1String("ppsx")
                         || suffix == QLatin1String("pps") || suffix == QLatin1String("odp"))
                    presentation = path;
                else if (isImageFile(path))
                    images << path;
            }
            if (!pdf.isEmpty())
                m_lesson->importPdfFile(pdf);
            else if (!presentation.isEmpty())
                m_lesson->importPresentationFile(presentation);
            else if (!images.isEmpty())
                m_lesson->insertImageFiles(images, pagePos);
            else if (!lesson.isEmpty())
                m_lesson->confirmDiscard([this, lesson]() { m_lesson->openFile(lesson); });
            else if (e->mimeData()->hasImage())
                m_edit->insertMimeData(e->mimeData(), pagePos, false);
            e->acceptProposedAction();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_lesson->isSaving()) {
        m_toast->showMessage(tr("Please wait until the lesson is saved."), 2500);
        event->ignore();
        return;
    }
    if (!m_closing && m_doc->isModified()) {
        event->ignore();
        m_lesson->confirmDiscard([this]() {
            m_closing = true;
            close();
        });
        return;
    }
    m_settings.saveTools(*m_tools);
    m_settings.setFullScreen(isFullScreen());
    if (!isFullScreen())
        m_settings.setWindowGeometry(saveGeometry());
    m_settings.sync();
    m_lesson->autosave().discard();
    event->accept();
}

} // namespace cb
