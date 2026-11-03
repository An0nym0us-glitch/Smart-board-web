#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

namespace cb {

struct UiContext;
class TouchButton;

/// Labelled numeric entry for exact values: "Length  [ − | 7 | + ] cm". Large − / + buttons make
/// it usable on a touch board without a keyboard; typing (with "." or ",") works too.
/// valueEdited() is emitted only for user edits, never for setValue().
class NumberField : public QWidget
{
    Q_OBJECT
public:
    NumberField(const UiContext& ui, const QString& label, const QString& unit, QWidget* parent = nullptr);

    void setValue(double value);
    double value() const { return m_value; }
    void setDecimals(int decimals) { m_decimals = decimals; }
    void setStep(double step) { m_step = step; }
    void setRange(double min, double max);
    void setUnit(const QString& unit);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
    QString labelText() const;
    QLineEdit* lineEdit() const { return m_edit; }

    /// Parses "7", "7.5", "7,5", "−3" (typographic minus) as numbers.
    static bool parse(const QString& text, double* value);

signals:
    void valueEdited(double value);

private:
    void commitText();
    void stepBy(double delta);
    void showValue();

    const UiContext& m_ui;
    QLabel* m_label = nullptr;
    QLineEdit* m_edit = nullptr;
    QLabel* m_unit = nullptr;
    TouchButton* m_minus = nullptr;
    TouchButton* m_plus = nullptr;
    double m_value = 0.0;
    double m_step = 1.0;
    double m_min = -1e12;
    double m_max = 1e12;
    int m_decimals = 3;
};

} // namespace cb
