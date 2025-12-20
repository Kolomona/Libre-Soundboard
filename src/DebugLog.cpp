#include "DebugLog.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

static QFile* g_logFile = nullptr;
static QtMessageHandler g_oldHandler = nullptr;
static QMutex g_logMutex;

static void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&g_logMutex);
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream ts(g_logFile);
        ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " ";
        switch (type) {
        case QtDebugMsg: ts << "DEBUG: "; break;
        case QtInfoMsg: ts << "INFO: "; break;
        case QtWarningMsg: ts << "WARN: "; break;
        case QtCriticalMsg: ts << "CRIT: "; break;
        case QtFatalMsg: ts << "FATAL: "; break;
        }
        ts << msg << "\n";
        ts.flush();
        if (type == QtFatalMsg) abort();
        return;
    }

    // No log file: suppress debug/info to avoid console spam; forward warnings/errors
    if (type == QtDebugMsg || type == QtInfoMsg) return;

    if (g_oldHandler) {
        g_oldHandler(type, context, msg);
    } else {
        QByteArray localMsg = msg.toLocal8Bit();
        fwrite(localMsg.constData(), 1, localMsg.size(), stderr);
        fwrite("\n", 1, 1, stderr);
    }
}

// Install a message handler at library load time to suppress debug/info to console
struct DebugLogInitializer {
    DebugLogInitializer() {
        g_oldHandler = qInstallMessageHandler(debugMessageHandler);
    }
};
static DebugLogInitializer s_debugLogInitializer;

void DebugLog::install(const QString& path)
{
    QMutexLocker locker(&g_logMutex);
    if (g_logFile) {
        // already enabled
        return;
    }
    if (!path.isEmpty()) {
        g_logFile = new QFile(path);
        if (!g_logFile->open(QIODevice::Append | QIODevice::Text)) {
            delete g_logFile;
            g_logFile = nullptr;
        }
    }
}

void DebugLog::uninstall()
{
    QMutexLocker locker(&g_logMutex);
    if (g_logFile) {
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
}
