#pragma once
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QString>

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

    // Phase 6: JACK connection settings
    QString jackClientName() const;                // default "libre-soundboard"
    void setJackClientName(const QString& name);
    bool jackRememberConnections() const;          // default true
    void setJackRememberConnections(bool enabled);

    enum LogLevel { Off = 0, Error = 1, Warning = 2, Info = 3, Debug = 4 };
    LogLevel logLevel() const;              // default Warning
    void setLogLevel(LogLevel lvl);

    // Phase 8: Startup behavior
    enum class StartupBehavior { RestoreLastSession = 0, StartEmpty = 1 };
    StartupBehavior startupBehavior() const;        // default RestoreLastSession
    void setStartupBehavior(StartupBehavior b);

    // Phase 3: Keep-Alive preferences
    enum class KeepAliveTarget { LastTabLastSound = 0, SpecificSlot = 1 };
    bool keepAliveEnabled() const;                 // default true
    void setKeepAliveEnabled(bool enabled);
    int keepAliveTimeoutSeconds() const;           // default 60
    void setKeepAliveTimeoutSeconds(int seconds);
    double keepAliveSensitivityDbfs() const;       // default -60 dBFS
    void setKeepAliveSensitivityDbfs(double dbfs);
    bool keepAliveAnyNonZero() const;              // default false
    void setKeepAliveAnyNonZero(bool any);
    KeepAliveTarget keepAliveTarget() const;       // default LastTabLastSound
    void setKeepAliveTarget(KeepAliveTarget t);
    int keepAliveTargetTab() const;                // zero-based, default 0
    void setKeepAliveTargetTab(int tab);
    int keepAliveTargetSlot() const;               // zero-based, default 0
    void setKeepAliveTargetSlot(int slot);
    bool keepAliveUseSlotVolume() const;           // default true
    void setKeepAliveUseSlotVolume(bool useSlotVolume);
    double keepAliveOverrideVolume() const;        // default 1.0
    void setKeepAliveOverrideVolume(double vol);
    bool keepAliveAutoConnectInput() const;        // default true
    void setKeepAliveAutoConnectInput(bool enabled);

    // Phase 4: File/Path preferences
    // Validate that a directory path exists and is writable
    bool validatePath(const QString& path) const;
    
    // Get the default directory for opening sound files
    QString defaultSoundDirectory() const;
    void setDefaultSoundDirectory(const QString& path);
    
    // Get the cache directory for waveform data
    QString cacheDirectory() const;
    void setCacheDirectory(const QString& path);

    // Phase 5: Grid layout dimensions
    int gridRows() const;                 // default 4, range [2,8]
    int gridCols() const;                 // default 8, range [4,16]
    void setGridRows(int rows);
    void setGridCols(int cols);

private:
    PreferencesManager();
    ~PreferencesManager();
    PreferencesManager(const PreferencesManager&) = delete;
    PreferencesManager& operator=(const PreferencesManager&) = delete;

    QSettings m_settings;
};
