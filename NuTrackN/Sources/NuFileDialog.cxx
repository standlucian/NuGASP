#include "NuFileDialog.h"
#include "RemoteConnectDialog.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QApplication>
#include <QDir>
#include <QCollator>

static QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

NuFileDialog::NuFileDialog(NuFileMode mode,
                           const QString &caption,
                           const QString &initialDir,
                           const QString &filter,
                           QWidget *parent)
    : QDialog(parent), m_mode(mode)
{
    m_session = RemoteSession::globalSession();

    // Default to Remote if currently connected, otherwise local
    if (m_session && m_session->isConnected()) {
        m_isRemoteMode = true;
    } else {
        m_isRemoteMode = false;
    }

    m_currentLocalDir = initialDir.isEmpty() ? QDir::currentPath() : initialDir;
    if (m_session && m_session->isConnected()) {
        m_currentRemoteDir = m_session->getCurrentRemoteDir();
        if (m_currentRemoteDir.isEmpty()) {
            m_currentRemoteDir = m_session->getProfile().lastUsedPath;
        }
        if (m_currentRemoteDir.isEmpty()) {
            m_currentRemoteDir = "/home/" + m_session->getProfile().user;
        }
    } else {
        m_currentRemoteDir = "/home";
    }

    resize(900, 620);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI(caption, filter);

    if (m_isRemoteMode) {
        loadDirectory(m_currentRemoteDir);
    } else {
        loadDirectory(m_currentLocalDir);
    }
}

