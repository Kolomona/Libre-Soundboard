#include "WaveformRenderer.h"
#include <QPainter>
#include <QColor>

namespace Waveform {

QImage renderLevelToImage(const WaveformLevel& level, int pixelWidth, float dpr, int heightCss) {
    if (pixelWidth <= 0) pixelWidth = 1;
    if (dpr <= 0.0f) dpr = 1.0f;
    if (heightCss <= 0) heightCss = 40;

    const int width = static_cast<int>(std::ceil(pixelWidth * dpr));
    const int height = static_cast<int>(std::ceil(heightCss * dpr));

    QImage img(width, height, QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);

    QColor fg(0,0,0,200);
    QColor mid(128,128,128,60);

    // draw background midline
    p.fillRect(0, 0, width, height, Qt::transparent);

    const int buckets = qMax(1, level.min.size());

    // Map bucket index to pixel x. We'll sample per pixel by scaling buckets->width.
    for (int x = 0; x < width; ++x) {
        // find corresponding bucket range (fractional)
        double bucketPos = (static_cast<double>(x) / static_cast<double>(width)) * static_cast<double>(buckets);
        int idx = qBound(0, static_cast<int>(std::floor(bucketPos)), buckets - 1);

        float vmin = level.min.isEmpty() ? 0.0f : level.min[idx];
        float vmax = level.max.isEmpty() ? 0.0f : level.max[idx];

        // values expected in [-1..1] but be defensive
        double top = (1.0 - ((vmax + 1.0) / 2.0)) * (height - 1);
        double bottom = (1.0 - ((vmin + 1.0) / 2.0)) * (height - 1);

        int itop = qBound(0, static_cast<int>(std::round(top)), height - 1);
        int ibottom = qBound(0, static_cast<int>(std::round(bottom)), height - 1);

        // draw vertical line for this x from itop to ibottom
        p.setPen(fg);
        p.drawLine(x, itop, x, ibottom);
    }

    // draw center line faint
    p.setPen(mid);
    int centerY = height / 2;
    p.drawLine(0, centerY, width - 1, centerY);

    p.end();
    return img;
}

} // namespace Waveform
