#pragma once

#include "core/Id.h"

#include <QWidget>

namespace cb {

struct AppServices;
class DocumentObject;

/// Compact precision panel for a selected measurement, line, vector, point or shape: exact
/// lengths (board and real units through the lesson scale), directions, angles, rise / run /
/// slope, vector magnitude and direction, shape width / height / area / perimeter. Every edit is
/// one undoable command; the panel is rebuilt afterwards so all values stay synchronised.
class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    PropertiesPanel(const AppServices& services, const ObjectId& id, QWidget* parent = nullptr);

    /// True if the object has numeric properties this panel can edit or show.
    static bool supports(const DocumentObject& object);
    /// Popover title for the object ("Line", "Angle", "Vector" ...).
    static QString titleFor(const DocumentObject& object);
};

} // namespace cb
