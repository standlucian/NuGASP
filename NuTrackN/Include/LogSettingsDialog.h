#ifndef LOGSETTINGSDIALOG_H
#define LOGSETTINGSDIALOG_H

#include <QDialog>
#include "Design.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QLabel;

class LogSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit LogSettingsDialog(QWidget *parent = nullptr);
    ~LogSettingsDialog() override = default;

private slots:
    void onBrowseDirClicked();
    void onResetDefaultDirClicked();
    void onOpenFolderClicked();
    void onPruneNowClicked();
    void onClearAllOldClicked();
    void onSaveClicked();
    void updateControlStates();
    void refreshStats();

private:
    void setupUI();
    CommandPrompt::LogConfig readConfigFromUI() const;

    QLabel      *m_lblActiveLog{nullptr};
    QLabel      *m_lblStats{nullptr};
    QPushButton *m_btnOpenFolder{nullptr};

    QCheckBox   *m_chkDiskEnabled{nullptr};
    QLineEdit   *m_editLogDir{nullptr};
    QPushButton *m_btnBrowseDir{nullptr};
    QPushButton *m_btnResetDefaultDir{nullptr};

    QCheckBox   *m_chkAutoPrune{nullptr};
    QCheckBox   *m_chkPruneByCount{nullptr};
    QSpinBox    *m_spinMaxFiles{nullptr};
    QCheckBox   *m_chkPruneByAge{nullptr};
    QSpinBox    *m_spinMaxDays{nullptr};

    QPushButton *m_btnPruneNow{nullptr};
    QPushButton *m_btnClearAllOld{nullptr};
    QPushButton *m_btnSave{nullptr};
    QPushButton *m_btnCancel{nullptr};
};

#endif // LOGSETTINGSDIALOG_H

