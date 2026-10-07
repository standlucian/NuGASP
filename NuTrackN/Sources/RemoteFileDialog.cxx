#include "RemoteFileDialog.h"
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
#include <QFileInfo>
#include <QApplication>
#include <QDir>
#include <cmath>

static QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

RemoteFileDialog::RemoteFileDialog(std::shared_ptr<RemoteSession> session, QWidget *parent)
    : QDialog(parent), m_session(session)
{
    setWindowTitle(tr("Remote Data Computer - File Browser"));
    resize(860, 620);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();

    if (m_session && m_session->isConnected()) {
        QString initialDir = m_session->getCurrentRemoteDir();
        if (initialDir.isEmpty()) {
            initialDir = m_session->getProfile().lastUsedPath;
        }
        if (initialDir.isEmpty()) {
            initialDir = "/home/" + m_session->getProfile().user;
        }
        loadDirectory(initialDir);
    }
}

void RemoteFileDialog::setupUI()
{
    const QFont dlgFont = Design::getDialogFont();
    const int pt = dlgFont.pointSize() > 0 ? dlgFont.pointSize() : 11;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. Top Navigation Bar
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);

    m_btnUp = new QPushButton(tr("▲ Up"), this);
    m_btnUp->setFont(dlgFont);
    m_btnUp->setToolTip(tr("Navigate to parent directory"));
    connect(m_btnUp, &QPushButton::clicked, this, &RemoteFileDialog::onNavigateUp);
    navLayout->addWidget(m_btnUp);

    m_btnHome = new QPushButton(tr("⌂ Home"), this);
    m_btnHome->setFont(dlgFont);
    m_btnHome->setToolTip(tr("Go to default user home folder"));
    connect(m_btnHome, &QPushButton::clicked, this, &RemoteFileDialog::onNavigateHome);
    navLayout->addWidget(m_btnHome);

    m_btnRefresh = new QPushButton(tr("↻ Refresh"), this);
    m_btnRefresh->setFont(dlgFont);
    m_btnRefresh->setToolTip(tr("Refresh current directory"));
    connect(m_btnRefresh, &QPushButton::clicked, this, &RemoteFileDialog::onRefresh);
    navLayout->addWidget(m_btnRefresh);

    m_editCurrentPath = new QLineEdit(this);
    m_editCurrentPath->setFont(dlgFont);
    m_editCurrentPath->setPlaceholderText(tr("Enter remote path and press Enter..."));
    connect(m_editCurrentPath, &QLineEdit::returnPressed, this, &RemoteFileDialog::onPathEntered);
    navLayout->addWidget(m_editCurrentPath, 1);

    mainLayout->addLayout(navLayout);

    // 2. Filter & Search Bar
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    QLabel *lblFilter = new QLabel(tr("Filter:"), this);
    lblFilter->setFont(dlgFont);
    filterLayout->addWidget(lblFilter);

    m_comboTypeFilter = new QComboBox(this);
    m_comboTypeFilter->setFont(dlgFont);
    m_comboTypeFilter->addItem(tr("All Nuclear Data (*.spe *.spk *.mat *.cmat *.chn *.root *.asc *.dat)"), "data");
    m_comboTypeFilter->addItem(tr("1D Spectra (*.spe *.spk *.chn *.asc *.dat)"), "spectra");
    m_comboTypeFilter->addItem(tr("2D Matrices (*.cmat *.mat)"), "matrix");
    m_comboTypeFilter->addItem(tr("ROOT Files (*.root)"), "root");
    m_comboTypeFilter->addItem(tr("All Files (*.*)"), "all");
    connect(m_comboTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteFileDialog::onFilterChanged);
    filterLayout->addWidget(m_comboTypeFilter);

    QLabel *lblSearch = new QLabel(tr("Search:"), this);
    lblSearch->setFont(dlgFont);
    filterLayout->addWidget(lblSearch);

    m_editSearchFilter = new QLineEdit(this);
    m_editSearchFilter->setFont(dlgFont);
    m_editSearchFilter->setPlaceholderText(tr("Filter by filename / run..."));
    connect(m_editSearchFilter, &QLineEdit::textChanged, this, &RemoteFileDialog::onFilterChanged);
    filterLayout->addWidget(m_editSearchFilter, 1);

    mainLayout->addLayout(filterLayout);

    // 3. Remote File Table
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
    m_tableFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_tableFiles, &QTableWidget::cellDoubleClicked, this, &RemoteFileDialog::onTableItemDoubleClicked);
    connect(m_tableFiles, &QTableWidget::itemSelectionChanged, this, &RemoteFileDialog::onTableSelectionChanged);

    mainLayout->addWidget(m_tableFiles, 1);

    // 4. Status, Info & Progress Bar
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

    // 5. Action Buttons Bar
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    m_btnOpenSpectrum = new QPushButton(tr("Open Spectrum (1D)"), this);
    m_btnOpenSpectrum->setFont(dlgFont);
    m_btnOpenSpectrum->setEnabled(false);
    m_btnOpenSpectrum->setStyleSheet(QString("QPushButton { background-color: %1; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
                                             "QPushButton:hover { opacity: 0.9; }")
                                     .arg(Design::getDialogAccentColor().name()));
    connect(m_btnOpenSpectrum, &QPushButton::clicked, this, &RemoteFileDialog::onOpenSpectrumClicked);
    actionLayout->addWidget(m_btnOpenSpectrum);

    m_btnOpenMatrix = new QPushButton(tr("Open Matrix (2D)"), this);
    m_btnOpenMatrix->setFont(dlgFont);
    m_btnOpenMatrix->setEnabled(false);
    connect(m_btnOpenMatrix, &QPushButton::clicked, this, &RemoteFileDialog::onOpenMatrixClicked);
    actionLayout->addWidget(m_btnOpenMatrix);

    m_btnDownloadOnly = new QPushButton(tr("Cache to Disk"), this);
    m_btnDownloadOnly->setFont(dlgFont);
    m_btnDownloadOnly->setEnabled(false);
    connect(m_btnDownloadOnly, &QPushButton::clicked, this, &RemoteFileDialog::onDownloadOnlyClicked);
    actionLayout->addWidget(m_btnDownloadOnly);

    actionLayout->addStretch(1);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setFont(dlgFont);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    actionLayout->addWidget(m_btnClose);

    mainLayout->addLayout(actionLayout);
}

