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
