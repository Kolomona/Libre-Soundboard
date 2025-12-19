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
    QString path = QStringLiteral("/tmp/libresoundboard-debug.log");
    QString line = QDateTime::currentDateTime().toString(Qt::ISODate) + " [" + QString::number(getpid()) + "] " + msg + "\n";
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
    qDebug().noquote() << line.trimmed();
}
}

SoundContainer::SoundContainer(QWidget* parent)
    : QFrame(parent)
{
    auto layout = new QVBoxLayout(this);
    m_waveform = new QLabel(tr("Drop audio file here"), this);
    m_waveform->setMinimumSize(160, 80);
    m_waveform->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_waveform->setIndent(6);

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
}

// Worker signal handlers
void SoundContainer::onWaveformReady(const WaveformJob& job, const WaveformResult& result)
{
    if (job.id != m_pendingJobId) return;
    // build a simple pixmap preview (fallback renderer for phase 4)
    QSize labelSize = m_waveform->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0) return;
    qreal dpr = job.dpr <= 0.0 ? 1.0 : job.dpr;
    QImage img(labelSize.width() * dpr, labelSize.height() * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    // background
    p.fillRect(img.rect(), QColor(40, 40, 40));
    // draw a naive representation: if we have min/max arrays, draw line strips
    if (!result.min.isEmpty() && result.min.size() == result.max.size()) {
        int n = result.min.size();
        int w = img.width() / dpr;
        int h = img.height() / dpr;
        QPen pen(QColor(180, 180, 220));
        pen.setWidthF(1.0);
        p.setPen(pen);
        for (int i = 0; i < n; ++i) {
            float mn = result.min[i];
            float mx = result.max[i];
            int x = static_cast<int>((double)i / (double)n * w);
            int y1 = static_cast<int>((0.5 - mn * 0.5) * h);
            int y2 = static_cast<int>((0.5 - mx * 0.5) * h);
            p.drawLine(x, y1, x, y2);
        }
    } else {
        // no waveform data: draw a placeholder banded graphic to indicate rendering done
        int w = img.width() / dpr;
        int h = img.height() / dpr;
        QPen pen(QColor(100, 160, 200));
        pen.setWidthF(1.0);
        p.setPen(pen);
        for (int x = 0; x < w; x += 4) {
            int hh = (x % 12 == 0) ? (h * 0.6) : (h * 0.3);
            p.drawLine(x, (h - hh) / 2, x, (h + hh) / 2);
        }
    }
    p.end();

    m_wavePixmap = QPixmap::fromImage(img);
    m_hasWavePixmap = true;
    m_waveform->setPixmap(m_wavePixmap.scaled(m_waveform->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    update();
}

void SoundContainer::onWaveformError(const WaveformJob& job, const QString& err)
{
    Q_UNUSED(job);
    Q_UNUSED(err);
    // For now, just keep placeholder text; ensure we don't mistakenly show previous pixmap
    m_hasWavePixmap = false;
    m_waveform->setPixmap(QPixmap());
    m_waveform->setText(m_filePath.isEmpty() ? tr("Drop audio file here") : QFileInfo(m_filePath).fileName());
    update();
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
        m_filePath.clear();
        m_waveform->setPixmap(QPixmap());
        m_hasWavePixmap = false;
        m_waveform->setText(tr("Drop audio file here"));
        m_waveform->setToolTip(QString());
        setVolume(0.8f);
        emit fileChanged(QString());
        return;
    }

    m_filePath = path;
    QFileInfo fi(path);
    m_waveform->setText(fi.fileName());
    // show filename on hover (not the full path)
    m_waveform->setToolTip(fi.fileName());
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
    // cancel previous job
    if (!m_pendingJobId.isNull()) {
        m_waveWorker->cancelJob(m_pendingJobId);
        m_pendingJobId = QUuid();
    }
    m_hasWavePixmap = false;
    m_waveform->setText(tr("Rendering..."));
    m_pendingJobId = m_waveWorker->enqueueJob(m_filePath, px, dpr);
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
        int px = qMax(1, m_waveform->width());
        qreal dpr = devicePixelRatioF();
        if (!m_pendingJobId.isNull()) {
            m_waveWorker->cancelJob(m_pendingJobId);
            m_pendingJobId = QUuid();
        }
        m_hasWavePixmap = false;
        m_waveform->setText(tr("Rendering..."));
        m_pendingJobId = m_waveWorker->enqueueJob(m_filePath, px, dpr);
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

float SoundContainer::volume() const
{
    if (!m_volume) return 1.0f;
    return static_cast<float>(m_volume->value()) / 100.0f;
}
