#pragma once

#include <QString>

class DebugLog {
public:
    // Install a Qt message handler that writes Qt messages to `path`.
    // If `path` is empty, debug/info messages are suppressed and warnings/errors go to stderr.
    static void install(const QString& path);
    static void uninstall();
};
