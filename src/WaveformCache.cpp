#include "WaveformCache.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include "PreferencesManager.h"

QString WaveformCache::cacheDirPath() {
    // Phase 4: Get cache directory from PreferencesManager
    // This allows users to configure it via preferences
    QString cacheDir = PreferencesManager::instance().cacheDirectory();
    
    QDir pdir(cacheDir);
    if (!pdir.exists()) pdir.mkpath(".");
    return cacheDir;
}

QString WaveformCache::makeKey(const QString& path, qint64 size, qint64 mtime, int channels, int samplerate, float dpr, int pixelWidth) {
    QByteArray ba;
    ba.append(path.toUtf8());
    ba.append(",");
    ba.append(QByteArray::number(size));
    ba.append(",");
    ba.append(QByteArray::number(mtime));
    ba.append(",ch:"); ba.append(QByteArray::number(channels));
    ba.append(",sr:"); ba.append(QByteArray::number(samplerate));
    ba.append(",dpr:"); ba.append(QByteArray::number(dpr));
    ba.append(",pw:"); ba.append(QByteArray::number(pixelWidth));

    QByteArray hash = QCryptographicHash::hash(ba, QCryptographicHash::Md5).toHex();
    return QString::fromUtf8(hash);
}

bool WaveformCache::write(const QString& key, const QImage& image, const QJsonObject& metadata) {
    QString dir = cacheDirPath();
    QDir d(dir);
    QString imgPath = d.filePath(key + ".png");
    QString metaPath = d.filePath(key + ".json");

    // Write image to temp file then rename
    QString tmpImg = imgPath + ".tmp";
    if (!image.save(tmpImg, "PNG")) return false;
    QFile::remove(imgPath);
    if (!QFile::rename(tmpImg, imgPath)) {
        QFile::remove(tmpImg);
        qWarning() << "WaveformCache::write failed to rename img tmp" << tmpImg << "->" << imgPath;
        return false;
    }

    // Write metadata
    QJsonDocument doc(metadata);
    QByteArray bytes = doc.toJson(QJsonDocument::Compact);
    QString tmpMeta = metaPath + ".tmp";
    QFile ftmp(tmpMeta);
    if (!ftmp.open(QIODevice::WriteOnly)) return false;
    ftmp.write(bytes);
    ftmp.close();
    QFile::remove(metaPath);
    if (!QFile::rename(tmpMeta, metaPath)) {
        QFile::remove(tmpMeta);
        qWarning() << "WaveformCache::write failed to rename meta tmp" << tmpMeta << "->" << metaPath;
        return false;
    }

    // write success (quiet)

    return true;
}

QImage WaveformCache::load(const QString& key, QJsonObject* outMetadata) {
    QString dir = cacheDirPath();
    QDir d(dir);
    QString imgPath = d.filePath(key + ".png");
    QString metaPath = d.filePath(key + ".json");

    if (!QFile::exists(imgPath) || !QFile::exists(metaPath)) return QImage();

    // Read metadata
    QFile fmeta(metaPath);
    if (!fmeta.open(QIODevice::ReadOnly)) return QImage();
    QByteArray mbytes = fmeta.readAll();
    fmeta.close();
    QJsonDocument doc = QJsonDocument::fromJson(mbytes);
    if (!doc.isObject()) return QImage();
    QJsonObject obj = doc.object();

    // Basic validation: ensure metadata contains the fields required to recompute the cache key
    const QStringList required = {"path", "size", "mtime", "channels", "samplerate", "dpr", "pixelWidth"};
    for (const QString& k : required) {
        if (!obj.contains(k)) {
            // corrupt metadata: remove both files and fail
            QFile::remove(imgPath);
            QFile::remove(metaPath);
            return QImage();
        }
    }

    // Recompute key from metadata and ensure it matches the provided key; if not, treat as mismatch and remove files.
    QString path = obj.value("path").toString();
    qint64 size = static_cast<qint64>(obj.value("size").toDouble());
    qint64 mtime = static_cast<qint64>(obj.value("mtime").toDouble());
    int channels = obj.value("channels").toInt();
    int samplerate = obj.value("samplerate").toInt();
    float dpr = static_cast<float>(obj.value("dpr").toDouble());
    int pixelWidth = obj.value("pixelWidth").toInt();

    QString recomputed = makeKey(path, size, mtime, channels, samplerate, dpr, pixelWidth);
    if (recomputed != key) {
        qWarning() << "WaveformCache: key mismatch" << key << "!=" << recomputed << "-- removing";
        qWarning() << "meta:" << obj;
        QFile::remove(imgPath);
        QFile::remove(metaPath);
        return QImage();
    }

    // Provide metadata to caller
    if (outMetadata) *outMetadata = obj;

    QImage img;
    if (!img.load(imgPath)) return QImage();
    // Ensure the loaded image carries the DPR recorded in metadata so
    // callers (which convert to QPixmap) can display it at the expected
    // device scale without unexpected pixel rounding/cropping.
    if (dpr <= 0.0f) dpr = 1.0f;
    img.setDevicePixelRatio(dpr);
    return img;
}