void NuFileDialog::setupUI(const QString &caption, const QString &initialFilter)
{
    const QFont dlgFont = Design::getDialogFont();

    QString title = caption;
    if (title.isEmpty()) {
        switch (m_mode) {
            case NuFileMode::OpenSpectrum: title = tr("Open Spectrum File"); break;
            case NuFileMode::OpenMatrix: title = tr("Open Coincidence Matrix"); break;
            case NuFileMode::OpenMultipleFiles: title = tr("Select Sequence Run Files"); break;
            case NuFileMode::SaveFile: title = tr("Save File Destination"); break;
            case NuFileMode::SelectDirectory: title = tr("Select Directory"); break;
            default: title = tr("Select File"); break;
        }
    }
    setWindowTitle(title);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. Top Location Selector Bar (Remote vs Local Switcher)
    QHBoxLayout *topLocationLayout = new QHBoxLayout();
    topLocationLayout->setSpacing(6);

    QLabel *lblLoc = new QLabel(tr("Location:"), this);
    lblLoc->setFont(dlgFont);
    topLocationLayout->addWidget(lblLoc);

    m_btnRemoteTab = new QPushButton(this);
    m_btnRemoteTab->setFont(dlgFont);
    m_btnRemoteTab->setCheckable(true);
    topLocationLayout->addWidget(m_btnRemoteTab);

    m_btnLocalTab = new QPushButton(tr("💻 Local Filesystem"), this);
    m_btnLocalTab->setFont(dlgFont);
    m_btnLocalTab->setCheckable(true);
    topLocationLayout->addWidget(m_btnLocalTab);

    topLocationLayout->addStretch(1);

    m_btnConnectSSH = new QPushButton(tr("SSH Parameters..."), this);
    m_btnConnectSSH->setFont(dlgFont);
    connect(m_btnConnectSSH, &QPushButton::clicked, this, &NuFileDialog::onConnectSSHClicked);
    topLocationLayout->addWidget(m_btnConnectSSH);

    mainLayout->addLayout(topLocationLayout);

    // 2. Navigation Bar (Up, Home, Refresh, New Folder, Path Edit)
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);

    m_btnUp = new QPushButton(tr("▲ Up"), this);
    m_btnUp->setFont(dlgFont);
    m_btnUp->setToolTip(tr("Parent Directory"));
    connect(m_btnUp, &QPushButton::clicked, this, &NuFileDialog::onNavigateUp);
    navLayout->addWidget(m_btnUp);

    m_btnHome = new QPushButton(tr("⌂ Home"), this);
    m_btnHome->setFont(dlgFont);
    m_btnHome->setToolTip(tr("Home Directory"));
    connect(m_btnHome, &QPushButton::clicked, this, &NuFileDialog::onNavigateHome);
    navLayout->addWidget(m_btnHome);

    m_btnRefresh = new QPushButton(tr("↻ Refresh"), this);
    m_btnRefresh->setFont(dlgFont);
    m_btnRefresh->setToolTip(tr("Refresh current listing"));
    connect(m_btnRefresh, &QPushButton::clicked, this, &NuFileDialog::onRefresh);
    navLayout->addWidget(m_btnRefresh);

    m_btnNewFolder = new QPushButton(tr("+ Folder"), this);
    m_btnNewFolder->setFont(dlgFont);
    m_btnNewFolder->setToolTip(tr("Create new directory"));
    connect(m_btnNewFolder, &QPushButton::clicked, this, &NuFileDialog::onNewFolder);
    navLayout->addWidget(m_btnNewFolder);

    m_editCurrentPath = new QLineEdit(this);
    m_editCurrentPath->setFont(dlgFont);
    m_editCurrentPath->setPlaceholderText(tr("Path..."));
    connect(m_editCurrentPath, &QLineEdit::returnPressed, this, &NuFileDialog::onPathEntered);
    navLayout->addWidget(m_editCurrentPath, 1);

    mainLayout->addLayout(navLayout);

    // 3. Filter & Search Bar
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    QLabel *lblFilter = new QLabel(tr("Filter:"), this);
    lblFilter->setFont(dlgFont);
    filterLayout->addWidget(lblFilter);

    m_comboTypeFilter = new QComboBox(this);
    m_comboTypeFilter->setFont(dlgFont);

    if (m_mode == NuFileMode::OpenSpectrum) {
        m_comboTypeFilter->addItem(tr("All Supported Spectra (*.spe *.spk *.chn *.asc *.dat)"), "spectra");
        m_comboTypeFilter->addItem(tr("GASPware Spectra (*.spe)"), "spe");
        m_comboTypeFilter->addItem(tr("ASCII / Fortran Data (*.asc *.dat *.txt)"), "asc");
        m_comboTypeFilter->addItem(tr("All Files (*.*)"), "all");
    } else if (m_mode == NuFileMode::OpenMatrix) {
        m_comboTypeFilter->addItem(tr("Coincidence Matrices (*.cmat *.mat)"), "matrix");
        m_comboTypeFilter->addItem(tr("GASPware Compressed Matrix (*.cmat)"), "cmat");
        m_comboTypeFilter->addItem(tr("All Files (*.*)"), "all");
    } else if (m_mode == NuFileMode::SelectDirectory) {
        m_comboTypeFilter->addItem(tr("Directories Only"), "dirs");
    } else {
        if (!initialFilter.isEmpty()) {
            QStringList filters = initialFilter.split(";;", Qt::SkipEmptyParts);
            for (const QString &f : filters) {
                m_comboTypeFilter->addItem(f.trimmed(), f.trimmed());
            }
        }
        m_comboTypeFilter->addItem(tr("All Nuclear Data (*.spe *.spk *.mat *.cmat *.chn *.cal *.root)"), "data");
        m_comboTypeFilter->addItem(tr("All Files (*.*)"), "all");
    }
    connect(m_comboTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NuFileDialog::onFilterChanged);
    filterLayout->addWidget(m_comboTypeFilter);

    QLabel *lblSearch = new QLabel(tr("Search:"), this);
    lblSearch->setFont(dlgFont);
    filterLayout->addWidget(lblSearch);

    m_editSearchFilter = new QLineEdit(this);
    m_editSearchFilter->setFont(dlgFont);
    m_editSearchFilter->setPlaceholderText(tr("Search by filename..."));
    connect(m_editSearchFilter, &QLineEdit::textChanged, this, &NuFileDialog::onFilterChanged);
    filterLayout->addWidget(m_editSearchFilter, 1);

    mainLayout->addLayout(filterLayout);

    // 4. File Table
    m_tableFiles = new QTableWidget(0, 4, this);
    m_tableFiles->setFont(dlgFont);
    m_tableFiles->setHorizontalHeaderLabels({
        tr("Name"), tr("Size"), tr("Type"), tr("Last Modified")
    });
    m_tableFiles->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableFiles->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableFiles->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableFiles->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableFiles->verticalHeader()->setVisible(false);
    m_tableFiles->setSelectionBehavior(QAbstractItemView::SelectRows);

    if (m_mode == NuFileMode::OpenMultipleFiles) {
        m_tableFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    } else {
        m_tableFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    }
    m_tableFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_tableFiles, &QTableWidget::cellDoubleClicked, this, &NuFileDialog::onTableItemDoubleClicked);
    connect(m_tableFiles, &QTableWidget::itemSelectionChanged, this, &NuFileDialog::onTableSelectionChanged);

    mainLayout->addWidget(m_tableFiles, 1);

    // 5. Save Filename Row (if in SaveFile mode)
    if (m_mode == NuFileMode::SaveFile) {
        QHBoxLayout *saveRow = new QHBoxLayout();
        saveRow->setSpacing(6);
        m_lblFileName = new QLabel(tr("File name:"), this);
        m_lblFileName->setFont(dlgFont);
        saveRow->addWidget(m_lblFileName);

        m_editSaveFileName = new QLineEdit(this);
        m_editSaveFileName->setFont(dlgFont);
        m_editSaveFileName->setPlaceholderText(tr("Enter target filename (e.g. output.spe)..."));
        saveRow->addWidget(m_editSaveFileName, 1);
        mainLayout->addLayout(saveRow);
    }

    // 6. Selection Info & Progress Bar
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_lblSelectionInfo = new QLabel(tr("Ready."), this);
    m_lblSelectionInfo->setFont(dlgFont);
    m_lblSelectionInfo->setStyleSheet(QString("color: %1;").arg(Design::getDialogAccentColor().name()));
    statusLayout->addWidget(m_lblSelectionInfo, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFont(dlgFont);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    statusLayout->addWidget(m_progressBar);

    mainLayout->addLayout(statusLayout);

    // 7. Bottom Action Buttons Bar
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    QString actionText = tr("Open");
    if (m_mode == NuFileMode::SaveFile) {
        actionText = tr("Save");
    } else if (m_mode == NuFileMode::SelectDirectory) {
        actionText = tr("Select Directory");
    } else if (m_mode == NuFileMode::OpenMatrix) {
        actionText = tr("Open Matrix");
    } else if (m_mode == NuFileMode::OpenSpectrum) {
        actionText = tr("Open Spectrum");
    }

    m_btnAction = new QPushButton(actionText, this);
    m_btnAction->setFont(dlgFont);
    m_btnAction->setEnabled(m_mode == NuFileMode::SelectDirectory || m_mode == NuFileMode::SaveFile);
    m_btnAction->setStyleSheet(QString("QPushButton { background-color: %1; color: white; font-weight: bold; padding: 6px 18px; border-radius: 4px; }"
                                       "QPushButton:hover { opacity: 0.9; }")
                               .arg(Design::getDialogAccentColor().name()));
    connect(m_btnAction, &QPushButton::clicked, this, &NuFileDialog::onAcceptActionClicked);
    actionLayout->addWidget(m_btnAction);

    actionLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setFont(dlgFont);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionLayout->addWidget(m_btnCancel);

    mainLayout->addLayout(actionLayout);

    connect(m_btnRemoteTab, &QPushButton::clicked, this, &NuFileDialog::onLocationToggled);
    connect(m_btnLocalTab, &QPushButton::clicked, this, &NuFileDialog::onLocationToggled);

    // Update location tab styles
    if (m_session && m_session->isConnected()) {
        const RemoteProfile &prof = m_session->getProfile();
        m_btnRemoteTab->setText(QString("🌐 Remote: %1@%2").arg(prof.user, prof.host));
        m_btnRemoteTab->setEnabled(true);
    } else {
        m_btnRemoteTab->setText(tr("🌐 Remote Server (Offline)"));
    }

    m_btnRemoteTab->setChecked(m_isRemoteMode);
    m_btnLocalTab->setChecked(!m_isRemoteMode);
}

