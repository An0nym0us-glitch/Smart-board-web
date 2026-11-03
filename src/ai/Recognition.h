#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

namespace cb {

/// Handwritten ink handed to a recogniser: strokes in page coordinates, in writing order.
struct InkSample
{
    QVector<QVector<QPointF>> strokes;
};

struct EquationCandidate
{
    QString latex;
    double confidence = 0.0; ///< 0..1
};

/// Interface for offline handwriting-to-formula recognisers (e.g. an ONNX model shipped as a
/// plug-in). ClassBoard ships no model; implementations register themselves with the registry
/// and the Equation popover then offers "Convert ink to formula".
class EquationRecognizer
{
public:
    virtual ~EquationRecognizer() = default;
    virtual QString name() const = 0;
    /// Returns candidates, best first. Must be thread-safe (called from a worker thread).
    virtual QVector<EquationCandidate> recognize(const InkSample& ink) const = 0;
};

/// Holds the available recognisers. Owned by the application (not a global).
class RecognizerRegistry
{
public:
    void registerEquationRecognizer(std::unique_ptr<EquationRecognizer> recognizer);
    const EquationRecognizer* equationRecognizer() const;
    bool hasEquationRecognizer() const { return !m_equation.empty(); }

    /// Loads recogniser plug-ins (shared libraries exporting a Qt plug-in implementing
    /// EquationRecognizerFactory) from a directory. Returns the number loaded.
    int loadPlugins(const QString& directory);

private:
    std::vector<std::unique_ptr<EquationRecognizer>> m_equation;
};

/// Qt plug-in interface for recogniser libraries.
class EquationRecognizerFactory
{
public:
    virtual ~EquationRecognizerFactory() = default;
    virtual std::unique_ptr<EquationRecognizer> create() = 0;
};

} // namespace cb

#include <QtPlugin>
#define ClassBoardEquationRecognizerFactory_iid "org.classboard.EquationRecognizerFactory/1.0"
Q_DECLARE_INTERFACE(cb::EquationRecognizerFactory, ClassBoardEquationRecognizerFactory_iid)
