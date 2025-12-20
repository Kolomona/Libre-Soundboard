#include "WaveformWorker.h"
#include <QThreadPool>
#include <QRunnable>
#include <QMetaObject>
#include <QDebug>
#include <QThread>
#include "AudioFile.h"
#include <cmath>
#include <QMutexLocker>
#include <sndfile.h>

class WaveformRunnable : public QRunnable {
public:
    WaveformRunnable(const WaveformJob& j, WaveformWorker* w)
        : job(j), worker(w) {
        setAutoDelete(true);
    }

    void run() override {
        // Perform synchronous decode using the helper (supports cancellation)
        WaveformResult res = WaveformWorker::decodeFile(job.path, job.pixelWidth, job.dpr, job.cancelToken);
        if (job.cancelToken && job.cancelToken->loadRelaxed() != 0) {
            QMetaObject::invokeMethod(worker, "notifyError",
                                      Qt::QueuedConnection,
                                      Q_ARG(WaveformJob, job),
                                      Q_ARG(QString, QStringLiteral("cancelled")));
            return;
        }

        // If decode produced no sample-rate we consider it an error
        if (res.sampleRate == 0) {
            QMetaObject::invokeMethod(worker, "notifyError",
                                      Qt::QueuedConnection,
                                      Q_ARG(WaveformJob, job),
                                      Q_ARG(QString, QStringLiteral("decode-failed")));
            return;
        }

        QMetaObject::invokeMethod(worker, "notifyReady",
                                  Qt::QueuedConnection,
                                  Q_ARG(WaveformJob, job),
                                  Q_ARG(WaveformResult, res));
    }

private:
    WaveformJob job;
    WaveformWorker* worker;
};

WaveformWorker::WaveformWorker(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<WaveformJob>("WaveformJob");
    qRegisterMetaType<WaveformResult>("WaveformResult");
    qDebug() << "WaveformWorker initialized";
}

WaveformWorker::~WaveformWorker() {
    // Wait for pool to finish briefly
    QThreadPool::globalInstance()->waitForDone(500);
}

QUuid WaveformWorker::enqueueJob(const QString& path, int pixelWidth, qreal dpr) {
    WaveformJob job;
    job.id = QUuid::createUuid();
    job.path = path;
    job.pixelWidth = pixelWidth;
    job.dpr = dpr;
    job.cancelToken = QSharedPointer<QAtomicInteger<int>>::create(0);

    {
        QMutexLocker l(&m_tokenLock);
        m_tokens.insert(job.id, job.cancelToken);
    }

    auto* r = new WaveformRunnable(job, this);
    QThreadPool::globalInstance()->start(r);

    qDebug() << "Enqueued waveform job" << job.id << job.path << "px" << pixelWidth << "dpr" << dpr;
    return job.id;
}

void WaveformWorker::cancelJob(const QUuid& id) {
    QMutexLocker l(&m_tokenLock);
    if (m_tokens.contains(id)) {
        auto t = m_tokens.value(id);
        if (!t.isNull()) t->storeRelaxed(1);
        m_tokens.remove(id);
        qDebug() << "cancelJob: requested cancel for" << id;
    }
}

void WaveformWorker::notifyReady(const WaveformJob& job, const WaveformResult& result) {
    emit waveformReady(job, result);
    QMutexLocker l(&m_tokenLock);
    m_tokens.remove(job.id);
}

void WaveformWorker::notifyError(const WaveformJob& job, const QString& err) {
    emit waveformError(job, err);
    QMutexLocker l(&m_tokenLock);
    m_tokens.remove(job.id);
}

