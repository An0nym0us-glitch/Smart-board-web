#pragma once
// Synthetic handwriting for tests: symbols drawn from our own descriptions (independent of the
// recogniser's templates) with jitter, slant and uneven sampling, laid out like a teacher writes.

#include "core/Geometry.h"

#include <QPointF>
#include <QRandomGenerator>
#include <QRectF>
#include <QVector>

#include <cmath>

namespace testink {
using Strokes = QVector<QVector<QPointF>>;

/// Hand-drawn looking strokes for a symbol inside box (y down): drawn from our own descriptions
/// (independent of the recogniser's templates) with jitter, slant and uneven sampling.
class Writer
{
public:
    explicit Writer(quint32 seed)
        : m_rng(seed)
    {
    }

    Strokes symbol(QChar c, const QRectF& box)
    {
        Strokes out;
        auto P = [&](double x, double y) { return QPointF(x, y); };
        switch (c.unicode()) {
        case '0': out << arc(P(0.5, 0.5), 0.36, 0.5, 100, 365); break;
        case '1': out << line({P(0.35, 0.18), P(0.55, 0.0), P(0.53, 1.0)}); break;
        case '2': out << line({P(0.12, 0.3), P(0.25, 0.05), P(0.55, 0.0), P(0.8, 0.12), P(0.78, 0.35), P(0.5, 0.65), P(0.12, 1.0), P(0.9, 0.98)}); break;
        case '3': out << line({P(0.15, 0.1), P(0.5, 0.0), P(0.8, 0.15), P(0.7, 0.4), P(0.42, 0.48), P(0.8, 0.62), P(0.8, 0.88), P(0.5, 1.0), P(0.15, 0.9)}); break;
        case '4': out << line({P(0.65, 0.0), P(0.1, 0.68), P(0.9, 0.68)}) << line({P(0.68, 0.25), P(0.66, 1.0)}); break;
        case '5': out << line({P(0.8, 0.02), P(0.25, 0.0)}) << line({P(0.25, 0.0), P(0.2, 0.45), P(0.55, 0.38), P(0.82, 0.58), P(0.78, 0.9), P(0.45, 1.0), P(0.15, 0.9)}); break;
        case '7': out << line({P(0.1, 0.02), P(0.9, 0.0), P(0.4, 1.0)}); break;
        case 'x': out << line({P(0.1, 0.1), P(0.9, 0.92)}) << line({P(0.88, 0.08), P(0.12, 0.95)}); break;
        case 'y': out << line({P(0.15, 0.0), P(0.5, 0.55)}) << line({P(0.85, 0.0), P(0.28, 1.0)}); break;
        case '+': out << line({P(0.5, 0.05), P(0.5, 0.95)}) << line({P(0.05, 0.5), P(0.95, 0.52)}); break;
        case '-': out << line({P(0.0, 0.5), P(1.0, 0.52)}); break;
        case '(': out << arc(P(1.1, 0.5), 0.75, 0.55, 140, 80); break;
        case ')': out << arc(P(-0.1, 0.5), 0.75, 0.55, -40, 80); break;
        default: break;
        }
        // Map to the box with a slight slant and jitter.
        const double slant = (m_rng.generateDouble() - 0.5) * 0.15;
        for (auto& stroke : out) {
            for (QPointF& p : stroke) {
                const double x = p.x() + slant * (0.5 - p.y()) + jitter(0.02);
                const double y = p.y() + jitter(0.02);
                p = QPointF(box.left() + x * box.width(), box.top() + y * box.height());
            }
        }
        return out;
    }

    /// A horizontal bar (fraction line / minus) from x0 to x1 at y.
    Strokes bar(double x0, double x1, double y)
    {
        return {line({QPointF(x0, y), QPointF(x1, y + jitter(2))})};
    }

private:
    double jitter(double amount) { return (m_rng.generateDouble() - 0.5) * 2.0 * amount; }

    QVector<QPointF> line(const QVector<QPointF>& control)
    {
        // Densely sampled with uneven spacing, like real pen input.
        QVector<QPointF> out;
        for (int i = 0; i + 1 < control.size(); ++i) {
            const int steps = 6 + static_cast<int>(m_rng.bounded(6));
            for (int k = 0; k < steps; ++k)
                out.push_back(control[i] + (control[i + 1] - control[i]) * (double(k) / steps));
        }
        out.push_back(control.last());
        return out;
    }

    QVector<QPointF> arc(QPointF c, double rx, double ry, double startDeg, double sweepDeg)
    {
        QVector<QPointF> out;
        const int n = 30 + static_cast<int>(m_rng.bounded(10));
        for (int i = 0; i <= n; ++i) {
            const double a = cb::geom::degToRad(startDeg + sweepDeg * i / n);
            out.push_back(c + QPointF(rx * std::cos(a), -ry * std::sin(a)));
        }
        return out;
    }

    QRandomGenerator m_rng;
};

inline Strokes operator+(Strokes a, const Strokes& b)
{
    a += b;
    return a;
}

/// Writes a plain expression left to right: normal symbols 60 px tall, '^' raises the next symbol.
inline Strokes write(Writer& w, const QString& text, QPointF origin = QPointF(100, 300))
{
    Strokes ink;
    double x = origin.x();
    bool sup = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('^')) {
            sup = true;
            continue;
        }
        if (c == QLatin1Char(' ')) {
            x += 20;
            continue;
        }
        const bool small = c == QLatin1Char('x') || c == QLatin1Char('y') || c == QLatin1Char('+') || c == QLatin1Char('-');
        double h = small ? 42 : 60;
        double top = origin.y() + (60 - h);
        double wdt = c == QLatin1Char('1') ? 22 : (c == QLatin1Char('(') || c == QLatin1Char(')') ? 24 : 40);
        if (c == QLatin1Char('+') || c == QLatin1Char('-')) {
            top = origin.y() + 12;
            wdt = 40;
            h = 36;
        }
        if (sup) {
            h *= 0.55;
            wdt *= 0.6;
            top = origin.y() - 18;
            sup = false;
        }
        ink += w.symbol(c, QRectF(x, top, wdt, h));
        x += wdt + 18;
    }
    return ink;
}

} // namespace testink
