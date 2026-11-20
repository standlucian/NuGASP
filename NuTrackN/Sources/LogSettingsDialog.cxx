#include "LogSettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

static QString formatLogBytes(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

LogSettingsDialog::LogSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Diagnostic Log Settings & Cleanup"));
    setMinimumWidth(620);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();

    const CommandPrompt::LogConfig cfg = CommandPrompt::getLogConfig();
    m_chkDiskEnabled->setChecked(cfg.diskLoggingEnabled);
    m_editLogDir->setText(cfg.logDirectory.isEmpty() ? CommandPrompt::getDefaultLogDirectory() : cfg.logDirectory);
    m_chkAutoPrune->setChecked(cfg.autoPruneEnabled);
    m_chkPruneByCount->setChecked(cfg.pruneByCount);
    m_spinMaxFiles->setValue(qBound(1, cfg.maxLogFiles, 1000));
    m_chkPruneByAge->setChecked(cfg.pruneByAge);
    m_spinMaxDays->setValue(qBound(1, cfg.maxLogAgeDays, 3650));

    updateControlStates();
    refreshStats();
}

void LogSettingsDialog::setupUI()
{
    const QFont dlgFont = Design::getDialogFont();
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 1. Current Session Status & Directory Stats
    QGroupBox *grpStatus = new QGroupBox(tr("Session Log Status"), this);
    QGridLayout *statusGrid = new QGridLayout(grpStatus);
    statusGrid->setSpacing(8);

    m_lblActiveLog = new QLabel(grpStatus);
    m_lblActiveLog->setFont(dlgFont);
    m_lblActiveLog->setWordWrap(true);
    statusGrid->addWidget(m_lblActiveLog, 0, 0);

    m_btnOpenFolder = new QPushButton(tr("Open Folder"), grpStatus);
    m_btnOpenFolder->setFont(dlgFont);
    m_btnOpenFolder->setAutoDefault(false);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &LogSettingsDialog::onOpenFolderClicked);
    statusGrid->addWidget(m_btnOpenFolder, 0, 1, Qt::AlignRight);

    m_lblStats = new QLabel(grpStatus);
    m_lblStats->setFont(dlgFont);
    m_lblStats->setWordWrap(true);
    statusGrid->addWidget(m_lblStats, 1, 0, 1, 2);

    mainLayout->addWidget(grpStatus);

    // 2. Local Disk Logging Location
    QGroupBox *grpLocation = new QGroupBox(tr("Local Disk Logging"), this);
    QVBoxLayout *locLayout = new QVBoxLayout(grpLocation);
    locLayout->setSpacing(8);

    m_chkDiskEnabled = new QCheckBox(tr("Write full timestamped diagnostic logs to local disk"), grpLocation);
    m_chkDiskEnabled->setFont(dlgFont);
    connect(m_chkDiskEnabled, &QCheckBox::toggled, this, &LogSettingsDialog::updateControlStates);
    locLayout->addWidget(m_chkDiskEnabled);

    QHBoxLayout *dirRow = new QHBoxLayout();
    dirRow->setSpacing(6);
    QLabel *lblDir = new QLabel(tr("Log Folder:"), grpLocation);
    lblDir->setFont(dlgFont);
    dirRow->addWidget(lblDir);

    m_editLogDir = new QLineEdit(grpLocation);
    m_editLogDir->setFont(dlgFont);
    connect(m_editLogDir, &QLineEdit::textChanged, this, &LogSettingsDialog::refreshStats);
    dirRow->addWidget(m_editLogDir, 1);

    m_btnBrowseDir = new QPushButton(tr("Browse..."), grpLocation);
    m_btnBrowseDir->setFont(dlgFont);
    m_btnBrowseDir->setAutoDefault(false);
    connect(m_btnBrowseDir, &QPushButton::clicked, this, &LogSettingsDialog::onBrowseDirClicked);
    dirRow->addWidget(m_btnBrowseDir);

    m_btnResetDefaultDir = new QPushButton(tr("Default"), grpLocation);
    m_btnResetDefaultDir->setFont(dlgFont);
    m_btnResetDefaultDir->setAutoDefault(false);
    m_btnResetDefaultDir->setToolTip(tr("Reset to default local folder: %1").arg(CommandPrompt::getDefaultLogDirectory()));
    connect(m_btnResetDefaultDir, &QPushButton::clicked, this, &LogSettingsDialog::onResetDefaultDirClicked);
    dirRow->addWidget(m_btnResetDefaultDir);

    locLayout->addLayout(dirRow);
    mainLayout->addWidget(grpLocation);

    // 3. Retention & Pruning Rules
    QGroupBox *grpPrune = new QGroupBox(tr("Retention && Pruning"), this);
    QGridLayout *pruneGrid = new QGridLayout(grpPrune);
    pruneGrid->setSpacing(8);

    m_chkAutoPrune = new QCheckBox(tr("Automatically prune old logs on startup"), grpPrune);
    m_chkAutoPrune->setFont(dlgFont);
    m_chkAutoPrune->setToolTip(tr("Off by default: logs are kept indefinitely unless enabled here or pruned manually below."));
    pruneGrid->addWidget(m_chkAutoPrune, 0, 0, 1, 3);

    m_chkPruneByCount = new QCheckBox(tr("Keep at most:"), grpPrune);
    m_chkPruneByCount->setFont(dlgFont);
    connect(m_chkPruneByCount, &QCheckBox::toggled, this, &LogSettingsDialog::updateControlStates);
    pruneGrid->addWidget(m_chkPruneByCount, 1, 0);

    m_spinMaxFiles = new QSpinBox(grpPrune);
    m_spinMaxFiles->setFont(dlgFont);
    m_spinMaxFiles->setRange(1, 1000);
    m_spinMaxFiles->setValue(20);
    m_spinMaxFiles->setSuffix(tr(" files"));
    pruneGrid->addWidget(m_spinMaxFiles, 1, 1);

    m_chkPruneByAge = new QCheckBox(tr("Delete logs older than:"), grpPrune);
    m_chkPruneByAge->setFont(dlgFont);
    connect(m_chkPruneByAge, &QCheckBox::toggled, this, &LogSettingsDialog::updateControlStates);
    pruneGrid->addWidget(m_chkPruneByAge, 2, 0);

    m_spinMaxDays = new QSpinBox(grpPrune);
    m_spinMaxDays->setFont(dlgFont);
    m_spinMaxDays->setRange(1, 3650);
    m_spinMaxDays->setValue(30);
    m_spinMaxDays->setSuffix(tr(" days"));
    pruneGrid->addWidget(m_spinMaxDays, 2, 1);

    pruneGrid->setColumnStretch(2, 1);

    QHBoxLayout *manualLayout = new QHBoxLayout();
    manualLayout->setSpacing(8);

    m_btnPruneNow = new QPushButton(tr("Prune by Rules Now"), grpPrune);
    m_btnPruneNow->setFont(dlgFont);
    m_btnPruneNow->setAutoDefault(false);
    connect(m_btnPruneNow, &QPushButton::clicked, this, &LogSettingsDialog::onPruneNowClicked);
    manualLayout->addWidget(m_btnPruneNow);

    m_btnClearAllOld = new QPushButton(tr("Clear All Old Logs"), grpPrune);
    m_btnClearAllOld->setFont(dlgFont);
    m_btnClearAllOld->setAutoDefault(false);
    m_btnClearAllOld->setStyleSheet("color: #ef9a9a; font-weight: bold;");
    connect(m_btnClearAllOld, &QPushButton::clicked, this, &LogSettingsDialog::onClearAllOldClicked);
    manualLayout->addWidget(m_btnClearAllOld);

    manualLayout->addStretch(1);
    pruneGrid->addLayout(manualLayout, 3, 0, 1, 3);

    QLabel *lblSafety = new QLabel(
        tr("Note: Only NuTrackN session files (nutrackn_*.log) are managed; the active session log is always preserved."),
        grpPrune);
    lblSafety->setFont(dlgFont);
    lblSafety->setWordWrap(true);
    lblSafety->setStyleSheet("font-style: italic;");
    pruneGrid->addWidget(lblSafety, 4, 0, 1, 3);

    mainLayout->addWidget(grpPrune);

    // 4. Bottom Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setFont(dlgFont);
    m_btnCancel->setAutoDefault(false);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnSave = new QPushButton(tr("Save && Close"), this);
    m_btnSave->setFont(dlgFont);
    m_btnSave->setDefault(true);
    m_btnSave->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"
                "QPushButton:hover { opacity: 0.9; }")
            .arg(Design::getDialogAccentColor().name()));
    connect(m_btnSave, &QPushButton::clicked, this, &LogSettingsDialog::onSaveClicked);
    btnLayout->addWidget(m_btnSave);

    mainLayout->addLayout(btnLayout);
}

