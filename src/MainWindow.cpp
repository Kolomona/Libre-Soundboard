#include "MainWindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QStatusBar>

#include <QMessageBox>
#include <QTabWidget>
#include <QScrollArea>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QTabBar>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QShortcut>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QCoreApplication>
#include <unistd.h>
#include <QTimer>

#include "SoundContainer.h"
#include "AudioEngine.h"
#include "KeepAliveMonitor.h"
#include "PlayheadManager.h"
#include "AudioFile.h"
#include "WaveformCache.h"
#include "CustomTabBar.h"
#include "CustomTabWidget.h"
#include "PreferencesDialog.h"
#include "PreferencesManager.h"
#include "DebugLog.h"

#include <QDateTime>
#include <QFile>
#include <QCoreApplication>
#include <unistd.h>


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Apply log level from preferences on startup
    DebugLog::setLevel(static_cast<int>(PreferencesManager::instance().logLevel()));

    // Try to initialize the audio engine (JACK)
    if (!m_audioEngine.init()) {
        statusBar()->showMessage(tr("JACK not available; audio disabled"), 5000);
    } else {
        statusBar()->showMessage(tr("Connected to JACK"), 2000);
    }

    // Initialize centralized playhead manager with audio engine
    PlayheadManager::instance()->init(&m_audioEngine);

    // Menu
    auto fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(tr("Open"));
    fileMenu->addAction(tr("Save"));
    fileMenu->addSeparator();
    // Quit action with confirmation dialog
    QAction* quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, this, [this]() {
        // show a confirmation dialog; pressing Ctrl+Q while this dialog is open will also quit
        QMessageBox dlg(QMessageBox::Warning, tr("Quit"), tr("Are you sure?"), QMessageBox::NoButton, this);
        auto quitBtn = dlg.addButton(tr("Quit"), QMessageBox::AcceptRole);
        dlg.addButton(tr("Cancel"), QMessageBox::RejectRole);
        // make a local shortcut that targets the dialog so Ctrl+Q will accept
        QShortcut sc(QKeySequence("Ctrl+Q"), &dlg);
        sc.setContext(Qt::ApplicationShortcut);
        QObject::connect(&sc, &QShortcut::activated, &dlg, &QDialog::accept);
        int res = dlg.exec();
        QAbstractButton* clicked = dlg.clickedButton();
        // Quit only if the Quit button was clicked, or if the dialog was accepted
        // via keyboard (no clicked button). This avoids quitting when Cancel is clicked.
        bool acceptedByButton = (clicked && dlg.buttonRole(clicked) == QMessageBox::AcceptRole);
        bool acceptedByKeyboard = (!clicked && res == QDialog::Accepted);
        if (acceptedByButton || acceptedByKeyboard) {
            qApp->quit();
        }
    });

    auto editMenu = menuBar()->addMenu(tr("Edit"));
    m_undoAction = editMenu->addAction(tr("Undo"), this, &MainWindow::performUndo);
    m_redoAction = editMenu->addAction(tr("Redo"), this, &MainWindow::performRedo);
    // Set shortcuts: Ctrl+Z for Undo, Shift+Ctrl+Z for Redo
    if (m_undoAction) m_undoAction->setShortcut(QKeySequence("Ctrl+Z"));
    if (m_redoAction) m_redoAction->setShortcut(QKeySequence("Shift+Ctrl+Z"));
    editMenu->addSeparator();
    QAction* prefsAction = editMenu->addAction(tr("Preferences"));
    QObject::connect(prefsAction, &QAction::triggered, this, [this]() {
        PreferencesDialog dlg(this);
        dlg.exec();
    });
    // Initially disabled until a rename occurs
    if (m_undoAction) m_undoAction->setEnabled(false);
    if (m_redoAction) m_redoAction->setEnabled(false);

    auto helpMenu = menuBar()->addMenu(tr("Help"));
    helpMenu->addAction(tr("About"));

    // Debug submenu under Help: expose cache clear and manual eviction for testing
    QMenu* debugMenu = helpMenu->addMenu(tr("Debug"));
    debugMenu->addAction(tr("Clear waveform cache"), this, [this]() {
        WaveformCache::clearAll();
        statusBar()->showMessage(tr("Waveform cache cleared"), 2000);
    });
    debugMenu->addAction(tr("Evict waveform cache now"), this, [this]() {
        // run eviction with default limits
        WaveformCache::evict();
        statusBar()->showMessage(tr("Waveform cache eviction complete"), 2000);
    });

    // Create tabs, each with 32 SoundContainers (4 rows x 8 cols)
    #include "CustomTabBar.h"
    m_tabs = new CustomTabWidget(this);
    // Use a custom tab bar that provides a ghost pixmap during drag and an animation when tabs move
    m_tabs->setTabBarPublic(new CustomTabBar(m_tabs));
    const int tabCount = 4;
    const int rows = 4;
    const int cols = 8;
    m_containers.resize(tabCount);

    for (int t = 0; t < tabCount; ++t) {
        QWidget* tab = new QWidget(m_tabs);
        auto* layout = new QGridLayout(tab);
        layout->setSpacing(8);
        // Ensure columns and rows stretch equally so each SoundContainer
        // receives an equal share of available width/height when the
        // main window is resized. Also allow columns to shrink to zero
        // minimum so the grid can fit smaller displays.
        for (int cc = 0; cc < cols; ++cc) {
            layout->setColumnStretch(cc, 1);
            layout->setColumnMinimumWidth(cc, 0);
        }
        for (int rr = 0; rr < rows; ++rr) {
            layout->setRowStretch(rr, 1);
            layout->setRowMinimumHeight(rr, 0);
        }

        m_containers[t].reserve(rows * cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                auto* sc = new SoundContainer(tab);
                // Let each SoundContainer expand so the grid fills the window
                // and keeps equal widths across columns. Also clear any
                // container minimum so columns can shrink evenly to fit.
                sc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                sc->setMinimumWidth(0);
                sc->setMinimumHeight(0);
                // Apply default gain from preferences on creation
                sc->setVolume(static_cast<float>(PreferencesManager::instance().defaultGain()));
                layout->addWidget(sc, r, c);
                m_containers[t].push_back(sc);
                connect(sc, &SoundContainer::playRequested, this, &MainWindow::onPlayRequested);
                connect(sc, &SoundContainer::swapRequested, this, &MainWindow::onSwapRequested);
                connect(sc, &SoundContainer::copyRequested, this, &MainWindow::onCopyRequested);
                connect(sc, &SoundContainer::fileChanged, this, [this](const QString& p){ statusBar()->showMessage(p, 2000); });
                connect(sc, &SoundContainer::clearRequested, this, &MainWindow::onClearRequested);
                // Update active voice gain when the slider changes
                connect(sc, &SoundContainer::volumeChanged, this, [this, sc](float v){
                    if (sc && !sc->file().isEmpty()) {
                        m_audioEngine.setVoiceGainById(sc->file().toStdString(), v);
                    }
                });

                // Right-click on play button stops the audio for this container
                connect(sc, &SoundContainer::stopRequested, this, [this](const QString& path, SoundContainer* sc){
                    Q_UNUSED(sc);
                    if (!path.isEmpty()) {
                        m_audioEngine.stopVoicesById(path.toStdString());
                        // notify playhead manager that playback stopped (for simulated case)
                        PlayheadManager::instance()->playbackStopped(path, sc);
                        statusBar()->showMessage(tr("Stopped: %1").arg(path), 1000);
                    }
                });
            }
        }

        tab->setLayout(layout);
        m_tabs->addTab(tab, tr("Board %1").arg(t + 1));
    }

    // Allow renaming tabs by double-clicking their title (inline editor)
    if (m_tabs->tabBar()) {
        m_tabs->tabBar()->installEventFilter(this);
        // Show context menu on tab right-click for Rename/Undo/Redo
        m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, [this](const QPoint& pt){
            QTabBar* bar = m_tabs->tabBar();
            int idx = bar->tabAt(pt);
            QMenu menu(bar);
            if (idx >= 0) {
                menu.addAction(tr("Rename"), this, [this, idx, bar]() {
                    // reuse inline editor logic
                    QRect r = bar->tabRect(idx);
                    QLineEdit* editor = new QLineEdit(bar);
                    editor->setText(m_tabs->tabText(idx));
                    QRect er = r.adjusted(4, 2, -4, -2);
                    editor->setGeometry(er);
                    editor->show();
                    editor->setFocus(Qt::OtherFocusReason);
                    editor->selectAll();
                    connect(editor, &QLineEdit::editingFinished, this, [this, editor, idx]() {
                        QString text = editor->text().trimmed();
                        if (!text.isEmpty() && text != m_tabs->tabText(idx)) {
                            // push undo as Operation
                            Operation op;
                            op.type = Operation::Rename;
                            op.index = idx;
                            op.oldName = m_tabs->tabText(idx);
                            op.newName = text;
                            m_undoStack.push_back(op);
                            m_redoStack.clear();
                            m_tabs->setTabText(idx, text);
                            if (m_undoAction) m_undoAction->setEnabled(true);
                            if (m_redoAction) m_redoAction->setEnabled(false);
                        }
                        editor->deleteLater();
                    });
                });
            }
            // Always offer Undo/Redo in the tab context menu
            if (m_undoAction) menu.addAction(m_undoAction->text(), this, &MainWindow::performUndo)->setEnabled(!m_undoStack.empty());
            if (m_redoAction) menu.addAction(m_redoAction->text(), this, &MainWindow::performRedo)->setEnabled(!m_redoStack.empty());
            menu.exec(bar->mapToGlobal(pt));
        });
        // Keep internal container mapping in-sync when tabs are reordered
        connect(m_tabs->tabBar(), &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
        // Also watch for explicit tab-order changes emitted by CustomTabBar
        CustomTabBar* cbar = qobject_cast<CustomTabBar*>(m_tabs->tabBar());
        if (cbar) connect(cbar, &CustomTabBar::tabOrderChanged, this, &MainWindow::onTabOrderChanged);
    }

    setCentralWidget(m_tabs);
    // After UI is constructed, restore previous layout if any
    restoreLayout();
    
    // Initialize KeepAliveMonitor for input monitoring
    initializeKeepAliveMonitor();
    
    // Emit a startup debug line so we can confirm the debug logger is being invoked
    writeDebugLog(QString("MainWindow constructed pid=%1").arg(getpid()));
    setWindowTitle(tr("LibreSoundboard"));
    resize(900, 600);
    // Allow shrinking down to a usable minimum width while keeping layouts flexible.
    // This ensures users can resize the app down to ~500px when desired.
    if (centralWidget()) centralWidget()->setMinimumWidth(0);
    setMinimumWidth(500);
}

