#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <vector>

/**
 * KeepAliveMonitor: Detects silence on JACK input and triggers keep-alive playback.
 *
 * Behavior:
 * - Continuously monitors input audio samples for silence
 * - After 60 seconds of continuous silence, triggers keepAliveTriggered() signal
 * - Timer resets after a trigger, allowing repeated keep-alive cycles
 * - Operates independently of soundboard playback
 */
class KeepAliveMonitor : public QObject
{
    Q_OBJECT

public:
    explicit KeepAliveMonitor(QObject* parent = nullptr);
    ~KeepAliveMonitor() override;

    // Process audio samples from JACK input (called from audio callback)
    // samples: interleaved float samples
    // numFrames: number of audio frames in this batch
    // numChannels: number of channels (e.g., 2 for stereo)
    // If ANY sample in the batch is non-zero, the silence timer resets.
    // m_lastFrameHadSound tracks whether the last frame specifically had sound.
    void processInputSamples(const std::vector<float>& samples, int numFrames, int numChannels);

    // Get current silence duration in seconds
    double silenceDuration() const;

    // Reset the silence timer manually
    void resetSilenceTimer();

    // Enable peak-based sensitivity using dBFS (decibels relative to full scale).
    // Example: setSensitivityDbfs(-60.0) -> threshold amplitude ≈ 0.001.
    // To disable thresholding and treat any non-zero as sound, call setSensitivityDbfsDisabled().
    void setSensitivityDbfs(double dbfs);
    void setSensitivityDbfsDisabled();
    bool sensitivityEnabled() const { return m_thresholdAmplitude > 0.0; }
    // Returns the configured sensitivity in dBFS when enabled; otherwise returns -inf.
    double sensitivityDbfs() const;

    // Check if last audio frame had any sound (for diagnostics)
    bool lastFrameHadSound() const { return m_lastFrameHadSound; }

signals:
    // Emitted when silence duration reaches 60 seconds
    void keepAliveTriggered();

private:

    // Detect if the given frame has any non-zero audio
    bool frameHasSound(const std::vector<float>& samples, int frameStart, int numChannels) const;

    // Silence timeout in milliseconds (60 seconds)
    static constexpr qint64 SILENCE_TIMEOUT_MS = 60 * 1000;

    // Track whether the last processed frame had sound
    bool m_lastFrameHadSound = false;

    // Track if we've already emitted keepAliveTriggered for this silence period
    bool m_hasTriggeredForSilencePeriod = false;

    // Elapsed timer for tracking silence
    QElapsedTimer m_timer;

    // Peak amplitude threshold in linear units [0,1]. 0.0 means disabled (legacy behavior).
    double m_thresholdAmplitude = 0.0;

};
