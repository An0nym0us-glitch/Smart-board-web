#include "ui/popovers/LessonPanels.h"

#include "app/AppServices.h"
#include "app/AppSettings.h"
#include "app/LessonController.h"
#include "app/PopoverController.h"
#include "canvas/CanvasWidget.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ImageStore.h"
#include "document/TemplateLibrary.h"
#include "geometry/InstrumentLayer.h"
#include "storage/PdfImporter.h"
#include "tools/EditOperations.h"
#include "tools/ToolSettings.h"
#include "ui/Ribbon.h"
#include "ui/Toast.h"
#include "ui/UiContext.h"
#include "ui/popovers/PanelUtil.h"
#include "ui/widgets/SegmentedControl.h"
#include "ui/widgets/ToggleRow.h"
#include "ui/widgets/TouchSlider.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>

namespace cb {

// =========================================================================================== MORE

MorePanel::MorePanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    auto push = [sp](const char* key) { return [sp, key]() { sp->popovers.push(QString::fromLatin1(key)); }; };
    auto instrument = [sp](Instrument::Kind kind) {
        return [sp, kind]() {
            sp->canvas.instruments().toggle(kind, sp->canvas.viewCenterInPage());
            sp->popovers.close();
        };
    };
    InstrumentLayer& layer = s.canvas.instruments();
    const QVector<panel::Tile> tiles = {
        {QStringLiteral("file"), tr("File"), push("lesson")},
        {QStringLiteral("shapes"), tr("Shapes"), push("shapes")},
        {QStringLiteral("geometry"), tr("Geometry"), push("geometry")},
        {QStringLiteral("equation"), tr("Equation"), push("equation")},
        {QStringLiteral("function"), tr("Function / Graph"),
         [sp]() {
             sp->activateTool(ToolId::Graph);
             sp->popovers.push(QStringLiteral("function"));
         }},
        {QStringLiteral("ruler"), tr("Ruler"), instrument(Instrument::Kind::Ruler), layer.isShown(Instrument::Kind::Ruler)},
        {QStringLiteral("protractor"), tr("Protractor"), instrument(Instrument::Kind::Protractor),
         layer.isShown(Instrument::Kind::Protractor)},
        {QStringLiteral("set-square"), tr("Set Square"), instrument(Instrument::Kind::SetSquare),
         layer.isShown(Instrument::Kind::SetSquare)},
        {QStringLiteral("compass"), tr("Compass"), instrument(Instrument::Kind::Compass), layer.isShown(Instrument::Kind::Compass)},
        {QStringLiteral("measure-distance"), tr("Measurements"), push("measure")},
        {QStringLiteral("calculator"), tr("Calculator"), push("calculator")},
        {QStringLiteral("image"), tr("Images"),
         [sp]() {
             sp->popovers.close();
             sp->lesson.importImages();
         }},
        {QStringLiteral("text"), tr("Text"),
         [sp]() {
             sp->activateTool(ToolId::Text);
             sp->popovers.push(QStringLiteral("text"));
         }},
        {QStringLiteral("table"), tr("Table"), push("table")},
        {QStringLiteral("template"), tr("Templates"), push("templates")},
        {QStringLiteral("import"), tr("Import"), push("import")},
        {QStringLiteral("pdf"), tr("PDF"), push("pdf")},
        {QStringLiteral("pptx"), tr("PPT / PPTX"), push("pptx")},
        {QStringLiteral("export"), tr("Export"), push("export")},
        {QStringLiteral("settings"), tr("Settings"), push("settings")},
    };
    layout->addWidget(panel::tileGrid(ui, 4, tiles, this));
}

// ========================================================================================= Lesson