void MainWindow::onTabMoved(int from, int to)
{
    if (from == to) return;
    writeDebugLog(QString("onTabMoved called from=%1 to=%2 pageFrom=%3 pageTo=%4")
                  .arg(from).arg(to)
                  .arg(reinterpret_cast<uintptr_t>(m_tabs->widget(from)))
                  .arg(reinterpret_cast<uintptr_t>(m_tabs->widget(to))));
    // record operation for undo/redo: user moved tab from->to
    Operation op;
    op.type = Operation::TabMove;
    op.moveFrom = from;
    op.moveTo = to;
    // record the widget pointer for the moved page from the original source index
    op.movePage = m_tabs->widget(from);
    // avoid duplicate consecutive TabMove entries for the same page/move
    if (m_undoStack.empty() || !(m_undoStack.back().type == Operation::TabMove && m_undoStack.back().movePage == op.movePage && m_undoStack.back().moveTo == op.moveTo)) {
        m_undoStack.push_back(op);
        writeDebugLog(QString("pushed TabMove op movePage=%1 from=%2 to=%3 stackSize=%4")
                      .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(op.moveFrom).arg(op.moveTo).arg(m_undoStack.size()));
    } else {
        writeDebugLog(QString("skipped duplicate TabMove for movePage=%1 to=%2")
                      .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(op.moveTo));
    }
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);
    if (from < 0 || from >= (int)m_containers.size()) return;
    if (to < 0) to = 0;
    if (to >= (int)m_containers.size()) to = static_cast<int>(m_containers.size()) - 1;

    // Move the vector element at 'from' to position 'to'
    auto moving = std::move(m_containers[from]);
    if (from < to) {
        // shift left the range (from+1 .. to) down by one
        for (int i = from; i < to; ++i) m_containers[i] = std::move(m_containers[i + 1]);
        m_containers[to] = std::move(moving);
    } else {
        // from > to: shift right the range (to .. from-1) up by one
        for (int i = from; i > to; --i) m_containers[i] = std::move(m_containers[i - 1]);
        m_containers[to] = std::move(moving);
    }
    // ensure internal mapping matches UI after a tab move
    QTimer::singleShot(0, this, [this]() { syncContainersWithUi(); });
}

