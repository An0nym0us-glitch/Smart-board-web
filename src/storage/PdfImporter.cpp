#include "storage/PdfImporter.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <algorithm>

#ifdef CLASSBOARD_HAVE_QTPDF
#include <QPdfDocument>
#endif

namespace cb {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("Import", s); }

QString findTool(const QString& name, const QStringList& extraDirs)
{
    QString exe = QStandardPaths::findExecutable(name);
    if (exe.isEmpty())
        exe = QStandardPaths::findExecutable(name, extraDirs);
    return exe;
}

/// Page number of "page-12.png" (pdftoppm zero-pads numbers).
int pageNumber(const QString& file)
{
    const int dash = file.lastIndexOf(QLatin1Char('-'));
    const int dot = file.lastIndexOf(QLatin1Char('.'));
    return file.mid(dash + 1, dot - dash - 1).toInt();
}
} // namespace

// ==================================================================================== PdfImporter

PdfImporter::PdfImporter(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QVector<cb::ImportedPage>>("QVector<cb::ImportedPage>");
}

PdfImporter::~PdfImporter()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString PdfImporter::pdftoppmPath()
{
    // A copy shipped next to ClassBoard (poppler/bin or poppler/Library/bin) is also accepted.
    const QString app = QCoreApplication::applicationDirPath();
    return findTool(QStringLiteral("pdftoppm"),
                    {app + QStringLiteral("/poppler/bin"), app + QStringLiteral("/poppler/Library/bin"), app});
}

bool PdfImporter::isAvailable()
{
#ifdef CLASSBOARD_HAVE_QTPDF
    return true;
#else
    return !pdftoppmPath().isEmpty();
#endif
}

QString PdfImporter::unavailableReason()
{
    return tr("PDF import needs Poppler (pdftoppm). The Windows download of ClassBoard includes it; otherwise "
              "install Poppler and add its bin folder to PATH, or put it in a “poppler” folder next to ClassBoard.");
}

void PdfImporter::start(const QString& pdfPath, int dpi)
{
    m_dpi = std::clamp(dpi, 36, 600);
    if (!QFileInfo(pdfPath).isFile()) {
        QTimer::singleShot(0, this, [this, pdfPath]() { emit finished({}, tr("The file %1 does not exist.").arg(pdfPath)); });
        return;
    }
#ifdef CLASSBOARD_HAVE_QTPDF
    const int renderDpi = m_dpi;
    auto* worker = QThread::create([this, pdfPath, renderDpi]() {
        QPdfDocument doc;
        QVector<ImportedPage> pages;
        QString error;
        if (doc.load(pdfPath) != QPdfDocument::NoError) {
            error = tr("The PDF could not be opened. It may be damaged or protected.");
        } else {
            for (int i = 0; i < doc.pageCount(); ++i) {
                const QSizeF pt = doc.pageSize(i);
                const QImage image = doc.render(i, (pt * renderDpi / 72.0).toSize());
                ImportedPage page;
                QBuffer buffer(&page.encoded);
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "PNG");
                page.pixelSize = image.size();
                page.sizeMm = pt * 25.4 / 72.0;
                pages.push_back(page);
            }
            if (pages.isEmpty())
                error = tr("The PDF does not contain any pages.");
        }
        QMetaObject::invokeMethod(this, [this, pages, error]() { emit finished(pages, error); }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
#else
    const QString exe = pdftoppmPath();
    if (exe.isEmpty()) {
        QTimer::singleShot(0, this, [this]() { emit finished({}, unavailableReason()); });
        return;
    }
    m_dir = std::make_unique<QTemporaryDir>();
    if (!m_dir->isValid()) {
        QTimer::singleShot(0, this, [this]() { emit finished({}, tr("No temporary folder is available.")); });
        return;
    }
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PdfImporter::finishProcess);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process && m_process->state() == QProcess::NotRunning)
            finishProcess();
    });
    // -cropbox: render the visible page area exactly as a PDF viewer shows it (no bleed, no cropping).
    m_process->start(exe, {QStringLiteral("-png"), QStringLiteral("-cropbox"), QStringLiteral("-r"), QString::number(m_dpi),
                           pdfPath, m_dir->filePath(QStringLiteral("page"))});
#endif
}

