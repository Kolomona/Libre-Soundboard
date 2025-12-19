#include "PlayheadManager.h"
#include "AudioEngine.h"
#include "SoundContainer.h"
#include "AudioFile.h"

#include <QCoreApplication>
#include <cmath>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDebug>

static PlayheadManager* g_instance = nullptr;

PlayheadManager::PlayheadManager(QObject* parent)
    : QObject(parent)
{
    // default 30 Hz
    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, &PlayheadManager::onTick);
    m_timer.start();
    g_instance = this;
}

static void writeDebugLogPM(const QString &msg)
{
    // keep qDebug output for developer visibility, but do not write to disk
    qDebug().noquote() << "[PM]" << msg;
}

PlayheadManager::~PlayheadManager()
{
    g_instance = nullptr;
}

void PlayheadManager::init(AudioEngine* engine)
{
    m_engine = engine;
}

PlayheadManager* PlayheadManager::instance()
{
    if (!g_instance) {
        // create on the application main thread
        g_instance = new PlayheadManager(QCoreApplication::instance());
    }
    return g_instance;
}

void PlayheadManager::registerContainer(const QString& id, SoundContainer* sc, double durationSeconds, int sampleRate)
{
    if (id.isEmpty() || !sc) return;
    QList<Entry>& list = m_map[id];
    // avoid duplicates
    for (auto &e : list) {
        if (e.sc == sc) return;
    }
    Entry en;
    en.sc = sc;
    en.duration = durationSeconds;
    en.sampleRate = sampleRate;
    en.lastPos = -1.0f;
    // If duration or sampleRate missing, attempt to read file metadata via AudioFile
    if ((en.duration <= 0.0 || en.sampleRate <= 0) && !id.isEmpty()) {
        AudioFile af;
        if (af.load(id)) {
            std::vector<float> samples;
            int sr = 0, ch = 0;
            if (af.readAllSamples(samples, sr, ch)) {
                if (sr > 0 && ch > 0) {
                    en.sampleRate = sr;
                    double frames = 0.0;
                    if (!samples.empty()) frames = static_cast<double>(samples.size()) / static_cast<double>(ch);
                    if (frames > 0.0) en.duration = frames / static_cast<double>(sr);
                }
            }
        }
    }

    list.append(en);
}

void PlayheadManager::playbackStarted(const QString& id, SoundContainer* sc)
{
    if (id.isEmpty() || !sc) return;
    if (!m_map.contains(id)) return;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto &list = m_map[id];
    for (auto &e : list) {
        if (e.sc == sc) {
            e.simStartMs = now;
            e.lastPos = -1.0f; // force update
        }
    }
}

void PlayheadManager::playbackStopped(const QString& id, SoundContainer* sc)
{
    if (id.isEmpty() || !sc) return;
    if (!m_map.contains(id)) return;
    auto &list = m_map[id];
    for (auto &e : list) {
        if (e.sc == sc) {
            e.simStartMs = -1;
            e.lastPos = -1.0f;
            if (e.sc) e.sc->setPlayheadPosition(-1.0f);
        }
    }
}

void PlayheadManager::unregisterContainer(const QString& id, SoundContainer* sc)
{
    if (id.isEmpty() || !sc) return;
    if (!m_map.contains(id)) return;
    auto &list = m_map[id];
    for (int i = list.size()-1; i >= 0; --i) {
        if (list[i].sc == sc) list.removeAt(i);
    }
    if (list.isEmpty()) m_map.remove(id);
}

void PlayheadManager::onTick()
{
    if (!m_engine) return;
    // iterate files
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        const QString id = it.key();
        auto &list = it.value();
        if (list.isEmpty()) continue;
        AudioEngine::PlaybackInfo pinfo = m_engine->getPlaybackInfoForId(id.toStdString());
        writeDebugLogPM(QString("onTick id=%1 pinfo.found=%2 frames=%3 sampleRate=%4 listSize=%5").arg(id).arg(pinfo.found).arg((qulonglong)pinfo.frames).arg(pinfo.sampleRate).arg(list.size()));
        if (!pinfo.found) {
            // no active voice: ensure containers stop if they were playing
            for (auto &e : list) {
                if (e.lastPos != -1.0f) {
                    e.lastPos = -1.0f;
                    if (e.sc) e.sc->setPlayheadPosition(-1.0f);
                }
            }
            continue;
        }
        if (pinfo.sampleRate <= 0) continue;
        double elapsed = double(pinfo.frames) / double(pinfo.sampleRate);
        writeDebugLogPM(QString("onTick id=%1 elapsed=%2s").arg(id).arg(elapsed));
        for (auto &e : list) {
            if (!e.sc) continue;
            float pos = -1.0f;
            // If we don't have a duration from waveform, try to compute it from playback totalFrames
            if (e.duration > 0.0) {
                pos = static_cast<float>(elapsed / e.duration);
            } else if (pinfo.totalFrames > 0 && pinfo.sampleRate > 0) {
                double dur = double(pinfo.totalFrames) / double(pinfo.sampleRate);
                if (dur > 0.0) {
                    e.duration = dur; // cache it for future ticks
                    pos = static_cast<float>(elapsed / e.duration);
                }
            }
            if (pos < 0.0f) pos = -1.0f;
            if (pos > 1.0f) pos = 1.0f;
            // only update if changed by a small epsilon
            if (std::abs(pos - e.lastPos) > 0.001f) {
                writeDebugLogPM(QString("update container id=%1 pos=%2 last=%3").arg(id).arg(pos).arg(e.lastPos));
                e.lastPos = pos;
                e.sc->setPlayheadPosition(pos);
            }
        }
    }

        // If playback not found via AudioEngine, support simulated playback based on start timestamps
        for (auto it2 = m_map.begin(); it2 != m_map.end(); ++it2) {
            const QString id2 = it2.key();
            auto &list2 = it2.value();
            for (auto &e : list2) {
                if (!e.sc) continue;
                if (e.simStartMs < 0) continue;
                if (e.duration <= 0.0) continue;
                qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                double elapsedSec = double(nowMs - e.simStartMs) / 1000.0;
                float pos = static_cast<float>(elapsedSec / e.duration);
                if (pos < 0.0f) pos = -1.0f;
                if (pos > 1.0f) {
                    // end simulation
                    e.simStartMs = -1;
                    pos = -1.0f;
                }
                if (std::abs(pos - e.lastPos) > 0.001f) {
                    writeDebugLogPM(QString("sim update id=%1 pos=%2 last=%3").arg(id2).arg(pos).arg(e.lastPos));
                    e.lastPos = pos;
                    e.sc->setPlayheadPosition(pos);
                }
            }
        }
}