void MainWindow::onTabOrderChanged()
{
    // Compare previous page order to new one to detect which tab moved.
    // Capture previous page pointers
    QVector<QWidget*> prevPages;
    for (int i = 0; i < m_tabs->count(); ++i) prevPages.append(m_tabs->widget(i));

    // sync will rebuild m_containers based on current UI; but we need the UI order after the change.
    // Because CustomTabBar already modified the tab widget order, the widgets in m_tabs reflect new order now.
    // Find differences between prevPages (which actually reflect the previous ordering only if called before reorder)
    // To be robust, instead compute page pointers before calling syncContainersWithUi by inspecting stored m_containers mapping.
    QVector<QWidget*> oldOrder;
    for (size_t t = 0; t < m_containers.size(); ++t) {
        QWidget* w = m_tabs->widget(static_cast<int>(t));
        Q_UNUSED(w);
    }

    // Instead, build a vector of page widgets from the previous m_containers state by using index mapping.
    QVector<QWidget*> beforePages;
    for (int i = 0; i < m_tabs->count(); ++i) {
        // attempt to get the page widget pointer that was at index i before sync by using m_tabs->widget(i)
        // Note: CustomTabBar has already changed widget order; to detect movement we compare previous m_containers indices
        // with current m_tabs order: find a page in m_tabs whose contained first slot widget matches m_containers[i][0]
        QWidget* matched = nullptr;
        if (i < (int)m_containers.size() && !m_containers[i].empty()) {
            SoundContainer* firstSc = m_containers[i][0];
            if (firstSc) matched = firstSc->parentWidget();
        }
        beforePages.append(matched);
    }

    // Now compute new order: current m_tabs widgets
    QVector<QWidget*> afterPages;
    for (int i = 0; i < m_tabs->count(); ++i) afterPages.append(m_tabs->widget(i));

    // Find which page moved: look for a page pointer that shifted index
    int movedFrom = -1, movedTo = -1;
    QWidget* movedPage = nullptr;
    for (int oldIdx = 0; oldIdx < beforePages.size(); ++oldIdx) {
        QWidget* p = beforePages[oldIdx];
        if (!p) continue;
        int newIdx = afterPages.indexOf(p);
        if (newIdx >= 0 && newIdx != oldIdx) {
            movedFrom = oldIdx;
            movedTo = newIdx;
            movedPage = p;
            break;
        }
    }

    // Record operation if we detected a move
    if (movedPage) {
        Operation op;
        op.type = Operation::TabMove;
        op.moveFrom = movedFrom;
        op.moveTo = movedTo;
        op.movePage = movedPage;
        if (m_undoStack.empty() || !(m_undoStack.back().type == Operation::TabMove && m_undoStack.back().movePage == op.movePage && m_undoStack.back().moveTo == op.moveTo)) {
            m_undoStack.push_back(op);
            writeDebugLog(QString("onTabOrderChanged: detected move page=%1 from=%2 to=%3 stackSize=%4")
                          .arg(reinterpret_cast<uintptr_t>(movedPage)).arg(movedFrom).arg(movedTo).arg(m_undoStack.size()));
            // Maintain undo/redo action state consistently (same as onTabMoved)
            m_redoStack.clear();
            if (m_undoAction) m_undoAction->setEnabled(true);
            if (m_redoAction) m_redoAction->setEnabled(false);
        } else {
            writeDebugLog(QString("onTabOrderChanged: duplicate move skipped page=%1 to=%2").arg(reinterpret_cast<uintptr_t>(movedPage)).arg(movedTo));
        }
    } else {
        writeDebugLog(QString("onTabOrderChanged: no move detected"));
    }

    // Rebuild internal mapping
    syncContainersWithUi();
}