LessonPanel::LessonPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    auto closeThen = [sp](std::function<void()> f) {
        return [sp, f]() {
            sp->popovers.close();
            f();
        };
    };
    layout->addWidget(panel::tileGrid(ui, 3,
                                      {{QStringLiteral("file-new"), tr("New"), closeThen([sp]() { sp->lesson.newLesson(); })},
                                       {QStringLiteral("folder-open"), tr("Open"), closeThen([sp]() { sp->lesson.open(); })},
                                       {QStringLiteral("save"), tr("Save"), closeThen([sp]() { sp->lesson.save(); })},
                                       {QStringLiteral("save-as"), tr("Save as"), closeThen([sp]() { sp->lesson.saveAs(); })},
                                       {QStringLiteral("import"), tr("Import"), [sp]() { sp->popovers.push(QStringLiteral("import")); }},
                                       {QStringLiteral("export"), tr("Export"), [sp]() { sp->popovers.push(QStringLiteral("export")); }}},
                                      this));
    const QStringList recent = s.settings.recentFiles();
    if (!recent.isEmpty()) {
        layout->addWidget(panel::section(ui, tr("Recent lessons"), this));
        for (const QString& path : recent) {
            auto* b = new TouchButton(ui, QStringLiteral("file"), QFileInfo(path).completeBaseName(), TouchButton::Style::Row, this);
            b->setToolTip(path);
            b->setEnabled(QFile::exists(path));
            connect(b, &QAbstractButton::clicked, this, [sp, path]() {
                sp->popovers.close();
                sp->lesson.confirmDiscard([sp, path]() { sp->lesson.openFile(path); });
            });
            layout->addWidget(b);
        }
    }
    QString status = s.doc.filePath().isEmpty() ? tr("Not saved yet") : s.doc.filePath();
    if (s.doc.isModified())
        status += QStringLiteral(" · ") + tr("unsaved changes (autosave protects them)");
    layout->addWidget(panel::hint(ui, status, this));
}

// ====================================================================================== Templates

namespace {
class TemplateTile : public QAbstractButton
{
public:
    TemplateTile(const UiContext& ui, const TemplateSpec& spec, const ImageStore& images, QWidget* parent)
        : QAbstractButton(parent)
        , m_ui(ui)
        , m_spec(spec)
        , m_images(images)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(spec.name);
        setFixedSize(ui.theme.dpi(128), ui.theme.dpi(100));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const Theme& t = m_ui.theme;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF thumb(t.dp(4), t.dp(4), width() - t.dp(8), (width() - t.dp(8)) * 9.0 / 16.0);
        const QRectF frame(0, 0, 1920, 1080);
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(thumb, t.dp(8), t.dp(8));
        p.setClipPath(clip);
        p.translate(thumb.topLeft());
        const qreal s = thumb.width() / frame.width();
        p.scale(s, s);
        const CoordinateSystem cs = CoordinateSystem::global(frame.center(), 40.0, QStringLiteral("cm"));
        TemplateRenderer::paint(p, m_spec, frame, s * 2.5, &m_images, cs, frame);
        p.restore();
        p.setPen(QPen(isChecked() ? t.color(ThemeColor::Accent) : t.color(ThemeColor::PopoverBorder), t.dp(isChecked() ? 3 : 1)));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(thumb, t.dp(8), t.dp(8));
        p.setPen(isChecked() ? t.color(ThemeColor::Accent) : t.color(ThemeColor::Text));
        p.setFont(t.font(t.metric(ThemeMetric::SmallFontSize), isChecked()));
        p.drawText(QRectF(0, thumb.bottom() + t.dp(2), width(), height() - thumb.bottom() - t.dp(2)), Qt::AlignCenter,
                   m_spec.name);
    }

private:
    const UiContext& m_ui;
    TemplateSpec m_spec;
    const ImageStore& m_images;
};
} // namespace

