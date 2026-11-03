#pragma once

#include "document/DocumentObject.h"

#include <QWidget>

#include <functional>

class QLineEdit;

namespace cb {

struct AppServices;
class GraphObject;

/// Geometry popover: instruments, measurements, constructions, coordinates.
class GeometryPanel : public QWidget
{
    Q_OBJECT
public:
    enum class Mode { Full, MeasureOnly };
    GeometryPanel(const AppServices& services, Mode mode, QWidget* parent = nullptr);
};

/// Equation popover: symbol palette, LaTeX input with live typeset preview, insert / update.
class EquationPanel : public QWidget
{
    Q_OBJECT
public:
    EquationPanel(const AppServices& services, QWidget* parent = nullptr);

private:
    void insertSnippet(const QString& snippet);
    void commit();

    const AppServices& m_s;
    QLineEdit* m_input = nullptr;
    QWidget* m_preview = nullptr;
    qreal m_size = 56.0;
};

/// Function / graph popover: functions, parameter sliders, view, points, vectors, value tables.
class FunctionPanel : public QWidget
{
    Q_OBJECT
public:
    FunctionPanel(const AppServices& services, QWidget* parent = nullptr);

private:
    GraphObject* graph() const;
    void modify(const QString& text, const std::function<void(GraphObject&)>& change);
    void rebuild();

    const AppServices& m_s;
    ObjectId m_graphId;
    ObjectPtr m_sliderBefore;
};

/// Table popover: insert tables or change rows / columns of the selected table.
class TablePanel : public QWidget
{
    Q_OBJECT
public:
    TablePanel(const AppServices& services, QWidget* parent = nullptr);
};

} // namespace cb