void MainWindow::syncContainersWithUi()
{
    Q_UNUSED(m_tabs);
    int tabCount = m_tabs->count();
    m_containers.clear();
    m_containers.resize(tabCount);
    for (int t = 0; t < tabCount; ++t) {
        QWidget* tabWidget = m_tabs->widget(t);
        if (!tabWidget) continue;
        QGridLayout* layout = qobject_cast<QGridLayout*>(tabWidget->layout());
        if (!layout) continue;
        // Collect widgets by row/col order. Assume same rows/cols used when building.
        int rows = 4, cols = 8; // keep in sync with creation
        m_containers[t].reserve(rows * cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                QLayoutItem* it = layout->itemAtPosition(r, c);
                if (it && it->widget()) m_containers[t].push_back(qobject_cast<SoundContainer*>(it->widget()));
                else m_containers[t].push_back(nullptr);
            }
        }
        // log the pointers for the tab
        QStringList ptrs;
        for (size_t i = 0; i < m_containers[t].size(); ++i) {
            auto sc = m_containers[t][i];
            ptrs << QString::number(reinterpret_cast<uintptr_t>(sc));
        }
        Q_UNUSED(ptrs);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // Ensure active tab's SoundContainers are set to equal widths so the
    // grid visually fills the available width exactly.
    int idx = m_tabs ? m_tabs->currentIndex() : -1;
    if (idx < 0 || idx >= (int)m_containers.size()) return;
    QWidget* page = m_tabs->widget(idx);
    if (!page) return;
    QLayout* l = page->layout();
    if (!l) return;
    const int cols = 8; // keep in sync with creation code
    int spacing = l->spacing();
    QMargins mg = l->contentsMargins();
    int avail = page->width() - mg.left() - mg.right();
    if (avail <= 0) return;
    // Let the layout stretching handle equal widths. Clear any per-container
    // max/min constraints so the window can shrink freely; layouts will
    // distribute available width equally because we set equal column stretches.
    for (int i = 0; i < (int)m_containers[idx].size(); ++i) {
        SoundContainer* sc = m_containers[idx][i];
        if (!sc) continue;
        sc->setMinimumWidth(0);
        sc->setMaximumWidth(QWIDGETSIZE_MAX);
        sc->updateGeometry();
    }
}

void MainWindow::writeDebugLog(const QString& msg)
{
    // Do not write debug lines to disk; emit to qDebug only
    qDebug().noquote() << msg;
}

MainWindow::~MainWindow()
{
    // Save layout before shutting down audio
    saveLayout();
    m_audioEngine.shutdown();
}

void MainWindow::onPlayRequested(const QString& path, SoundContainer* src)
{
    AudioFile af;
    if (!af.load(path)) {
        QMessageBox::warning(this, tr("Load Failed"), tr("Unable to load audio file."));
        return;
    }

    std::vector<float> samples;
    int sr = 0, ch = 0;
    if (!af.readAllSamples(samples, sr, ch)) {
        QMessageBox::warning(this, tr("Read Failed"), tr("Unable to decode audio file."));
        return;
    }

    // Use the file path as the voice id so repeated presses restart that sound
    float vol = 1.0f;
    if (src) vol = src->volume();
    // notify playhead manager that playback started (simulated fallback)
    PlayheadManager::instance()->playbackStarted(path, src);
    if (!m_audioEngine.playBuffer(samples, sr, ch, path.toStdString(), vol)) {
        statusBar()->showMessage(tr("Playback failed (JACK?)"), 3000);
    } else {
        statusBar()->showMessage(tr("Playing: %1").arg(path), 2000);
    }
}

void MainWindow::onSwapRequested(SoundContainer* src, SoundContainer* dst)
{
    if (!src || !dst) return;
    writeDebugLog(QString("onSwapRequested: src=%1 dst=%2").arg(reinterpret_cast<uintptr_t>(src)).arg(reinterpret_cast<uintptr_t>(dst)));
    // Determine the tab/page and slot index by querying the widget parent/layout
    auto findPos = [this](SoundContainer* sc) -> std::pair<int,int> {
        if (!sc) return {-1,-1};
        QWidget* page = sc->parentWidget();
        if (!page) { return {-1,-1}; }
        int tabIndex = m_tabs->indexOf(page);
        if (tabIndex < 0) return {-1,-1};
        QGridLayout* layout = qobject_cast<QGridLayout*>(page->layout());
        if (!layout) { return {tabIndex, -1}; }
        // find item's position
        int rows = 4, cols = 8;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                QLayoutItem* it = layout->itemAtPosition(r, c);
                if (it && it->widget() == sc) {
                    Q_UNUSED(sc); Q_UNUSED(page); Q_UNUSED(r); Q_UNUSED(c); Q_UNUSED(tabIndex); Q_UNUSED(layout);
                    return {tabIndex, r * cols + c};
                }
            }
        }
        Q_UNUSED(sc); Q_UNUSED(page); Q_UNUSED(tabIndex);
        return {tabIndex, -1};
    };

    auto sPos = findPos(src);
    auto dPos = findPos(dst);
    int srcTab = sPos.first, srcIdx = sPos.second;
    int dstTab = dPos.first, dstIdx = dPos.second;
    Q_UNUSED(src); Q_UNUSED(dst); Q_UNUSED(srcTab); Q_UNUSED(srcIdx); Q_UNUSED(dstTab); Q_UNUSED(dstIdx);
    if (srcTab == -1 || dstTab == -1 || srcIdx == -1 || dstIdx == -1) return;

    // swap in data structure: perform direct reparenting of the widgets
    QWidget* sPage = src->parentWidget();
    QWidget* dPage = dst->parentWidget();
    QGridLayout* sLayout = sPage ? qobject_cast<QGridLayout*>(sPage->layout()) : nullptr;
    QGridLayout* dLayout = dPage ? qobject_cast<QGridLayout*>(dPage->layout()) : nullptr;

    const int cols = 8;
    int sR = srcIdx / cols, sC = srcIdx % cols;
    int dR = dstIdx / cols, dC = dstIdx % cols;

    Q_UNUSED(sPage); Q_UNUSED(dPage); Q_UNUSED(sLayout); Q_UNUSED(dLayout); Q_UNUSED(sR); Q_UNUSED(sC); Q_UNUSED(dR); Q_UNUSED(dC);

    if (dLayout && src) {
        dLayout->addWidget(src, dR, dC);
    }
    if (sLayout && dst) {
        sLayout->addWidget(dst, sR, sC);
    }
    Q_UNUSED(srcTab); Q_UNUSED(srcIdx); Q_UNUSED(dstTab); Q_UNUSED(dstIdx);
    // Rebuild internal mapping from the UI to avoid stale index issues
    syncContainersWithUi();
    Q_UNUSED(src); Q_UNUSED(dst);

    // record operation for undo/redo
    Operation op;
    op.type = Operation::Swap;
    // For simplicity record on source tab (if different tabs, store srcTab and indices)
    op.tab = srcTab;
    op.srcIdx = srcIdx;
    op.dstIdx = dstIdx;
    m_undoStack.push_back(op);
    writeDebugLog(QString("pushed Swap op tab=%1 s=%2 d=%3 stackSize=%4").arg(op.tab).arg(op.srcIdx).arg(op.dstIdx).arg(m_undoStack.size()));
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);
}