TemplatesPanel::TemplatesPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);

    layout->addWidget(panel::section(ui, tr("Apply to"), this));
    auto* scope = new SegmentedControl(ui, {{QString(), tr("This page")}, {QString(), tr("All pages")}, {QString(), tr("New pages")}}, this);
    layout->addWidget(scope);

    for (const TemplateSpec& spec : s.templates.templates())
        s.templates.ensureImage(spec, s.doc.images());

    auto* gridWidget = new QWidget(this);
    auto* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(ui.theme.dpi(8));
    const QString currentId = s.doc.currentPage() ? s.doc.currentPage()->background().id : QString();
    int i = 0;
    for (const TemplateSpec& spec : s.templates.templates()) {
        auto* tile = new TemplateTile(ui, spec, s.doc.images(), gridWidget);
        tile->setChecked(spec.id == currentId);
        connect(tile, &QAbstractButton::clicked, this, [sp, spec, scope]() {
            sp->templates.ensureImage(spec, sp->doc.images());
            Document& doc = sp->doc;
            switch (scope->currentIndex()) {
            case 0:
                if (Page* page = doc.currentPage())
                    doc.commands().push(std::make_unique<ModifyPageCommand>(page->id(), page->name(), spec, tr("Change background")));
                break;
            case 1: {
                auto macro = std::make_unique<CompositeCommand>(tr("Change all backgrounds"));
                for (int p = 0; p < doc.pageCount(); ++p) {
                    Page* page = doc.page(p);
                    auto cmd = std::make_unique<ModifyPageCommand>(page->id(), page->name(), spec, QString());
                    cmd->redo(doc);
                    macro->add(std::move(cmd));
                }
                doc.commands().pushApplied(std::move(macro));
                break;
            }
            case 2:
                doc.setDefaultTemplate(spec);
                sp->settings.setDefaultTemplateId(spec.id);
                sp->toast.showMessage(tr("New pages will use “%1”.").arg(spec.name), 2500);
                break;
            }
            sp->popovers.close();
        });
        grid->addWidget(tile, i / 4, i % 4);
        ++i;
    }
    layout->addWidget(gridWidget);

    layout->addWidget(panel::section(ui, tr("Custom"), this));
    auto* fromImage = panel::pill(ui, QStringLiteral("image"), tr("Background from image"), this, [sp]() {
        const QString path = QFileDialog::getOpenFileName(sp->canvas.window(), tr("Background image"),
                                                          sp->settings.lastDirectory(), LessonController::imageFilter());
        if (path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        TemplateSpec spec;
        spec.name = QFileInfo(path).completeBaseName();
        spec.background = QColor(255, 255, 255);
        QString error;
        if (!sp->templates.addCustom(spec, f.readAll(), &error)) {
            sp->toast.showMessage(tr("Could not add the template: %1").arg(error), 5000);
            return;
        }
        sp->popovers.refresh();
    });
    auto* savePage = panel::pill(ui, QStringLiteral("save"), tr("Save this background"), this, [sp]() {
        Page* page = sp->doc.currentPage();
        if (!page)
            return;
        TemplateSpec spec = page->background();
        spec.id.clear();
        spec.name = tr("My %1").arg(spec.name);
        QByteArray bytes;
        if (spec.kind == TemplateKind::Image)
            bytes = sp->doc.images().encodedData(spec.imageKey);
        sp->templates.addCustom(spec, bytes);
        sp->popovers.refresh();
    });
    layout->addWidget(panel::row(ui, {fromImage, savePage}, this));
}

// ========================================================================================= Import

ImportPanel::ImportPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    const bool pdf = PdfImporter::isAvailable();
    const bool presentation = PresentationImporter::isAvailable();
    layout->addWidget(panel::tileGrid(ui, 5,
                                      {{QStringLiteral("image"), tr("Images"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.importImages();
                                        }},
                                       {QStringLiteral("file"), tr("Lesson pages"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.importLesson();
                                        }},
                                       {QStringLiteral("pdf"), tr("PDF"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.importPdf();
                                        },
                                        false, pdf},
                                       {QStringLiteral("pptx"), tr("PowerPoint"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.importPresentation();
                                        },
                                        false, presentation},
                                       {QStringLiteral("paste"), tr("Clipboard"),
                                        [sp]() {
                                            sp->popovers.close();
                                            if (!sp->edit.paste(sp->canvas.viewCenterInPage()))
                                                sp->toast.showMessage(tr("The clipboard is empty."), 2000);
                                        },
                                        false, s.edit.canPaste()}},
                                      this));
    if (!pdf)
        layout->addWidget(panel::hint(ui, PdfImporter::unavailableReason(), this));
    else if (!presentation)
        layout->addWidget(panel::hint(ui, PresentationImporter::unavailableReason(), this));
    layout->addWidget(panel::hint(ui, tr("You can also drag images, PDFs and presentations onto the board."), this));
}

// ========================================================================================= Export

ExportPanel::ExportPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    layout->addWidget(panel::tileGrid(ui, 5,
                                      {{QStringLiteral("save"), tr("Save lesson"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.save();
                                        }},
                                       {QStringLiteral("save-as"), tr("Save as"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->lesson.saveAs();
                                        }},
                                       {QStringLiteral("pdf"), tr("PDF"), [sp]() { sp->popovers.push(QStringLiteral("pdf")); }},
                                       {QStringLiteral("pptx"), tr("PowerPoint"), [sp]() { sp->popovers.push(QStringLiteral("pptx")); }},
                                       {QStringLiteral("image"), tr("Page as PNG"),
                                        [sp]() {
                                            sp->popovers.close();
                                            sp->exporter.exportPages(ExportController::Format::Png, {sp->doc.currentPageIndex()});
                                        }}},
                                      this));
    layout->addWidget(panel::hint(ui, tr("Lessons are saved as .classboard files and can be opened on any computer with ClassBoard."), this));
}

