#include "WaveformCache.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QCryptographicHash>

QString WaveformCache::cacheDirPath() {
    // Allow override for tests or custom locations
    const char* env = std::getenv("LIBRE_WAVEFORM_CACHE_DIR");
    if (env && env[0]) {
        QString p = QString::fromUtf8(env);
        QDir pd(p);
        if (!pd.exists()) pd.mkpath(".");
        return p;
    }

    // Prefer platform cache location so files survive reboots.
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.cache";
    QDir d(base);
    QString p = d.filePath("libresoundboard/waveforms");
    QDir pdir(p);
    if (!pdir.exists()) pdir.mkpath(".");
    return p;
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
        return false;
    }

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

    // Provide metadata to caller
    if (outMetadata) *outMetadata = obj;

    QImage img;
    if (!img.load(imgPath)) return QImage();
    return img;
}
