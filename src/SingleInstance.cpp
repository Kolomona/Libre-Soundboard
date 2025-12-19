#include "SingleInstance.h"
#include "MainWindow.h"

#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include <QtCore/QLockFile>
#include <memory>
#include <QtCore/QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>

// Try to notify an existing instance (unix socket first, then QLocalSocket).
bool notifyExistingInstance()
{
    const QString instanceKey = "libresoundboard_instance";
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (cfgDir.isEmpty()) cfgDir = QDir::homePath() + "/.local/share";
    QDir d(cfgDir + "/libresoundboard");
    if (!d.exists()) d.mkpath(".");
    const QString unixSocketPath = QString("/tmp/libresoundboard_instance.sock");

    // Try POSIX unix socket fallback first
    auto tryUnixNotify = [&](const QString &path)->bool{
        int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (s < 0) return false;
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        QByteArray pb = path.toLocal8Bit();
        strncpy(addr.sun_path, pb.constData(), sizeof(addr.sun_path)-1);
        int res = ::connect(s, (struct sockaddr*)&addr, sizeof(addr));
        if (res < 0) { ::close(s); return false; }
        const char *msg = "raise";
        ::send(s, msg, strlen(msg), 0);
        ::close(s);
        return true;
    };

    if (tryUnixNotify(unixSocketPath)) {
        return true;
    }

    // Fallback to QLocalSocket notify
    QLocalSocket socket;
    socket.connectToServer(instanceKey);
    if (socket.waitForConnected(500)) {
        socket.write("raise");
        socket.flush();
        socket.waitForBytesWritten(500);
        socket.disconnectFromServer();
        return true;
    }
    return false;
}

// Start the single-instance server. Returns false on success (we are primary), true means caller should exit.
bool startSingleInstanceServer(MainWindow* mainWindow)
{
    const QString instanceKey = "libresoundboard_instance";
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (cfgDir.isEmpty()) cfgDir = QDir::homePath() + "/.local/share";
    QDir d(cfgDir + "/libresoundboard");
    if (!d.exists()) d.mkpath(".");
    QString lockPath = d.filePath("libresoundboard.lock");
    static std::unique_ptr<QLockFile> s_lockFile;
    if (!s_lockFile) s_lockFile.reset(new QLockFile(lockPath));
    QLockFile &lockFile = *s_lockFile;
    lockFile.setStaleLockTime(30000);

    // Prepare stable paths for logging and a thread-safe logger copy
    const QString userLogPath = d.filePath("instance.log");
    const QString appLogPath = QCoreApplication::applicationDirPath() + QDir::separator() + QStringLiteral("instance.log");
    auto writeLog = [userLogPath, appLogPath](const QString& line) {
        QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
        QString out = QString("%1 [%2] %3\n").arg(ts).arg((int)QCoreApplication::applicationPid()).arg(line);
        // user data dir
        QFile f(userLogPath);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream tsout(&f);
            tsout << out;
            f.close();
        }
        // application dir (useful for panel-launched instances; within workspace)
        QFile fa(appLogPath);
        if (fa.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream tsout(&fa);
            tsout << out;
            fa.close();
        }
        qInfo().noquote() << out.trimmed();
    };

    writeLog(QString("SingleInstance: lockPath=%1").arg(lockPath));
    // log some useful environment vars to diagnose launcher differences
    QString envSummary = QString("env: XDG_RUNTIME_DIR=%1 DISPLAY=%2 DBUS_SESSION_BUS_ADDRESS=%3 USER=%4 HOME=%5")
            .arg(QString::fromUtf8(qgetenv("XDG_RUNTIME_DIR")))
            .arg(QString::fromUtf8(qgetenv("DISPLAY")))
            .arg(QString::fromUtf8(qgetenv("DBUS_SESSION_BUS_ADDRESS")))
            .arg(QString::fromUtf8(qgetenv("USER")))
            .arg(QString::fromUtf8(qgetenv("HOME")));
    writeLog(envSummary);

    const QString unixSocketPath = QString("/tmp/libresoundboard_instance.sock");

    if (!lockFile.tryLock(100)) {
        writeLog("SingleInstance: could not acquire lock immediately, attempting to notify existing instance...");
        if (notifyExistingInstance()) {
            writeLog("SingleInstance: notified existing instance; exiting");
            return true;
        }
        writeLog(QString("SingleInstance: failed to notify existing instance; assuming stale lock. Removing stale lock file: %1").arg(lockPath));
        if (!QFile::remove(lockPath)) {
            writeLog(QString("SingleInstance: failed to remove stale lock file %1").arg(lockPath));
        }
        if (!lockFile.tryLock(1000)) {
            writeLog("SingleInstance: still could not acquire lock after removing stale lock");
            return true; // give up, avoid multiple instances
        }
        writeLog("SingleInstance: lock acquired after removing stale lock");
    } else {
        writeLog("SingleInstance: lock acquired");
    }

    // We are the primary instance; start unix-domain socket server for notifications.
    writeLog(QString("SingleInstance: primary instance (using unix socket)"));

    // Start a simple POSIX unix-domain socket server.
    static std::atomic<int> s_unix_server_fd{-1};
    if (s_unix_server_fd.load() == -1) {
        // remove stale socket file
        ::unlink(unixSocketPath.toUtf8().constData());
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd != -1) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            QByteArray pathBytes = unixSocketPath.toLocal8Bit();
            strncpy(addr.sun_path, pathBytes.constData(), sizeof(addr.sun_path) - 1);
            if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0 && listen(fd, 5) == 0) {
                s_unix_server_fd.store(fd);
                // set non-blocking
                int fl = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, fl | O_NONBLOCK);
                writeLog(QString("SingleInstance: unix socket server listening on %1").arg(unixSocketPath));
                // Use QSocketNotifier to integrate with Qt event loop
                QSocketNotifier* notifier = new QSocketNotifier(fd, QSocketNotifier::Read, mainWindow);
                QObject::connect(notifier, &QSocketNotifier::activated, mainWindow, [fd, mainWindow, writeLog]() {
                    // accept all pending clients
                    while (true) {
                        int client = accept(fd, nullptr, nullptr);
                        if (client < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                            if (errno == EINTR) continue;
                            writeLog(QString("SingleInstance: accept error: %1").arg(QString::fromUtf8(strerror(errno))));
                            break;
                        }
                        // read up to 256 bytes
                        char buf[256];
                        ssize_t r = ::recv(client, buf, sizeof(buf)-1, 0);
                        if (r > 0) {
                            buf[r] = '\0';
                            QString msg = QString::fromUtf8(buf);
                            writeLog(QString("SingleInstance: unix-socket received message: %1").arg(msg));
                            if (msg.trimmed() == "raise") {
                                writeLog("SingleInstance: raising main window (unix socket)");
                                if (mainWindow) {
                                    QMetaObject::invokeMethod(mainWindow, "raise", Qt::QueuedConnection);
                                    QMetaObject::invokeMethod(mainWindow, "activateWindow", Qt::QueuedConnection);
                                }
                            }
                        }
                        ::close(client);
                    }
                });
            } else {
                ::close(fd);
                writeLog(QString("SingleInstance: failed to bind/listen unix socket %1: %2").arg(unixSocketPath).arg(QString::fromUtf8(strerror(errno))));
            }
        } else {
            writeLog(QString("SingleInstance: failed to create unix socket: %1").arg(QString::fromUtf8(strerror(errno))));
        }
    }

    qInfo() << "SingleInstance: primary instance running";
    return false;
}