CommandPrompt::LogConfig LogSettingsDialog::readConfigFromUI() const
{
    CommandPrompt::LogConfig cfg;
    cfg.diskLoggingEnabled = m_chkDiskEnabled->isChecked();
    cfg.logDirectory = m_editLogDir->text().trimmed();
    if (cfg.logDirectory.isEmpty()) {
        cfg.logDirectory = CommandPrompt::getDefaultLogDirectory();
    }
    cfg.autoPruneEnabled = m_chkAutoPrune->isChecked();
    cfg.pruneByCount = m_chkPruneByCount->isChecked();
    cfg.maxLogFiles = m_spinMaxFiles->value();
    cfg.pruneByAge = m_chkPruneByAge->isChecked();
    cfg.maxLogAgeDays = m_spinMaxDays->value();
    return cfg;
}

void LogSettingsDialog::updateControlStates()
{
    const bool hasRule = m_chkPruneByCount->isChecked() || m_chkPruneByAge->isChecked();
    m_spinMaxFiles->setEnabled(m_chkPruneByCount->isChecked());
    m_spinMaxDays->setEnabled(m_chkPruneByAge->isChecked());
    m_btnPruneNow->setEnabled(hasRule);
}

void LogSettingsDialog::refreshStats()
{
    const QString activeFile = CommandPrompt::getCurrentLogFilePath();
    if (activeFile.isEmpty()) {
        m_lblActiveLog->setText(tr("<b>Active Session Log:</b> Disabled (disk logging off)"));
    } else {
        m_lblActiveLog->setText(
            tr("<b>Active Session Log:</b> %1").arg(QFileInfo(activeFile).fileName()));
    }

    QString dirPath = m_editLogDir->text().trimmed();
    if (dirPath.isEmpty()) {
        dirPath = CommandPrompt::getDefaultLogDirectory();
    }
    const CommandPrompt::LogDirectoryStats stats = CommandPrompt::getLogDirectoryStats(dirPath);
    m_lblStats->setText(
        tr("<b>Folder Usage:</b> %1 log file(s) (%2 archived), %3 total")
            .arg(stats.totalFiles)
            .arg(stats.oldFiles)
            .arg(formatLogBytes(stats.totalBytes)));

    m_btnClearAllOld->setEnabled(stats.oldFiles > 0);
}

