#include "PreferencesPages.h"
#include "PreferencesManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include "DebugLog.h"

// Implement the existing page classes with UI and apply/reset logic

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
}

void PrefWaveformCachePage::reset()
{
	auto& pm = PreferencesManager::instance();
	m_size->setValue(pm.cacheSoftLimitMB());
	m_ttl->setValue(pm.cacheTtlDays());
}

// Audio Engine
PrefAudioEnginePage::PrefAudioEnginePage(QWidget* parent)
	: PreferencesPage(parent)
{
	auto* v = new QVBoxLayout(this);
	auto* form = new QFormLayout();
	m_gain = new QDoubleSpinBox(this);
	m_gain->setObjectName("spinDefaultGain");
	m_gain->setRange(0.0, 1.0);
	m_gain->setSingleStep(0.1);
	m_gain->setDecimals(2);
	form->addRow(tr("Default Gain"), m_gain);
	v->addLayout(form);
	v->addStretch();
	setLayout(v);
	reset();
}

void PrefAudioEnginePage::apply()
{
	PreferencesManager::instance().setDefaultGain(m_gain->value());
}

void PrefAudioEnginePage::reset()
{
	m_gain->setValue(PreferencesManager::instance().defaultGain());
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