void RemoteFileDialog::loadDirectory(const QString &dirPath)
{
    if (!m_session || !m_session->isConnected()) return;

    m_lblSelectionInfo->setText(tr("Loading directory: %1 ...").arg(dirPath));
    qApp->processEvents();

    QString err;
    if (!m_session->listDirectory(dirPath, m_currentEntries, &err)) {
        QMessageBox::critical(this, tr("Remote Directory Error"), err);
        m_lblSelectionInfo->setText(tr("Failed to read directory."));
        return;
    }

    m_currentDir = m_session->getCurrentRemoteDir();
    m_editCurrentPath->setText(m_currentDir);
    populateTable();
    m_lblSelectionInfo->setText(tr("%1 items in %2").arg(m_currentEntries.size()).arg(m_currentDir));
}

void RemoteFileDialog::populateTable()
{
    m_tableFiles->setRowCount(0);
    const QString search = m_editSearchFilter->text().trimmed().toLower();
    const QString filterType = m_comboTypeFilter->currentData().toString();

    int row = 0;
    for (const auto &entry : m_currentEntries) {
        // Filter out non-matching search text
        if (!search.isEmpty() && !entry.name.toLower().contains(search)) {
            continue;
        }

        QString suffix = QFileInfo(entry.name).suffix().toLower();
        if (!entry.isDirectory && filterType != "all") {
            if (filterType == "data") {
                if (suffix != "spe" && suffix != "spk" && suffix != "mat" &&
                    suffix != "cmat" && suffix != "chn" && suffix != "root" &&
                    suffix != "asc" && suffix != "dat" && suffix != "cal") {
                    continue;
                }
            } else if (filterType == "spectra") {
                if (suffix != "spe" && suffix != "spk" && suffix != "chn" &&
                    suffix != "asc" && suffix != "dat") {
                    continue;
                }
            } else if (filterType == "matrix") {
                if (suffix != "cmat" && suffix != "mat") {
                    continue;
                }
            } else if (filterType == "root") {
                if (suffix != "root") {
                    continue;
                }
            }
        }

        m_tableFiles->insertRow(row);

        // Name item
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

        // Size item
        QString sizeStr = entry.isDirectory ? "--" : formatFileSize(entry.size);
        QTableWidgetItem *itemSize = new QTableWidgetItem(sizeStr);
        itemSize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableFiles->setItem(row, 1, itemSize);

        // Type item
        QString typeStr;
        if (entry.isDirectory) {
            typeStr = tr("Directory");
        } else if (suffix == "cmat" || suffix == "mat") {
            typeStr = tr("2D Coincidence Matrix");
        } else if (suffix == "spe" || suffix == "spk" || suffix == "chn") {
            typeStr = tr("1D Spectrum");
        } else if (suffix == "root") {
            typeStr = tr("ROOT File");
        } else if (suffix == "cal") {
            typeStr = tr("Calibration Data");
        } else {
            typeStr = suffix.toUpper() + tr(" File");
        }
        m_tableFiles->setItem(row, 2, new QTableWidgetItem(typeStr));

        // Date item
        QString dateStr = entry.lastModified.isValid() ? entry.lastModified.toString("yyyy-MM-dd hh:mm") : "--";
        m_tableFiles->setItem(row, 3, new QTableWidgetItem(dateStr));

        m_tableFiles->setRowHeight(row, 28);
        row++;
    }
}

