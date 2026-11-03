#include "platform/WebPlatform.h"

#if defined(Q_OS_WASM)

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QTimer>

#include <emscripten.h>
#include <emscripten/bind.h>

#include <string>

namespace cb::web {

namespace {

std::function<void()>& flushHandler()
{
    static std::function<void()> handler;
    return handler;
}

std::function<void(bool)>& fullScreenHandler()
{
    static std::function<void(bool)> handler;
    return handler;
}

std::function<void(const QStringList&, const QPoint&)>& dropHandler()
{
    static std::function<void(const QStringList&, const QPoint&)> handler;
    return handler;
}

QStringList& droppedFiles()
{
    static QStringList files;
    return files;
}

struct PdfRequest
{
    std::function<void(const QByteArray&, const QSize&, const QSizeF&)> page;
    std::function<void(const QString&)> finished;
};

QHash<int, PdfRequest>& pdfRequests()
{
    static QHash<int, PdfRequest> requests;
    return requests;
}

int g_nextPdfId = 1;

// Called from JavaScript (web/classboard-web.js).
void onFlush()
{
    if (flushHandler())
        flushHandler()();
    persist();
}

void onFullScreenChanged(bool on)
{
    if (fullScreenHandler())
        fullScreenHandler()(on);
}

void onDropFile(const std::string& name, const std::string& content)
{
    const QString fileName = QFileInfo(QString::fromStdString(name)).fileName();
    if (fileName.isEmpty())
        return;
    const bool lesson = fileName.endsWith(QStringLiteral(".classboard"), Qt::CaseInsensitive);
    const QString dir = lesson ? lessonsDirectory() : uploadDirectory();
    QDir().mkpath(dir);
    QFile file(dir + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(content.data(), static_cast<qint64>(content.size()));
    file.close();
    droppedFiles() << file.fileName();
    if (lesson)
        persist();
}

void onDropDone(int x, int y)
{
    const QStringList files = droppedFiles();
    droppedFiles().clear();
    if (files.isEmpty() || !dropHandler())
        return;
    const QPoint pos(x, y);
    QTimer::singleShot(0, [files, pos]() {
        if (dropHandler())
            dropHandler()(files, pos);
    });
}

void onPdfPage(int id, const std::string& png, int widthPx, int heightPx, double widthPt, double heightPt)
{
    const auto it = pdfRequests().constFind(id);
    if (it == pdfRequests().constEnd() || !it->page)
        return;
    it->page(QByteArray(png.data(), static_cast<int>(png.size())), QSize(widthPx, heightPx), QSizeF(widthPt, heightPt));
}

void onPdfDone(int id, const std::string& error)
{
    const PdfRequest request = pdfRequests().take(id);
    if (request.finished) {
        const QString message = QString::fromStdString(error);
        // Leave the JavaScript callback before the application continues.
        QTimer::singleShot(0, [request, message]() { request.finished(message); });
    }
}

} // namespace

QString persistentRoot()
{
    return QStringLiteral("/classboard");
}

QString lessonsDirectory()
{
    const QString dir = persistentRoot() + QStringLiteral("/lessons");
    QDir().mkpath(dir);
    return dir;
}

QString uploadDirectory()
{
    return QDir::tempPath() + QStringLiteral("/classboard-uploads");
}

void setDropHandler(std::function<void(const QStringList&, const QPoint&)> handler)
{
    dropHandler() = std::move(handler);
}

void persist()
{
    EM_ASM({
        if (window.classboardWeb && window.classboardWeb.persist)
            window.classboardWeb.persist();
    });
}

void openFile(const QString& accept, std::function<void(const QString&, const QByteArray&)> done)
{
    QFileDialog::getOpenFileContent(accept, [done](const QString& fileName, const QByteArray& content) {
        if (fileName.isEmpty())
            return; // cancelled
        done(fileName, content);
    });
}

void download(const QByteArray& content, const QString& fileName)
{
    QFileDialog::saveFileContent(content, fileName);
}

void setFlushHandler(std::function<void()> handler)
{
    flushHandler() = std::move(handler);
}

void setBrowserFullScreen(bool on)
{
    EM_ASM({
        if (window.classboardWeb && window.classboardWeb.setFullScreen)
            window.classboardWeb.setFullScreen(!!$0);
    }, on ? 1 : 0);
}

bool isBrowserFullScreen()
{
    return EM_ASM_INT({ return (window.classboardWeb && window.classboardWeb.isFullScreen()) ? 1 : 0; }) != 0;
}

void setFullScreenChangedHandler(std::function<void(bool)> handler)
{
    fullScreenHandler() = std::move(handler);
}

bool pdfRendererAvailable()
{
    return EM_ASM_INT({ return (window.classboardWeb && window.classboardWeb.canRenderPdf()) ? 1 : 0; }) != 0;
}

int renderPdf(const QByteArray& pdf, int dpi, std::function<void(const QByteArray&, const QSize&, const QSizeF&)> page,
              std::function<void(const QString&)> finished)
{
    const int id = g_nextPdfId++;
    pdfRequests().insert(id, PdfRequest{std::move(page), std::move(finished)});
    EM_ASM({
        var bytes = HEAPU8.slice($1, $1 + $2);
        window.classboardWeb.renderPdf($0, bytes, $3);
    }, id, pdf.constData(), pdf.size(), dpi);
    return id;
}

void cancelPdf(int id)
{
    pdfRequests().remove(id);
    EM_ASM({
        if (window.classboardWeb && window.classboardWeb.cancelPdf)
            window.classboardWeb.cancelPdf($0);
    }, id);
}

} // namespace cb::web

EMSCRIPTEN_BINDINGS(classboard_web)
{
    emscripten::function("classboardFlush", &cb::web::onFlush);
    emscripten::function("classboardFullScreenChanged", &cb::web::onFullScreenChanged);
    emscripten::function("classboardDropFile", &cb::web::onDropFile);
    emscripten::function("classboardDropDone", &cb::web::onDropDone);
    emscripten::function("classboardPdfPage", &cb::web::onPdfPage);
    emscripten::function("classboardPdfDone", &cb::web::onPdfDone);
}

#endif // Q_OS_WASM
