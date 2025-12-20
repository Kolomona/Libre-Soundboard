#include "WaveformPyramid.h"
#include <algorithm>
#include <cmath>

QVector<WaveformLevel> WaveformPyramid::build(const QVector<float>& interleavedSamples, int channels, int baseBucket) {
    QVector<WaveformLevel> levels;
    if (channels <= 0 || baseBucket <= 0) return levels;

    const int totalSamples = interleavedSamples.size();
    const int totalFrames = totalSamples / channels;
    if (totalFrames <= 0) return levels;

    // Level 0: compute min/max per bucket of baseBucket frames
    WaveformLevel level0;
    int framesPerBucket = baseBucket;
    level0.samplesPerBucket = framesPerBucket;
    int numBuckets = (totalFrames + framesPerBucket - 1) / framesPerBucket;
    level0.min.reserve(numBuckets);
    level0.max.reserve(numBuckets);

    for (int b = 0; b < numBuckets; ++b) {
        int frameStart = b * framesPerBucket;
        int frameEnd = std::min(frameStart + framesPerBucket, totalFrames);
        float bucketMin = std::numeric_limits<float>::infinity();
        float bucketMax = -std::numeric_limits<float>::infinity();

        for (int f = frameStart; f < frameEnd; ++f) {
            int baseIdx = f * channels;
            for (int c = 0; c < channels; ++c) {
                float v = interleavedSamples[baseIdx + c];
                if (v < bucketMin) bucketMin = v;
                if (v > bucketMax) bucketMax = v;
            }
        }

        // If bucket had no samples (shouldn't happen) fallback to 0
        if (bucketMin == std::numeric_limits<float>::infinity()) bucketMin = 0.0f;
        if (bucketMax == -std::numeric_limits<float>::infinity()) bucketMax = 0.0f;

        level0.min.push_back(bucketMin);
        level0.max.push_back(bucketMax);
    }

    levels.push_back(std::move(level0));

    // Build coarser levels by combining pairs. Each coarser level doubles the samplesPerBucket.
    while (levels.back().min.size() > 1) {
        const WaveformLevel& prev = levels.back();
        WaveformLevel next;
        next.samplesPerBucket = prev.samplesPerBucket * 2;
        int prevBuckets = prev.min.size();
        int nextBuckets = (prevBuckets + 1) / 2;
        next.min.reserve(nextBuckets);
        next.max.reserve(nextBuckets);

        for (int i = 0; i < prevBuckets; i += 2) {
            float a_min = prev.min[i];
            float a_max = prev.max[i];
            if (i + 1 < prevBuckets) {
                float b_min = prev.min[i+1];
                float b_max = prev.max[i+1];
                next.min.push_back(std::min(a_min, b_min));
                next.max.push_back(std::max(a_max, b_max));
            } else {
                // odd last bucket: carry forward
                next.min.push_back(a_min);
                next.max.push_back(a_max);
            }
        }

        levels.push_back(std::move(next));
    }

    return levels;
}

int WaveformPyramid::selectLevelForPixelWidth(int totalFrames, int baseBucket, int desiredPixelWidth, int maxLevels) {
    if (desiredPixelWidth <= 0) return 0;
    if (baseBucket <= 0) baseBucket = 1;
    // samples (frames) per pixel desired
    double framesPerPixel = static_cast<double>(totalFrames) / static_cast<double>(desiredPixelWidth);

    // level k has bucketSize = baseBucket * (2^k)
    int level = 0;
    double bucketSize = baseBucket;
    while (level + 1 < maxLevels && bucketSize < framesPerPixel) {
        bucketSize *= 2.0;
        ++level;
    }
    return level;
}