WaveformResult WaveformWorker::decodeFile(const QString& path, int pixelWidth, qreal dpr,
                                          QSharedPointer<QAtomicInteger<int>> cancelToken)
{
    WaveformResult out;
    if (path.isEmpty()) return out;

    // Prefer streaming via libsndfile when available to avoid buffering entire file
    QByteArray ba = path.toLocal8Bit();
    const char* cpath = ba.constData();
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE* snd = sf_open(cpath, SFM_READ, &sfinfo);
    if (snd) {
        int sampleRate = sfinfo.samplerate;
        int channels = sfinfo.channels;
        sf_count_t frames = sfinfo.frames;
        if (sampleRate <= 0 || channels <= 0 || frames <= 0) {
            sf_close(snd);
            return out;
        }

        out.sampleRate = sampleRate;
        out.channels = channels;
        out.duration = static_cast<double>(frames) / static_cast<double>(sampleRate);

        int targetPixels = qMax(1, static_cast<int>(std::ceil(pixelWidth * dpr)));
        sf_count_t samplesPerBucket = (frames + targetPixels - 1) / targetPixels;
        if (samplesPerBucket <= 0) samplesPerBucket = 1;

        out.min.reserve(targetPixels);
        out.max.reserve(targetPixels);

        const int CHUNK_FRAMES = 4096;
        std::vector<float> buf(static_cast<size_t>(CHUNK_FRAMES * channels));

        sf_count_t bucketFramesSeen = 0;
        float bucketMin = std::numeric_limits<float>::infinity();
        float bucketMax = -std::numeric_limits<float>::infinity();
        sf_count_t totalFramesRead = 0;

        while (totalFramesRead < frames) {
            if (cancelToken && cancelToken->loadRelaxed() != 0) {
                // Signal cancellation via empty result (sampleRate == 0)
                out.min.clear(); out.max.clear(); out.sampleRate = 0;
                sf_close(snd);
                return out;
            }

            int want = static_cast<int>(std::min<sf_count_t>(CHUNK_FRAMES, frames - totalFramesRead));
            sf_count_t got = sf_readf_float(snd, buf.data(), want);
            if (got <= 0) break;

            for (sf_count_t f = 0; f < got; ++f) {
                // compute downmixed per-frame amplitude (max abs across channels)
                float sampleVal = 0.0f;
                size_t baseIdx = static_cast<size_t>(f) * static_cast<size_t>(channels);
                for (int c = 0; c < channels; ++c) {
                    float s = buf[baseIdx + c];
                    sampleVal = std::max(sampleVal, std::abs(s));
                }
                bucketMin = std::min(bucketMin, -sampleVal);
                bucketMax = std::max(bucketMax, sampleVal);
                ++bucketFramesSeen;
                ++totalFramesRead;

                if (bucketFramesSeen >= samplesPerBucket) {
                    // finished bucket
                    if (bucketMin == std::numeric_limits<float>::infinity()) bucketMin = 0.0f;
                    if (bucketMax == -std::numeric_limits<float>::infinity()) bucketMax = 0.0f;
                    out.min.push_back(bucketMin);
                    out.max.push_back(bucketMax);
                    bucketFramesSeen = 0;
                    bucketMin = std::numeric_limits<float>::infinity();
                    bucketMax = -std::numeric_limits<float>::infinity();
                    // early exit if we've gathered enough pixels
                    if (static_cast<int>(out.min.size()) >= targetPixels) break;
                }
            }

            if (static_cast<int>(out.min.size()) >= targetPixels) break;
        }

        // If there is a trailing partial bucket and we still need pixels, push it
        if (static_cast<int>(out.min.size()) < targetPixels && bucketFramesSeen > 0) {
            if (bucketMin == std::numeric_limits<float>::infinity()) bucketMin = 0.0f;
            if (bucketMax == -std::numeric_limits<float>::infinity()) bucketMax = 0.0f;
            out.min.push_back(bucketMin);
            out.max.push_back(bucketMax);
        }

        // If we produced fewer pixels than target, pad with zeros
        while (static_cast<int>(out.min.size()) < targetPixels) {
            out.min.push_back(0.0f);
            out.max.push_back(0.0f);
        }

        sf_close(snd);
        return out;
    }

    // Fallback: use AudioFile (reads entire file) — keeps previous behavior for formats
    AudioFile af;
    if (!af.load(path)) {
        return out;
    }

    std::vector<float> samples;
    int sampleRate = 0;
    int channels = 0;
    if (!af.readAllSamples(samples, sampleRate, channels)) {
        return out;
    }

    if (sampleRate <= 0 || channels <= 0) return out;

    const size_t totalFrames = samples.size() / static_cast<size_t>(channels);
    out.sampleRate = sampleRate;
    out.channels = channels;
    out.duration = static_cast<double>(totalFrames) / static_cast<double>(sampleRate);

    int targetPixels = qMax(1, static_cast<int>(std::ceil(pixelWidth * dpr)));
    size_t samplesPerBucket = (totalFrames + targetPixels - 1) / targetPixels; // ceil div
    if (samplesPerBucket == 0) samplesPerBucket = 1;

    out.min.reserve(targetPixels);
    out.max.reserve(targetPixels);

    size_t framesSeen = 0;
    float vmin = std::numeric_limits<float>::infinity();
    float vmax = -std::numeric_limits<float>::infinity();

    for (size_t f = 0; f < totalFrames; ++f) {
        if (cancelToken && cancelToken->loadRelaxed() != 0) {
            out.min.clear(); out.max.clear(); out.sampleRate = 0; return out;
        }
        float sampleVal = 0.0f;
        for (int c = 0; c < channels; ++c) {
            float s = samples[f * channels + c];
            sampleVal = std::max(sampleVal, std::abs(s));
        }
        vmin = std::min(vmin, -sampleVal);
        vmax = std::max(vmax, sampleVal);
        ++framesSeen;
        if (framesSeen >= samplesPerBucket) {
            if (vmin == std::numeric_limits<float>::infinity()) vmin = 0.0f;
            if (vmax == -std::numeric_limits<float>::infinity()) vmax = 0.0f;
            out.min.push_back(vmin);
            out.max.push_back(vmax);
            framesSeen = 0;
            vmin = std::numeric_limits<float>::infinity();
            vmax = -std::numeric_limits<float>::infinity();
            if (static_cast<int>(out.min.size()) >= targetPixels) break;
        }
    }

    if (static_cast<int>(out.min.size()) < targetPixels && framesSeen > 0) {
        if (vmin == std::numeric_limits<float>::infinity()) vmin = 0.0f;
        if (vmax == -std::numeric_limits<float>::infinity()) vmax = 0.0f;
        out.min.push_back(vmin);
        out.max.push_back(vmax);
    }

    while (static_cast<int>(out.min.size()) < targetPixels) {
        out.min.push_back(0.0f);
        out.max.push_back(0.0f);
    }

    return out;
}
