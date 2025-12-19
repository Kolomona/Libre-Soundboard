#pragma once

#include <QWidget>

/**
 * Lightweight waveform preview widget. Stubbed for initial scaffolding.
 */
class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    ~WaveformWidget() override = default;
};
