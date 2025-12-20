#include "SoundContainer.h"

#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QDrag>
#include <QApplication>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QDateTime>
#include <QFile>
#include <unistd.h>
#include <QResizeEvent>
#include <QPaintEvent>
#include "WaveformWorker.h"
#include "PlayheadManager.h"
#include "WaveformCache.h"
#include "WaveformRenderer.h"
#include <sndfile.h>
#include <cmath>

QSize SoundContainer::availableDisplaySize() const
{
    // Start with the container's contents rect width (available logical pixels)
    int availW = contentsRect().width();
    int availH = 0;
    // If we have a waveform widget, use its logical height; otherwise infer a reasonable height
    if (m_waveform && m_waveform->height() > 0) {
        availH = m_waveform->height();
    } else {
        availH = contentsRect().height() / 3;
    }

    // Account for the layout's contents margins which may reduce available width
    QLayout* l = layout();
    if (l) {
        QMargins m = l->contentsMargins();
        availW -= (m.left() + m.right());
    }

    if (availW < 1) availW = 1;
    if (availH < 1) availH = 1;
    return QSize(availW, availH);
}

namespace {
static QPixmap makeDragCursorPixmap(const QString& text)
{
    const int w = 24;
    const int h = 24;
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::white);
    p.setPen(pen);
    p.setBrush(QColor(50, 50, 50, 200));
    p.drawEllipse(0, 0, w-1, h-1);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(10);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(pm.rect(), Qt::AlignCenter, text);
    return pm;
}

static void writeLocalDebug(const QString& msg)
{
    // do not write to disk; keep qDebug output only
    qDebug().noquote() << msg;
}
}

SoundContainer::SoundContainer(QWidget* parent)
    : QFrame(parent)
{
    auto layout = new QVBoxLayout(this);
    // filename label above the waveform (left-aligned)
    m_filenameLabel = new QLabel(tr("Drop audio file here"), this);
    m_filenameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_filenameLabel->setIndent(6);
    layout->addWidget(m_filenameLabel);

    m_waveform = new QLabel(this);
    // Reduce the minimum size to allow the overall window to be shrunk
    // (previously 160x80 which prevented fitting on some displays).
    m_waveform->setMinimumSize(120, 60);
    // Align waveform to left, not centered, and remove internal label margins so
    // the image sits flush against the container left edge.
    m_waveform->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_waveform->setContentsMargins(0,0,0,0);
    m_waveform->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_playBtn = new QPushButton(tr("Play"), this);
    layout->addWidget(m_waveform);

    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setRange(0, 100);
    m_volume->setValue(80);
    // Make the slider expand horizontally to fill the container width
    m_volume->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_volume);

    layout->addWidget(m_playBtn);
    layout->setAlignment(m_playBtn, Qt::AlignLeft);
    setLayout(layout);

    setAcceptDrops(true);

    // Use QFrame styling for a single outer border around the whole container
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setMidLineWidth(0);
    setContentsMargins(6,6,6,6);

    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (!m_filePath.isEmpty()) {
            // locally mark as playing (UI playhead overlay) and notify app
            m_playing = true;
            m_playheadPos = 0.0f;
            update();
            emit playRequested(m_filePath, this);
        }
    });

    // Right-click on the play button should stop playback for this container
    m_playBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playBtn, &QWidget::customContextMenuRequested, this, [this](const QPoint&) {
        if (!m_filePath.isEmpty()) {
            emit stopRequested(m_filePath, this);
        }
    });

    connect(m_volume, &QSlider::valueChanged, this, [this](int v){
        float vol = static_cast<float>(v) / 100.0f;
        emit volumeChanged(vol);
    });

    // (Border is handled by QFrame settings above)

    // Context menu for the container itself (right-click on the widget)
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pt){
        Q_UNUSED(pt);
        // old handler left in place but we now use contextMenuEvent override
    });

    // Note: contextMenuEvent is overridden below to prevent the menu
    // from appearing while a right-button drag/press is in progress.

    // Ensure the initial UI matches the default appearance used for cleared slots
    resetToDefaultAppearance();
}

