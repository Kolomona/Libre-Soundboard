#include "AudioEngine.h"
#include "KeepAliveMonitor.h"

#include <jack/jack.h>
#include <samplerate.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <mutex>

#include "AudioEnginePlay.h"

struct AudioEnginePrivate {
    jack_client_t* client = nullptr;
    jack_port_t* out_ports[2] = {nullptr, nullptr};
    jack_port_t* in_port = nullptr;
    unsigned int jack_sample_rate = 48000;
    AudioEnginePlay player;
    KeepAliveMonitor* keepAliveMonitor = nullptr;
    
    // For testing: inject input samples
    std::vector<float> testInputSamples;
    std::mutex testInputLock;
};

static int jack_process(jack_nframes_t nframes, void* arg)
{
    AudioEnginePrivate* d = reinterpret_cast<AudioEnginePrivate*>(arg);
    if (!d) return 0;
    
    // Handle output
    jack_port_t* port_l = d->out_ports[0];
    jack_port_t* port_r = d->out_ports[1];
    float* out_l = (float*)jack_port_get_buffer(port_l, nframes);
    float* out_r = (float*)jack_port_get_buffer(port_r, nframes);

    float* outputs[2] = { out_l, out_r };
    d->player.process(outputs, nframes, 2);
    
    // Handle input - feed to KeepAliveMonitor
    if (d->in_port && d->keepAliveMonitor) {
        float* in_buf = (float*)jack_port_get_buffer(d->in_port, nframes);
        if (in_buf) {
            std::vector<float> input_samples(in_buf, in_buf + nframes);
            // Process 1 frame (mono input from JACK)
            d->keepAliveMonitor->processInputSamples(input_samples, nframes, 1);
        }
    }
    
    return 0;
}

AudioEngine::AudioEngine()
    : m_priv(new AudioEnginePrivate())
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
    delete m_priv;
}

bool AudioEngine::init()
{
    const char* client_name = "libre_soundboard_client";
    jack_status_t status;
    m_priv->client = jack_client_open(client_name, JackNullOption, &status);
    if (!m_priv->client) {
        std::cerr << "Failed to open JACK client\n";
        return false;
    }

    m_priv->jack_sample_rate = jack_get_sample_rate(m_priv->client);
    m_priv->out_ports[0] = jack_port_register(m_priv->client, "out_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    m_priv->out_ports[1] = jack_port_register(m_priv->client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    m_priv->in_port = jack_port_register(m_priv->client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    jack_set_process_callback(m_priv->client, jack_process, m_priv);

    if (jack_activate(m_priv->client)) {
        std::cerr << "Failed to activate JACK client" << std::endl;
        return false;
    }

    // Attempt to restore previous connections after activation
    restoreConnections();

    return true;
}

void AudioEngine::shutdown()
{
    if (m_priv && m_priv->client) {
        // Save current connections before closing
        saveConnections();
        jack_client_close(m_priv->client);
        m_priv->client = nullptr;
    }
}

static std::string configPath()
{
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    std::string dir = std::string(home) + "/.config/libresoundboard";
    // ensure directory exists
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        mkdir(dir.c_str(), 0700);
    }
    return dir + "/jack_connections.cfg";
}

void AudioEngine::saveConnections() const
{
    if (!m_priv || !m_priv->client) return;
    std::string path = configPath();
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs) return;

    // Save output ports (left and right)
    for (int i = 0; i < 2; ++i) {
        jack_port_t* p = m_priv->out_ports[i];
        if (!p) continue;
        const char* pname = jack_port_name(p);
        if (!pname) continue;
        const char** conns = jack_port_get_connections(p);
        ofs << pname << "|";
        if (conns) {
            for (int j = 0; conns[j]; ++j) {
                ofs << conns[j];
                if (conns[j+1]) ofs << ",";
            }
            jack_free(conns);
        }
        ofs << "\n";
    }

    // Save input port (with reversed connection direction)
    // Input port connections are SOURCES feeding INTO the input port,
    // so we need to save them in reverse order: source|input_port
    // so that jack_connect(source, input_port) works on restore
    if (m_priv->in_port) {
        const char* pname = jack_port_name(m_priv->in_port);
        if (pname) {
            const char** conns = jack_port_get_connections(m_priv->in_port);
            if (conns) {
                for (int j = 0; conns[j]; ++j) {
                    // For input ports, reverse the direction: source|input_port
                    ofs << conns[j] << "|" << pname << "\n";
                }
                jack_free(conns);
            } else {
                // No connections, but still record the port exists (empty target)
                ofs << pname << "|\n";
            }
        }
    }
    ofs.close();
}

void AudioEngine::restoreConnections()
{
    if (!m_priv || !m_priv->client) return;
    std::string path = configPath();
    std::ifstream ifs(path);
    if (!ifs) return;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto pos = line.find('|');
        if (pos == std::string::npos) continue;
        std::string src = line.substr(0, pos);
        std::string rest = line.substr(pos + 1);
        if (rest.empty()) continue;
        std::istringstream ss(rest);
        std::string target;
        while (std::getline(ss, target, ',')) {
            if (target.empty()) continue;
            // Try to connect -- ignore errors
            jack_connect(m_priv->client, src.c_str(), target.c_str());
        }
    }
}

