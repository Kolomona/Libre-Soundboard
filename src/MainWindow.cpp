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

#include "SoundContainer.h"
#include "AudioEngine.h"
#include "AudioFile.h"
#include "CustomTabBar.h"
#include "CustomTabWidget.h"


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Try to initialize the audio engine (JACK)
    if (!m_audioEngine.init()) {
        statusBar()->showMessage(tr("JACK not available; audio disabled"), 5000);
    } else {
        statusBar()->showMessage(tr("Connected to JACK"), 2000);
    }

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
    m_undoAction = editMenu->addAction(tr("Undo"), this, &MainWindow::undoRename);
    m_redoAction = editMenu->addAction(tr("Redo"), this, &MainWindow::redoRename);
    // Set shortcuts: Ctrl+Z for Undo, Shift+Ctrl+Z for Redo
    if (m_undoAction) m_undoAction->setShortcut(QKeySequence("Ctrl+Z"));
    if (m_redoAction) m_redoAction->setShortcut(QKeySequence("Shift+Ctrl+Z"));
    editMenu->addSeparator();
    editMenu->addAction(tr("Preferences"));
    // Initially disabled until a rename occurs
    if (m_undoAction) m_undoAction->setEnabled(false);
    if (m_redoAction) m_redoAction->setEnabled(false);

    auto helpMenu = menuBar()->addMenu(tr("Help"));
    helpMenu->addAction(tr("About"));

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

        m_containers[t].reserve(rows * cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                auto* sc = new SoundContainer(tab);
                layout->addWidget(sc, r, c);
                m_containers[t].push_back(sc);
                connect(sc, &SoundContainer::playRequested, this, &MainWindow::onPlayRequested);
                connect(sc, &SoundContainer::swapRequested, this, &MainWindow::onSwapRequested);
                connect(sc, &SoundContainer::copyRequested, this, &MainWindow::onCopyRequested);
                connect(sc, &SoundContainer::fileChanged, this, [this](const QString& p){ statusBar()->showMessage(p, 2000); });
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
            if (m_undoAction) menu.addAction(m_undoAction->text(), this, &MainWindow::undoRename)->setEnabled(!m_undoStack.empty());
            if (m_redoAction) menu.addAction(m_redoAction->text(), this, &MainWindow::redoRename)->setEnabled(!m_redoStack.empty());
            menu.exec(bar->mapToGlobal(pt));
        });
    }

    setCentralWidget(m_tabs);
    // After UI is constructed, restore previous layout if any
    restoreLayout();
    setWindowTitle(tr("LibreSoundboard"));
    resize(900, 600);
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
    if (!m_audioEngine.playBuffer(samples, sr, ch, path.toStdString(), vol)) {
        statusBar()->showMessage(tr("Playback failed (JACK?)"), 3000);
    } else {
        statusBar()->showMessage(tr("Playing: %1").arg(path), 2000);
    }
}

void MainWindow::onSwapRequested(SoundContainer* src, SoundContainer* dst)
{
    if (!src || !dst) return;
    int srcTab = -1, srcIdx = -1, dstTab = -1, dstIdx = -1;
    for (int t = 0; t < (int)m_containers.size(); ++t) {
        for (int i = 0; i < (int)m_containers[t].size(); ++i) {
            if (m_containers[t][i] == src) { srcTab = t; srcIdx = i; }
            if (m_containers[t][i] == dst) { dstTab = t; dstIdx = i; }
        }
    }
    if (srcTab == -1 || dstTab == -1) return;

    // swap in data structure
    std::swap(m_containers[srcTab][srcIdx], m_containers[dstTab][dstIdx]);

    // update layouts for both tabs
    auto updateWidgetPos = [this](int tab, int idx){
        QWidget* tabWidget = m_tabs->widget(tab);
        if (!tabWidget) return;
        QGridLayout* layout = qobject_cast<QGridLayout*>(tabWidget->layout());
        if (!layout) return;
        const int cols = 8;
        int r = idx / cols; int c = idx % cols;
        layout->addWidget(m_containers[tab][idx], r, c);
    };
    updateWidgetPos(srcTab, srcIdx);
    updateWidgetPos(dstTab, dstIdx);

    // record operation for undo/redo
    Operation op;
    op.type = Operation::Swap;
    // For simplicity record on source tab (if different tabs, store srcTab and indices)
    op.tab = srcTab;
    op.srcIdx = srcIdx;
    op.dstIdx = dstIdx;
    m_undoStack.push_back(op);
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);
}

