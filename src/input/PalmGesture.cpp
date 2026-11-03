#include "input/PalmGesture.h"

#include "core/Geometry.h"

#include <algorithm>

namespace cb {

PalmGroupShape measurePalmGroup(const QVector<QPointF>& contacts)
{
    PalmGroupShape shape;
    shape.count = contacts.size();
    if (contacts.isEmpty())
        return shape;
    qreal left = contacts.first().x(), right = left;
    qreal top = contacts.first().y(), bottom = top;
    for (int i = 0; i < contacts.size(); ++i) {
        const QPointF& p = contacts[i];
        left = std::min(left, p.x());
        right = std::max(right, p.x());
        top = std::min(top, p.y());
        bottom = std::max(bottom, p.y());
        for (int j = i + 1; j < contacts.size(); ++j)
            shape.maxSpacing = std::max(shape.maxSpacing, geom::distance(p, contacts[j]));
    }
    shape.bounds = QRectF(QPointF(left, top), QPointF(right, bottom));
    return shape;
}

bool isTightPalmGroup(const PalmGroupShape& shape, qreal pixelsPerMm)
{
    if (shape.count != PALM_ERASER_FINGER_COUNT)
        return false;
    const qreal maxBox = PALM_ERASER_MAX_GROUP_SIZE_MM * pixelsPerMm;
    return shape.maxSpacing <= PALM_ERASER_MAX_SPACING_MM * pixelsPerMm && shape.bounds.width() <= maxBox
        && shape.bounds.height() <= maxBox;
}

} // namespace cb
