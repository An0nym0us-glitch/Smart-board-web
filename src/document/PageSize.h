#pragma once

#include <QSizeF>
#include <QString>
#include <QVector>

namespace cb {

/// Standard logical page sizes. Page coordinates are logical document units (40 units = 1 cm on
/// the board, the same unit the measurement system uses), so a page size never depends on the
/// monitor resolution, DPI or zoom.
namespace pagesize {

constexpr double kUnitsPerCm = 40.0;

enum class Preset {
    Board16x9, ///< classroom board, 48 × 27 cm (1920 × 1080 units)
    Board4x3,  ///< 36 × 27 cm
    A4,        ///< 21 × 29.7 cm
    A3,        ///< 29.7 × 42 cm
    Custom,
};

enum class Orientation { Landscape, Portrait };

struct Info
{
    Preset preset;
    QString id;
    QString label;
    QSizeF landscapeCm; ///< width >= height
};

const QVector<Info>& presets();

/// Logical size in document units for a preset and orientation (Custom returns the 16:9 size).
QSizeF sizeFor(Preset preset, Orientation orientation);
/// Size in document units for a custom size given in centimetres (clamped to 5 cm .. 500 cm).
QSizeF fromCentimetres(const QSizeF& cm);
QSizeF toCentimetres(const QSizeF& units);

/// Recognises the preset of a page size (Custom if it matches none).
Preset presetOf(const QSizeF& size);
Orientation orientationOf(const QSizeF& size);
/// "Classroom board 16:9", "A4 portrait", "Custom 30 × 20 cm"
QString describe(const QSizeF& size);

/// Size (in document units) of a page that shows content with the given aspect ratio: the long
/// side matches the classroom board, so imported slides and PDF pages are neither cropped nor
/// stretched.
QSizeF forAspect(qreal width, qreal height);

} // namespace pagesize
} // namespace cb
