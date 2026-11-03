#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

class QWidget;

namespace cb {

class AppSettings;
class Document;
class Toast;

/// Runs PDF / PowerPoint / PNG exports on a worker thread with progress in the toast.
class ExportController : public QObject
{
    Q_OBJECT
public:
    enum class Format { Pdf, Pptx, Png };

    ExportController(Document& doc, AppSettings& settings, Toast& toast, QWidget* window, QObject* parent = nullptr);

    /// pages: zero-based indices (empty = all pages).
    void exportPages(Format format, const QVector<int>& pages);
    bool isBusy() const { return m_busy; }

    /// Parses "1-3, 5, 8-" style ranges (1-based) into zero-based indices. Returns false on errors.
    static bool parseRange(const QString& text, int pageCount, QVector<int>* pages);

signals:
    void busyChanged(bool busy);

private:
    Document& m_doc;
    AppSettings& m_settings;
    Toast& m_toast;
    QPointer<QWidget> m_window;
    bool m_busy = false;
};

} // namespace cb