QImage WaveformCache::loadBest(const QString& path, qint64 size, qint64 mtime, int channels, int samplerate, float dpr, int pixelWidth, QJsonObject* outMetadata) {
    // Try exact match first
    QString exactKey = makeKey(path, size, mtime, channels, samplerate, dpr, pixelWidth);
    QImage exact = load(exactKey, outMetadata);
    if (!exact.isNull()) return exact;

    // Scan cache dir for metadata JSON files and find candidates that match the
    // identifying fields. Prefer candidate with pixelWidth >= requested and
    // with minimal pixelWidth; otherwise pick the largest available smaller one.
    QString dir = cacheDirPath();
    QDir d(dir);
    if (!d.exists()) return QImage();
    QStringList jsonFiles = d.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

    int bestGreater = INT_MAX;
    int bestSmaller = -1;
    QString bestKeyGreater;
    QString bestKeySmaller;
    QJsonObject bestMeta;

    for (const QString& jf : jsonFiles) {
        QString metaPath = d.filePath(jf);
        QFile fmeta(metaPath);
        if (!fmeta.open(QIODevice::ReadOnly)) continue;
        QByteArray mbytes = fmeta.readAll();
        fmeta.close();
        QJsonDocument doc = QJsonDocument::fromJson(mbytes);
        if (!doc.isObject()) continue;
        QJsonObject obj = doc.object();
        // match identity fields
        if (obj.value("path").toString() != path) continue;
        if (static_cast<qint64>(obj.value("size").toDouble()) != size) continue;
        if (static_cast<qint64>(obj.value("mtime").toDouble()) != mtime) continue;
        if (obj.value("channels").toInt() != channels) continue;
        if (obj.value("samplerate").toInt() != samplerate) continue;
        if (static_cast<int>(obj.value("dpr").toDouble()) != static_cast<int>(dpr)) continue;
        int pw = obj.value("pixelWidth").toInt();
        // compute key for this metadata file (filename without .json)
        QString base = jf;
        if (base.endsWith(".json")) base.chop(5);
        QString candidateKey = base;
        if (pw >= pixelWidth) {
            if (pw < bestGreater) {
                bestGreater = pw;
                bestKeyGreater = candidateKey;
                bestMeta = obj;
            }
        } else {
            if (pw > bestSmaller) {
                bestSmaller = pw;
                bestKeySmaller = candidateKey;
                bestMeta = obj;
            }
        }
    }

    QString chosenKey;
    QJsonObject chosenMeta;
    if (!bestKeyGreater.isEmpty()) {
        chosenKey = bestKeyGreater;
        chosenMeta = bestMeta;
    } else if (!bestKeySmaller.isEmpty()) {
        chosenKey = bestKeySmaller;
        chosenMeta = bestMeta;
    } else {
        return QImage();
    }

    QImage img;
    QString imgPath = d.filePath(chosenKey + ".png");
    if (!img.load(imgPath)) return QImage();

    // If the image pixelWidth differs, scale it to requested size (downscale preferred)
    int availablePW = chosenMeta.value("pixelWidth").toInt();
    if (availablePW != pixelWidth) {
        // Scale image to the requested pixelWidth taking DPR into account.
        float chosenDpr = static_cast<float>(chosenMeta.value("dpr").toDouble());
        if (chosenDpr <= 0.0f) chosenDpr = 1.0f;
        int targetWpx = static_cast<int>(std::ceil(static_cast<float>(pixelWidth) * chosenDpr));
        int targetHpx = 1;
        if (img.width() > 0) targetHpx = static_cast<int>(std::ceil(static_cast<float>(img.height()) * (static_cast<float>(targetWpx) / static_cast<float>(img.width()))));
        img = img.scaled(targetWpx, targetHpx, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img.setDevicePixelRatio(chosenDpr);
    }

    if (outMetadata) *outMetadata = chosenMeta;
    return img;
}

void WaveformCache::evict(qint64 softLimitBytes, int ttlDays)
{
    // Use preferences values regardless of provided defaults
    int mb = PreferencesManager::instance().cacheSoftLimitMB();
    int days = PreferencesManager::instance().cacheTtlDays();
    if (mb < 0) mb = 0; if (days < 0) days = 0;
    softLimitBytes = static_cast<qint64>(mb) * 1024 * 1024;
    ttlDays = days;
    QString dir = cacheDirPath();
    QDir d(dir);
    if (!d.exists()) return;

    // Collect candidate base names (without extension) for json/png pairs
    QStringList jsonFiles = d.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
    struct Entry { QString base; qint64 size; QDateTime mtime; };
    QVector<Entry> entries;
    qint64 total = 0;
    for (const QString& jf : jsonFiles) {
        QString base = jf;
        if (base.endsWith(".json")) base.chop(5);
        QString metaPath = d.filePath(base + ".json");
        QString imgPath = d.filePath(base + ".png");
        QFileInfo imi(metaPath);
        if (!imi.exists()) continue;
        QFileInfo ifi(imgPath);
        qint64 s = imi.size() + (ifi.exists() ? ifi.size() : 0);
        QDateTime m = imi.lastModified();
        entries.push_back({base, s, m});
        total += s;
    }

    // Also consider stray png files without json
    QStringList pngFiles = d.entryList(QStringList() << "*.png", QDir::Files, QDir::Name);
    for (const QString& pf : pngFiles) {
        QString base = pf;
        if (base.endsWith(".png")) base.chop(4);
        // if we already accounted via json, skip
        bool found = false;
        for (const Entry& e : entries) if (e.base == base) { found = true; break; }
        if (found) continue;
        QString imgPath = d.filePath(base + ".png");
        QFileInfo ifi(imgPath);
        qint64 s = ifi.exists() ? ifi.size() : 0;
        QDateTime m = ifi.lastModified();
        entries.push_back({base, s, m});
        total += s;
    }

    // Remove entries older than TTL first
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-ttlDays);
    bool removedAny = false;
    for (const Entry& e : entries) {
        if (e.mtime < cutoff) {
            QString imgPath = d.filePath(e.base + ".png");
            QString metaPath = d.filePath(e.base + ".json");
            bool r1 = QFile::remove(imgPath);
            bool r2 = QFile::remove(metaPath);
            if (r1 || r2) removedAny = true;
            total -= e.size;
            qDebug() << "WaveformCache::evict removed (TTL)" << e.base << e.size;
        }
    }

    if (total <= softLimitBytes) return;

    // Build remaining entries list
    QVector<Entry> remain;
    for (const Entry& e : entries) {
        QString metaPath = d.filePath(e.base + ".json");
        QString imgPath = d.filePath(e.base + ".png");
        QFileInfo imi(metaPath);
        QDateTime m = imi.exists() ? imi.lastModified() : QFileInfo(imgPath).lastModified();
        // re-evaluate size
        qint64 s = 0;
        if (imi.exists()) s += imi.size();
        QFileInfo ifi(imgPath);
        if (ifi.exists()) s += ifi.size();
        remain.push_back({e.base, s, m});
    }

    // Sort by mtime ascending (oldest first)
    std::sort(remain.begin(), remain.end(), [](const Entry& a, const Entry& b){ return a.mtime < b.mtime; });

    for (const Entry& e : remain) {
        if (total <= softLimitBytes) break;
        QString imgPath = d.filePath(e.base + ".png");
        QString metaPath = d.filePath(e.base + ".json");
        qint64 removedSize = 0;
        QFileInfo ifi(imgPath);
        if (ifi.exists()) { removedSize += ifi.size(); QFile::remove(imgPath); }
        QFileInfo imi(metaPath);
        if (imi.exists()) { removedSize += imi.size(); QFile::remove(metaPath); }
        total -= removedSize;
        qDebug() << "WaveformCache::evict removed" << e.base << removedSize << "remaining:" << total;
    }
}

void WaveformCache::clearAll()
{
    QString dir = cacheDirPath();
    QDir d(dir);
    if (!d.exists()) return;
    QStringList pngFiles = d.entryList(QStringList() << "*.png", QDir::Files);
    for (const QString& f : pngFiles) QFile::remove(d.filePath(f));
    QStringList jsonFiles = d.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString& f : jsonFiles) QFile::remove(d.filePath(f));
}