ExportPagesPanel::ExportPagesPanel(const AppServices& s, ExportController::Format format, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    auto* layout = panel::makeLayout(ui, this);
    if (format == ExportController::Format::Pdf || format == ExportController::Format::Pptx) {
        const bool pdfFormat = format == ExportController::Format::Pdf;
        const bool available = pdfFormat ? PdfImporter::isAvailable() : PresentationImporter::isAvailable();
        layout->addWidget(panel::section(ui, tr("Import"), this));
        auto* import = panel::pill(ui, QStringLiteral("import"), pdfFormat ? tr("Import PDF…") : tr("Import PowerPoint…"), this,
                                   [sp, pdfFormat]() {
                                       sp->popovers.close();
                                       if (pdfFormat)
                                           sp->lesson.importPdf();
                                       else
                                           sp->lesson.importPresentation();
                                   });
        import->setEnabled(available);
        layout->addWidget(panel::row(ui, {import}, this));
        if (!available)
            layout->addWidget(panel::hint(ui, pdfFormat ? PdfImporter::unavailableReason() : PresentationImporter::unavailableReason(), this));
        layout->addWidget(panel::section(ui, tr("Export"), this));
    }
    auto run = [sp, format](const QVector<int>& pages) {
        sp->popovers.close();
        sp->exporter.exportPages(format, pages);
    };
    auto* current = panel::pill(ui, QStringLiteral("page"), tr("Current page"), this, [sp, run]() { run({sp->doc.currentPageIndex()}); });
    auto* all = panel::pill(ui, QStringLiteral("pages"), tr("Whole lesson (%n pages)", "", s.doc.pageCount()), this,
                            [run]() { run({}); }, true);
    layout->addWidget(panel::row(ui, {current, all}, this));
    layout->addWidget(panel::section(ui, tr("Selected pages"), this));
    auto* range = new QLineEdit(this);
    range->setPlaceholderText(tr("e.g. 1-3, 5"));
    range->setMinimumHeight(ui.theme.dpi(46));
    auto* go = panel::pill(ui, QStringLiteral("export"), tr("Export"), this, [sp, range, run]() {
        QVector<int> pages;
        if (!ExportController::parseRange(range->text(), sp->doc.pageCount(), &pages)) {
            sp->toast.showMessage(tr("Enter pages like 1-3, 5 (the lesson has %n pages).", "", sp->doc.pageCount()), 3500);
            return;
        }
        run(pages);
    });
    auto* rangeRow = new QWidget(this);
    auto* rr = new QHBoxLayout(rangeRow);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->addWidget(range, 1);
    rr->addWidget(go);
    layout->addWidget(rangeRow);
    const QString note = format == ExportController::Format::Pdf
        ? tr("Text, shapes, formulas, graphs and ink stay sharp (vector PDF).")
        : tr("Each page becomes a slide showing the complete page in high resolution.");
    layout->addWidget(panel::hint(ui, note, this));
}

// ======================================================================================= Settings

