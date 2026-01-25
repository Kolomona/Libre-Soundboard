#include "PreferencesManager.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <cstdlib>

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

QString PreferencesManager::jackClientName() const {
    QString name = m_settings.value("audio/jackClientName", QStringLiteral("libre-soundboard")).toString();
    if (name.trimmed().isEmpty()) {
        name = QStringLiteral("libre-soundboard");
    }
    return name;
}

void PreferencesManager::setJackClientName(const QString& name) {
    QString n = name.trimmed();
    if (n.isEmpty()) {
        n = QStringLiteral("libre-soundboard");
    }
    m_settings.setValue("audio/jackClientName", n);
}

bool PreferencesManager::jackRememberConnections() const {
    return m_settings.value("audio/jackRememberConnections", true).toBool();
}

void PreferencesManager::setJackRememberConnections(bool enabled) {
    m_settings.setValue("audio/jackRememberConnections", enabled);
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

bool PreferencesManager::keepAliveEnabled() const {
    return m_settings.value("keepalive/enabled", true).toBool();
}

void PreferencesManager::setKeepAliveEnabled(bool enabled) {
    m_settings.setValue("keepalive/enabled", enabled);
}

int PreferencesManager::keepAliveTimeoutSeconds() const {
    int v = m_settings.value("keepalive/timeoutSeconds", 60).toInt();
    if (v < 1) v = 1;
    if (v > 3600) v = 3600;
    return v;
}

void PreferencesManager::setKeepAliveTimeoutSeconds(int seconds) {
    if (seconds < 1) seconds = 1;
    if (seconds > 3600) seconds = 3600;
    m_settings.setValue("keepalive/timeoutSeconds", seconds);
}

double PreferencesManager::keepAliveSensitivityDbfs() const {
    return m_settings.value("keepalive/sensitivityDbfs", -60.0).toDouble();
}

void PreferencesManager::setKeepAliveSensitivityDbfs(double dbfs) {
    if (dbfs > 0.0) dbfs = 0.0;
    if (dbfs < -120.0) dbfs = -120.0;
    m_settings.setValue("keepalive/sensitivityDbfs", dbfs);
}

bool PreferencesManager::keepAliveAnyNonZero() const {
    return m_settings.value("keepalive/anyNonZero", false).toBool();
}

void PreferencesManager::setKeepAliveAnyNonZero(bool any) {
    m_settings.setValue("keepalive/anyNonZero", any);
}

PreferencesManager::KeepAliveTarget PreferencesManager::keepAliveTarget() const {
    int v = m_settings.value("keepalive/target", 0).toInt();
    if (v < 0) v = 0; if (v > 1) v = 1;
    return static_cast<KeepAliveTarget>(v);
}

void PreferencesManager::setKeepAliveTarget(PreferencesManager::KeepAliveTarget t) {
    m_settings.setValue("keepalive/target", static_cast<int>(t));
}

int PreferencesManager::keepAliveTargetTab() const {
    int v = m_settings.value("keepalive/targetTab", 0).toInt();
    if (v < 0) v = 0;
    return v;
}

void PreferencesManager::setKeepAliveTargetTab(int tab) {
    if (tab < 0) tab = 0;
    m_settings.setValue("keepalive/targetTab", tab);
}

int PreferencesManager::keepAliveTargetSlot() const {
    int v = m_settings.value("keepalive/targetSlot", 0).toInt();
    if (v < 0) v = 0;
    return v;
}

void PreferencesManager::setKeepAliveTargetSlot(int slot) {
    if (slot < 0) slot = 0;
    m_settings.setValue("keepalive/targetSlot", slot);
}

bool PreferencesManager::keepAliveUseSlotVolume() const {
    return m_settings.value("keepalive/useSlotVolume", true).toBool();
}

void PreferencesManager::setKeepAliveUseSlotVolume(bool useSlotVolume) {
    m_settings.setValue("keepalive/useSlotVolume", useSlotVolume);
}

double PreferencesManager::keepAliveOverrideVolume() const {
    double v = m_settings.value("keepalive/overrideVolume", 1.0).toDouble();
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return v;
}

void PreferencesManager::setKeepAliveOverrideVolume(double vol) {
    if (vol < 0.0) vol = 0.0;
    if (vol > 1.0) vol = 1.0;
    m_settings.setValue("keepalive/overrideVolume", vol);
}

bool PreferencesManager::keepAliveAutoConnectInput() const {
    return m_settings.value("keepalive/autoConnectInput", true).toBool();
}

void PreferencesManager::setKeepAliveAutoConnectInput(bool enabled) {
    m_settings.setValue("keepalive/autoConnectInput", enabled);
}

int PreferencesManager::gridRows() const {
    int v = m_settings.value("grid/rows", 4).toInt();
    if (v < 2) v = 2;
    if (v > 8) v = 8;
    return v;
}

int PreferencesManager::gridCols() const {
    int v = m_settings.value("grid/cols", 8).toInt();
    if (v < 4) v = 4;
    if (v > 16) v = 16;
    return v;
}

void PreferencesManager::setGridRows(int rows) {
    if (rows < 2) rows = 2;
    if (rows > 8) rows = 8;
    m_settings.setValue("grid/rows", rows);
}

void PreferencesManager::setGridCols(int cols) {
    if (cols < 4) cols = 4;
    if (cols > 16) cols = 16;
    m_settings.setValue("grid/cols", cols);
}

// Phase 4: File/Path preferences
bool PreferencesManager::validatePath(const QString& path) const {
    QDir d(path);
    if (!d.exists()) {
        return false;
    }
    // Check if directory is writable by trying to create a test file
    QString testFile = d.filePath(".writabletest");
    QFile f(testFile);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.close();
    QFile::remove(testFile);
    return true;
}

QString PreferencesManager::defaultSoundDirectory() const {
    QString def = QDir::homePath();
    return m_settings.value("file/defaultSoundDirectory", def).toString();
}

void PreferencesManager::setDefaultSoundDirectory(const QString& path) {
    m_settings.setValue("file/defaultSoundDirectory", path);
}

QString PreferencesManager::cacheDirectory() const {
    // Return the configured cache directory, or compute a default
    QString stored = m_settings.value("cache/cacheDirectory", "").toString();
    if (!stored.isEmpty()) {
        return stored;
    }
    
    // Compute default based on WaveformCache logic
    const char* env = std::getenv("LIBRE_WAVEFORM_CACHE_DIR");
    if (env && env[0]) {
        return QString::fromUtf8(env);
    }
    
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.cache";
    
    QDir d(base);
    while (d.dirName() == "libresoundboard") {
        if (!d.cdUp()) break;
    }
    
    QString finalBase = QDir(d.absolutePath()).filePath("libresoundboard");
    return QDir(finalBase).filePath("waveforms");
}

void PreferencesManager::setCacheDirectory(const QString& path) {
    m_settings.setValue("cache/cacheDirectory", path);
}