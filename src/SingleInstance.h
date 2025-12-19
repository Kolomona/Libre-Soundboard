#pragma once

#include <QObject>

class MainWindow;

// Try to notify an existing instance. Returns true if another instance was found and notified (caller should exit).
bool notifyExistingInstance();

// Start the single-instance server using the provided `mainWindow` for raise/activation callbacks.
// Returns false on failure to become primary (caller should exit).
bool startSingleInstanceServer(MainWindow* mainWindow);
