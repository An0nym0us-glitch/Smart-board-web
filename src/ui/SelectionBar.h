#pragma once

#include <QWidget>

namespace cb {

struct AppServices;
class TouchButton;

/// Compact action bar that floats above the current selection (edit, colour, arrange,
/// duplicate, copy, delete) so touch users never need a keyboard or a context menu.
class SelectionBar : public QWidget
{
    Q_OBJECT
public:
    SelectionBar(const AppServices& services, QWidget* canvas);

    /// Re-evaluates visibility and position.
    void refresh();

signals:
    void editRequested();
    void magicRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const AppServices& m_s;
    TouchButton* m_edit = nullptr;
    TouchButton* m_color = nullptr;
    TouchButton* m_precision = nullptr;
    TouchButton* m_magic = nullptr;
};

} // namespace cb
