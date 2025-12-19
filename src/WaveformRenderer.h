#pragma once

#include "WaveformPyramid.h"
#include <QImage>

namespace Waveform {
    // Render a single pyramid level to a DPR-aware QImage.
    // - level: min/max arrays (one bucket per horizontal sample)
    // - pixelWidth: target width in CSS pixels
    // - dpr: device pixel ratio (e.g. 2.0 for retina)
    // - heightCss: height in CSS pixels
    QImage renderLevelToImage(const WaveformLevel& level, int pixelWidth, float dpr = 1.0f, int heightCss = 40);
}
