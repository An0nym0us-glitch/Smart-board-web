#include "app/ExportController.h"

#include "app/AppSettings.h"
#include "document/Document.h"
#include "export/Exporters.h"
#include "ui/Toast.h"
#include "core/Background.h"

#if defined(Q_OS_WASM)
#include "platform/WebPlatform.h"
#include <QDir>
#include <QFile>
#endif

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <atomic>

namespace cb {

ExportController::ExportController(Document& doc, AppSettings& settings, Toast& toast, QWidget* window, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
    , m_settings(settings)
    , m_toast(toast)
    , m_window(window)
{
}

bool ExportController::parseRange(const QString& text, int pageCount, QVector<int>* pages)
{
    QVector<int> out;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;
    static const QRegularExpression rangeRe(QStringLiteral("^(\\d*)\\s*-\\s*(\\d*)$"));
    for (const QString& part : parts) {
        const QRegularExpressionMatch m = rangeRe.match(part);
        int a = 0;
        int b = 0;
        if (m.hasMatch()) {
            a = m.captured(1).isEmpty() ? 1 : m.captured(1).toInt();
            b = m.captured(2).isEmpty() ? pageCount : m.captured(2).toInt();
        } else {
            bool ok = false;
            a = b = part.toInt(&ok);
            if (!ok)
                return false;
        }
        if (a < 1 || b < a || b > pageCount)
            return false;
        for (int i = a; i <= b; ++i)
            if (!out.contains(i - 1))
                out.push_back(i - 1);
    }
    std::sort(out.begin(), out.end());
    if (pages)
        *pages = out;
    return !out.isEmpty();
}

void ExportController::exportPages(Format format, const QVector<int>& pagesIn)
{
    if (m_busy) {
        m_toast.showMessage(tr("An export is already running."), 2500);
        return;
    }
    QVector<int> pages = pagesIn;
    if (pages.isEmpty())
        for (int i = 0; i < m_doc.pageCount(); ++i)
            pages.push_back(i);

    QString filter;
    QString suffix;
    switch (format) {
    case Format::Pdf:
        filter = tr("PDF document (*.pdf)");
        suffix = QStringLiteral("pdf");
        break;
    case Format::Pptx:
        filter = tr("PowerPoint presentation (*.pptx)");
        suffix = QStringLiteral("pptx");
        break;
    case Format::Png:
        filter = tr("PNG image (*.png)");
        suffix = QStringLiteral("png");
        break;
    }
    QString base = m_doc.displayName();
    if (format == Format::Png && pages.size() == 1)
        base += QStringLiteral(" - page %1").arg(pages.first() + 1);
#if defined(Q_OS_WASM)
    // In the browser the export is written to memory and then downloaded.
    const QString exportDir = QDir::tempPath() + QStringLiteral("/classboard-export");
    QDir().mkpath(exportDir);
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    const QString path = exportDir + QLatin1Char('/') + base + QLatin1Char('.') + suffix;
    Q_UNUSED(filter);
#else
    QString dir = m_settings.lastDirectory();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString path = QFileDialog::getSaveFileName(m_window, tr("Export"), dir + QLatin1Char('/') + base + QLatin1Char('.') + suffix, filter);
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1Char('.') + suffix, Qt::CaseInsensitive))
        path += QLatin1Char('.') + suffix;
    m_settings.setLastDirectory(QFileInfo(path).absolutePath());
#endif

    // Snapshot on the GUI thread; the worker only sees immutable copies.
    std::shared_ptr<DocumentSnapshot> snapshot(m_doc.snapshot(pages).release());
    if (snapshot->metadata.title.isEmpty())
        snapshot->metadata.title = m_doc.displayName();
    m_busy = true;
    emit busyChanged(true);
    const QString label = tr("Exporting %1").arg(QFileInfo(path).fileName());
    m_toast.showProgress(label, 0);

    auto error = std::make_shared<QString>();
    auto ok = std::make_shared<bool>(false);
    QPointer<ExportController> self(this);
    const ExportProgress progress = [self, label](int done, int total) {
        const int percent = total > 0 ? done * 100 / total : 0;
        QMetaObject::invokeMethod(qApp, [self, label, percent]() {
            if (self && self->m_busy)
                self->m_toast.showProgress(label, percent);
        }, Qt::QueuedConnection);
        return true;
    };
    runInBackground(this, [snapshot, path, format, error, ok, progress]() {
        switch (format) {
        case Format::Pdf:
            *ok = PdfExporter::exportPages(*snapshot, path, error.get(), progress);
            break;
        case Format::Pptx:
            *ok = PptxExporter::exportPages(*snapshot, path, error.get(), progress);
            break;
        case Format::Png:
            *ok = ImageExporter::exportPage(*snapshot, 0, path, error.get());
            break;
        }
    }, [self, path, error, ok]() {
        if (!self)
            return;
        self->m_busy = false;
        emit self->busyChanged(false);
#if defined(Q_OS_WASM)
        if (*ok) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                web::download(file.readAll(), QFileInfo(path).fileName());
                file.close();
                self->m_toast.showMessage(tr("Exported %1 (see your downloads)").arg(QFileInfo(path).fileName()), 4000);
            } else {
                self->m_toast.showMessage(tr("Export failed: %1").arg(file.errorString()), 7000);
            }
        } else {
            self->m_toast.showMessage(tr("Export failed: %1").arg(*error), 7000);
        }
        QFile::remove(path);
        return;
#endif
        if (*ok) {
            self->m_toast.showActions(tr("Exported %1").arg(QFileInfo(path).fileName()),
                                      {{tr("Open"), [path]() { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); }, true},
                                       {tr("Show folder"),
                                        [path]() {
                                            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                                        },
                                        false}},
                                      8000);
        } else {
            self->m_toast.showMessage(tr("Export failed: %1").arg(*error), 7000);
        }
    }, true);
}

} // namespace cb
