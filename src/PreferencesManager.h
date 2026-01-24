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

    // Phase 2: simple scalar preferences
    int cacheSoftLimitMB() const;           // default 200
    void setCacheSoftLimitMB(int mb);
    int cacheTtlDays() const;               // default 90
    void setCacheTtlDays(int days);
    double defaultGain() const;             // default 0.8
    void setDefaultGain(double g);

    enum LogLevel { Off = 0, Error = 1, Warning = 2, Info = 3, Debug = 4 };
    LogLevel logLevel() const;              // default Warning
    void setLogLevel(LogLevel lvl);

private:
    PreferencesManager();
    ~PreferencesManager();
    PreferencesManager(const PreferencesManager&) = delete;
    PreferencesManager& operator=(const PreferencesManager&) = delete;

    QSettings m_settings;
};
