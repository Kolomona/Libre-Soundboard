#pragma once

#include <QMainWindow>
#include "AudioEngine.h"
#include <QString>
#include <vector>

class QTabWidget;
class CustomTabWidget;
class SoundContainer;
class QKeyEvent;

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
    void saveLayout();
    void restoreLayout();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void undoRename();
    void redoRename();

private:
    struct Operation {
        enum Type { Rename, Swap, CopyReplace } type;
        // Rename data
        int index = -1;
        QString oldName;
        QString newName;
        // Swap data
        int tab = -1;
        int srcIdx = -1;
        int dstIdx = -1;
        // CopyReplace data
        QString prevFile;
        float prevVolume = 1.0f;
        QString newFile;
        float newVolume = 1.0f;
    };
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
};