void MainWindow::onCopyRequested(SoundContainer* src, SoundContainer* dst)
{
    if (!src || !dst) return;
    // find destination position dynamically
    auto findPos = [this](SoundContainer* sc) -> std::pair<int,int> {
        if (!sc) return {-1,-1};
        QWidget* page = sc->parentWidget();
        if (!page) return {-1,-1};
        int tabIndex = m_tabs->indexOf(page);
        if (tabIndex < 0) return {-1,-1};
        QGridLayout* layout = qobject_cast<QGridLayout*>(page->layout());
        if (!layout) return {tabIndex, -1};
        int rows = 4, cols = 8;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                QLayoutItem* it = layout->itemAtPosition(r, c);
                if (it && it->widget() == sc) return {tabIndex, r * cols + c};
            }
        }
        return {tabIndex, -1};
    };

    auto dPos = findPos(dst);
    int dstTab = dPos.first, dstIdx = dPos.second;
    Q_UNUSED(src); Q_UNUSED(dst); Q_UNUSED(dstTab); Q_UNUSED(dstIdx);
    if (dstTab == -1 || dstIdx == -1) return;

    // Save previous dst state for undo
    QString prevFile = dst->file();
    float prevVol = dst->volume();

    // Copy src -> dst (overwrite destination)
    if (!src->file().isEmpty()) {
        dst->setFile(src->file());
        dst->setVolume(src->volume());
    } else {
        // if source has no file, clear dest
        dst->setFile(QString());
        dst->setVolume(static_cast<float>(PreferencesManager::instance().defaultGain()));
    }

    // Record operation for undo/redo
    Operation op;
    op.type = Operation::CopyReplace;
    op.tab = dstTab;
    op.dstIdx = dstIdx;
    op.prevFile = prevFile;
    op.prevVolume = prevVol;
    op.newFile = dst->file();
    op.newVolume = dst->volume();
    m_undoStack.push_back(op);
    writeDebugLog(QString("pushed CopyReplace op tab=%1 idx=%2 prevFile=%3 prevVol=%4 newFile=%5 newVol=%6 stackSize=%7")
                  .arg(op.tab).arg(op.dstIdx).arg(op.prevFile).arg(op.prevVolume).arg(op.newFile).arg(op.newVolume).arg(m_undoStack.size()));
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);
    // Rebuild internal mapping from the UI to avoid stale index issues
    syncContainersWithUi();
}

