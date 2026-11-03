#pragma once

#include <QPointF>

namespace cb {

class ToolHost;
class DocumentObject;

/// Snapping for mathematical tools: to existing construction points first, then (optionally) to
/// whole coordinates of the applicable coordinate system (page grid or graph).
QPointF snapPoint(ToolHost& host, const QPointF& pagePos, bool* snapped = nullptr,
                  const DocumentObject* exclude = nullptr);

} // namespace cb
