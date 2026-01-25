#pragma once

#include <vector>
#include <string>

class KeepAliveMonitor;

/**
 * AudioEngine: thin wrapper around JACK client for playback control.
 * This is a stubbed implementation for initial development.
 */
struct AudioEnginePrivate;
class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    bool init();
    void shutdown();
    /**
     * Play interleaved float samples (any sample rate/channels). This will resample
     * to the JACK sample rate and queue for playback. An optional `id` may be
     * provided so subsequent play requests with the same id will restart that
     * voice instead of adding a new concurrent voice.
     */
    bool playBuffer(const std::vector<float>& samples, int sampleRate, int channels, const std::string& id = std::string(), float gain = 1.0f);

    // Stop all currently playing voices
    void stopAll();

    // Update gain for active voices matching id
    void setVoiceGainById(const std::string& id, float gain);

    // Stop voices matching id
    void stopVoicesById(const std::string& id);

    struct PlaybackInfo {
        bool found = false;
        uint64_t frames = 0; // frames (not interleaved samples)
        int sampleRate = 0;
        uint64_t totalFrames = 0;
    };

    // Thread-safe query to obtain current playback frames/sampleRate for a voice id
    PlaybackInfo getPlaybackInfoForId(const std::string& id) const;

    // Persist and restore JACK connections
    void saveConnections() const;
    void restoreConnections();

    // KeepAliveMonitor integration
    void setKeepAliveMonitor(KeepAliveMonitor* monitor);
    KeepAliveMonitor* getKeepAliveMonitor() const;
    void autoConnectInputPort();

    // Preference tracking / test helpers
    std::string clientName() const;
    bool autoConnectOutputsEnabled() const;
    int initCount() const;

    // Update connections file when client name changes
    static void updateConnectionsForClientRename(const std::string& oldClientName, const std::string& newClientName);

    // Get input samples from JACK input port
    std::vector<float> getInputSamples() const;

    // Process input samples through KeepAliveMonitor (called from JACK thread)
    void processKeepAliveInput();

    // For testing: inject samples directly
    void injectInputSamplesForTesting(const std::vector<float>& samples);

private:
    AudioEnginePrivate* m_priv = nullptr;
};
