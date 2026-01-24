#include "PreferencesDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>

#include "PreferencesManager.h"
#include "PreferencesPages.h"

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    buildUi();
    connectSignals();
    resize(700, 450);
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::buildUi() {
    auto* root = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(root);

    // Split pane: tree on left, stacked pages on right
    auto* contentLayout = new QHBoxLayout();
    m_tree = new QTreeWidget(root);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setMinimumWidth(220);
    m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);

    m_stack = new QStackedWidget(root);

    QStringList names = PreferencesManager::categoryNames();
    // Create stub pages and populate tree/stack
    for (int i = 0; i < names.size(); ++i) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, names[i]);
        m_tree->addTopLevelItem(item);
        // Instantiate the appropriate page class
        PreferencesPage* page = nullptr;
        switch (i) {
            case 0: page = new PrefAudioEnginePage(m_stack); break;
            case 1: page = new PrefGridLayoutPage(m_stack); break;
            case 2: page = new PrefWaveformCachePage(m_stack); break;
            case 3: page = new PrefFileHandlingPage(m_stack); break;
            case 4: page = new PrefKeyboardShortcutsPage(m_stack); break;
            case 5: page = new PrefStartupPage(m_stack); break;
            case 6: page = new PrefDebugPage(m_stack); break;
            case 7: page = new PrefKeepAlivePage(m_stack); break;
            default: page = new PreferencesPage(m_stack); break;
        }
        // Empty pages show a neutral label for now
        auto* lbl = new QLabel(tr("%1 settings (coming soon)").arg(names[i]), page);
        lbl->setAlignment(Qt::AlignCenter);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->addWidget(lbl);
        pageLayout->addStretch();
        m_stack->addWidget(page);
    }
    m_tree->setCurrentItem(m_tree->topLevelItem(0));
    m_stack->setCurrentIndex(0);

    contentLayout->addWidget(m_tree);
    contentLayout->addWidget(m_stack, 1);
    mainLayout->addLayout(contentLayout, 1);

    // Buttons
    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch();
    m_btnReset = new QPushButton(tr("Reset"), root);
    m_btnReset->setObjectName("btnReset");
    m_btnCancel = new QPushButton(tr("Cancel"), root);
    m_btnCancel->setObjectName("btnCancel");
    m_btnSave = new QPushButton(tr("Save"), root);
    m_btnSave->setObjectName("btnSave");
    buttonsLayout->addWidget(m_btnReset);
    buttonsLayout->addWidget(m_btnCancel);
    buttonsLayout->addWidget(m_btnSave);
    mainLayout->addLayout(buttonsLayout);

    setLayout(mainLayout);
}

void PreferencesDialog::connectSignals() {
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current){
                if (!current) return;
                int idx = m_tree->indexOfTopLevelItem(current);
                if (idx >= 0 && idx < m_stack->count()) {
                    m_stack->setCurrentIndex(idx);
                }
            });
    // Cancel discards changes (no-op for now) and closes
    connect(m_btnCancel, &QPushButton::clicked, this, [this]() { reject(); });
    // Save applies page changes (no-op stubs for now) and closes
    connect(m_btnSave, &QPushButton::clicked, this, [this]() {
        // Iterate pages and call apply()
        for (int i = 0; i < m_stack->count(); ++i) {
            auto* page = qobject_cast<PreferencesPage*>(m_stack->widget(i));
            if (page) page->apply();
        }
        accept();
    });
    // Reset reloads settings (non-functional for Phase 1)
    connect(m_btnReset, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_stack->count(); ++i) {
            auto* page = qobject_cast<PreferencesPage*>(m_stack->widget(i));
            if (page) page->reset();
        }
    });
}

QStringList PreferencesDialog::categoryNames() const {
    QStringList out;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* item = m_tree->topLevelItem(i);
        if (item) out << item->text(0);
    }
    return out;
}
