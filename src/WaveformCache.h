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

    // Load an exact cached image if available. If not found, search the
    // cache directory for candidate metadata entries that match path/size/mtime/
    // channels/samplerate/dpr and return the best match where pixelWidth >=
    // requested (prefer smallest >= requested). If none >= requested exist,
    // return the largest available smaller entry. Returns null image if no
    // candidates found. outMetadata will contain the chosen metadata when
    // available.
    static QImage loadBest(const QString& path, qint64 size, qint64 mtime, int channels, int samplerate, float dpr, int pixelWidth, QJsonObject* outMetadata = nullptr);

    // For tests/tools: get cache directory path
    static QString cacheDirPath();
    // Evict cache entries so total size is <= softLimitBytes (in bytes).
    // Removes oldest entries first based on filesystem modification time.
    // Also removes entries older than ttlDays regardless of size.
    static void evict(qint64 softLimitBytes = 200 * 1024 * 1024, int ttlDays = 90);

    // Remove all cache files. Useful for tests and debug actions.
    static void clearAll();
};