void PdfImporter::finishProcess()
{
    if (!m_process)
        return;
    QString error;
    QVector<ImportedPage> pages;
    if (m_process->error() == QProcess::FailedToStart) {
        error = tr("pdftoppm could not be started.");
    } else if (m_process->exitStatus() != QProcess::NormalExit || m_process->exitCode() != 0) {
        const QString detail = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed().section(QLatin1Char('\n'), 0, 1);
        error = tr("The PDF could not be opened. It may be damaged, protected or not a PDF file.");
        if (!detail.isEmpty())
            error += QStringLiteral(" (") + detail + QLatin1Char(')');
    } else {
        QDir dir(m_dir->path());
        QStringList files = dir.entryList({QStringLiteral("page*.png")}, QDir::Files);
        std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) { return pageNumber(a) < pageNumber(b); });
        for (const QString& f : files) {
            QFile file(dir.filePath(f));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            ImportedPage page;
            page.encoded = file.readAll();
            QBuffer buffer(&page.encoded);
            buffer.open(QIODevice::ReadOnly);
            page.pixelSize = QImageReader(&buffer).size();
            if (page.pixelSize.isEmpty())
                continue;
            page.sizeMm = QSizeF(page.pixelSize) * 25.4 / m_dpi;
            pages.push_back(page);
        }
        if (pages.isEmpty())
            error = tr("The PDF does not contain any pages.");
    }
    m_process->deleteLater();
    m_process = nullptr;
    m_dir.reset();
    emit finished(pages, error);
}

// =========================================================================== PresentationImporter

PresentationImporter::PresentationImporter(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QVector<cb::ImportedPage>>("QVector<cb::ImportedPage>");
}

PresentationImporter::~PresentationImporter()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

QString PresentationImporter::libreOfficePath()
{
    const QString app = QCoreApplication::applicationDirPath();
    QStringList dirs = {app + QStringLiteral("/libreoffice/program")};
#ifdef Q_OS_WIN
    for (const QByteArray& var : {QByteArrayLiteral("ProgramFiles"), QByteArrayLiteral("ProgramFiles(x86)"), QByteArrayLiteral("ProgramW6432")}) {
        const QString base = QString::fromLocal8Bit(qgetenv(var.constData()));
        if (!base.isEmpty())
            dirs << base + QStringLiteral("/LibreOffice/program");
    }
    dirs << QStringLiteral("C:/Program Files/LibreOffice/program") << QStringLiteral("C:/Program Files (x86)/LibreOffice/program");
#elif defined(Q_OS_MACOS)
    dirs << QStringLiteral("/Applications/LibreOffice.app/Contents/MacOS");
#endif
    QString exe = findTool(QStringLiteral("soffice"), dirs);
    if (exe.isEmpty())
        exe = findTool(QStringLiteral("libreoffice"), dirs);
    return exe;
}

PresentationImporter::Converter PresentationImporter::availableConverter()
{
#ifdef Q_OS_WIN
    // Microsoft PowerPoint registers the "PowerPoint.Application" automation class.
    const QSettings reg(QStringLiteral("HKEY_CLASSES_ROOT\\PowerPoint.Application"), QSettings::NativeFormat);
    if (reg.childGroups().contains(QStringLiteral("CLSID"), Qt::CaseInsensitive))
        return Converter::PowerPoint;
#endif
    if (!libreOfficePath().isEmpty())
        return Converter::LibreOffice;
    return Converter::None;
}

bool PresentationImporter::isAvailable()
{
    return availableConverter() != Converter::None && PdfImporter::isAvailable();
}

QString PresentationImporter::unavailableReason()
{
    if (!PdfImporter::isAvailable())
        return PdfImporter::unavailableReason();
    return tr("PowerPoint import needs Microsoft PowerPoint or the free LibreOffice (both work offline). "
              "Install one of them and try again.");
}