void NuFileDialog::onConnectSSHClicked()
{
    RemoteConnectDialog dlg(m_session, this);
    if (dlg.exec() == QDialog::Accepted && m_session && m_session->isConnected()) {
        const RemoteProfile &prof = m_session->getProfile();
        m_btnRemoteTab->setText(QString("🌐 Remote: %1@%2").arg(prof.user, prof.host));
        m_btnRemoteTab->setChecked(true);
        m_btnLocalTab->setChecked(false);
        m_isRemoteMode = true;

        m_currentRemoteDir = m_session->getCurrentRemoteDir();
        if (m_currentRemoteDir.isEmpty()) m_currentRemoteDir = prof.lastUsedPath;
        if (m_currentRemoteDir.isEmpty()) m_currentRemoteDir = "/home/" + prof.user;
        loadDirectory(m_currentRemoteDir);
    }
}

void NuFileDialog::onLocationToggled()
{
    if (sender() == m_btnRemoteTab) {
        if (!m_session || !m_session->isConnected()) {
            QMessageBox::information(this, tr("SSH Not Connected"),
                tr("There is currently no active SSH connection.\nPlease configure host parameters and connect."));
            onConnectSSHClicked();
            return;
        }
        m_isRemoteMode = true;
        m_btnRemoteTab->setChecked(true);
        m_btnLocalTab->setChecked(false);
        loadDirectory(m_currentRemoteDir);
    } else {
        m_isRemoteMode = false;
        m_btnLocalTab->setChecked(true);
        m_btnRemoteTab->setChecked(false);
        loadDirectory(m_currentLocalDir);
    }
}