void MainWindow::onClearRequested(SoundContainer* sc)
{
    if (!sc) return;
    // find the position of this container
    auto findPos = [this](SoundContainer* sc) -> std::pair<int,int> {
        if (!sc) return {-1,-1};
        QWidget* page = sc->parentWidget();
        if (!page) return {-1,-1};
        int tabIndex = m_tabs->indexOf(page);
        if (tabIndex < 0) return {-1,-1};
        QGridLayout* layout = qobject_cast<QGridLayout*>(page->layout());
        if (!layout) return {tabIndex, -1};
        int rows = 4, cols = 8;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                QLayoutItem* it = layout->itemAtPosition(r, c);
                if (it && it->widget() == sc) return {tabIndex, r * cols + c};
            }
        }
        return {tabIndex, -1};
    };

    auto pos = findPos(sc);
    int tab = pos.first, idx = pos.second;
    if (tab == -1 || idx == -1) return;

    // record previous state for undo
    Operation op;
    op.type = Operation::Clear;
    op.tab = tab;
    op.dstIdx = idx;
    op.prevFile = sc->file();
    op.prevVolume = sc->volume();
    // record previous backdrop color
    QColor prev = sc->backdropColor();
    if (prev.isValid()) {
        op.hadBackdrop = true;
        op.prevBackdrop = prev.rgba();
    } else {
        op.hadBackdrop = false;
        op.prevBackdrop = 0;
    }
    m_undoStack.push_back(op);
    writeDebugLog(QString("onClearRequested: sc=%1 tab=%2 idx=%3 prevFile=%4 prevVol=%5 stackSize=%6")
                  .arg(reinterpret_cast<uintptr_t>(sc)).arg(tab).arg(idx).arg(op.prevFile).arg(op.prevVolume).arg(m_undoStack.size()));
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);

    // perform clear
    sc->setFile(QString());
    sc->setVolume(static_cast<float>(PreferencesManager::instance().defaultGain()));
    // also clear any user-selected backdrop color for this slot
    sc->setBackdropColor(QColor());
    syncContainersWithUi();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!event) return;
    // Esc -> stop all audio
    if (event->key() == Qt::Key_Escape) {
        m_audioEngine.stopAll();
        statusBar()->showMessage(tr("Stopped all audio"), 1000);
        // Notify playhead manager to clear any UI playheads as audio stopped
        PlayheadManager::instance()->stopAll();
        return;
    }

    // Number keys 1-9 and 0 map to first tab's first 10 containers
    if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
        int n = 0;
        int key = event->key();
        if (key == Qt::Key_0) n = 9; else n = key - Qt::Key_1;
        if (n >= 0 && n < 10) {
            if (!m_containers.empty() && m_containers[0].size() > static_cast<size_t>(n)) {
                SoundContainer* sc = m_containers[0][n];
                if (sc && !sc->file().isEmpty()) {
                    onPlayRequested(sc->file(), sc);
                }
            }
        }
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::saveLayout()
{
    // Save tab layouts (file paths and volumes) to a JSON file under config
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/libresoundboard";
    QDir d(cfgDir);
    if (!d.exists()) d.mkpath(".");
    QString filePath = cfgDir + "/layout.json";
    QJsonArray tabsArr;
    for (auto &tabVec : m_containers) {
        QJsonArray slotArr;
        for (auto* sc : tabVec) {
            QJsonObject obj;
            obj["file"] = sc ? sc->file() : QString();
            obj["volume"] = sc ? sc->volume() : 1.0;
            if (sc && sc->backdropColor().isValid()) {
                // store as ARGB integer
                obj["backdrop"] = static_cast<double>(sc->backdropColor().rgba());
            }
            slotArr.append(obj);
        }
        tabsArr.append(slotArr);
    }
    // Persist tab titles along with slots/volumes
    QJsonArray titlesArr;
    for (int i = 0; i < m_tabs->count(); ++i) {
        titlesArr.append(m_tabs->tabText(i));
    }

    QJsonObject root;
    root["titles"] = titlesArr;
    root["tabs"] = tabsArr;
    QJsonDocument doc(root);
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(doc.toJson());
        f.close();
    }
}