void LogSettingsDialog::onBrowseDirClicked()
{
    QString current = m_editLogDir->text().trimmed();
    if (current.isEmpty() || !QDir(current).exists()) {
        current = CommandPrompt::getDefaultLogDirectory();
        QDir().mkpath(current);
    }
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Select Log Directory"), current);
    if (!chosen.isEmpty()) {
        m_editLogDir->setText(chosen);
        refreshStats();
    }
}

void LogSettingsDialog::onResetDefaultDirClicked()
{
    m_editLogDir->setText(CommandPrompt::getDefaultLogDirectory());
    refreshStats();
}

void LogSettingsDialog::onOpenFolderClicked()
{
    QString dirPath = m_editLogDir->text().trimmed();
    if (dirPath.isEmpty()) {
        dirPath = CommandPrompt::getDefaultLogDirectory();
    }
    if (!QDir(dirPath).exists()) {
        QMessageBox::information(
            this, tr("Log Folder Not Found"),
            tr("The folder '%1' does not exist yet.").arg(dirPath));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
}

void LogSettingsDialog::onPruneNowClicked()
{
    const CommandPrompt::LogConfig cfg = readConfigFromUI();
    if (!cfg.pruneByCount && !cfg.pruneByAge) {
        QMessageBox::information(
            this, tr("Prune Logs"),
            tr("Enable at least one retention rule (max files or max age) before pruning."));
        return;
    }

    qint64 bytesToFree = 0;
    const QStringList targets = CommandPrompt::findPrunableLogs(cfg, false, &bytesToFree);
    if (targets.isEmpty()) {
        QMessageBox::information(
            this, tr("Prune Logs"),
            tr("No old log files match the selected pruning rules."));
        return;
    }

    if (QMessageBox::question(
            this, tr("Confirm Manual Log Pruning"),
            tr("Delete %1 old log file(s) totaling %2 matching your retention rules?\n"
               "The currently active session log will be preserved.")
                .arg(targets.size())
                .arg(formatLogBytes(bytesToFree)),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    qint64 freedBytes = 0;
    const int removed = CommandPrompt::pruneLogs(cfg, false, &freedBytes);
    CommandPrompt::logInfo(
        "Log",
        QString("Pruned %1 old log file(s) (%2 freed).")
            .arg(removed)
            .arg(formatLogBytes(freedBytes)));
    refreshStats();
    QMessageBox::information(
        this, tr("Prune Complete"),
        tr("Removed %1 old log file(s) (%2 freed).").arg(removed).arg(formatLogBytes(freedBytes)));
}

void LogSettingsDialog::onClearAllOldClicked()
{
    const CommandPrompt::LogConfig cfg = readConfigFromUI();
    qint64 bytesToFree = 0;
    const QStringList targets = CommandPrompt::findPrunableLogs(cfg, true, &bytesToFree);
    if (targets.isEmpty()) {
        QMessageBox::information(
            this, tr("Clear Old Logs"),
            tr("There are no archived session logs to delete."));
        return;
    }

    if (QMessageBox::question(
            this, tr("Confirm Clear All Old Logs"),
            tr("Permanently delete ALL %1 archived log file(s) (%2) in:\n%3?\n\n"
               "The currently active session log will be preserved.")
                .arg(targets.size())
                .arg(formatLogBytes(bytesToFree), cfg.logDirectory),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    qint64 freedBytes = 0;
    const int removed = CommandPrompt::pruneLogs(cfg, true, &freedBytes);
    CommandPrompt::logInfo(
        "Log",
        QString("Cleared %1 old log file(s) (%2 freed).")
            .arg(removed)
            .arg(formatLogBytes(freedBytes)));
    refreshStats();
    QMessageBox::information(
        this, tr("Cleanup Complete"),
        tr("Removed %1 old log file(s) (%2 freed).").arg(removed).arg(formatLogBytes(freedBytes)));
}

void LogSettingsDialog::onSaveClicked()
{
    const CommandPrompt::LogConfig cfg = readConfigFromUI();
    CommandPrompt::setLogConfig(cfg);
    CommandPrompt::logDebug(
        "Log",
        QString("Saved log configuration (enabled=%1, dir='%2', autoPrune=%3, byCount=%4(%5), byAge=%6(%7d)).")
            .arg(cfg.diskLoggingEnabled ? "true" : "false")
            .arg(cfg.logDirectory)
            .arg(cfg.autoPruneEnabled ? "true" : "false")
            .arg(cfg.pruneByCount ? "true" : "false")
            .arg(cfg.maxLogFiles)
            .arg(cfg.pruneByAge ? "true" : "false")
            .arg(cfg.maxLogAgeDays));
    accept();
}

