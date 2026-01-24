#pragma once

#include <QString>

class DebugLog {
public:
    // Install a Qt message handler that writes Qt messages to `path`.
    // If `path` is empty, debug/info messages are suppressed and warnings/errors go to stderr.
    static void install(const QString& path);
    static void uninstall();
    // Set log level: 0 Off, 1 Error, 2 Warning, 3 Info, 4 Debug
    static void setLevel(int level);
    static int level();
};