void MainWindow::onCopyRequested(SoundContainer* src, SoundContainer* dst)
{
    if (!src || !dst) return;
    int srcTab = -1, srcIdx = -1, dstTab = -1, dstIdx = -1;
    for (int t = 0; t < (int)m_containers.size(); ++t) {
        for (int i = 0; i < (int)m_containers[t].size(); ++i) {
            if (m_containers[t][i] == src) { srcTab = t; srcIdx = i; }
            if (m_containers[t][i] == dst) { dstTab = t; dstIdx = i; }
        }
    }
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
        dst->setVolume(0.8f);
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
    m_redoStack.clear();
    if (m_undoAction) m_undoAction->setEnabled(true);
    if (m_redoAction) m_redoAction->setEnabled(false);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!event) return;
    // Esc -> stop all audio
    if (event->key() == Qt::Key_Escape) {
        m_audioEngine.stopAll();
        statusBar()->showMessage(tr("Stopped all audio"), 1000);
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

void MainWindow::undoRename()
{
    if (m_undoStack.empty()) return;
    Operation op = m_undoStack.back();
    m_undoStack.pop_back();
    if (op.type == Operation::Rename) {
        if (op.index >= 0 && op.index < m_tabs->count()) {
            m_tabs->setTabText(op.index, op.oldName);
            m_redoStack.push_back(op);
        }
    } else if (op.type == Operation::Swap) {
        // swap back
        if (op.tab >= 0 && op.tab < (int)m_containers.size()) {
            int s = op.srcIdx; int d = op.dstIdx;
            if (s >= 0 && s < (int)m_containers[op.tab].size() && d >= 0 && d < (int)m_containers[op.tab].size()) {
                // perform swap in data
                std::swap(m_containers[op.tab][s], m_containers[op.tab][d]);
                // update layouts
                QWidget* tabWidget = m_tabs->widget(op.tab);
                if (tabWidget) {
                    QGridLayout* layout = qobject_cast<QGridLayout*>(tabWidget->layout());
                    if (layout) {
                        int cols = 8; // same layout grid used when creating
                        int rs = s / cols; int cs = s % cols;
                        int rd = d / cols; int cd = d % cols;
                        layout->addWidget(m_containers[op.tab][s], rs, cs);
                        layout->addWidget(m_containers[op.tab][d], rd, cd);
                    }
                }
                m_redoStack.push_back(op);
            }
        }
    }
    if (m_undoAction) m_undoAction->setEnabled(!m_undoStack.empty());
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.empty());
}

void MainWindow::redoRename()
{
    if (m_redoStack.empty()) return;
    Operation op = m_redoStack.back();
    m_redoStack.pop_back();
    if (op.type == Operation::Rename) {
        if (op.index >= 0 && op.index < m_tabs->count()) {
            m_tabs->setTabText(op.index, op.newName);
            m_undoStack.push_back(op);
        }
    } else if (op.type == Operation::Swap) {
        if (op.tab >= 0 && op.tab < (int)m_containers.size()) {
            int s = op.srcIdx; int d = op.dstIdx;
            if (s >= 0 && s < (int)m_containers[op.tab].size() && d >= 0 && d < (int)m_containers[op.tab].size()) {
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
                m_undoStack.push_back(op);
            }
        }
    }
    if (m_undoAction) m_undoAction->setEnabled(!m_undoStack.empty());
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.empty());
}
