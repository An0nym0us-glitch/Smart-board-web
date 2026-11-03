#pragma once

#include <QColor>
#include <QPointF>
#include <QSizeF>
#include <QString>

class QPainter;

namespace cb {

/// Draws a readable value label (rounded dark plate + coloured text) centred at anchor.
/// counterRotation undoes the object's rotation so the label stays upright.
void paintValueLabel(QPainter& painter, const QPointF& anchor, const QString& text, const QColor& color,
                     qreal counterRotation = 0.0, qreal pixelSize = 22.0);

/// Size of the plate paintValueLabel() draws for text.
QSizeF valueLabelSize(const QString& text, qreal pixelSize = 22.0);

/// Anchor for a label beside a segment a-b: at the midpoint, pushed away from the segment along
/// its normal far enough that the whole label clears the line (preferring the upper side).
QPointF labelBesideSegment(const QPointF& a, const QPointF& b, const QString& text, qreal gap, qreal pixelSize = 22.0);

} // namespace cb
