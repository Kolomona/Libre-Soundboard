#pragma once

#include <vector>
#include <atomic>
#include <memory>
#include <string>
#include <mutex>

class AudioEnginePlay
{
public:
    AudioEnginePlay();
    ~AudioEnginePlay();

    // Add a new voice for playback (appends). If `id` is non-empty, it is used
    // to identify/restart the voice on subsequent requests.
    void addVoice(std::vector<float>&& buf, int sampleRate, int channels, const std::string& id = std::string(), float gain = 1.0f);

    // Restart any existing voice(s) matching id (set position to 0). Returns true if any restarted.
    bool restartVoicesById(const std::string& id);

    void clear();

    // Update gain for voices matching id
    void setGainById(const std::string& id, float gain);
    void stopVoicesById(const std::string& id);

    // Called by real-time thread to fill output (mixes active voices)
    void process(float** outputs, int nframes, int nOutChannels);

private:
    struct Voice {
        std::shared_ptr<std::vector<float>> buf;
        std::atomic<size_t> pos{0};
        int channels = 0;
        std::string id;
        std::atomic<float> gain{1.0f};
    };

    std::mutex m_lock; // protects m_voices when adding/removing
    std::vector<std::shared_ptr<Voice>> m_voices;

    // Snapshot used by real-time thread to avoid locking. We store as a
    // `shared_ptr` and use `std::atomic_store`/`std::atomic_load` helpers for
    // thread-safe publication without making the shared_ptr itself atomic.
    std::shared_ptr<std::vector<std::shared_ptr<Voice>>> m_voiceSnapshot;
};
