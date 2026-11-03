#pragma once

#include "tools/Tool.h"
#include "ui/UiContext.h"

#include <QVector>
#include <QWidget>

namespace cb {

class TouchButton;

/// The bottom command ribbon: always visible, large touch targets, the primary command centre.
///
/// MORE | PEN ERASER SELECT [tool] | UNDO REDO | FILE INSERT EDIT | ‹ 3 / 12 › NEW PAGE, PAGE ▾ | − 100% + FIT | full screen
///
/// Grouped buttons (File, Insert, Edit, Page, View) open attached popovers. When the window is too
/// narrow the optional groups are hidden (everything stays reachable through MORE).
///
/// The ribbon has its own size on top of the interface size (Settings → Ribbon size), so a
/// large board can have a slim ribbon, or a small screen a bigger one, without changing menus.
class Ribbon : public QWidget
{
    Q_OBJECT
public:
    static constexpr qreal kMinScale = 0.6;
    static constexpr qreal kMaxScale = 1.5;

    /// scale: the ribbon size relative to the interface size (1 = normal).
    explicit Ribbon(const UiContext& ui, qreal scale = 1.0, QWidget* parent = nullptr);

    qreal scale() const { return m_scale; }
    /// Resizes the ribbon in place (buttons, icons, labels and height).
    void setScale(qreal scale);

    TouchButton* moreButton() const { return m_more; }
    TouchButton* penButton() const { return m_pen; }
    TouchButton* eraserButton() const { return m_eraser; }
    TouchButton* selectButton() const { return m_select; }
    TouchButton* toolChip() const { return m_toolChip; }
    TouchButton* fileButton() const { return m_file; }
    TouchButton* insertButton() const { return m_insert; }
    TouchButton* editButton() const { return m_edit; }
    TouchButton* pageButton() const { return m_page; }
    TouchButton* pageMenuButton() const { return m_pageMenu; }
    TouchButton* newPageButton() const { return m_newPage; }
    TouchButton* zoomButton() const { return m_zoom; }
    TouchButton* zoomInButton() const { return m_zoomIn; }
    TouchButton* zoomOutButton() const { return m_zoomOut; }
    TouchButton* fitButton() const { return m_fit; }

    void setActiveTool(ToolId id);
    /// Shows a chip for tools reached through MORE (shape, text, measure ...). Empty icon hides it.
    void setToolChip(const QString& icon, const QString& label);
    void setPenColor(const QColor& color);
    void setPageInfo(int currentIndex, int count);
    void setUndoRedo(bool canUndo, bool canRedo, const QString& undoText, const QString& redoText);
    void setZoomPercent(int percent);
    void setFullScreen(bool on);
    void setMoreOpen(bool open);

    QSize sizeHint() const override;

signals:
    void moreClicked();
    void penClicked();
    void eraserClicked();
    void selectClicked();
    void toolChipClicked();
    void undoClicked();
    void redoClicked();
    void fileClicked();
    void insertClicked();
    void editClicked();
    void previousPageClicked();
    void nextPageClicked();
    void pageIndicatorClicked();
    void newPageClicked();
    void pageMenuClicked();
    void zoomClicked();
    void zoomInClicked();
    void zoomOutClicked();
    void fitClicked();
    void fullScreenClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    TouchButton* addButton(const QString& icon, const QString& text);
    /// Hides optional groups until everything fits into the available width.
    void updateFit();
    void applyMetrics();

    /// The ribbon's own theme: the interface theme at the ribbon's size.
    UiContext m_ui;
    qreal m_baseScale = 1.0; ///< the interface size
    qreal m_scale = 1.0;
    QVector<QWidget*> m_separators;
    TouchButton* m_more = nullptr;
    TouchButton* m_pen = nullptr;
    TouchButton* m_eraser = nullptr;
    TouchButton* m_select = nullptr;
    TouchButton* m_toolChip = nullptr;
    TouchButton* m_undo = nullptr;
    TouchButton* m_redo = nullptr;
    TouchButton* m_file = nullptr;
    TouchButton* m_insert = nullptr;
    TouchButton* m_edit = nullptr;
    TouchButton* m_prev = nullptr;
    TouchButton* m_page = nullptr;
    TouchButton* m_next = nullptr;
    TouchButton* m_newPage = nullptr;
    TouchButton* m_pageMenu = nullptr;
    TouchButton* m_zoomOut = nullptr;
    TouchButton* m_zoom = nullptr;
    TouchButton* m_zoomIn = nullptr;
    TouchButton* m_fit = nullptr;
    TouchButton* m_fullScreen = nullptr;
    QWidget* m_groupSeparator = nullptr;
    /// Optional widgets in the order in which they are hidden when space runs out.
    QVector<QVector<QWidget*>> m_optional;
    bool m_fitting = false;
};

} // namespace cb