void MainWindow::restoreLayout()
{
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/libresoundboard";
    QString filePath = cfgDir + "/layout.json";
    QFile f(filePath);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray data = f.readAll();
    f.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray tabsArr;
    QJsonArray titlesArr;
    if (doc.isObject()) {
        QJsonObject root = doc.object();
        if (root.contains("tabs") && root.value("tabs").isArray()) tabsArr = root.value("tabs").toArray();
        if (root.contains("titles") && root.value("titles").isArray()) titlesArr = root.value("titles").toArray();
    } else if (doc.isArray()) {
        // legacy format: root is just the tabs array
        tabsArr = doc.array();
    } else {
        return;
    }

    // Restore tab titles if present
    for (int i = 0; i < (int)titlesArr.size() && i < m_tabs->count(); ++i) {
        if (titlesArr[i].isString()) {
            m_tabs->setTabText(i, titlesArr[i].toString());
        }
    }

    int t = 0;
    for (auto tabVal : tabsArr) {
        if (t >= (int)m_containers.size()) break;
        if (!tabVal.isArray()) { ++t; continue; }
        QJsonArray slotArr = tabVal.toArray();
        int s = 0;
            for (auto slotVal : slotArr) {
            if (s >= (int)m_containers[t].size()) break;
            if (!slotVal.isObject()) { ++s; continue; }
            QJsonObject obj = slotVal.toObject();
            QString path = obj.value("file").toString();
            double vol = obj.value("volume").toDouble(1.0);
            SoundContainer* sc = m_containers[t][s];
            // restore backdrop color if present
            if (sc && obj.contains("backdrop")) {
                quint32 rgba = static_cast<quint32>(static_cast<uint64_t>(obj.value("backdrop").toDouble()));
                QColor c = QColor::fromRgba(rgba);
                sc->setBackdropColor(c);
            }
            if (sc) {
                if (!path.isEmpty()) sc->setFile(path);
                sc->setVolume(static_cast<float>(vol));
            }
            ++s;
        }
        ++t;
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_tabs->tabBar() && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        int idx = m_tabs->tabBar()->tabAt(me->pos());
        if (idx >= 0) {
            // Create inline editor over the tab label
            QTabBar* bar = m_tabs->tabBar();
            QRect r = bar->tabRect(idx);
            QLineEdit* editor = new QLineEdit(bar);
            editor->setText(m_tabs->tabText(idx));
            // Slight padding
            QRect er = r.adjusted(4, 2, -4, -2);
            editor->setGeometry(er);
            editor->show();
            editor->setFocus(Qt::OtherFocusReason);
            editor->selectAll();

            // When editing finished, apply the new name
            connect(editor, &QLineEdit::editingFinished, this, [this, editor, idx]() {
                QString text = editor->text().trimmed();
                if (!text.isEmpty() && text != m_tabs->tabText(idx)) {
                    // record rename op for undo
                    Operation op;
                    op.type = Operation::Rename;
                    op.index = idx;
                    op.oldName = m_tabs->tabText(idx);
                    op.newName = text;
                    m_undoStack.push_back(op);
                    m_redoStack.clear();
                    m_tabs->setTabText(idx, text);
                    if (m_undoAction) m_undoAction->setEnabled(true);
                    if (m_redoAction) m_redoAction->setEnabled(false);
                }
                editor->deleteLater();
            });

            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::performUndo()
{
    writeDebugLog(QString("performUndo called stackSize=%1").arg(m_undoStack.size()));
    if (m_undoStack.empty()) return;
    Operation op = m_undoStack.back();
    m_undoStack.pop_back();
    switch (op.type) {
    case Operation::Rename: undoRenameOp(op); break;
    case Operation::Swap: undoSwap(op); break;
    case Operation::CopyReplace: /* copy/replace undo handled by same as clear/copy logic */ undoSwap(op); break;
    case Operation::Clear: undoClear(op); break;
    case Operation::TabMove: undoTabMove(op); break;
    default: break;
    }
    if (m_undoAction) m_undoAction->setEnabled(!m_undoStack.empty());
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.empty());
}

void MainWindow::undoTabMove(const Operation& op)
{
    if (!op.movePage) return;
    int curFrom = m_tabs->indexOf(op.movePage);
    int curTo = op.moveFrom;
    writeDebugLog(QString("undo TabMove: movePage=%1 curFrom=%2 curTo=%3")
                  .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(curFrom).arg(curTo));
    if (curFrom >= 0 && curTo >= 0 && curTo < m_tabs->count()) {
        QWidget* page = op.movePage;
        QString txt = m_tabs->tabText(curFrom);
        QIcon ic = m_tabs->tabIcon(curFrom);
        m_tabs->removeTab(curFrom);
        m_tabs->insertTab(curTo, page, ic, txt);
        auto moving = std::move(m_containers[curFrom]);
        if (curFrom < curTo) {
            for (int i = curFrom; i < curTo; ++i) m_containers[i] = std::move(m_containers[i + 1]);
            m_containers[curTo] = std::move(moving);
        } else {
            for (int i = curFrom; i > curTo; --i) m_containers[i] = std::move(m_containers[i - 1]);
            m_containers[curTo] = std::move(moving);
        }
        m_redoStack.push_back(op);
        writeDebugLog(QString("undo TabMove done: moved page=%1 to %2")
                      .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(curTo));
    }
}

void MainWindow::undoSwap(const Operation& op)
{
    if (op.tab < 0 || op.tab >= (int)m_containers.size()) return;
    int s = op.srcIdx; int d = op.dstIdx;
    if (s < 0 || d < 0) return;
    if (s >= (int)m_containers[op.tab].size() || d >= (int)m_containers[op.tab].size()) return;
    writeDebugLog(QString("undo Swap: tab=%1 s=%2 d=%3").arg(op.tab).arg(s).arg(d));
    std::swap(m_containers[op.tab][s], m_containers[op.tab][d]);
    QWidget* tabWidget = m_tabs->widget(op.tab);
    if (tabWidget) {
        QGridLayout* layout = qobject_cast<QGridLayout*>(tabWidget->layout());
        if (layout) {
            int cols = 8;
            int rs = s / cols; int cs = s % cols;
            int rd = d / cols; int cd = d % cols;
            layout->addWidget(m_containers[op.tab][s], rs, cs);
            layout->addWidget(m_containers[op.tab][d], rd, cd);
        }
    }
    writeDebugLog(QString("undo Swap done: tab=%1 s=%2 d=%3").arg(op.tab).arg(s).arg(d));
    m_redoStack.push_back(op);
}

void MainWindow::undoClear(const Operation& op)
{
    if (op.tab < 0 || op.tab >= (int)m_containers.size()) return;
    int idx = op.dstIdx;
    if (idx < 0 || idx >= (int)m_containers[op.tab].size()) return;
    SoundContainer* sc = m_containers[op.tab][idx];
    if (!sc) return;
    sc->setFile(op.prevFile);
    sc->setVolume(op.prevVolume);
    // restore previous backdrop color if any
    if (op.hadBackdrop) sc->setBackdropColor(QColor::fromRgba(op.prevBackdrop));
    else sc->setBackdropColor(QColor());
    m_redoStack.push_back(op);
}

void MainWindow::undoRenameOp(const Operation& op)
{
    if (op.index >= 0 && op.index < m_tabs->count()) {
        m_tabs->setTabText(op.index, op.oldName);
        m_redoStack.push_back(op);
    }
}

void MainWindow::performRedo()
{
    writeDebugLog(QString("performRedo called stackSize=%1").arg(m_redoStack.size()));
    if (m_redoStack.empty()) return;
    Operation op = m_redoStack.back();
    m_redoStack.pop_back();
    switch (op.type) {
    case Operation::Rename: redoRenameOp(op); break;
    case Operation::Swap: redoSwap(op); break;
    case Operation::CopyReplace: /* treat as swap/copy */ redoSwap(op); break;
    case Operation::Clear: redoClear(op); break;
    case Operation::TabMove: redoTabMove(op); break;
    default: break;
    }
    if (m_undoAction) m_undoAction->setEnabled(!m_undoStack.empty());
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.empty());
}

void MainWindow::redoTabMove(const Operation& op)
{
    if (!op.movePage) return;
    int curFrom = m_tabs->indexOf(op.movePage);
    int curTo = op.moveTo;
    writeDebugLog(QString("redo TabMove: movePage=%1 curFrom=%2 curTo=%3")
                  .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(curFrom).arg(curTo));
    if (curFrom >= 0 && curTo >= 0 && curTo < m_tabs->count()) {
        QWidget* page = op.movePage;
        QString txt = m_tabs->tabText(curFrom);
        QIcon ic = m_tabs->tabIcon(curFrom);
        m_tabs->removeTab(curFrom);
        m_tabs->insertTab(curTo, page, ic, txt);
        auto moving = std::move(m_containers[curFrom]);
        if (curFrom < curTo) {
            for (int i = curFrom; i < curTo; ++i) m_containers[i] = std::move(m_containers[i + 1]);
            m_containers[curTo] = std::move(moving);
        } else {
            for (int i = curFrom; i > curTo; --i) m_containers[i] = std::move(m_containers[i - 1]);
            m_containers[curTo] = std::move(moving);
        }
        m_undoStack.push_back(op);
        writeDebugLog(QString("redo TabMove done: moved page=%1 to %2")
                      .arg(reinterpret_cast<uintptr_t>(op.movePage)).arg(curTo));
    }
}

void MainWindow::redoSwap(const Operation& op)
{
    if (op.tab < 0 || op.tab >= (int)m_containers.size()) return;
    int s = op.srcIdx; int d = op.dstIdx;
    if (s < 0 || d < 0) return;
    if (s >= (int)m_containers[op.tab].size() || d >= (int)m_containers[op.tab].size()) return;
    writeDebugLog(QString("redo Swap: tab=%1 s=%2 d=%3").arg(op.tab).arg(s).arg(d));
    std::swap(m_containers[op.tab][s], m_containers[op.tab][d]);
    QWidget* tabWidget = m_tabs->widget(op.tab);
    if (tabWidget) {
        QGridLayout* layout = qobject_cast<QGridLayout*>(tabWidget->layout());
        if (layout) {
            int cols = 8;
            int rs = s / cols; int cs = s % cols;
            int rd = d / cols; int cd = d % cols;
            layout->addWidget(m_containers[op.tab][s], rs, cs);
            layout->addWidget(m_containers[op.tab][d], rd, cd);
        }
    }
    writeDebugLog(QString("redo Swap done: tab=%1 s=%2 d=%3").arg(op.tab).arg(s).arg(d));
    m_undoStack.push_back(op);
}

void MainWindow::redoClear(const Operation& op)
{
    if (op.tab < 0 || op.tab >= (int)m_containers.size()) return;
    int idx = op.dstIdx;
    if (idx < 0 || idx >= (int)m_containers[op.tab].size()) return;
    SoundContainer* sc = m_containers[op.tab][idx];
    if (!sc) return;
    sc->setFile(QString());
    sc->setVolume(static_cast<float>(PreferencesManager::instance().defaultGain()));
    // clear backdrop color on redo of clear
    sc->setBackdropColor(QColor());
    m_undoStack.push_back(op);
}

void MainWindow::redoRenameOp(const Operation& op)
{
    if (op.index >= 0 && op.index < m_tabs->count()) {
        m_tabs->setTabText(op.index, op.newName);
        m_undoStack.push_back(op);
    }
}

void MainWindow::initializeKeepAliveMonitor()
{
    m_keepAliveMonitor = new KeepAliveMonitor();
    m_audioEngine.setKeepAliveMonitor(m_keepAliveMonitor);
    
    // Connect the keepAliveTriggered signal to our handler
    connect(m_keepAliveMonitor, &KeepAliveMonitor::keepAliveTriggered,
            this, &MainWindow::onKeepAliveTriggered);
    
    writeDebugLog("KeepAliveMonitor initialized");
}

KeepAliveMonitor* MainWindow::getKeepAliveMonitor() const
{
    return m_keepAliveMonitor;
}

AudioEngine* MainWindow::getAudioEngine()
{
    return &m_audioEngine;
}

void MainWindow::onKeepAliveTriggered()
{
    writeDebugLog("onKeepAliveTriggered: Keep-alive triggered");
    
    // Find the last tab (highest index)
    if (!m_tabs || m_tabs->count() == 0) {
        writeDebugLog("onKeepAliveTriggered: No tabs available");
        return;
    }
    
    int lastTabIndex = m_tabs->count() - 1;
    
    // Find the last sound container with a file in the last tab
    if (lastTabIndex < 0 || lastTabIndex >= (int)m_containers.size()) {
        writeDebugLog("onKeepAliveTriggered: Last tab index out of range");
        return;
    }
    
    auto& lastTabContainers = m_containers[lastTabIndex];
    
    // Search from the end backwards to find the last non-empty container
    SoundContainer* lastContainer = nullptr;
    for (auto it = lastTabContainers.rbegin(); it != lastTabContainers.rend(); ++it) {
        if (*it && !(*it)->file().isEmpty()) {
            lastContainer = *it;
            break;
        }
    }
    
    if (!lastContainer) {
        writeDebugLog("onKeepAliveTriggered: No loaded sounds in last tab");
        return;
    }
    
    // Trigger playback of this sound
    QString filePath = lastContainer->file();
    float volume = lastContainer->volume();
    
    writeDebugLog(QString("onKeepAliveTriggered: Playing '%1' at volume %2").arg(filePath).arg(volume));
    
    // Use the playRequested signal to trigger playback
    emit lastContainer->playRequested(filePath, lastContainer);
}
