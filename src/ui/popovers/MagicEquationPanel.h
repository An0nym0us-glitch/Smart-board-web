#pragma once

#include "core/Id.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;

namespace cb {

struct AppServices;
class TouchButton;

/// ✨ Magic Equation Maker: recognises handwriting the teacher explicitly selected and shows the
/// result as a preview. Nothing on the page changes until Accept; Cancel (or closing the popover)
/// leaves the handwriting untouched. On failure it offers Try again, Edit manually (the Equation
/// Making Centre) and Cancel.
class MagicEquationPanel : public QWidget
{
    Q_OBJECT
public:
    enum class State { Recognizing, Result, Failed, Accepted, Cancelled };

    MagicEquationPanel(const AppServices& services, const QVector<ObjectId>& strokes, QWidget* parent = nullptr);
    ~MagicEquationPanel() override;

    State state() const { return m_state; }
    QString latex() const;
    double confidence() const { return m_confidence; }

    // Actions (also used by tests).
    void accept();
    void edit();
    void cancel();
    void tryAgain();
    void editManually();

signals:
    void stateChanged(cb::MagicEquationPanel::State state);

private:
    void startRecognition(bool relaxed);
    void showResult(const QString& latex, double confidence);
    void showFailure(const QString& reason);
    void setState(State state);

    const AppServices& m_s;
    QVector<ObjectId> m_strokes;
    State m_state = State::Recognizing;
    double m_confidence = 0.0;
    int m_generation = 0;
    bool m_relaxedTried = false;

    QWidget* m_working = nullptr;
    QWidget* m_result = nullptr;
    QWidget* m_failure = nullptr;
    QWidget* m_preview = nullptr;
    QLineEdit* m_latex = nullptr;
    QLabel* m_confidenceLabel = nullptr;
    QLabel* m_failureLabel = nullptr;
    TouchButton* m_tryAgain = nullptr;
};

} // namespace cb