void NuFileDialog::loadDirectory(const QString &dirPath)
{
    m_lblSelectionInfo->setText(tr("Loading directory: %1 ...").arg(dirPath));
    qApp->processEvents();

    m_currentEntries.clear();

    if (m_isRemoteMode) {
        if (!m_session || !m_session->isConnected()) return;

        QString err;
        if (!m_session->listDirectory(dirPath, m_currentEntries, &err)) {
            QMessageBox::critical(this, tr("Remote Directory Error"), err);
            m_lblSelectionInfo->setText(tr("Failed to read remote directory."));
            return;
        }
        m_currentRemoteDir = m_session->getCurrentRemoteDir();
        m_editCurrentPath->setText(m_currentRemoteDir);
    } else {
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir = QDir::current();
        }
        m_currentLocalDir = dir.canonicalPath();
        m_editCurrentPath->setText(m_currentLocalDir);

        QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                               QDir::DirsFirst | QDir::Name);
        for (const auto &fi : list) {
            RemoteFileInfo info;
            info.name = fi.fileName();
            info.fullPath = fi.absoluteFilePath();
            info.isDirectory = fi.isDir();
            info.size = fi.size();
            info.lastModified = fi.lastModified();
            info.permissions = static_cast<uint32_t>(fi.permissions());
            m_currentEntries.push_back(info);
        }
    }

    populateTable();

    QString hostLabel = m_isRemoteMode ? QString("[%1] ").arg(m_session->getProfile().host) : tr("[Local] ");
    m_lblSelectionInfo->setText(tr("%1%2 items in %3")
        .arg(hostLabel).arg(m_currentEntries.size()).arg(m_editCurrentPath->text()));
}

