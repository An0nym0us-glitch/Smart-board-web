#pragma once

#include "ai/Recognition.h"

#include <QRectF>

namespace cb {

/// Built-in offline handwriting recogniser for simple mathematical expressions.
///
/// Strokes are grouped into symbols (overlapping / crossing strokes form one symbol), fraction bars
/// and equals signs are found geometrically, and every other symbol is classified with a $P
/// point-cloud matcher (stroke order and direction independent) against built-in templates of the
/// digits 0–9, the letters a b c n x y, "+" and parentheses. The layout step detects superscripts
/// (x²) and fractions and produces LaTeX, e.g. "x^{2}+1" or "\frac{1}{2}".
///
/// It needs no model files, no network and runs in milliseconds. It is intentionally conservative:
/// if any symbol is uncertain it reports failure instead of guessing (unless relaxed is set), so
/// handwriting is never replaced by a wrong formula without the teacher's decision.
class MathInkRecognizer final : public EquationRecognizer
{
public:
    struct Options
    {
        /// Minimum per-symbol score (0..1) for a strict result.
        double minScore = 0.6;
        /// Relaxed mode ("Try again"): always returns the best guess, with its low confidence.
        bool relaxed = false;
    };

    MathInkRecognizer() = default;
    explicit MathInkRecognizer(const Options& options)
        : m_options(options)
    {
    }

    QString name() const override { return QStringLiteral("ClassBoard offline math"); }
    QVector<EquationCandidate> recognize(const InkSample& ink) const override;

    /// Classifies a single symbol (for tests and diagnostics). Returns the symbol and its score.
    QString classifySymbol(const QVector<QVector<QPointF>>& strokes, double* score = nullptr) const;
    /// Symbols the built-in templates cover.
    static QStringList supportedSymbols();

private:
    Options m_options;
};

} // namespace cb
