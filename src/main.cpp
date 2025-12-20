
#include <QApplication>
#include <QFile>
#include "MainWindow.h"
#include "SingleInstance.h"
#include <QDebug>
#include "DebugLog.h"

int main(int argc, char** argv)
{
    // If the env var LIBRE_WAVEFORM_DEBUG_LOG_PATH is set, enable file logging (opt-in)
    const char* dbgPath = getenv("LIBRE_WAVEFORM_DEBUG_LOG_PATH");
    if (dbgPath && dbgPath[0] != '\0') {
        DebugLog::install(QString::fromUtf8(dbgPath));
    }

    QApplication app(argc, argv);
    // Prefer SVG, fallback to PNG or ICO
    QIcon icon;
    if (QFile::exists(":/icons/icon.svg"))
        icon = QIcon(":/icons/icon.svg");
    else if (QFile::exists(":/icons/icon.png"))
        icon = QIcon(":/icons/icon.png");
    else if (QFile::exists(":/icons/icon-64.ico"))
        icon = QIcon(":/icons/icon-64.ico");
    if (!icon.isNull())
        app.setWindowIcon(icon);

    qInfo() << "main: starting single-instance check";
    // Single-instance: handled by helper in core library
    // First, try to notify an existing instance before constructing the main UI.
    if (notifyExistingInstance()) {
        return 0;
    }

    MainWindow w;
    if (!icon.isNull())
        w.setWindowIcon(icon);

    if (startSingleInstanceServer(&w)) {
        // Could not become primary instance; exit.
        return 0;
    }

    w.show();

    return app.exec();
}
