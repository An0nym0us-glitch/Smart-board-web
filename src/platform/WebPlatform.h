#pragma once

// Browser integration for the web version of ClassBoard (Qt for WebAssembly).
//
// The web version runs the same application code as the desktop app. Only the parts that touch
// the operating system are different, and they go through these functions:
//
// * Files: "Open" uses the browser's file picker; "Save" keeps the lesson in the browser's
//   storage and downloads a .classboard file; exports are downloaded.
// * Storage: settings, custom templates, saved lessons and the autosave / recovery copies live in
//   a folder that is kept in the browser's IndexedDB, so they survive closing the tab.
// * PDF import renders pages with pdf.js in the browser.
//
// This header is only used when building for WebAssembly (Q_OS_WASM).

#include <QByteArray>
#include <QSize>
#include <QSizeF>
#include <QPoint>
#include <QString>
#include <QStringList>

#include <functional>

namespace cb::web {

/// Root of the folder that is kept in the browser's storage (IndexedDB).
QString persistentRoot();
/// Folder in the browser's storage where saved and opened lessons are kept.
QString lessonsDirectory();
/// Temporary folder (memory only) for files picked or dropped in the browser.
QString uploadDirectory();

/// Writes changes in the persistent folder to the browser's storage (asynchronous).
void persist();

/// Shows the browser's file picker. \a accept is an HTML accept list such as ".classboard" or
/// "image/*". \a done receives the file name and content once the file has been read; it is not
/// called when the user cancels.
void openFile(const QString& accept, std::function<void(const QString& fileName, const QByteArray& content)> done);

/// Hands \a content to the browser as a download named \a fileName.
void download(const QByteArray& content, const QString& fileName);

/// Called when the browser tab is hidden or closed, so unsaved work can be written first.
void setFlushHandler(std::function<void()> handler);

/// Called when files are dropped onto the page: their copies (lessons in lessonsDirectory(),
/// everything else in uploadDirectory()) and the drop position in global coordinates.
void setDropHandler(std::function<void(const QStringList& paths, const QPoint& globalPos)> handler);

/// Browser full screen (the whole page, not just the application window).
void setBrowserFullScreen(bool on);
bool isBrowserFullScreen();
/// Called when the browser enters or leaves full screen (also when the user presses Esc).
void setFullScreenChangedHandler(std::function<void(bool on)> handler);

/// True if the browser can render PDF pages (pdf.js loaded).
bool pdfRendererAvailable();

/// Renders every page of a PDF document with pdf.js at \a dpi. \a page is called for each
/// rendered page (PNG bytes, pixel size, page size in points), then \a finished once with an
/// empty error on success. Returns an id for cancelPdf().
int renderPdf(const QByteArray& pdf, int dpi,
              std::function<void(const QByteArray& png, const QSize& pixels, const QSizeF& points)> page,
              std::function<void(const QString& error)> finished);
void cancelPdf(int id);

} // namespace cb::web