void AudioEngine::autoConnectInputPort()
{
    if (!m_priv || !m_priv->client || !m_priv->in_port) return;
    const char* inName = jack_port_name(m_priv->in_port);
    if (!inName) return;

    // Attempt to connect common system capture ports; ignore errors if JACK not present
    jack_connect(m_priv->client, "system:capture_1", inName);
    jack_connect(m_priv->client, "system:capture_2", inName);
}

bool AudioEngine::playBuffer(const std::vector<float>& samples, int sampleRate, int channels, const std::string& id, float gain)
{
    if (!m_priv || !m_priv->client)
        return false;

    // If sample rate differs, resample the entire buffer to JACK rate using libsamplerate
    if (sampleRate != static_cast<int>(m_priv->jack_sample_rate)) {
        SRC_DATA src_data;
        memset(&src_data, 0, sizeof(src_data));
        src_data.data_in = samples.data();
        src_data.input_frames = static_cast<long>(samples.size() / channels);
        double ratio = double(m_priv->jack_sample_rate) / double(sampleRate);
        src_data.output_frames = static_cast<long>(src_data.input_frames * ratio) + 1;
        std::vector<float> temp_out(static_cast<size_t>(src_data.output_frames) * channels);
        src_data.data_out = temp_out.data();
        src_data.src_ratio = ratio;
        src_data.end_of_input = 1;
        // Use a better-quality converter than the fastest low-quality option
        int error = src_simple(&src_data, SRC_SINC_MEDIUM_QUALITY, channels);
        if (error != 0) {
            std::cerr << "libsamplerate error: " << src_strerror(error) << std::endl;
            return false;
        }
        // Resize to the actual number of frames produced
        long produced = src_data.output_frames_gen;
        if (produced > 0) {
            temp_out.resize(static_cast<size_t>(produced) * channels);
        } else {
            temp_out.clear();
        }
        // Queue resampled buffer for playback
        // Diagnostic log
        std::cerr << "AudioEngine: resampled " << src_data.input_frames << " frames @ " << sampleRate
                  << " -> " << produced << " frames @ " << m_priv->jack_sample_rate << "\n";
        if (!id.empty()) {
            // restart existing voices with same id when requested
            // add new voice if no restart
            if (!m_priv->player.restartVoicesById(id)) {
                m_priv->player.addVoice(std::move(temp_out), static_cast<int>(m_priv->jack_sample_rate), channels, id, gain);
            }
        } else {
            m_priv->player.addVoice(std::move(temp_out), static_cast<int>(m_priv->jack_sample_rate), channels, std::string(), gain);
        }
    } else {
        std::cerr << "AudioEngine: no resample needed; frames=" << (samples.size() / channels) << " @ " << sampleRate << "\n";
        if (!id.empty()) {
            if (!m_priv->player.restartVoicesById(id)) {
                m_priv->player.addVoice(std::vector<float>(samples.begin(), samples.end()), sampleRate, channels, id, gain);
            }
        } else {
            m_priv->player.addVoice(std::vector<float>(samples.begin(), samples.end()), sampleRate, channels, std::string(), gain);
        }
    }
    return true;
}

void AudioEngine::stopAll()
{
    if (!m_priv) return;
    m_priv->player.clear();
}

void AudioEngine::setVoiceGainById(const std::string& id, float gain)
{
    if (!m_priv) return;
    m_priv->player.setGainById(id, gain);
}

void AudioEngine::stopVoicesById(const std::string& id)
{
    if (!m_priv) return;
    m_priv->player.stopVoicesById(id);
}

AudioEngine::PlaybackInfo AudioEngine::getPlaybackInfoForId(const std::string& id) const
{
    AudioEngine::PlaybackInfo out;
    if (!m_priv) return out;
    auto pinfo = m_priv->player.getPlaybackInfoById(id);
    out.found = pinfo.found;
    out.frames = pinfo.frames;
    out.sampleRate = pinfo.sampleRate;
    out.totalFrames = pinfo.totalFrames;
    return out;
}

void AudioEngine::setKeepAliveMonitor(KeepAliveMonitor* monitor)
{
    if (m_priv) {
        m_priv->keepAliveMonitor = monitor;
    }
}

KeepAliveMonitor* AudioEngine::getKeepAliveMonitor() const
{
    if (!m_priv) return nullptr;
    return m_priv->keepAliveMonitor;
}

std::vector<float> AudioEngine::getInputSamples() const
{
    if (!m_priv || !m_priv->in_port || !m_priv->client) {
        return std::vector<float>();
    }
    
    // Can only be safely called from the JACK process callback
    // For now, return empty (real implementation would use atomic buffer exchange)
    return std::vector<float>();
}

void AudioEngine::processKeepAliveInput()
{
    // This is called from outside the JACK thread to process test inputs
    if (!m_priv || !m_priv->keepAliveMonitor) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_priv->testInputLock);
    if (!m_priv->testInputSamples.empty()) {
        // Process 1 frame with 1 channel (mono input for testing)
        m_priv->keepAliveMonitor->processInputSamples(
            m_priv->testInputSamples,
            m_priv->testInputSamples.size(),
            1
        );
        m_priv->testInputSamples.clear();
    }
}

void AudioEngine::injectInputSamplesForTesting(const std::vector<float>& samples)
{
    if (!m_priv) return;
    std::lock_guard<std::mutex> lock(m_priv->testInputLock);
    m_priv->testInputSamples = samples;
}
