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

/**
 * Main application window. Contains the menu and central grid layout.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
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

    // KeepAliveMonitor integration accessors
    KeepAliveMonitor* getKeepAliveMonitor() const;
    AudioEngine* getAudioEngine();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void performUndo();
    void performRedo();

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
    // containers[tabIndex][slotIndex]
    std::vector<std::vector<SoundContainer*>> m_containers;
    QString m_layoutPath;
    KeepAliveMonitor* m_keepAliveMonitor = nullptr;
    
    void syncContainersWithUi();
    void writeDebugLog(const QString& msg);
    void initializeKeepAliveMonitor();
};

