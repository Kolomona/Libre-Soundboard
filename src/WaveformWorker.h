#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QSharedPointer>
#include <QAtomicInteger>
#include <QUuid>

struct WaveformJob {
    QUuid id;
    QString path;
    int pixelWidth = 0;
    qreal dpr = 1.0;
    // Cancellation token: 0 = running, 1 = cancel requested
    QSharedPointer<QAtomicInteger<int>> cancelToken;
};

struct WaveformResult {
    QVector<float> min;
    QVector<float> max;
    double duration = 0.0;
    int sampleRate = 0;
    int channels = 0;
};

Q_DECLARE_METATYPE(WaveformJob)
Q_DECLARE_METATYPE(WaveformResult)

class WaveformWorker : public QObject {
    Q_OBJECT
public:
    explicit WaveformWorker(QObject* parent = nullptr);
    ~WaveformWorker() override;

    // Enqueue a job; returns the job id.
    QUuid enqueueJob(const QString& path, int pixelWidth, qreal dpr = 1.0);

    // Request cancellation of a job by id.
    void cancelJob(const QUuid& id);

signals:
    // Emitted on the main (GUI) thread when job completes successfully
    void waveformReady(const WaveformJob& job, const WaveformResult& result);
    // Emitted on the main (GUI) thread when job fails or is cancelled
    void waveformError(const WaveformJob& job, const QString& error);

private:
    Q_DISABLE_COPY(WaveformWorker)

    // Internal invokables called from worker threads via queued connection
    Q_INVOKABLE void notifyReady(const WaveformJob& job, const WaveformResult& result);
    Q_INVOKABLE void notifyError(const WaveformJob& job, const QString& err);
};
