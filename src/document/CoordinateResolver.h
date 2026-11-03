#pragma once

#include <QPointF>
#include <QVector>

#include "math/CoordinateSystem.h"

namespace cb {

class DocumentObject;
class Page;

/// Chooses the mathematical coordinate system for page points: if all points lie inside an
/// object with its own coordinate system (a function graph) that one is used, otherwise the
/// page-global system. This is what makes measurements inside a graph use graph units.
const CoordinateSystem* resolveCoordinateSystem(const Page* page, const CoordinateSystem* global,
                                                const QVector<QPointF>& pagePoints,
                                                const DocumentObject* exclude = nullptr);

/// The document's global coordinate system as it applies to a page: the origin sits at the page
/// centre (for the default board size this is the stored system unchanged), units and scale are
/// shared by every page.
CoordinateSystem pageCoordinateSystem(const CoordinateSystem& global, const Page* page);

} // namespace cb
