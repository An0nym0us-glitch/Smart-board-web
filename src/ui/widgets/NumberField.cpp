#include "ui/widgets/NumberField.h"

#include "core/Geometry.h"
#include "ui/UiContext.h"
#include "ui/widgets/TouchButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

#include <cmath>

namespace cb {

NumberField::NumberField(const UiContext& ui, const QString& label, const QString& unit, QWidget* parent)
    : QWidget(parent)
    , m_ui(ui)
{
    const Theme& t = ui.theme;
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(t.dpi(6));
    m_label = new QLabel(label, this);
    m_label->setMinimumWidth(t.dpi(96));
    m_minus = new TouchButton(ui, QString(), QStringLiteral("−"), TouchButton::Style::Pill, this);
    m_plus = new TouchButton(ui, QString(), QStringLiteral("+"), TouchButton::Style::Pill, this);
    m_minus->setToolTip(tr("Decrease"));
    m_plus->setToolTip(tr("Increase"));
    for (TouchButton* b : {m_minus, m_plus})
        b->setFixedWidth(t.dpi(48));
    m_edit = new QLineEdit(this);
    m_edit->setAlignment(Qt::AlignCenter);
    m_edit->setMinimumHeight(t.dpi(44));
    m_edit->setMinimumWidth(t.dpi(92));
    m_edit->setInputMethodHints(Qt::ImhFormattedNumbersOnly);
    m_unit = new QLabel(unit, this);
    m_unit->setMinimumWidth(t.dpi(30));
    layout->addWidget(m_label);
    layout->addWidget(m_minus);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_plus);
    layout->addWidget(m_unit);
    connect(m_edit, &QLineEdit::editingFinished, this, &NumberField::commitText);
    connect(m_minus, &QAbstractButton::clicked, this, [this]() { stepBy(-m_step); });
    connect(m_plus, &QAbstractButton::clicked, this, [this]() { stepBy(m_step); });
    showValue();
}

bool NumberField::parse(const QString& text, double* value)
{
    QString s = text.trimmed();
    s.replace(QChar(0x2212), QLatin1Char('-'));
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    s.remove(QLatin1Char(' '));
    if (s.isEmpty())
        return false;
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    if (value)
        *value = v;
    return true;
}

void NumberField::setValue(double value)
{
    m_value = value;
    showValue();
}

void NumberField::setRange(double min, double max)
{
    m_min = min;
    m_max = max;
}

void NumberField::setUnit(const QString& unit)
{
    m_unit->setText(unit);
}

void NumberField::setReadOnly(bool readOnly)
{
    m_edit->setReadOnly(readOnly);
    m_minus->setVisible(!readOnly);
    m_plus->setVisible(!readOnly);
}

bool NumberField::isReadOnly() const
{
    return m_edit->isReadOnly();
}

QString NumberField::labelText() const
{
    return m_label->text();
}

void NumberField::showValue()
{
    m_edit->setText(geom::formatNumber(m_value, m_decimals).replace(QChar(0x2212), QLatin1Char('-')));
    m_edit->setStyleSheet(QString());
}

void NumberField::commitText()
{
    if (m_edit->isReadOnly())
        return;
    double v = 0.0;
    if (!parse(m_edit->text(), &v) || v < m_min || v > m_max) {
        // Keep the previous value; mark the entry so the teacher sees it was not accepted.
        showValue();
        m_edit->setStyleSheet(QStringLiteral("border: 2px solid #ef5350;"));
        return;
    }
    if (std::abs(v - m_value) < 1e-12)
        return;
    m_value = v;
    showValue();
    emit valueEdited(v);
}

void NumberField::stepBy(double delta)
{
    double v = m_value + delta;
    // Snap to the step grid so repeated taps give round numbers.
    if (m_step > 0)
        v = std::round(v / m_step) * m_step;
    v = std::clamp(v, m_min, m_max);
    if (std::abs(v - m_value) < 1e-12)
        return;
    m_value = v;
    showValue();
    emit valueEdited(v);
}

} // namespace cb