// Worker signal handlers
void SoundContainer::onWaveformReady(const WaveformJob& job, const WaveformResult& result)
{
    qDebug() << "onWaveformReady called job.id=" << job.id << "pending=" << m_pendingJobId << "path=" << job.path << "res.samples=" << result.min.size();
    if (job.id != m_pendingJobId) return;

    // Use the waveform widget's logical size as the authoritative width/height
    // for rendering and scaling. This keeps behavior stable and matches the
    // QLabel that will display the QPixmap.
            QSize labelSize = availableDisplaySize();
            if (labelSize.width() <= 0 || labelSize.height() <= 0) return;

    // Render canonical image using the job's pixelWidth and dpr so cache files match the
    // worker's intended resolution (avoids writing images sized to the widget).
    float fdpr = static_cast<float>(job.dpr <= 0.0 ? 1.0 : job.dpr);
    int pixelWidth = job.pixelWidth > 0 ? job.pixelWidth : labelSize.width();
    int heightCss = labelSize.height();

    WaveformLevel level;
    level.min = result.min;
    level.max = result.max;

    QImage img = Waveform::renderLevelToImage(level, pixelWidth, fdpr, heightCss);

    m_wavePixmap = QPixmap::fromImage(img);
    m_hasWavePixmap = true;
        {
            // Force the pixmap to the container's logical height (ignore aspect ratio)
            QSize labelSize = availableDisplaySize();
            int containerW = labelSize.width();
            int containerH = labelSize.height();
            qreal widgetDpr = devicePixelRatioF();
            // Use floor for width so the logical pixmap width never exceeds the container
            int targetWpx = static_cast<int>(std::floor(containerW * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
            // Use ceil for height to ensure we preserve the visual height
            int targetHpx = static_cast<int>(std::ceil(containerH * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
            qDebug() << "scale->widget labelSize=" << labelSize << "widgetDpr=" << widgetDpr << "srcPixmap=" << m_wavePixmap.size() << "srcDpr=" << m_wavePixmap.devicePixelRatio() << "targetPx=" << QSize(targetWpx, targetHpx);
            QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(widgetDpr);
            m_waveform->setText(QString());
            m_waveform->setPixmap(scaled);
        }
    update();

    // Write rendered canonical image to cache for future loads
    if (!job.path.isEmpty()) {
        QFileInfo fi(job.path);
        qint64 size = fi.size();
        qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        int channels = result.channels;
        int samplerate = result.sampleRate;
        QString key = WaveformCache::makeKey(job.path, size, mtime, channels, samplerate, fdpr, pixelWidth);
        QJsonObject meta;
        meta["path"] = job.path;
        meta["size"] = static_cast<double>(size);
        meta["mtime"] = static_cast<double>(mtime);
        meta["channels"] = channels;
        meta["samplerate"] = samplerate;
        meta["dpr"] = fdpr;
        meta["pixelWidth"] = pixelWidth;
        meta["width"] = img.width();
        meta["height"] = img.height();
        meta["duration"] = result.duration;
        QString cacheDir = WaveformCache::cacheDirPath();
        qDebug() << "WaveformCache::write key=" << key << "dir=" << cacheDir;
        WaveformCache::write(key, img, meta);

        // Register this container with the playhead manager so it can receive updates
        PlayheadManager::instance()->registerContainer(job.path, this, result.duration, result.sampleRate);
    }
}

void SoundContainer::onWaveformError(const WaveformJob& job, const QString& err)
{
    qDebug() << "onWaveformError job.id=" << job.id << "err=" << err;
    Q_UNUSED(job);
    Q_UNUSED(err);
    // For now, just clear waveform display and if no file is set restore default appearance
    m_hasWavePixmap = false;
    m_waveform->setPixmap(QPixmap());
    if (m_filePath.isEmpty()) resetToDefaultAppearance();
    else {
        m_filenameLabel->setText(QFileInfo(m_filePath).fileName());
    }
    update();
}

void SoundContainer::applyWaveformResultForTest(const WaveformResult& result)
{
    // Construct a job that matches current pending id/path and call the same
    // rendering path as a real job completion. This mirrors onWaveformReady
    // but is safe to call from unit tests (no worker thread involvement).
    WaveformJob job;
    job.id = m_pendingJobId;
    job.path = m_filePath;
    job.dpr = devicePixelRatioF();
    // Call the same renderer used in onWaveformReady
    onWaveformReady(job, result);
}


void SoundContainer::mousePressEvent(QMouseEvent* event)
{
    if ((event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
        // start drag only if press is on the waveform label and file exists
        if (!m_filePath.isEmpty() && m_waveform && m_waveform->geometry().contains(event->pos())) {
            m_dragStart = event->pos();
            m_dragButton = event->button();
            event->accept();
            return;
        }
    }
    QFrame::mousePressEvent(event);
}

void SoundContainer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragButton == Qt::NoButton) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    // check that the original button is still down
    if (!(event->buttons() & m_dragButton)) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if (m_dragStart.isNull()) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }

    // Start drag with this container pointer encoded
    QDrag* drag = new QDrag(this);
    QMimeData* md = new QMimeData();
    QByteArray ba = QByteArray::number(reinterpret_cast<uintptr_t>(this));
    // prepare small cursor pixmaps for copy/move feedback
    static QPixmap copyCursor = makeDragCursorPixmap(QStringLiteral("C"));
    static QPixmap moveCursor = makeDragCursorPixmap(QStringLiteral("M"));

    if (m_dragButton == Qt::RightButton) {
        md->setData("application/x-soundcontainer-copy", ba);
        drag->setMimeData(md);
        // use a small pixmap of the waveform label for visual feedback
        if (m_waveform) {
            QPixmap pm = m_waveform->grab();
            if (!pm.isNull()) {
                QPixmap scaled = pm.scaled(160, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                drag->setPixmap(scaled);
                drag->setHotSpot(QPoint(scaled.width()/2, scaled.height()/2));
            }
        }
        // set drag cursors for visual feedback
        drag->setDragCursor(copyCursor, Qt::CopyAction);
        drag->setDragCursor(moveCursor, Qt::MoveAction);
        m_dragging = true;
        drag->exec(Qt::CopyAction);
        m_dragging = false;
    } else {
        md->setData("application/x-soundcontainer", ba);
        drag->setMimeData(md);
        if (m_waveform) {
            QPixmap pm = m_waveform->grab();
            if (!pm.isNull()) {
                QPixmap scaled = pm.scaled(160, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                drag->setPixmap(scaled);
                drag->setHotSpot(QPoint(scaled.width()/2, scaled.height()/2));
            }
        }
        drag->setDragCursor(copyCursor, Qt::CopyAction);
        drag->setDragCursor(moveCursor, Qt::MoveAction);
        m_dragging = true;
        drag->exec(Qt::MoveAction);
        m_dragging = false;
    }

    m_dragStart = QPoint();
    m_dragButton = Qt::NoButton;
}

void SoundContainer::mouseReleaseEvent(QMouseEvent* event)
{
    // If this is the right button, decide whether to show the context menu
    if (event->button() == Qt::RightButton) {
        // If a drag was in progress, we don't show the menu
        if (m_dragging) {
            m_dragging = false;
            m_dragStart = QPoint();
            m_dragButton = Qt::NoButton;
            QFrame::mouseReleaseEvent(event);
            return;
        }

        // If we just received a drop, suppress menu and clear the flag
        if (m_justDropped) {
            m_justDropped = false;
            m_dragStart = QPoint();
            m_dragButton = Qt::NoButton;
            QFrame::mouseReleaseEvent(event);
            return;
        }

        // If this was a simple right-click (no movement beyond threshold), show menu
        if (!m_dragStart.isNull() && (event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
            // Show the context menu at the release position
            QContextMenuEvent ctx(QContextMenuEvent::Mouse, event->pos(), event->globalPosition().toPoint());
            contextMenuEvent(&ctx);
            m_dragStart = QPoint();
            m_dragButton = Qt::NoButton;
            QFrame::mouseReleaseEvent(event);
            return;
        }
    }

    // Clear drag tracking state on release for other situations
    m_dragStart = QPoint();
    m_dragButton = Qt::NoButton;
    QFrame::mouseReleaseEvent(event);
}

void SoundContainer::contextMenuEvent(QContextMenuEvent* event)
{
    // don't show context menu while a drag operation is in progress or just after a drop
    if (m_dragging || m_justDropped) {
        event->ignore();
        return;
    }

    QMenu menu(this);
    if (!m_filePath.isEmpty()) {
        menu.addAction(tr("Play Sound"), this, [this]() {
            emit playRequested(m_filePath, this);
        });
        // allow clearing this container back to the default state
        menu.addAction(tr("Clear"), this, [this]() {
            // request clear via signal so MainWindow can record undo/redo
            emit clearRequested(this);
        });
    } else {
        QAction* a = menu.addAction(tr("Play Sound"));
        a->setEnabled(false);
    }
    menu.exec(event->globalPos());
}

void SoundContainer::setFile(const QString& path)
{
    if (path == m_filePath)
        return;

    if (path.isEmpty()) {
        // Clear to default state
        // cancel outstanding job
        if (m_waveWorker && !m_pendingJobId.isNull()) {
            m_waveWorker->cancelJob(m_pendingJobId);
            m_pendingJobId = QUuid();
        }
        // unregister from playhead manager
        if (!m_filePath.isEmpty()) {
            PlayheadManager::instance()->unregisterContainer(m_filePath, this);
        }
        m_filePath.clear();
        resetToDefaultAppearance();
        setVolume(0.8f);
        emit fileChanged(QString());
        return;
    }

    m_filePath = path;
    QFileInfo fi(path);
    m_filenameLabel->setText(fi.fileName());
    // show filename on hover (not the full path)
    m_filenameLabel->setToolTip(fi.fileName());
    // Reset volume to default when a new file is assigned via drag-and-drop
    // (restoreLayout will call setVolume afterward if restoring saved state)
    setVolume(0.8f);
    emit fileChanged(path);

    // ensure we have a waveform worker
    if (!m_waveWorker) {
        m_waveWorker = new WaveformWorker(this);
        connect(m_waveWorker, &WaveformWorker::waveformReady, this, &SoundContainer::onWaveformReady, Qt::QueuedConnection);
        connect(m_waveWorker, &WaveformWorker::waveformError, this, &SoundContainer::onWaveformError, Qt::QueuedConnection);
    }

    // enqueue a render for current label width and DPR
    int px = qMax(1, m_waveform->width());
    qreal dpr = devicePixelRatioF();

    // Try to load cached rendered image first. We need file size, mtime and audio metadata (channels/samplerate)
    qint64 size = fi.size();
    qint64 mtime = fi.lastModified().toSecsSinceEpoch();
    int channels = 0;
    int samplerate = 0;
    // Try libsndfile to probe header quickly
    QByteArray ba = m_filePath.toLocal8Bit();
    const char* cpath = ba.constData();
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE* snd = sf_open(cpath, SFM_READ, &sfinfo);
    if (snd) {
        samplerate = sfinfo.samplerate;
        channels = sfinfo.channels;
        sf_close(snd);
    }

    QString key;
    const int preferredCachePx = 500;
    if (channels > 0 && samplerate > 0) {
        QJsonObject meta;
        // Try to load the canonical large cache (preferredCachePx). If present,
        // use it (scaled) and avoid enqueuing work for the current widget size.
        QImage cached = WaveformCache::load(WaveformCache::makeKey(m_filePath, size, mtime, channels, samplerate, static_cast<float>(dpr), preferredCachePx), &meta);
        if (!cached.isNull()) {
            m_wavePixmap = QPixmap::fromImage(cached);
            m_hasWavePixmap = true;
                {
                    // Force cached pixmap to the container's logical height and width.
                    QSize labelSize = availableDisplaySize();
                    int containerW = labelSize.width();
                    int containerH = labelSize.height();
                    qreal widgetDpr = devicePixelRatioF();
                    int targetWpx = static_cast<int>(std::floor(containerW * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
                    int targetHpx = static_cast<int>(std::ceil(containerH * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
                    qDebug() << "resize->cache labelSize=" << labelSize << "widgetDpr=" << widgetDpr << "srcPixmap=" << m_wavePixmap.size() << "srcDpr=" << m_wavePixmap.devicePixelRatio() << "targetPx=" << QSize(targetWpx, targetHpx);
                    QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    scaled.setDevicePixelRatio(widgetDpr);
                    m_waveform->setText(QString());
                    m_waveform->setPixmap(scaled);
                }
            update();
            double duration = meta.contains("duration") ? meta.value("duration").toDouble() : 0.0;
            int sr = meta.contains("samplerate") ? meta.value("samplerate").toInt() : samplerate;
            if (!m_filePath.isEmpty()) {
                PlayheadManager::instance()->registerContainer(m_filePath, this, duration, sr);
            }
            return;
        }
    }

    // cancel previous job
    if (!m_pendingJobId.isNull()) {
        m_waveWorker->cancelJob(m_pendingJobId);
        m_pendingJobId = QUuid();
    }
    m_hasWavePixmap = false;
    m_waveform->setText(tr("Rendering..."));
    // Enqueue a job to generate the canonical large cache (preferredCachePx)
    m_pendingJobId = m_waveWorker->enqueueJob(m_filePath, preferredCachePx, dpr);
}

void SoundContainer::setVolume(float v)
{
    if (!m_volume) return;
    int vi = static_cast<int>(v * 100.0f);
    if (vi < m_volume->minimum()) vi = m_volume->minimum();
    if (vi > m_volume->maximum()) vi = m_volume->maximum();
    m_volume->setValue(vi);
}

void SoundContainer::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* md = event->mimeData();
    if (md->hasUrls()) {
        auto urls = md->urls();
        if (!urls.isEmpty()) {
            QFileInfo fi(urls.first().toLocalFile());
            if (fi.exists() && fi.isFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    } else if (md->hasFormat("application/x-soundcontainer") || md->hasFormat("application/x-soundcontainer-copy")) {
        // allow moving between containers
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void SoundContainer::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    // when resized, request a new waveform render if we have a file
    if (!m_filePath.isEmpty()) {
        if (!m_waveWorker) return;
        // Prefer a canonical large cache image and scale it for any widget size
        const int preferredCachePx = 500;
        qreal dpr = devicePixelRatioF();

        // If we already have a pixmap (from cache or prior render), just rescale
        if (m_hasWavePixmap) {
            // Force existing pixmap to the container's logical height and width.
            QSize labelSize = availableDisplaySize();
            int containerW = labelSize.width();
            int containerH = labelSize.height();
            qreal widgetDpr = devicePixelRatioF();
            int targetWpx = static_cast<int>(std::floor(containerW * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
            int targetHpx = static_cast<int>(std::ceil(containerH * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
            QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(widgetDpr);
            m_waveform->setText(QString());
            m_waveform->setPixmap(scaled);
            update();
            return;
        }

        // If a generation job is already pending, don't enqueue again
        if (!m_pendingJobId.isNull()) return;

        // Try to pre-load the canonical cache; if found, use it
        QFileInfo fi(m_filePath);
        qint64 size = fi.size();
        qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        int channels = 0;
        int samplerate = 0;
        QByteArray ba = m_filePath.toLocal8Bit();
        const char* cpath = ba.constData();
        SF_INFO sfinfo;
        memset(&sfinfo, 0, sizeof(sfinfo));
        SNDFILE* snd = sf_open(cpath, SFM_READ, &sfinfo);
        if (snd) {
            samplerate = sfinfo.samplerate;
            channels = sfinfo.channels;
            sf_close(snd);
        }

        if (channels > 0 && samplerate > 0) {
            QJsonObject meta;
            QImage cached = WaveformCache::load(WaveformCache::makeKey(m_filePath, size, mtime, channels, samplerate, static_cast<float>(dpr), preferredCachePx), &meta);
            if (!cached.isNull()) {
                m_wavePixmap = QPixmap::fromImage(cached);
                m_hasWavePixmap = true;
                {
                    // Force cached pixmap to the container's logical height and width.
                    QSize labelSize = availableDisplaySize();
                    int containerW = labelSize.width();
                    int containerH = labelSize.height();
                    qreal widgetDpr = devicePixelRatioF();
                    int targetWpx = static_cast<int>(std::ceil(containerW * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
                    int targetHpx = static_cast<int>(std::ceil(containerH * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
                    qDebug() << "setfile->cache labelSize=" << labelSize << "widgetDpr=" << widgetDpr << "srcPixmap=" << m_wavePixmap.size() << "srcDpr=" << m_wavePixmap.devicePixelRatio() << "targetPx=" << QSize(targetWpx, targetHpx);
                    QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    scaled.setDevicePixelRatio(widgetDpr);
                    m_waveform->setText(QString());
                    m_waveform->setPixmap(scaled);
                }
                update();
                double duration = meta.contains("duration") ? meta.value("duration").toDouble() : 0.0;
                int sr = meta.contains("samplerate") ? meta.value("samplerate").toInt() : samplerate;
                PlayheadManager::instance()->registerContainer(m_filePath, this, duration, sr);
                return;
            }
        }

        // No cache found and no job pending: enqueue a canonical generation job
        m_hasWavePixmap = false;
        m_waveform->setText(tr("Rendering..."));
        m_pendingJobId = m_waveWorker->enqueueJob(m_filePath, preferredCachePx, dpr);
    }
}

void SoundContainer::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    if (!m_hasWavePixmap) return;
    if (!m_playing) return;

    QPainter p(this);
    QRect wfRect = m_waveform->geometry();
    p.setRenderHint(QPainter::Antialiasing);
    // compute playhead x
    if (m_playheadPos < 0.0f) return;
    int x = wfRect.left() + static_cast<int>(m_playheadPos * wfRect.width());
    QPen pen(QColor(255,200,60, 220));
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(x, wfRect.top()+2, x, wfRect.bottom()-2);
}

void SoundContainer::setPlayheadPosition(float pos)
{
    // pos in [0,1], negative -> hidden/stopped
    if (pos < 0.0f) {
        m_playing = false;
        m_playheadPos = -1.0f;
        // restore original waveform pixmap when stopped
                if (m_hasWavePixmap) {
                    // Restore original waveform pixmap scaled to container height and width
                    QSize labelSize = availableDisplaySize();
                    int containerW = labelSize.width();
                    int containerH = labelSize.height();
                    qreal widgetDpr = devicePixelRatioF();
                    int targetWpx = static_cast<int>(std::ceil(containerW * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
                    int targetHpx = static_cast<int>(std::ceil(containerH * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
                    qDebug() << "playhead-restore labelSize=" << labelSize << "widgetDpr=" << widgetDpr << "srcPixmap=" << m_wavePixmap.size() << "srcDpr=" << m_wavePixmap.devicePixelRatio() << "targetPx=" << QSize(targetWpx, targetHpx);
                    QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    scaled.setDevicePixelRatio(widgetDpr);
                    m_waveform->setText(QString());
                    m_waveform->setPixmap(scaled);
                }
    } else {
        m_playing = true;
        m_playheadPos = pos;
        // Ensure the label shows the original waveform pixmap (without any drawn
        // playhead). Previously we mutated the label pixmap to draw the playhead,
        // which produced a duplicate when `paintEvent` also drew the overlay.
        if (m_hasWavePixmap && !m_wavePixmap.isNull()) {
            QSize labelSize = availableDisplaySize();
            if (labelSize.width() > 0 && labelSize.height() > 0) {
                qreal widgetDpr = devicePixelRatioF();
                int targetWpx = static_cast<int>(std::ceil(labelSize.width() * widgetDpr)); if (targetWpx < 1) targetWpx = 1;
                int targetHpx = static_cast<int>(std::ceil(labelSize.height() * widgetDpr)); if (targetHpx < 1) targetHpx = 1;
                QPixmap scaled = m_wavePixmap.scaled(QSize(targetWpx, targetHpx), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                scaled.setDevicePixelRatio(widgetDpr);
                m_waveform->setText(QString());
                m_waveform->setPixmap(scaled);
            }
        }
        // We rely on `paintEvent` to draw the transient playhead overlay above the waveform
        // to avoid mutating the cached pixmap and producing duplicate playheads.
    }
    // Log to debug file and update UI
    writeLocalDebug(QString("setPlayheadPosition this=%1 pos=%2 playing=%3").arg(reinterpret_cast<uintptr_t>(this)).arg(pos).arg(m_playing));
    update();
}


void SoundContainer::dropEvent(QDropEvent* event)
{
    const QMimeData* md = event->mimeData();
    if (md->hasUrls()) {
        auto urls = md->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            QFileInfo fi(path);
            if (fi.exists() && fi.isFile()) {
                setFile(path);
                event->acceptProposedAction();
                return;
            }
        }
    } else if (md->hasFormat("application/x-soundcontainer") || md->hasFormat("application/x-soundcontainer-copy")) {
        QByteArray ba;
        bool isCopy = false;
        if (md->hasFormat("application/x-soundcontainer-copy")) { ba = md->data("application/x-soundcontainer-copy"); isCopy = true; }
        else ba = md->data("application/x-soundcontainer");
        bool ok = false;
        uintptr_t ptr = static_cast<uintptr_t>(ba.toULongLong(&ok));
        if (ok && ptr != 0) {
            SoundContainer* src = reinterpret_cast<SoundContainer*>(ptr);
            if (src && src != this) {
                // mark that we just received a drop so we can suppress
                // the context menu on the subsequent mouse release
                m_justDropped = true;
                QTimer::singleShot(300, this, [this]() { m_justDropped = false; });
                if (isCopy) {
                    writeLocalDebug(QString("copyRequested emitted src=%1 dst=%2").arg(reinterpret_cast<uintptr_t>(src)).arg(reinterpret_cast<uintptr_t>(this)));
                    emit copyRequested(src, this);
                } else {
                    writeLocalDebug(QString("swapRequested emitted src=%1 dst=%2").arg(reinterpret_cast<uintptr_t>(src)).arg(reinterpret_cast<uintptr_t>(this)));
                    emit swapRequested(src, this);
                }
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void SoundContainer::resetToDefaultAppearance()
{
    // Clear waveform display
    m_waveform->setPixmap(QPixmap());
    m_hasWavePixmap = false;
    // Clear waveform text area
    m_waveform->setText(QString());
    m_waveform->setToolTip(QString());
    // Ensure waveform label alignment/margins match constructor defaults
    m_waveform->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_waveform->setContentsMargins(0,0,0,0);
    // Re-apply constructor visual constraints so cleared slots match fresh ones
    m_waveform->setMinimumSize(120, 60);
    m_waveform->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Show prompt in filename label (untouched appearance)
    if (m_filenameLabel) {
        m_filenameLabel->setText(tr("Drop audio file here"));
        m_filenameLabel->setToolTip(QString());
        m_filenameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_filenameLabel->setIndent(6);
    }
    // Clear playhead overlay
    m_playing = false;
    m_playheadPos = -1.0f;
    // Reset control widgets to constructor defaults
    if (m_playBtn) m_playBtn->setText(tr("Play"));
    if (m_volume) {
        m_volume->setRange(0, 100);
        m_volume->setValue(80);
        m_volume->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    // Ensure frame and margins match constructor defaults
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setMidLineWidth(0);
    setContentsMargins(6,6,6,6);
    update();
}

float SoundContainer::volume() const
{
    if (!m_volume) return 1.0f;
    return static_cast<float>(m_volume->value()) / 100.0f;
}
