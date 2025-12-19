#pragma once

#include <QTabBar>
#include <QPointer>
class QLabel;

class CustomTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit CustomTabBar(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;

private slots:
    void onTabMoved(int from, int to);

private:
    int m_pressIndex = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
    QLabel* m_dropIndicator = nullptr;
    int m_currentDropIndex = -1;
};
