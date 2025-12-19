#include "WaveformWorker.h"
#include <QThreadPool>
#include <QRunnable>
#include <QMetaObject>
#include <QDebug>
#include <QThread>

class WaveformRunnable : public QRunnable {
public:
    WaveformRunnable(const WaveformJob& j, WaveformWorker* w)
        : job(j), worker(w) {
        setAutoDelete(true);
    }

    void run() override {
        // Simple simulated work: check cancel token periodically
        for (int i = 0; i < 10; ++i) {
            if (!job.cancelToken.isNull() && job.cancelToken->loadRelaxed() != 0) {
                // Cancelled
                QMetaObject::invokeMethod(worker, "notifyError",
                                          Qt::QueuedConnection,
                                          Q_ARG(WaveformJob, job),
                                          Q_ARG(QString, QStringLiteral("cancelled")));
                return;
            }
            QThread::msleep(20);
        }

        // Prepare a trivial WaveformResult (empty arrays) as placeholder
        WaveformResult res;
        res.duration = 0.0;
        res.sampleRate = 0;
        res.channels = 0;

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

    auto* r = new WaveformRunnable(job, this);
    QThreadPool::globalInstance()->start(r);

    qDebug() << "Enqueued waveform job" << job.id << job.path << "px" << pixelWidth << "dpr" << dpr;
    return job.id;
}

void WaveformWorker::cancelJob(const QUuid& id) {
    Q_UNUSED(id)
    // For this simple phase-1 implementation we don't track tokens by id.
    // Cancellation will be supported in later phases where we keep a map of tokens.
    qDebug() << "cancelJob called (no-op in phase 1)" << id;
}

void WaveformWorker::notifyReady(const WaveformJob& job, const WaveformResult& result) {
    emit waveformReady(job, result);
}

void WaveformWorker::notifyError(const WaveformJob& job, const QString& err) {
    emit waveformError(job, err);
}