void PresentationImporter::start(const QString& path, int dpi)
{
    m_dpi = dpi;
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    auto fail = [this](const QString& message) { QTimer::singleShot(0, this, [this, message]() { emit finished({}, message); }); };
    if (!info.isFile())
        return fail(tr("The file %1 does not exist.").arg(path));
    if (suffix != QLatin1String("ppt") && suffix != QLatin1String("pptx") && suffix != QLatin1String("pps")
        && suffix != QLatin1String("ppsx") && suffix != QLatin1String("odp"))
        return fail(tr("%1 is not a PowerPoint presentation.").arg(info.fileName()));
    // Check the file really is a presentation before handing it to an office suite (which may
    // otherwise "convert" any file, e.g. plain text, into a page).
    {
        QFile file(info.absoluteFilePath());
        const QByteArray head = file.open(QIODevice::ReadOnly) ? file.read(8) : QByteArray();
        const bool zip = head.startsWith("PK\x03\x04");                                   // .pptx .ppsx .odp
        const bool ole = head == QByteArray::fromHex("d0cf11e0a1b11ae1");                   // .ppt .pps
        const bool expectZip = suffix == QLatin1String("pptx") || suffix == QLatin1String("ppsx") || suffix == QLatin1String("odp");
        if ((expectZip && !zip) || (!expectZip && !ole))
            return fail(tr("%1 is damaged or not a real PowerPoint file.").arg(info.fileName()));
    }
    if (!PdfImporter::isAvailable())
        return fail(PdfImporter::unavailableReason());
    const Converter converter = availableConverter();
    if (converter == Converter::None)
        return fail(unavailableReason());
    m_dir = std::make_unique<QTemporaryDir>();
    if (!m_dir->isValid())
        return fail(tr("No temporary folder is available."));

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PresentationImporter::conversionFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process && m_process->state() == QProcess::NotRunning)
            conversionFinished();
    });
    const QString source = QDir::toNativeSeparators(info.absoluteFilePath());
    if (converter == Converter::PowerPoint) {
        m_expectedPdf = m_dir->filePath(QStringLiteral("slides.pdf"));
        const QString target = QDir::toNativeSeparators(m_expectedPdf);
        auto quote = [](QString s) { return QLatin1Char('\'') + s.replace(QLatin1Char('\''), QStringLiteral("''")) + QLatin1Char('\''); };
        // ppSaveAsPDF = 32. The presentation is opened read-only and without a window.
        const QString script = QStringLiteral("$ErrorActionPreference='Stop';"
                                              "$app=New-Object -ComObject PowerPoint.Application;"
                                              "try{$p=$app.Presentations.Open(%1,$true,$false,$false);"
                                              "$p.SaveAs(%2,32);$p.Close()}finally{$app.Quit()}")
                                   .arg(quote(source), quote(target));
        m_process->start(QStringLiteral("powershell.exe"),
                         {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
                          QStringLiteral("Bypass"), QStringLiteral("-Command"), script});
    } else {
        m_expectedPdf = m_dir->filePath(info.completeBaseName() + QStringLiteral(".pdf"));
        // A private profile avoids clashes with a LibreOffice window that is already open.
        const QString profile = QUrl::fromLocalFile(m_dir->filePath(QStringLiteral("profile"))).toString();
        m_process->start(libreOfficePath(),
                         {QStringLiteral("-env:UserInstallation=") + profile, QStringLiteral("--headless"),
                          QStringLiteral("--norestore"), QStringLiteral("--convert-to"), QStringLiteral("pdf"),
                          QStringLiteral("--outdir"), m_dir->path(), info.absoluteFilePath()});
    }
}

void PresentationImporter::conversionFinished()
{
    if (!m_process)
        return;
    const bool ok = m_process->exitStatus() == QProcess::NormalExit && m_process->exitCode() == 0
        && QFileInfo(m_expectedPdf).isFile() && QFileInfo(m_expectedPdf).size() > 0;
    const QString detail = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed().section(QLatin1Char('\n'), 0, 1);
    m_process->deleteLater();
    m_process = nullptr;
    if (!ok) {
        QString error = tr("The presentation could not be converted. It may be damaged, protected or open in another program.");
        if (!detail.isEmpty())
            error += QStringLiteral(" (") + detail + QLatin1Char(')');
        m_dir.reset();
        emit finished({}, error);
        return;
    }
    m_pdf = new PdfImporter(this);
    connect(m_pdf, &PdfImporter::finished, this, [this](const QVector<ImportedPage>& pages, const QString& error) {
        m_pdf->deleteLater();
        m_pdf = nullptr;
        m_dir.reset();
        emit finished(pages, error);
    });
    m_pdf->start(m_expectedPdf, m_dpi);
}

} // namespace cb
