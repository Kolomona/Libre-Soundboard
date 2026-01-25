#include "PreferencesPages.h"
#include "PreferencesManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include "DebugLog.h"

// Implement the existing page classes with UI and apply/reset logic

// Grid & Layout
PrefGridLayoutPage::PrefGridLayoutPage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();

	m_rows = new QSpinBox(this);
	m_rows->setObjectName("spinGridRows");
	m_rows->setRange(2, 8);

	m_cols = new QSpinBox(this);
	m_cols->setObjectName("spinGridCols");
	m_cols->setRange(4, 16);

	form->addRow(tr("Rows"), m_rows);
	form->addRow(tr("Columns"), m_cols);

	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefGridLayoutPage::apply()
{
	auto& pm = PreferencesManager::instance();
	int oldRows = pm.gridRows();
	int oldCols = pm.gridCols();
	int newRows = m_rows->value();
	int newCols = m_cols->value();
	pm.setGridRows(newRows);
	pm.setGridCols(newCols);
	if (newRows != oldRows || newCols != oldCols) {
		emit dimensionsChanged(newRows, newCols);
	}
}

void PrefGridLayoutPage::reset()
{
	m_rows->setValue(PreferencesManager::instance().gridRows());
	m_cols->setValue(PreferencesManager::instance().gridCols());
}

// Waveform Cache
PrefWaveformCachePage::PrefWaveformCachePage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();
	m_size = new QSpinBox(this);
	m_size->setObjectName("spinCacheSizeMB");
	m_size->setRange(0, 4096);
	m_size->setSuffix(" MB");
	form->addRow(tr("Cache Size Limit"), m_size);
	m_ttl = new QSpinBox(this);
	m_ttl->setObjectName("spinCacheTTLDays");
	m_ttl->setRange(0, 3650);
	m_ttl->setSuffix(" days");
	form->addRow(tr("Cache TTL"), m_ttl);
	
	// Phase 4: Add cache directory field
	m_cacheDir = new QLineEdit(this);
	m_cacheDir->setObjectName("editCacheDir");
	m_cacheDir->setReadOnly(true);
	
	auto* dirRow = new QHBoxLayout();
	dirRow->addWidget(m_cacheDir);
	
	auto* btnBrowse = new QPushButton(tr("Browse..."), this);
	btnBrowse->setObjectName("btnBrowseCacheDir");
	dirRow->addWidget(btnBrowse);
	
	form->addRow(tr("Cache Directory"), dirRow);
	
	connect(btnBrowse, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(
			this,
			tr("Select Cache Directory"),
			m_cacheDir->text()
		);
		if (!path.isEmpty()) {
			if (PreferencesManager::instance().validatePath(path)) {
				m_cacheDir->setText(path);
			} else {
				QMessageBox::warning(this, tr("Invalid Directory"), 
					tr("Selected directory does not exist or is not writable."));
			}
		}
	});
	
	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefWaveformCachePage::apply()
{
	auto& pm = PreferencesManager::instance();
	pm.setCacheSoftLimitMB(m_size->value());
	pm.setCacheTtlDays(m_ttl->value());
	if (m_cacheDir) {
		pm.setCacheDirectory(m_cacheDir->text());
	}
}

void PrefWaveformCachePage::reset()
{
	auto& pm = PreferencesManager::instance();
	m_size->setValue(pm.cacheSoftLimitMB());
	m_ttl->setValue(pm.cacheTtlDays());
	if (m_cacheDir) {
		m_cacheDir->setText(pm.cacheDirectory());
	}
}

// Audio Engine
PrefAudioEnginePage::PrefAudioEnginePage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();

	m_jackName = new QLineEdit(this);
	m_jackName->setObjectName("editJackClientName");
	form->addRow(tr("JACK Client Name"), m_jackName);

	m_rememberConnections = new QCheckBox(tr("Remember Jack Connections"), this);
	m_rememberConnections->setObjectName("chkJackRememberConnections");
	form->addRow(QString(), m_rememberConnections);
	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefAudioEnginePage::apply()
{
	auto& pm = PreferencesManager::instance();
	pm.setJackClientName(m_jackName->text());
	pm.setJackRememberConnections(m_rememberConnections->isChecked());
}

void PrefAudioEnginePage::reset()
{
	auto& pm = PreferencesManager::instance();
	m_jackName->setText(pm.jackClientName());
	m_rememberConnections->setChecked(pm.jackRememberConnections());
}

