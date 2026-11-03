#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

#if !defined(Q_OS_WASM)
#include <QThread>
#endif

namespace cb {

/// Runs \a work away from the interface and then \a done on the interface thread, as long as
/// \a context is still alive.
///
/// The desktop app runs \a work on a worker thread, so saving, autosave and exports never freeze
/// the board. The web version (WebAssembly) has no threads: there \a work runs on the interface
/// thread a moment after the call, which leaves the browser time to paint progress messages first.
inline void runInBackground(QObject* context, std::function<void()> work, std::function<void()> done = {},
                            bool lowPriority = false)
{
#if defined(Q_OS_WASM)
    Q_UNUSED(lowPriority);
    QTimer::singleShot(30, context, [work = std::move(work), done = std::move(done)]() {
        work();
        if (done)
            done();
    });
#else
    QThread* worker = QThread::create(std::move(work));
    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    if (done)
        QObject::connect(worker, &QThread::finished, context, [done = std::move(done)]() { done(); });
    worker->start(lowPriority ? QThread::LowPriority : QThread::InheritPriority);
#endif
}

} // namespace cb