SettingsPanel::SettingsPanel(const AppServices& s, QWidget* parent)
    : QWidget(parent)
{
    const UiContext& ui = s.ui;
    const AppServices* sp = &s;
    AppSettings* settings = &s.settings;
    auto* layout = panel::makeLayout(ui, this);

    layout->addWidget(panel::section(ui, tr("Display"), this));
    auto* scale = new TouchSlider(ui, tr("Interface size"), this);
    scale->setRange(0.75, 2.5);
    scale->setStep(0.05);
    scale->setValue(ui.theme.uiScale());
    scale->setFormatter([](double v) { return QString::number(qRound(v * 100)) + QStringLiteral(" %"); });
    connect(scale, &TouchSlider::sliderReleased, this, [sp, settings, scale]() {
        settings->setUiScale(scale->value());
        sp->applyUiScale(scale->value());
    });
    layout->addWidget(scale);
    auto* autoScale = panel::pill(ui, QStringLiteral("reset"), tr("Automatic size"), this, [sp, settings]() {
        settings->setUiScale(0);
        sp->applyUiScale(0);
    });
    layout->addWidget(panel::row(ui, {autoScale}, this));
    // The bottom ribbon has its own size: slimmer on a big board, bigger on a small screen.
    auto* ribbon = new TouchSlider(ui, tr("Ribbon size"), this);
    ribbon->setObjectName(QStringLiteral("ribbonSize"));
    ribbon->setRange(Ribbon::kMinScale, Ribbon::kMaxScale);
    ribbon->setStep(0.05);
    ribbon->setValue(settings->ribbonScale());
    ribbon->setFormatter([](double v) { return QString::number(qRound(v * 100)) + QStringLiteral(" %"); });
    connect(ribbon, &TouchSlider::valueChanged, this, [sp](double v) { sp->applyRibbonScale(v); });
    connect(ribbon, &TouchSlider::sliderReleased, this, [settings, ribbon]() { settings->setRibbonScale(ribbon->value()); });
    layout->addWidget(ribbon);
    auto* normalRibbon = panel::pill(ui, QStringLiteral("reset"), tr("Normal ribbon size"), this, [sp, settings, ribbon]() {
        settings->setRibbonScale(1.0);
        ribbon->setValue(1.0);
        sp->applyRibbonScale(1.0);
    });
    normalRibbon->setObjectName(QStringLiteral("normalRibbon"));
    layout->addWidget(panel::row(ui, {normalRibbon}, this));
    auto* frame = new ToggleRow(ui, tr("Show page frame"), this);
    frame->setDescription(tr("Outline of the area used for export and slides"));
    frame->setChecked(settings->showPageFrame());
    connect(frame, &ToggleRow::toggled, this, [sp, settings](bool on) {
        settings->setShowPageFrame(on);
        sp->canvas.setShowPageFrame(on);
    });
    layout->addWidget(frame);

    layout->addWidget(panel::section(ui, tr("Touch and pen"), this));
    auto* palm = new ToggleRow(ui, tr("Palm and three-finger wipe"), this);
    palm->setChecked(settings->palmErase());
    connect(palm, &ToggleRow::toggled, this, [sp, settings](bool on) {
        settings->setPalmErase(on);
        sp->canvas.input().setPalmEraseEnabled(on);
    });
    layout->addWidget(palm);
    auto* multi = new ToggleRow(ui, tr("Every finger draws"), this);
    multi->setDescription(tr("Turns two-finger zoom off, so fingers close together also draw. "
                             "Several people can always write at once."));
    multi->setChecked(settings->multiUserTouch());
    connect(multi, &ToggleRow::toggled, this, [sp, settings](bool on) {
        settings->setMultiUserTouch(on);
        sp->canvas.input().setMultiUserTouch(on);
    });
    layout->addWidget(multi);
    auto* smoothing = new TouchSlider(ui, tr("Ink smoothing"), this);
    smoothing->setRange(0, 1);
    smoothing->setStep(0.05);
    smoothing->setValue(s.tools.smoothing());
    smoothing->setFormatter([](double v) { return QString::number(qRound(v * 100)) + QStringLiteral(" %"); });
    ToolSettings* tp = &s.tools;
    connect(smoothing, &TouchSlider::valueChanged, this, [tp](double v) { tp->setSmoothing(v); });
    layout->addWidget(smoothing);

    layout->addWidget(panel::section(ui, tr("Safety"), this));
    auto* autosave = new TouchSlider(ui, tr("Autosave every"), this);
    autosave->setRange(15, 600);
    autosave->setStep(15);
    autosave->setValue(settings->autosaveSeconds());
    autosave->setFormatter([](double v) {
        const int sec = qRound(v);
        return sec < 60 ? QStringLiteral("%1 s").arg(sec) : QStringLiteral("%1 min %2 s").arg(sec / 60).arg(sec % 60);
    });
    connect(autosave, &TouchSlider::sliderReleased, this, [settings, autosave]() { settings->setAutosaveSeconds(qRound(autosave->value())); });
    layout->addWidget(autosave);

    layout->addWidget(panel::section(ui, tr("About"), this));
    layout->addWidget(panel::hint(ui,
                                  tr("ClassBoard %1 — open-source digital classroom board. Works fully offline. "
                                     "Licensed under the MIT License. Includes Qt (LGPL 3) and Poppler (GPL); "
                                     "see doc\\THIRD_PARTY_NOTICES.md in the installation folder.")
                                      .arg(QCoreApplication::applicationVersion()),
                                  this));
}

} // namespace cb
