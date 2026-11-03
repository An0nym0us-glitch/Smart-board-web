#pragma once

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QString>

#include <memory>
#include <vector>

class QPainter;

namespace cb::mathtype {

/// A laid-out box of typeset mathematics. Coordinates: x grows right, the baseline is at y = 0,
/// ascent extends upwards (negative y) and descent downwards.
class Box
{
public:
    virtual ~Box() = default;
    qreal width = 0;
    qreal ascent = 0;
    qreal descent = 0;
    qreal height() const { return ascent + descent; }
    /// Paints with the baseline origin at pos.
    virtual void paint(QPainter& p, const QPointF& pos, const QColor& color) const = 0;
};

using BoxPtr = std::unique_ptr<Box>;

/// Typesets a LaTeX-style formula (\frac, \sqrt, ^, _, \int, \sum, \lim, \vec, matrices, Greek
/// letters, relations and arrows) into a box tree that renders with plain QPainter text and
/// paths, so it is crisp at any zoom and stays vector in PDF export.
class MathTypesetter
{
public:
    explicit MathTypesetter(const QString& fontFamily = QString());

    /// Lays out a formula at the given base pixel size. Never fails: unknown commands are shown
    /// literally so the user can see and fix them.
    BoxPtr layout(const QString& latex, qreal pixelSize) const;

private:
    QString m_family;
};

} // namespace cb::mathtype
