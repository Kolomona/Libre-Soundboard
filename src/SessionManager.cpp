#include "SessionManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>

SessionManager& SessionManager::instance() {
    static SessionManager mgr;
    return mgr;
}

SessionManager::SessionManager()
    : QObject(nullptr)
{
    loadRecentSessions();
}

SessionManager::~SessionManager() = default;

bool SessionManager::saveSession(const QString& filePath, const QJsonDocument& doc)
{
    // Ensure directory exists
    QDir dir(QFileInfo(filePath).dir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    
    f.write(doc.toJson());
    f.close();
    
    // Update current path and add to recent
    setCurrentSessionPath(filePath);
    updateRecentSessions(filePath);
    saveRecentSessions();
    
    return true;
}

QJsonDocument SessionManager::loadSession(const QString& filePath)
{
    QFile f(filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return QJsonDocument();
    }
    
    QByteArray data = f.readAll();
    f.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    // Update current path when loading
    setCurrentSessionPath(filePath);
    updateRecentSessions(filePath);
    saveRecentSessions();
    
    return doc;
}

QString SessionManager::currentSessionPath() const
{
    return m_currentSessionPath;
}

void SessionManager::setCurrentSessionPath(const QString& path)
{
    m_currentSessionPath = path;
}

void SessionManager::updateRecentSessions(const QString& sessionPath)
{
    // Remove if already in list to move to front
    m_recentSessions.removeAll(sessionPath);
    
    // Add to front (most recent)
    m_recentSessions.prepend(sessionPath);
    
    // Trim to max size
    while (m_recentSessions.size() > MAX_RECENT_SESSIONS) {
        m_recentSessions.removeLast();
    }
}

QStringList SessionManager::recentSessions() const
{
    return m_recentSessions;
}

void SessionManager::clearRecentSessions()
{
    m_recentSessions.clear();
    saveRecentSessions();
}

void SessionManager::cleanRecentSessions()
{
    // Remove any paths that no longer exist
    QStringList cleaned;
    for (const QString& path : m_recentSessions) {
        if (QFile::exists(path)) {
            cleaned.append(path);
        }
    }
    m_recentSessions = cleaned;
    saveRecentSessions();
}

void SessionManager::saveRecentSessions()
{
    QSettings s("libresoundboard", "libresoundboard");
    s.setValue("sessions/recent", m_recentSessions);
    s.sync();
}

void SessionManager::loadRecentSessions()
{
    QSettings s("libresoundboard", "libresoundboard");
    m_recentSessions = s.value("sessions/recent", QStringList()).toStringList();
}
