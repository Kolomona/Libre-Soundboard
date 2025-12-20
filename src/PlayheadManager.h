#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QList>
#include <QString>

class SoundContainer;
class AudioEngine;

class PlayheadManager : public QObject {
    Q_OBJECT
public:
    explicit PlayheadManager(QObject* parent = nullptr);
    ~PlayheadManager() override;

    // initialize with pointer to AudioEngine (must outlive this manager)
    void init(AudioEngine* engine);

    // Singleton access
    static PlayheadManager* instance();

    // Register/unregister containers associated with a given file id
    void registerContainer(const QString& id, SoundContainer* sc, double durationSeconds, int sampleRate);
    void unregisterContainer(const QString& id, SoundContainer* sc);

        // Test helper: return last cached normalized position for a registered container
        float getLastPos(const QString& id, SoundContainer* sc) const;

    // Notify manager that playback was started/stopped for an id (used when AudioEngine isn't available)
    void playbackStarted(const QString& id, SoundContainer* sc);
    void playbackStopped(const QString& id, SoundContainer* sc);
    // Notify manager that all playback stopped (clear all playheads)
    void stopAll();

private slots:
    void onTick();

private:
    struct Entry {
        SoundContainer* sc = nullptr;
        double duration = 0.0;
        int sampleRate = 0;
        float lastPos = -1.0f;
        // simulated playback start time in ms since epoch; -1 = not simulating
        qint64 simStartMs = -1;
    };

    AudioEngine* m_engine = nullptr;
    QTimer m_timer;
    // map file id -> list of entries
    QMap<QString, QList<Entry>> m_map;
};