// Debug
PrefDebugPage::PrefDebugPage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();
	m_level = new QComboBox(this);
	m_level->setObjectName("comboLogLevel");
	m_level->addItems({tr("Off"), tr("Error"), tr("Warning"), tr("Info"), tr("Debug")});
	form->addRow(tr("Logging Level"), m_level);
	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefDebugPage::apply()
{
	auto idx = m_level->currentIndex();
	PreferencesManager::instance().setLogLevel(static_cast<PreferencesManager::LogLevel>(idx));
	DebugLog::setLevel(idx);
}

void PrefDebugPage::reset()
{
	auto lvl = PreferencesManager::instance().logLevel();
	m_level->setCurrentIndex(static_cast<int>(lvl));
}

// Keep-Alive
PrefKeepAlivePage::PrefKeepAlivePage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();

	m_enable = new QCheckBox(tr("Enable"), this);
	m_enable->setObjectName("chkKeepAliveEnable");
	form->addRow(tr("Keep-Alive"), m_enable);

	m_timeout = new QSpinBox(this);
	m_timeout->setObjectName("spinKeepAliveTimeout");
	m_timeout->setRange(1, 3600);
	m_timeout->setSuffix(tr(" sec"));
	form->addRow(tr("Silence Timeout"), m_timeout);

	m_sensitivity = new QDoubleSpinBox(this);
	m_sensitivity->setObjectName("spinKeepAliveSensitivity");
	m_sensitivity->setRange(-120.0, 0.0);
	m_sensitivity->setDecimals(1);
	m_sensitivity->setSingleStep(1.0);
	m_anyNonZero = new QCheckBox(tr("Any non-zero"), this);
	m_anyNonZero->setObjectName("chkKeepAliveAnyNonZero");

auto* sensRow = new QHBoxLayout();
	sensRow->addWidget(m_sensitivity);
	sensRow->addWidget(m_anyNonZero);
	form->addRow(tr("Sensitivity (dBFS)"), sensRow);

	m_target = new QComboBox(this);
	m_target->setObjectName("comboKeepAliveTarget");
	m_target->addItems({tr("Last tab, last sound"), tr("Specific slot")});
	form->addRow(tr("Trigger Target"), m_target);

	m_tabIndex = new QSpinBox(this);
	m_tabIndex->setObjectName("spinKeepAliveTab");
	m_tabIndex->setRange(1, 32);
	m_slotIndex = new QSpinBox(this);
	m_slotIndex->setObjectName("spinKeepAliveSlot");
	m_slotIndex->setRange(1, 128);
	auto* slotRow = new QHBoxLayout();
	slotRow->addWidget(new QLabel(tr("Tab"), this));
	slotRow->addWidget(m_tabIndex);
	slotRow->addSpacing(8);
	slotRow->addWidget(new QLabel(tr("Slot"), this));
	slotRow->addWidget(m_slotIndex);
	form->addRow(tr("Specific Slot"), slotRow);

	m_useSlotVolume = new QCheckBox(tr("Use slot volume"), this);
	m_useSlotVolume->setObjectName("chkKeepAliveUseSlotVolume");
	m_overrideVolume = new QDoubleSpinBox(this);
	m_overrideVolume->setObjectName("spinKeepAliveOverrideVolume");
	m_overrideVolume->setRange(0.0, 1.0);
	m_overrideVolume->setSingleStep(0.05);
	m_overrideVolume->setDecimals(2);
	auto* volRow = new QHBoxLayout();
	volRow->addWidget(m_useSlotVolume);
	volRow->addWidget(new QLabel(tr("Override"), this));
	volRow->addWidget(m_overrideVolume);
	form->addRow(tr("Volume Policy"), volRow);

	m_autoConnect = new QCheckBox(tr("Auto-connect keep-alive input"), this);
	m_autoConnect->setObjectName("chkKeepAliveAutoConnect");
	form->addRow(tr("JACK Input"), m_autoConnect);

	m_playTest = new QPushButton(tr("Play test"), this);
	m_playTest->setObjectName("btnKeepAlivePlayTest");
	form->addRow(QString(), m_playTest);

	v->addLayout(form);
	v->addStretch();
	setLayout(v);

	connect(m_anyNonZero, &QCheckBox::toggled, this, [this](){ updateSensitivityEnabled(); });
	connect(m_target, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int){ updateTargetControls(); });
	connect(m_useSlotVolume, &QCheckBox::toggled, this, [this](){ updateVolumeControls(); });

	// Wire play test button to callback (set by MainWindow if available)
	connect(m_playTest, &QPushButton::clicked, this, [this]() {
		if (m_playTestCallback) {
			float overrideVol = static_cast<float>(m_overrideVolume->value());
			bool useSlotVol = m_useSlotVolume->isChecked();
			bool isSpecific = (m_target->currentIndex() == 1); // SpecificSlot = 1
			// Convert from 1-indexed UI display to 0-indexed array indices
			int targetTab = m_tabIndex->value() - 1;
			int targetSlot = m_slotIndex->value() - 1;
			m_playTestCallback(overrideVol, targetTab, targetSlot, isSpecific, useSlotVol);
		}
	});

	reset();
}

