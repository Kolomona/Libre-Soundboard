#pragma once
#include <QDialog>
#include <QStringList>

class QTreeWidget;
class QStackedWidget;
class QPushButton;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    QStringList categoryNames() const;

private:
    void buildUi();
    void connectSignals();

    QTreeWidget* m_tree = nullptr;
    QStackedWidget* m_stack = nullptr;
    QPushButton* m_btnSave = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnReset = nullptr;
};