void RemoteFileDialog::onNavigateUp()
{
    if (m_currentDir == "/" || m_currentDir.isEmpty()) return;
    int lastSlash = m_currentDir.lastIndexOf('/');
    QString parent = (lastSlash <= 0) ? "/" : m_currentDir.left(lastSlash);
    loadDirectory(parent);
}

void RemoteFileDialog::onNavigateHome()
{
    if (!m_session) return;
    QString home = "/home/" + m_session->getProfile().user;
    loadDirectory(home);
}

void RemoteFileDialog::onRefresh()
{
    loadDirectory(m_currentDir);
}

void RemoteFileDialog::onPathEntered()
{
    QString entered = m_editCurrentPath->text().trimmed();
    if (!entered.isEmpty()) {
        loadDirectory(entered);
    }
}

void RemoteFileDialog::onFilterChanged()
{
    populateTable();
}

void RemoteFileDialog::onTableSelectionChanged()
{
    int row = m_tableFiles->currentRow();
    if (row < 0) {
        m_btnOpenSpectrum->setEnabled(false);
        m_btnOpenMatrix->setEnabled(false);
        m_btnDownloadOnly->setEnabled(false);
        return;
    }

    QTableWidgetItem *item = m_tableFiles->item(row, 0);
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString path = item->data(Qt::UserRole).toString();
    m_selectedRemotePath = path;

    if (isDir) {
        m_btnOpenSpectrum->setEnabled(false);
        m_btnOpenMatrix->setEnabled(false);
        m_btnDownloadOnly->setEnabled(false);
        m_lblSelectionInfo->setText(tr("Folder: %1").arg(path));
    } else {
        QString suffix = QFileInfo(path).suffix().toLower();
        bool isMatrix = (suffix == "cmat" || suffix == "mat");
        m_btnOpenSpectrum->setEnabled(!isMatrix);
        m_btnOpenMatrix->setEnabled(isMatrix);
        m_btnDownloadOnly->setEnabled(true);
        m_lblSelectionInfo->setText(tr("Selected: %1 (%2)").arg(QFileInfo(path).fileName(), m_tableFiles->item(row, 1)->text()));
    }
}

void RemoteFileDialog::onTableItemDoubleClicked(int row, int /*column*/)
{
    if (row < 0) return;
    QTableWidgetItem *item = m_tableFiles->item(row, 0);
    if (!item) return;

    bool isDir = item->data(Qt::UserRole + 1).toBool();
    QString path = item->data(Qt::UserRole).toString();

    if (isDir) {
        loadDirectory(path);
    } else {
        QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == "cmat" || suffix == "mat") {
            onOpenMatrixClicked();
        } else {
            onOpenSpectrumClicked();
        }
    }
}

bool RemoteFileDialog::downloadSelectedFile(QString &outLocalPath)
{
    if (m_selectedRemotePath.isEmpty()) return false;

    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_lblSelectionInfo->setText(tr("Downloading %1 to local cache...").arg(QFileInfo(m_selectedRemotePath).fileName()));
    qApp->processEvents();

    QString err;
    bool ok = m_session->syncRemoteFileToCache(
        m_selectedRemotePath,
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
        QMessageBox::critical(this, tr("Download Error"),
            tr("Failed to fetch file from remote computer:\n%1").arg(err));
        m_lblSelectionInfo->setText(tr("Download failed."));
        return false;
    }

    m_localCachedPath = outLocalPath;
    m_lblSelectionInfo->setText(tr("Cached locally: %1").arg(QFileInfo(outLocalPath).fileName()));
    return true;
}

void RemoteFileDialog::onOpenSpectrumClicked()
{
    QString localPath;
    if (downloadSelectedFile(localPath)) {
        m_openAction = RemoteOpenAction::OpenSpectrum;
        accept();
    }
}

void RemoteFileDialog::onOpenMatrixClicked()
{
    QString localPath;
    if (downloadSelectedFile(localPath)) {
        m_openAction = RemoteOpenAction::OpenMatrix;
        accept();
    }
}

void RemoteFileDialog::onDownloadOnlyClicked()
{
    QString localPath;
    if (downloadSelectedFile(localPath)) {
        QMessageBox::information(this, tr("File Cached"),
            tr("Remote file cached successfully to:\n%1").arg(localPath));
    }
}
