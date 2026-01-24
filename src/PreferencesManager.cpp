#include "PreferencesManager.h"

PreferencesManager& PreferencesManager::instance() {
    static PreferencesManager mgr;
    return mgr;
}

PreferencesManager::PreferencesManager()
    : QObject(nullptr)
    , m_settings("libresoundboard", "libresoundboard") // org, app
{
}

PreferencesManager::~PreferencesManager() = default;

QSettings& PreferencesManager::settings() {
    return m_settings;
}

QStringList PreferencesManager::categoryNames() {
    return {
        QStringLiteral("Audio Engine"),
        QStringLiteral("Grid & Layout"),
        QStringLiteral("Waveform Cache"),
        QStringLiteral("File Handling"),
        QStringLiteral("Keyboard & Shortcuts"),
        QStringLiteral("Startup"),
        QStringLiteral("Debug"),
        QStringLiteral("Keep-Alive")
    };
}

int PreferencesManager::cacheSoftLimitMB() const {
    return m_settings.value("cache/softLimitMB", 200).toInt();
}

void PreferencesManager::setCacheSoftLimitMB(int mb) {
    if (mb < 0) mb = 0;
    m_settings.setValue("cache/softLimitMB", mb);
}

int PreferencesManager::cacheTtlDays() const {
    return m_settings.value("cache/ttlDays", 90).toInt();
}

void PreferencesManager::setCacheTtlDays(int days) {
    if (days < 0) days = 0;
    m_settings.setValue("cache/ttlDays", days);
}

double PreferencesManager::defaultGain() const {
    return m_settings.value("audio/defaultGain", 0.8).toDouble();
}

void PreferencesManager::setDefaultGain(double g) {
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    m_settings.setValue("audio/defaultGain", g);
}

PreferencesManager::LogLevel PreferencesManager::logLevel() const {
    int v = m_settings.value("debug/logLevel", static_cast<int>(Warning)).toInt();
    if (v < static_cast<int>(Off)) v = static_cast<int>(Off);
    if (v > static_cast<int>(Debug)) v = static_cast<int>(Debug);
    return static_cast<LogLevel>(v);
}

void PreferencesManager::setLogLevel(PreferencesManager::LogLevel lvl) {
    m_settings.setValue("debug/logLevel", static_cast<int>(lvl));
}
