#include "AudioEnginePlay.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <ctime>

AudioEnginePlay::AudioEnginePlay()
{
    std::atomic_store(&m_voiceSnapshot, std::make_shared<std::vector<std::shared_ptr<Voice>>>());
}

AudioEnginePlay::~AudioEnginePlay() = default;

void AudioEnginePlay::addVoice(std::vector<float>&& buf, int sampleRate, int channels, const std::string& id, float gain)
{
    auto v = std::make_shared<Voice>();
    v->buf = std::make_shared<std::vector<float>>(std::move(buf));
    v->channels = channels;
    v->sampleRate = sampleRate;
    if (channels > 0 && v->buf) v->totalFrames = v->buf->size() / static_cast<size_t>(channels);
    v->pos.store(0);
    v->id = id;
    v->gain.store(gain);

    {
        std::lock_guard<std::mutex> lk(m_lock);
        m_voices.push_back(v);
        // publish snapshot
        auto snap = std::make_shared<std::vector<std::shared_ptr<Voice>>>(m_voices);
        std::atomic_store(&m_voiceSnapshot, snap);
    }
}

AudioEnginePlay::PlaybackInfo AudioEnginePlay::getPlaybackInfoById(const std::string& id) const
{
    PlaybackInfo out;
    auto snap = std::atomic_load(&m_voiceSnapshot);
    if (!snap) return out;
    for (auto& v : *snap) {
        if (!v) continue;
        if (!id.empty() && v->id == id) {
            size_t sampleIndex = v->pos.load();
            int ch = v->channels > 0 ? v->channels : 1;
            uint64_t frames = sampleIndex / static_cast<size_t>(ch);
            out.found = true;
            out.frames = frames;
            out.sampleRate = v->sampleRate;
            out.totalFrames = v->totalFrames;
            // previously wrote debug info to /tmp; removed per request
            return out;
        }
    }
    return out;
}

bool AudioEnginePlay::restartVoicesById(const std::string& id)
{
    bool restarted = false;
    std::lock_guard<std::mutex> lk(m_lock);
    for (auto& v : m_voices) {
        if (!id.empty() && v->id == id) {
            v->pos.store(0);
            restarted = true;
        }
    }
    if (restarted) {
        auto snap = std::make_shared<std::vector<std::shared_ptr<Voice>>>(m_voices);
        std::atomic_store(&m_voiceSnapshot, snap);
    }
    return restarted;
}

void AudioEnginePlay::setGainById(const std::string& id, float gain)
{
    std::lock_guard<std::mutex> lk(m_lock);
    bool changed = false;
    for (auto& v : m_voices) {
        if (v && !id.empty() && v->id == id) {
            v->gain.store(gain);
            changed = true;
        }
    }
    if (changed) {
        // publish snapshot so reader sees updated vector (gain is atomic anyway)
        auto snap = std::make_shared<std::vector<std::shared_ptr<Voice>>>(m_voices);
        std::atomic_store(&m_voiceSnapshot, snap);
    }
}

void AudioEnginePlay::clear()
{
    std::lock_guard<std::mutex> lk(m_lock);
    m_voices.clear();
    std::atomic_store(&m_voiceSnapshot, std::make_shared<std::vector<std::shared_ptr<Voice>>>());
}

void AudioEnginePlay::stopVoicesById(const std::string& id)
{
    std::lock_guard<std::mutex> lk(m_lock);
    bool removed = false;
    auto it = std::remove_if(m_voices.begin(), m_voices.end(), [&](const std::shared_ptr<Voice>& v){
        return v && !id.empty() && v->id == id;
    });
    if (it != m_voices.end()) {
        m_voices.erase(it, m_voices.end());
        removed = true;
    }
    if (removed) {
        auto snap = std::make_shared<std::vector<std::shared_ptr<Voice>>>(m_voices);
        std::atomic_store(&m_voiceSnapshot, snap);
    }
}

void AudioEnginePlay::process(float** outputs, int nframes, int nOutChannels)
{
    // Zero outputs
    for (int ch = 0; ch < nOutChannels; ++ch) {
        std::memset(outputs[ch], 0, sizeof(float) * nframes);
    }

    auto snap = std::atomic_load(&m_voiceSnapshot);
    if (!snap || snap->empty())
        return;

    // Mix all voices into outputs
    for (auto& v : *snap) {
        if (!v || !v->buf) continue;
        const std::vector<float>& b = *v->buf;
        size_t bsize = b.size();
        size_t pos = v->pos.load();
        int channels = v->channels;

        for (int i = 0; i < nframes; ++i) {
            if (pos >= bsize) break;

            float left = 0.0f;
            float right = 0.0f;
            if (channels == 1) {
                left = right = b[pos++];
            } else {
                left = b[pos++];
                if (pos < bsize) right = b[pos++]; else right = 0.0f;
                for (int c = 2; c < channels; ++c) {
                    if (pos < bsize) pos++; else break;
                }
            }

            // Apply per-voice gain (load atomically)
            float g = v->gain.load();
            left *= g;
            right *= g;
            if (nOutChannels > 0) outputs[0][i] += left;
            if (nOutChannels > 1) outputs[1][i] += right;
        }

        v->pos.store(pos);
    }

    // Simple clipping to [-1,1]
    for (int ch = 0; ch < nOutChannels; ++ch) {
        for (int i = 0; i < nframes; ++i) {
            float v = outputs[ch][i];
            if (v > 1.0f) outputs[ch][i] = 1.0f;
            else if (v < -1.0f) outputs[ch][i] = -1.0f;
        }
    }
}