void NuFileDialog::populateTable()
{
    m_tableFiles->setRowCount(0);
    const QString search = m_editSearchFilter->text().trimmed().toLower();
    const QString filterType = m_comboTypeFilter->currentData().toString();

    int row = 0;
    for (const auto &entry : m_currentEntries) {
        if (!search.isEmpty() && !entry.name.toLower().contains(search)) {
            continue;
        }

        QString suffix = QFileInfo(entry.name).suffix().toLower();

        if (m_mode == NuFileMode::SelectDirectory && !entry.isDirectory) {
            continue;
        }

        if (!entry.isDirectory && filterType != "all" && !filterType.isEmpty()) {
            if (filterType == "spectra") {
                if (suffix != "spe" && suffix != "spk" && suffix != "chn" &&
                    suffix != "asc" && suffix != "dat") {
                    continue;
                }
            } else if (filterType == "spe") {
                if (suffix != "spe") continue;
            } else if (filterType == "matrix") {
                if (suffix != "cmat" && suffix != "mat") continue;
            } else if (filterType == "cmat") {
                if (suffix != "cmat") continue;
            } else if (filterType == "asc") {
                if (suffix != "asc" && suffix != "dat" && suffix != "txt") continue;
            } else if (filterType == "data") {
                if (suffix != "spe" && suffix != "spk" && suffix != "mat" &&
                    suffix != "cmat" && suffix != "chn" && suffix != "root" &&
                    suffix != "cal" && suffix != "mcal" && suffix != "ucal" &&
                    suffix != "asc" && suffix != "dat") {
                    continue;
                }
            }
        }

        m_tableFiles->insertRow(row);

        QString iconPrefix = entry.isDirectory ? "📁 " : "📄 ";
        QTableWidgetItem *itemName = new QTableWidgetItem(iconPrefix + entry.name);
        itemName->setData(Qt::UserRole, entry.fullPath);
        itemName->setData(Qt::UserRole + 1, entry.isDirectory);
        if (entry.isDirectory) {
            itemName->setForeground(QBrush(QColor("#4fc3f7")));
            QFont bold = itemName->font();
            bold.setBold(true);
            itemName->setFont(bold);
        }
        m_tableFiles->setItem(row, 0, itemName);

        // Size
        QString sizeStr = entry.isDirectory ? "--" : formatFileSize(entry.size);
        QTableWidgetItem *itemSize = new QTableWidgetItem(sizeStr);
        itemSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableFiles->setItem(row, 1, itemSize);

        // Type
        QString typeStr;
        if (entry.isDirectory) {
            typeStr = tr("Directory");
        } else if (suffix == "cmat" || suffix == "mat") {
            typeStr = tr("2D Coincidence Matrix");
        } else if (suffix == "spe" || suffix == "spk" || suffix == "chn") {
            typeStr = tr("1D Spectrum");
        } else if (suffix == "cal" || suffix == "mcal" || suffix == "ucal") {
            typeStr = tr("Calibration File");
        } else if (suffix == "root") {
            typeStr = tr("ROOT File");
        } else {
            typeStr = suffix.toUpper() + tr(" File");
        }
        m_tableFiles->setItem(row, 2, new QTableWidgetItem(typeStr));

        // Date
        QString dateStr = entry.lastModified.isValid() ? entry.lastModified.toString("yyyy-MM-dd hh:mm") : "--";
        m_tableFiles->setItem(row, 3, new QTableWidgetItem(dateStr));

        m_tableFiles->setRowHeight(row, 28);
        row++;
    }
}

void NuFileDialog::onNavigateUp()
{
    QString cur = m_editCurrentPath->text().trimmed();
    if (cur == "/" || cur.isEmpty()) return;
    int lastSlash = cur.lastIndexOf('/');
    QString parent = (lastSlash <= 0) ? "/" : cur.left(lastSlash);
    loadDirectory(parent);
}

void NuFileDialog::onNavigateHome()
{
    if (m_isRemoteMode) {
        if (!m_session) return;
        QString home = "/home/" + m_session->getProfile().user;
        loadDirectory(home);
    } else {
        loadDirectory(QDir::homePath());
    }
}

void NuFileDialog::onRefresh()
{
    loadDirectory(m_editCurrentPath->text().trimmed());
}

