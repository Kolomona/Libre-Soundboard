#pragma once
#include <QWidget>

class PreferencesPage : public QWidget {
    Q_OBJECT
public:
    explicit PreferencesPage(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~PreferencesPage() = default;
    virtual void apply() {}
    virtual void reset() {}
};
