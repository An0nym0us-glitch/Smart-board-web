#pragma once

#include "core/Id.h"

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>

#include <utility>

namespace cb {

struct AppServices;
class Popover;
class PopoverHost;

/// Creates ClassBoard popovers by key and opens them anchored to ribbon buttons or canvas
/// objects. All contextual panels go through here so they behave identically.
class PopoverController : public QObject
{
    Q_OBJECT
public:
    explicit PopoverController(PopoverHost& host, QObject* parent = nullptr);

    void setServices(const AppServices* services) { m_services = services; }

    /// Opens key anchored to a widget, or closes it if it is already open there.
    void toggle(const QString& key, QWidget* anchor);
    void open(const QString& key, QWidget* anchor);
    void openAt(const QString& key, const QRect& anchorRect);
    /// Opens a child popover in place of the current one (with a back button).
    void push(const QString& key);
    void close();
    /// Rebuilds the open popover (after its underlying data changed structurally).
    void refresh();
    bool isOpen(const QString& key) const;
    QString currentKey() const;

    /// Editors for existing objects.
    void editEquation(const ObjectId& id, const QRect& anchorRect);
    /// Precision properties of a measurement / line / vector / shape.
    void editProperties(const ObjectId& id, const QRect& anchorRect);
    /// Magic Equation Maker for the given handwriting strokes.
    void openMagicEquation(const QVector<ObjectId>& strokes, const QRect& anchorRect);
    QVector<ObjectId> magicTargets() const { return m_magicTargets; }
    ObjectId equationTarget() const { return m_equationTarget; }
    /// LaTeX to start the next Equation popover with (used by the Magic Equation Maker).
    void setEquationPrefill(const QString& latex) { m_equationPrefill = latex; }
    QString takeEquationPrefill() { return std::exchange(m_equationPrefill, QString()); }
    void clearEquationTarget() { m_equationTarget = ObjectId(); }

    PopoverHost& host() { return m_host; }

signals:
    void opened(const QString& key);
    void closed(const QString& key);

private:
    Popover* create(const QString& key);

    PopoverHost& m_host;
    const AppServices* m_services = nullptr;
    ObjectId m_equationTarget;
    ObjectId m_propertiesTarget;
    QString m_equationPrefill;
    QVector<ObjectId> m_magicTargets;
};

} // namespace cb
