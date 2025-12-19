#pragma once

#include <QFrame>
#include <QString>

#include <QUuid>
class QPushButton;
class QLabel;
class QSlider;
class QResizeEvent;
class QPaintEvent;

// forward declare worker structs
struct WaveformJob;
struct WaveformResult;

/**
 * A single sound container shown in the grid. Displays a waveform, play button and volume knob.
 */
class SoundContainer : public QFrame
{
    Q_OBJECT
public:
    explicit SoundContainer(QWidget* parent = nullptr);
    ~SoundContainer() override = default;

    void setFile(const QString& path);
    QString file() const { return m_filePath; }
    void setVolume(float v);

signals:
    void swapRequested(SoundContainer* source, SoundContainer* target);
    void copyRequested(SoundContainer* source, SoundContainer* target);

    void playRequested(const QString& path, SoundContainer* self);
    void stopRequested(const QString& path, SoundContainer* self);
    void fileChanged(const QString& path);
    void volumeChanged(float volume);
    void clearRequested(SoundContainer* self);

public:
    // Volume in range [0.0, 1.0]
    float volume() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onWaveformReady(const WaveformJob& job, const WaveformResult& result);
    void onWaveformError(const WaveformJob& job, const QString& err);

private:
    QPushButton* m_playBtn;
    QLabel* m_waveform;
    QSlider* m_volume;
    QString m_filePath;
    QPoint m_dragStart;
    bool m_dragging = false;
    Qt::MouseButton m_dragButton = Qt::NoButton;
    // Set when this container received a drop so a following release won't show the menu
    bool m_justDropped = false;
    // Waveform integration (phase 4)
    class WaveformWorker* m_waveWorker = nullptr;
    QUuid m_pendingJobId;
    QPixmap m_wavePixmap;
    bool m_hasWavePixmap = false;
    bool m_playing = false;
    // Normalized [0,1] playhead position; negative means hidden
    float m_playheadPos = -1.0f;
};
