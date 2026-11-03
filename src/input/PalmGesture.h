#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QtGlobal>

namespace cb {

// Tuning of the palm / wipe eraser gesture. Every threshold of the gesture lives here. Distances
// are physical (millimetres on the board) and are converted with the screen density, so the
// gesture feels the same on a 65" and on an 86" display.

/// Number of fingers held tightly together that turn into a wipe.
constexpr int PALM_ERASER_FINGER_COUNT = 3;
/// Largest centre-to-centre distance allowed between any two fingers of the group. Three fingers
/// held together (index, middle, ring) are about 30-38 mm apart from the first to the last;
/// fingers spread for writing or pinching are further apart.
constexpr qreal PALM_ERASER_MAX_SPACING_MM = 40.0;
/// The bounding box of the group must fit inside a square of this size.
constexpr qreal PALM_ERASER_MAX_GROUP_SIZE_MM = 40.0;
/// All fingers of the group must land within this time window: a hand is placed at once, while
/// fingers added later belong to a pinch or to another student.
constexpr qint64 PALM_ERASER_GROUP_WINDOW_MS = 450;
/// A finger that has already travelled this far is writing and is not taken into a wipe.
constexpr qreal PALM_ERASER_MAX_TRAVEL_MM = 20.0;
/// A single contact at least this wide (flat palm, side of the fist) wipes on its own.
constexpr qreal PALM_ERASER_PALM_CONTACT_MM = 24.0;
/// Size of the wipe: it covers every finger of the group plus a margin, and is never smaller
/// than the minimum radius.
constexpr qreal PALM_ERASER_MIN_RADIUS_MM = 14.0;
constexpr qreal PALM_ERASER_RADIUS_MARGIN_MM = 6.0;

/// Geometry of a group of touch contacts (positions in pixels).
struct PalmGroupShape
{
    QRectF bounds;          ///< bounding box of the contact centres
    qreal maxSpacing = 0.0; ///< largest distance between any two contacts
    int count = 0;
};

PalmGroupShape measurePalmGroup(const QVector<QPointF>& contacts);

/// True if the group has exactly PALM_ERASER_FINGER_COUNT contacts packed tightly enough to be
/// fingers held together (maximum spacing and bounding box both within the limits).
bool isTightPalmGroup(const PalmGroupShape& shape, qreal pixelsPerMm);

} // namespace cb
