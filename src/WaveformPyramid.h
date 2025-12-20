#pragma once

#include <QVector>

struct WaveformLevel {
    QVector<float> min;
    QVector<float> max;
    // Number of frames (samples per channel) that each bucket in this level represents
    int samplesPerBucket = 0;
};

class WaveformPyramid {
public:
    // Build pyramid from interleaved samples. `channels` is number of channels.
    // `baseBucket` is samples-per-bucket at level 0 (per channel frames).
    static QVector<WaveformLevel> build(const QVector<float>& interleavedSamples, int channels, int baseBucket = 256);

    // Choose pyramid level index for desired pixel width given total frames and baseBucket.
    // Returns 0..levels-1 (clamped).
    static int selectLevelForPixelWidth(int totalFrames, int baseBucket, int desiredPixelWidth, int maxLevels);
};