void NuFileDialog::onNewFolder()
{
    bool ok = false;
    QString folderName = QInputDialog::getText(
        this, tr("Create New Directory"),
        tr("Enter new directory name:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || folderName.trimmed().isEmpty()) return;

    folderName = folderName.trimmed();
    QString cur = m_editCurrentPath->text().trimmed();
    QString fullPath = cur.endsWith('/') ? cur + folderName : cur + "/" + folderName;

    if (m_isRemoteMode) {
        QString err;
        if (!m_session->createDirectory(fullPath, &err)) {
            QMessageBox::critical(this, tr("Directory Error"), tr("Failed to create remote directory:\n%1").arg(err));
            return;
        }
    } else {
        QDir d(cur);
        if (!d.mkdir(folderName)) {
            QMessageBox::critical(this, tr("Directory Error"), tr("Failed to create local directory '%1'.").arg(fullPath));
            return;
        }
    }

    loadDirectory(fullPath);
}

void NuFileDialog::onPathEntered()
{
    QString entered = m_editCurrentPath->text().trimmed();
    if (!entered.isEmpty()) {
        loadDirectory(entered);
    }
}

void NuFileDialog::onFilterChanged()
{
    populateTable();
}

void NuFileDialog::onTableSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_tableFiles->selectedItems();
    if (selectedItems.isEmpty()) {
        if (m_mode != NuFileMode::SelectDirectory && m_mode != NuFileMode::SaveFile) {
            m_btnAction->setEnabled(false);
        }
        return;
    }

    int row = m_tableFiles->currentRow();
    if (row < 0) return;
    QTableWidgetItem *item = m_tableFiles->item(row, 0);
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString path = item->data(Qt::UserRole).toString();

    if (m_mode == NuFileMode::SaveFile) {
        if (!isDir && m_editSaveFileName) {
            m_editSaveFileName->setText(QFileInfo(path).fileName());
        }
        m_btnAction->setEnabled(true);
        return;
    }

    if (m_mode == NuFileMode::SelectDirectory) {
        m_btnAction->setEnabled(true);
        return;
    }

    if (isDir) {
        m_btnAction->setEnabled(false);
        m_lblSelectionInfo->setText(tr("Directory: %1").arg(path));
    } else {
        m_btnAction->setEnabled(true);
        m_lblSelectionInfo->setText(tr("Selected: %1 (%2)").arg(QFileInfo(path).fileName(), m_tableFiles->item(row, 1)->text()));
    }
}

void NuFileDialog::onTableItemDoubleClicked(int row, int /*col*/)
{
    if (row < 0) return;
    QTableWidgetItem *item = m_tableFiles->item(row, 0);
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString path = item->data(Qt::UserRole).toString();

    if (isDir) {
        loadDirectory(path);
    } else {
        onAcceptActionClicked();
    }
}

bool NuFileDialog::downloadRemoteFile(const QString &remotePath, QString &outLocalPath)
{
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_lblSelectionInfo->setText(tr("Downloading %1 from remote DAQ host...").arg(QFileInfo(remotePath).fileName()));
    qApp->processEvents();

    QString err;
    bool ok = m_session->syncRemoteFileToCache(
        remotePath,
        outLocalPath,
        [this](qint64 done, qint64 total) {
            if (total > 0) {
                int pct = static_cast<int>((done * 100) / total);
                m_progressBar->setValue(pct);
                qApp->processEvents();
            }
        },
        &err
    );

    m_progressBar->setVisible(false);

    if (!ok) {
        QMessageBox::critical(this, tr("Remote Download Error"),
            tr("Failed to download '%1':\n%2").arg(remotePath, err));
        m_lblSelectionInfo->setText(tr("Download failed."));
        return false;
    }

    return true;
}

bool NuFileDialog::downloadRemoteFiles(const QStringList &remotePaths, QStringList &outLocalPaths)
{
    outLocalPaths.clear();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    int count = remotePaths.size();
    for (int i = 0; i < count; ++i) {
        const QString &rp = remotePaths[i];
        m_lblSelectionInfo->setText(tr("Downloading remote run (%1/%2): %3 ...")
            .arg(i + 1).arg(count).arg(QFileInfo(rp).fileName()));
        qApp->processEvents();

        QString localCached;
        QString err;
        bool ok = m_session->syncRemoteFileToCache(rp, localCached, nullptr, &err);
        if (!ok) {
            m_progressBar->setVisible(false);
            QMessageBox::critical(this, tr("Download Error"),
                tr("Failed to fetch file '%1':\n%2").arg(rp, err));
            return false;
        }
        outLocalPaths.push_back(localCached);
        m_progressBar->setValue(static_cast<int>((i + 1) * 100 / count));
        qApp->processEvents();
    }

    m_progressBar->setVisible(false);
    return true;
}

void NuFileDialog::onAcceptActionClicked()
{
    // Mode 1: Select Directory
    if (m_mode == NuFileMode::SelectDirectory) {
        if (m_isRemoteMode) {
            m_selectedRemotePath = m_currentRemoteDir;
            m_selectedPath = m_session->getLocalCachePath(m_currentRemoteDir);
        } else {
            m_selectedPath = m_currentLocalDir;
            m_selectedRemotePath.clear();
        }
        accept();
        return;
    }

    // Mode 2: Save File
    if (m_mode == NuFileMode::SaveFile) {
        QString fileName = m_editSaveFileName ? m_editSaveFileName->text().trimmed() : QString();
        if (fileName.isEmpty()) {
            QMessageBox::warning(this, tr("File Name Required"), tr("Please specify a target file name to save."));
            return;
        }

        if (m_isRemoteMode) {
            m_selectedRemotePath = m_currentRemoteDir.endsWith('/')
                ? m_currentRemoteDir + fileName
                : m_currentRemoteDir + "/" + fileName;
            m_selectedPath = m_session->getLocalCachePath(m_selectedRemotePath);
        } else {
            m_selectedPath = m_currentLocalDir.endsWith('/')
                ? m_currentLocalDir + fileName
                : m_currentLocalDir + "/" + fileName;
            m_selectedRemotePath.clear();
        }
        accept();
        return;
    }

    // Mode 3: Open Multiple Files
    if (m_mode == NuFileMode::OpenMultipleFiles) {
        QModelIndexList selectedRows = m_tableFiles->selectionModel()->selectedRows();
        if (selectedRows.isEmpty()) {
            QMessageBox::warning(this, tr("No Files Selected"), tr("Please select at least one run file."));
            return;
        }

        QStringList chosenPaths;
        for (const auto &idx : selectedRows) {
            QTableWidgetItem *item = m_tableFiles->item(idx.row(), 0);
            if (item) {
                bool isDir = item->data(Qt::UserRole + 1).toBool();
                if (!isDir) {
                    chosenPaths.push_back(item->data(Qt::UserRole).toString());
                }
            }
        }

        if (chosenPaths.isEmpty()) return;

        if (m_isRemoteMode) {
            m_selectedRemotePaths = chosenPaths;
            if (!downloadRemoteFiles(chosenPaths, m_selectedPaths)) {
                return;
            }
        } else {
            m_selectedPaths = chosenPaths;
            m_selectedRemotePaths.clear();
        }
        accept();
        return;
    }

    // Mode 4: Open Single File (Spectrum, Matrix, Generic File)
    int row = m_tableFiles->currentRow();
    if (row < 0) return;
    QTableWidgetItem *item = m_tableFiles->item(row, 0);
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString path = item->data(Qt::UserRole).toString();

    if (isDir) {
        loadDirectory(path);
        return;
    }

    if (m_isRemoteMode) {
        m_selectedRemotePath = path;
        QString localCached;
        if (!downloadRemoteFile(path, localCached)) {
            return;
        }
        m_selectedPath = localCached;
    } else {
        m_selectedPath = path;
        m_selectedRemotePath.clear();
    }

    accept();
}

QString NuFileDialog::getSelectedPath() const
{
    return m_selectedPath;
}

QString NuFileDialog::getSelectedRemotePath() const
{
    return m_selectedRemotePath;
}

QStringList NuFileDialog::getSelectedPaths() const
{
    return m_selectedPaths.isEmpty() && !m_selectedPath.isEmpty()
        ? QStringList{m_selectedPath}
        : m_selectedPaths;
}

QStringList NuFileDialog::getSelectedRemotePaths() const
{
    return m_selectedRemotePaths.isEmpty() && !m_selectedRemotePath.isEmpty()
        ? QStringList{m_selectedRemotePath}
        : m_selectedRemotePaths;
}

//==============================================================================
// Static Convenience Methods
//==============================================================================

QString NuFileDialog::getOpenSpectrum(QWidget *parent,
                                      const QString &caption,
                                      const QString &dir,
                                      QString *outRemotePath,
                                      bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::OpenSpectrum, caption, dir, "", parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePath) *outRemotePath = dlg.getSelectedRemotePath();
    return dlg.getSelectedPath();
}

