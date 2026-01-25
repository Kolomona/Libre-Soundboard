#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonDocument>

class SessionManager : public QObject {
    Q_OBJECT
public:
    static SessionManager& instance();
    
    // Session I/O
    bool saveSession(const QString& filePath, const QJsonDocument& doc);
    QJsonDocument loadSession(const QString& filePath);
    
    // Current session tracking
    QString currentSessionPath() const;
    void setCurrentSessionPath(const QString& path);
    
    // Recent sessions management
    void updateRecentSessions(const QString& sessionPath);
    QStringList recentSessions() const;
    void clearRecentSessions();
    void cleanRecentSessions();  // Remove non-existent files
    
    // Persistence
    void saveRecentSessions();
    void loadRecentSessions();
    
    static constexpr int MAX_RECENT_SESSIONS = 10;
    
private:
    SessionManager();
    ~SessionManager();
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    QString m_currentSessionPath;
    QStringList m_recentSessions;
};
