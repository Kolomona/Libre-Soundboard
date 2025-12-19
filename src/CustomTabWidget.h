#pragma once

#include <QTabWidget>

class CustomTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    using QTabWidget::QTabWidget;
    void setTabBarPublic(QTabBar* b) { QTabWidget::setTabBar(b); }
};