QString NuFileDialog::getOpenMatrix(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    QString *outRemotePath,
                                    bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::OpenMatrix, caption, dir, "", parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePath) *outRemotePath = dlg.getSelectedRemotePath();
    return dlg.getSelectedPath();
}

QString NuFileDialog::getOpenFileName(QWidget *parent,
                                      const QString &caption,
                                      const QString &dir,
                                      const QString &filter,
                                      QString * /*selectedFilter*/,
                                      QString *outRemotePath,
                                      bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::OpenFile, caption, dir, filter, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePath) *outRemotePath = dlg.getSelectedRemotePath();
    return dlg.getSelectedPath();
}

QStringList NuFileDialog::getOpenFileNames(QWidget *parent,
                                           const QString &caption,
                                           const QString &dir,
                                           const QString &filter,
                                           QString * /*selectedFilter*/,
                                           QStringList *outRemotePaths,
                                           bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::OpenMultipleFiles, caption, dir, filter, parent);
    if (dlg.exec() != QDialog::Accepted) return QStringList();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePaths) *outRemotePaths = dlg.getSelectedRemotePaths();
    return dlg.getSelectedPaths();
}

QString NuFileDialog::getSaveFileName(QWidget *parent,
                                      const QString &caption,
                                      const QString &dir,
                                      const QString &filter,
                                      QString * /*selectedFilter*/,
                                      QString *outRemotePath,
                                      bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::SaveFile, caption, dir, filter, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePath) *outRemotePath = dlg.getSelectedRemotePath();
    return dlg.getSelectedPath();
}