void PrefKeepAlivePage::apply()
{
	auto& pm = PreferencesManager::instance();
	pm.setKeepAliveEnabled(m_enable->isChecked());
	pm.setKeepAliveTimeoutSeconds(m_timeout->value());
	pm.setKeepAliveAnyNonZero(m_anyNonZero->isChecked());
	pm.setKeepAliveSensitivityDbfs(m_sensitivity->value());
	auto target = m_target->currentIndex() == 0 ? PreferencesManager::KeepAliveTarget::LastTabLastSound
	                                          : PreferencesManager::KeepAliveTarget::SpecificSlot;
	pm.setKeepAliveTarget(target);
	pm.setKeepAliveTargetTab(m_tabIndex->value() - 1);
	pm.setKeepAliveTargetSlot(m_slotIndex->value() - 1);
	pm.setKeepAliveUseSlotVolume(m_useSlotVolume->isChecked());
	pm.setKeepAliveOverrideVolume(m_overrideVolume->value());
	pm.setKeepAliveAutoConnectInput(m_autoConnect->isChecked());
}

void PrefKeepAlivePage::reset()
{
	auto& pm = PreferencesManager::instance();
	m_enable->setChecked(pm.keepAliveEnabled());
	m_timeout->setValue(pm.keepAliveTimeoutSeconds());
	m_sensitivity->setValue(pm.keepAliveSensitivityDbfs());
	m_anyNonZero->setChecked(pm.keepAliveAnyNonZero());
	m_target->setCurrentIndex(pm.keepAliveTarget() == PreferencesManager::KeepAliveTarget::LastTabLastSound ? 0 : 1);
	m_tabIndex->setValue(pm.keepAliveTargetTab() + 1);
	m_slotIndex->setValue(pm.keepAliveTargetSlot() + 1);
	m_useSlotVolume->setChecked(pm.keepAliveUseSlotVolume());
	m_overrideVolume->setValue(pm.keepAliveOverrideVolume());
	m_autoConnect->setChecked(pm.keepAliveAutoConnectInput());

	updateSensitivityEnabled();
	updateTargetControls();
	updateVolumeControls();
}

void PrefKeepAlivePage::updateSensitivityEnabled()
{
	m_sensitivity->setEnabled(!m_anyNonZero->isChecked());
}

void PrefKeepAlivePage::updateTargetControls()
{
	bool specific = (m_target->currentIndex() == 1);
	m_tabIndex->setEnabled(specific);
	m_slotIndex->setEnabled(specific);
}

void PrefKeepAlivePage::updateVolumeControls()
{
	// Override volume should be enabled only when NOT using slot volume
	m_overrideVolume->setEnabled(!m_useSlotVolume->isChecked());
}
// Phase 4: File Handling Page
PrefFileHandlingPage::PrefFileHandlingPage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();
	
	m_soundDir = new QLineEdit(this);
	m_soundDir->setObjectName("editDefaultSoundDir");
	m_soundDir->setReadOnly(true);
	
	auto* dirRow = new QHBoxLayout();
	dirRow->addWidget(m_soundDir);
	
	auto* btnBrowse = new QPushButton(tr("Browse..."), this);
	btnBrowse->setObjectName("btnBrowseSoundDir");
	dirRow->addWidget(btnBrowse);
	
	form->addRow(tr("Default Sound Directory"), dirRow);
	
	connect(btnBrowse, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(
			this,
			tr("Select Default Sound Directory"),
			m_soundDir->text()
		);
		if (!path.isEmpty()) {
			if (PreferencesManager::instance().validatePath(path)) {
				m_soundDir->setText(path);
			} else {
				QMessageBox::warning(this, tr("Invalid Directory"), 
					tr("Selected directory does not exist or is not writable."));
			}
		}
	});
	
	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefFileHandlingPage::apply()
{
	PreferencesManager::instance().setDefaultSoundDirectory(m_soundDir->text());
}

void PrefFileHandlingPage::reset()
{
	m_soundDir->setText(PreferencesManager::instance().defaultSoundDirectory());
}

