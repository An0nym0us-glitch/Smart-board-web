#pragma once

#include "geometry/Instrument.h"

#include <QHash>
#include <QObject>

#include <memory>
#include <vector>

class QPainter;

namespace cb {

class ViewTransform;

/// Holds the instruments currently lying on the board and routes pointer interactions to them.
class InstrumentLayer : public QObject
{
    Q_OBJECT
public:
    explicit InstrumentLayer(QObject* parent = nullptr);
    ~InstrumentLayer() override;

    bool isShown(Instrument::Kind kind) const { return find(kind) != nullptr; }
    /// Shows an instrument centred at pagePos (creates it on first use).
    void show(Instrument::Kind kind, const QPointF& pagePos);
    void hide(Instrument::Kind kind);
    void toggle(Instrument::Kind kind, const QPointF& pagePos);
    bool isEmpty() const { return m_instruments.empty(); }

    Instrument* find(Instrument::Kind kind) const;

    void setUnits(qreal pxPerUnit, const QString& label);

    /// Handle of the topmost instrument under pagePos (Instrument::kNoHandle if none).
    int handleAt(const QPointF& pagePos, qreal tol) const;

    /// Returns true if pagePos hits an instrument or one of its handles.
    bool hitTest(const QPointF& pagePos, qreal tol) const;

    /// Pointer routing. pointerDown returns true when an instrument takes the pointer.
    bool pointerDown(int pointerId, const QPointF& pagePos, qreal tol);
    bool ownsPointer(int pointerId) const { return m_owners.contains(pointerId); }
    void pointerMove(int pointerId, const QPointF& pagePos);
    /// Finishes the interaction; returns content to add to the page.
    InstrumentResult pointerUp(int pointerId);
    void pointerCancel(int pointerId);

    /// Nearest edge constraint to pagePos within tol (page units).
    bool edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const;

    /// Instrument under a page point, used for two-finger manipulation.
    Instrument* instrumentAt(const QPointF& pagePos, qreal tol) const;

    void paint(QPainter& painter, qreal zoom, const CoordinateSystem* coordinates) const;
    QRectF sceneBounds() const;

signals:
    /// Something visible changed; oldBounds/newBounds are page rects to repaint.
    void changed();
    void statusText(const QString& text);
    void visibilityChanged();

private:
    std::vector<std::unique_ptr<Instrument>> m_instruments; // z-order: last on top
    QHash<int, Instrument*> m_owners;
    qreal m_pxPerUnit = 40.0;
    QString m_unitLabel = QStringLiteral("cm");
};

} // namespace cb
