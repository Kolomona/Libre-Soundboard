#pragma once
#include <QObject>
#include <QSettings>
#include <QStringList>

class PreferencesManager : public QObject {
    Q_OBJECT
public:
    static PreferencesManager& instance();

    QSettings& settings();

    static QStringList categoryNames();

private:
    PreferencesManager();
    ~PreferencesManager();
    PreferencesManager(const PreferencesManager&) = delete;
    PreferencesManager& operator=(const PreferencesManager&) = delete;

    QSettings m_settings;
};
