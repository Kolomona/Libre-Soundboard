#include "KeepAliveMonitor.h"
#include <algorithm>
#include <cmath>

KeepAliveMonitor::KeepAliveMonitor(QObject* parent)
    : QObject(parent)
    , m_lastFrameHadSound(false)
    , m_hasTriggeredForSilencePeriod(false)
{
    // Initialize silence timer
    m_timer.start();
    // Default sensitivity: -60 dBFS (peak) -> amplitude ≈ 0.001
    m_thresholdAmplitude = std::pow(10.0, -60.0 / 20.0);

}

KeepAliveMonitor::~KeepAliveMonitor() = default;

void KeepAliveMonitor::processInputSamples(const std::vector<float>& samples, int numFrames, int numChannels)
{
    if (numFrames <= 0 || numChannels <= 0) {
        return;
    }

    // Check if ANY sample in the input batch has sound
    bool batchHasSound = false;
    if (m_thresholdAmplitude <= 0.0) {
        // Legacy behavior: any non-zero sample counts as sound
        for (const auto& sample : samples) {
            if (sample != 0.0f) {
                batchHasSound = true;
                break;
            }
        }
    } else {
        // Thresholded behavior: use peak absolute amplitude
        float peak = 0.0f;
        for (const auto& sample : samples) {
            float a = std::abs(sample);
            if (a > peak) peak = a;
            if (peak >= static_cast<float>(m_thresholdAmplitude)) {
                batchHasSound = true;
                break;
            }
        }
    }

    // Also track whether the last frame specifically had sound
    if (numFrames > 0) {
        m_lastFrameHadSound = frameHasSound(samples, (numFrames - 1) * numChannels, numChannels);
    }

    if (batchHasSound) {
        // Sound detected in any frame - reset silence timer
        resetSilenceTimer();
    } else {
        // Silence detected - check if we've reached the threshold
        qint64 elapsedMs = m_timer.elapsed();

        if (elapsedMs >= SILENCE_TIMEOUT_MS && !m_hasTriggeredForSilencePeriod) {
            m_hasTriggeredForSilencePeriod = true;
            emit keepAliveTriggered();
            
            // Immediately restart the timer so the next 60-second cycle begins
            // This allows the keep-alive to repeat indefinitely (true keep-alive behavior)
            m_timer.restart();
            m_hasTriggeredForSilencePeriod = false;
        }
    }
}

double KeepAliveMonitor::silenceDuration() const
{
    qint64 elapsedMs = m_timer.elapsed();
    return elapsedMs / 1000.0; // Convert to seconds
}

void KeepAliveMonitor::resetSilenceTimer()
{
    m_timer.restart();
    m_hasTriggeredForSilencePeriod = false;
}

bool KeepAliveMonitor::frameHasSound(const std::vector<float>& samples, int frameStart, int numChannels) const
{
    if (frameStart < 0 || frameStart >= static_cast<int>(samples.size())) {
        return false;
    }

    // Check all channels of the given frame
    int sampleCount = frameStart + numChannels;
    if (sampleCount > static_cast<int>(samples.size())) {
        sampleCount = samples.size();
    }

    for (int i = frameStart; i < sampleCount; ++i) {
        if (samples[i] != 0.0f) {
            return true;
        }
    }

    return false;
}


void KeepAliveMonitor::setSensitivityDbfs(double dbfs)
{
    // Convert dBFS to linear peak amplitude: A = 10^(D/20)
    // Clamp to [0,1]. Values <= 0 enable legacy behavior, so enforce minimal >0 for enabled.
    double amp = std::pow(10.0, dbfs / 20.0);
    if (amp <= 0.0) {
        m_thresholdAmplitude = 0.0;
    } else if (amp > 1.0) {
        m_thresholdAmplitude = 1.0;
    } else {
        m_thresholdAmplitude = amp;
    }
}

void KeepAliveMonitor::setSensitivityDbfsDisabled()
{
    m_thresholdAmplitude = 0.0;
}

double KeepAliveMonitor::sensitivityDbfs() const
{
    if (m_thresholdAmplitude <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    return 20.0 * std::log10(m_thresholdAmplitude);
}

