#pragma once

#include <QByteArray>
#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QVector>

#include <memory>

class QProcess;
class QTemporaryDir;

namespace cb {

/// One imported page: encoded image bytes (PNG) plus the physical page size.
struct ImportedPage
{
    QByteArray encoded;
    QSize pixelSize;
    QSizeF sizeMm; ///< page size in millimetres as defined by the source document
};

/// Converts PDF pages to images for annotation, preserving each page's dimensions.
///
/// Uses Qt PDF when ClassBoard is built against it; otherwise the Poppler command line tool
/// "pdftoppm" (on PATH or in a poppler/bin folder next to ClassBoard). The conversion runs
/// asynchronously, completely offline, and never blocks the interface.
class PdfImporter : public QObject
{
    Q_OBJECT
public:
    static constexpr int kDefaultDpi = 150;

    explicit PdfImporter(QObject* parent = nullptr);
    ~PdfImporter() override;

    /// True if a PDF backend is available on this machine.
    static bool isAvailable();
    static QString unavailableReason();
    /// Path of pdftoppm if found (empty otherwise).
    static QString pdftoppmPath();

    /// Starts converting; emits finished() when done.
    void start(const QString& pdfPath, int dpi = kDefaultDpi);

signals:
    void finished(const QVector<cb::ImportedPage>& pages, const QString& error);

private:
    void finishProcess();
    std::unique_ptr<QTemporaryDir> m_dir;
    QProcess* m_process = nullptr;
    int m_dpi = kDefaultDpi;
};

/// Imports PowerPoint presentations (.ppt and .pptx). Slides are converted to PDF offline with an
/// installed office suite (Microsoft PowerPoint through its automation interface on Windows, or
/// LibreOffice in headless mode on any platform) and then rendered page by page, so every slide
/// keeps its complete area and aspect ratio.
class PresentationImporter : public QObject
{
    Q_OBJECT
public:
    enum class Converter { None, PowerPoint, LibreOffice };

    explicit PresentationImporter(QObject* parent = nullptr);
    ~PresentationImporter() override;

    static Converter availableConverter();
    static bool isAvailable();
    static QString unavailableReason();
    /// Path of LibreOffice's soffice executable if found.
    static QString libreOfficePath();

    void start(const QString& presentationPath, int dpi = PdfImporter::kDefaultDpi);

signals:
    void finished(const QVector<cb::ImportedPage>& pages, const QString& error);

private:
    void conversionFinished();
    std::unique_ptr<QTemporaryDir> m_dir;
    QProcess* m_process = nullptr;
    PdfImporter* m_pdf = nullptr;
    QString m_expectedPdf;
    int m_dpi = PdfImporter::kDefaultDpi;
};

} // namespace cb

Q_DECLARE_METATYPE(cb::ImportedPage)
