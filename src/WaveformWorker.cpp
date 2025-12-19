#include "WaveformWorker.h"
#include <QThreadPool>
#include <QRunnable>
#include <QMetaObject>
#include <QDebug>
#include <QThread>
#include "AudioFile.h"
#include <cmath>
#include <QMutexLocker>

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
    // Use AudioFile to read all samples (simple for phase 1). This may be
    // optimized later to stream and avoid full-file buffering.
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

    // Simple per-pixel min/max buckets for the requested pixel width
    int targetPixels = qMax(1, static_cast<int>(std::ceil(pixelWidth * dpr)));
    size_t samplesPerBucket = (totalFrames + targetPixels - 1) / targetPixels; // ceil div
    if (samplesPerBucket == 0) samplesPerBucket = 1;

    out.min.resize(targetPixels);
    out.max.resize(targetPixels);

    for (int p = 0; p < targetPixels; ++p) {
        if (cancelToken && cancelToken->loadRelaxed() != 0) {
            // Return what we have so far (caller will treat cancellation)
            out.min.resize(0);
            out.max.resize(0);
            out.sampleRate = 0;
            return out;
        }
        float vmin = std::numeric_limits<float>::infinity();
        float vmax = -std::numeric_limits<float>::infinity();
        size_t start = static_cast<size_t>(p) * samplesPerBucket;
        size_t end = std::min(start + samplesPerBucket, totalFrames);
        for (size_t f = start; f < end; ++f) {
            // Downmix to mono by taking max absolute across channels
            float sampleVal = 0.0f;
            for (int c = 0; c < channels; ++c) {
                float s = samples[f * channels + c];
                sampleVal = std::max(sampleVal, std::abs(s));
            }
            vmin = std::min(vmin, -sampleVal);
            vmax = std::max(vmax, sampleVal);
        }
        if (start >= end) {
            vmin = 0.0f; vmax = 0.0f;
        }
        out.min[p] = vmin;
        out.max[p] = vmax;
    }

    return out;
}
