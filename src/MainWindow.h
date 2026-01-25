#pragma once

#include <QMainWindow>
#include "AudioEngine.h"
#include <QString>
#include <vector>

class QTabWidget;
class CustomTabWidget;
class SoundContainer;
class QKeyEvent;
class QWidget;
class KeepAliveMonitor;
class QLabel;

/**
 * Main application window. Contains the menu and central grid layout.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum class SavePromptResult { SaveSession, DiscardChanges, Cancel };

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void onPlayRequested(const QString& path, SoundContainer* src);
    void onSwapRequested(SoundContainer* src, SoundContainer* dst);
    void onCopyRequested(SoundContainer* src, SoundContainer* dst);
    void onClearRequested(SoundContainer* sc);
    void onTabMoved(int from, int to);
    void onTabOrderChanged();
    void saveLayout();
    void restoreLayout();
    void onKeepAliveTriggered();
    void restartAudioEngineWithPreferences(const QString& oldClientName = QString());

    // Session I/O
    void saveSessionAs(const QString& filePath);
    void loadSession(const QString& filePath);

    // Dirty state tracking
    bool isSessionDirty() const { return m_sessionDirty; }
    SavePromptResult promptToSaveIfDirty();

    // KeepAliveMonitor integration accessors
    KeepAliveMonitor* getKeepAliveMonitor() const;
    AudioEngine* getAudioEngine();

    // Grid layout preferences
    int gridRows() const { return m_gridRows; }
    int gridCols() const { return m_gridCols; }
    SoundContainer* containerAt(int tab, int index) const;
    int containerCountForTab(int tab) const;

public slots:
    void onGridDimensionsChanged(int rows, int cols);

    // Public method for preferences dialog to test audio playback
    // Parameters: overrideVolume, targetTab, targetSlot, isSpecificSlot, useSlotVolume
    void playTestSound(float overrideVolume, int targetTab = 0, int targetSlot = 0, bool isSpecificSlot = false, bool useSlotVolume = true);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void performUndo();
    void performRedo();

private slots:
    void onNewSession();
    void onOpen();
    void onSaveSession();
    void onSaveSessionAs();
    void onLoadRecentSession();
    void onClearRecentSessions();
    void onSessionModified();

    // Session management helpers
    void handleCloseEvent();     // Handle window close event

private:
    struct Operation {
        enum Type { Rename, Swap, CopyReplace, Clear, TabMove } type;
        // Rename data
        int index = -1;
        QString oldName;
        QString newName;
        // Tab move data
        int moveFrom = -1;
        int moveTo = -1;
        QWidget* movePage = nullptr;
        // Swap data
        int tab = -1;
        int srcIdx = -1;
        int dstIdx = -1;
        // CopyReplace data
        QString prevFile;
        float prevVolume = 1.0f;
        // previous backdrop color stored as ARGB (QColor::rgba()), and flag if present
        quint32 prevBackdrop = 0;
        bool hadBackdrop = false;
        QString newFile;
        float newVolume = 1.0f;
    };
    // Per-operation undo/redo helpers
    void undoTabMove(const Operation& op);
    void redoTabMove(const Operation& op);
    void undoSwap(const Operation& op);
    void redoSwap(const Operation& op);
    void undoClear(const Operation& op);
    void redoClear(const Operation& op);
    void undoRenameOp(const Operation& op);
    void redoRenameOp(const Operation& op);
    std::vector<Operation> m_undoStack;
    std::vector<Operation> m_redoStack;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

private:
    AudioEngine m_audioEngine;
    CustomTabWidget* m_tabs = nullptr;
    int m_gridRows = 4;
    int m_gridCols = 8;
    // containers[tabIndex][slotIndex]
    std::vector<std::vector<SoundContainer*>> m_containers;
    QString m_layoutPath;
    QString m_currentSessionPath;
    QMenu* m_recentMenu = nullptr;
    bool m_sessionDirty = false;
    KeepAliveMonitor* m_keepAliveMonitor = nullptr;
    QLabel* m_keepAliveStatusLabel = nullptr;
    void applyKeepAlivePreferences();
    bool playAudioFile(const QString& path, SoundContainer* src, float volumeOverride, bool useOverrideVolume);
    void updateRecentSessionsMenu();
    void updateWindowTitle();
    void markSessionDirty();
    
    void syncContainersWithUi();
    void writeDebugLog(const QString& msg);
    void initializeKeepAliveMonitor();
};

