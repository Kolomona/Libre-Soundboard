#pragma once

#include <QImage>
#include <QJsonObject>
#include <QString>

class WaveformCache {
public:
    // Generate a simple cache key string (MD5-like) from inputs. For tests this is a simple concatenation.
    static QString makeKey(const QString& path, qint64 size, qint64 mtime, int channels, int samplerate, float dpr, int pixelWidth);

    // Write image and metadata atomically. Returns true on success.
    static bool write(const QString& key, const QImage& image, const QJsonObject& metadata);

    // Load image if present and metadata matches (basic check). Returns null image if not found/invalid.
    static QImage load(const QString& key, QJsonObject* outMetadata = nullptr);

    // For tests/tools: get cache directory path
    static QString cacheDirPath();
};