QString NuFileDialog::getExistingDirectory(QWidget *parent,
                                           const QString &caption,
                                           const QString &dir,
                                           QString *outRemotePath,
                                           bool *outIsRemote)
{
    NuFileDialog dlg(NuFileMode::SelectDirectory, caption, dir, "", parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    if (outIsRemote) *outIsRemote = dlg.isRemoteSelected();
    if (outRemotePath) *outRemotePath = dlg.getSelectedRemotePath();
    return dlg.getSelectedPath();
}

bool NuFileDialog::finishRemoteSave(const QString &localStagedPath,
                                    const QString &remoteDestPath,
                                    QString *errorMessage,
                                    QWidget *parent)
{
    if (remoteDestPath.isEmpty() || localStagedPath == remoteDestPath) {
        return true; // Local file only, nothing to upload
    }

    auto session = RemoteSession::globalSession();
    if (!session || !session->isConnected()) {
        if (errorMessage) *errorMessage = tr("Cannot upload to remote destination: SSH session is not active.");
        if (parent) {
            QMessageBox::warning(parent, tr("Upload Warning"), *errorMessage);
        }
        return false;
    }

    QString uploadErr;
    bool ok = session->uploadFile(localStagedPath, remoteDestPath, nullptr, &uploadErr);
    if (!ok) {
        if (errorMessage) *errorMessage = uploadErr;
        if (parent) {
            QMessageBox::critical(parent, tr("SFTP Upload Error"),
                tr("Failed to upload file to remote destination:\n%1").arg(uploadErr));
        }
        return false;
    }

    return true;
}
