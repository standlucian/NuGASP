#include "RunByRunManager.h"
#include "canvas.h"
#include "Design.h"
#include "calib.h"
#include "tracknhistogram.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QCollator>
#include <QToolTip>

RunByRunManager::RunByRunManager(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Run-by-Run Energy Calibration Manager (In-Beam Drift Alignment)"));
    resize(1120, 760);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    // Default reference anchors for in-beam HPGe arrays
    m_anchors = {
        { 238.97,  239.0, 15.0, 25.0, 2.5 },
        { 511.00,  511.0, 15.0, 20.0, 3.5 },
        { 1055.00, 1055.0, 15.0, 20.0, 2.5 }
    };

    discoverRuns();
    setupUI();
}

RunByRunManager::~RunByRunManager()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_engine) m_engine->requestStop();
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

//==============================================================================
// discoverRuns
//==============================================================================
void RunByRunManager::discoverRuns()
{
    m_runFilePaths.clear();
    QString baseDir;
    QString currentFile;

    if (m_mainCanvas) {
        currentFile = m_mainCanvas->getCurrentSpectrumFile();
    }

    if (!currentFile.isEmpty() && QFile::exists(currentFile)) {
        baseDir = QFileInfo(currentFile).dir().path();
    } else if (QDir("seqRuns").exists()) {
        baseDir = "seqRuns";
    } else if (QDir("NuTrackN/seqRuns").exists()) {
        baseDir = "NuTrackN/seqRuns";
    } else {
        baseDir = QDir::currentPath();
    }

    QDir dir(baseDir);
    QString currentFileName = QFileInfo(currentFile).fileName();
    if (currentFileName.isEmpty()) currentFileName = "G0.0001";

    int lastDot = currentFileName.lastIndexOf('.');
    QString prefix = (lastDot > 0) ? currentFileName.left(lastDot + 1) : "G0.";
    m_commonPrefix = prefix;

    QStringList entries = dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort);
    struct RunSortItem { QString path; int runNum; };
    std::vector<RunSortItem> foundRuns;

    for (const auto &entry : entries) {
        if (entry.startsWith(prefix, Qt::CaseInsensitive)) {
            QString rest = entry.mid(prefix.length());
            bool ok = false;
            int r = rest.toInt(&ok);
            if (ok) {
                foundRuns.push_back({dir.filePath(entry), r});
            }
        }
    }

    std::sort(foundRuns.begin(), foundRuns.end(), [](const RunSortItem &a, const RunSortItem &b) {
        return a.runNum < b.runNum;
    });

    for (const auto &item : foundRuns) {
        m_runFilePaths.push_back(item.path);
    }
}

//==============================================================================
// setupUI
//==============================================================================
void RunByRunManager::setupUI()
{
    const QFont dlgFont = Design::getDialogFont();
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // TOP: Setup Panel
    QGroupBox *grpSetup = new QGroupBox(tr("1. Run Sequence & Detector Configuration"), this);
    QGridLayout *setupGrid = new QGridLayout(grpSetup);
    setupGrid->setSpacing(6);

    // Runs Summary and Browse button
    m_lblRunsSummary = new QLabel(this);
    if (!m_runFilePaths.empty()) {
        m_lblRunsSummary->setText(tr("Found <b>%1</b> runs matching prefix '<b>%2</b>' in %3 (%4 to %5)")
                                      .arg(m_runFilePaths.size())
                                      .arg(m_commonPrefix)
                                      .arg(QFileInfo(m_runFilePaths.front()).dir().dirName())
                                      .arg(QFileInfo(m_runFilePaths.front()).fileName())
                                      .arg(QFileInfo(m_runFilePaths.back()).fileName()));
    } else {
        m_lblRunsSummary->setText(tr("<font color='red'>No sequence run files detected.</font>"));
    }
    setupGrid->addWidget(m_lblRunsSummary, 0, 0, 1, 3);

    QPushButton *btnBrowseRuns = new QPushButton(tr("📁 Browse Runs..."), this);
    connect(btnBrowseRuns, &QPushButton::clicked, this, &RunByRunManager::onBrowseRunsClicked);
    setupGrid->addWidget(btnBrowseRuns, 0, 3);

    // Reference Run selector
    setupGrid->addWidget(new QLabel(tr("Reference Anchor Run:"), this), 1, 0);
    m_comboRefRun = new QComboBox(this);
    for (size_t i = 0; i < m_runFilePaths.size(); ++i) {
        m_comboRefRun->addItem(QString("[%1] %2").arg(i + 1).arg(QFileInfo(m_runFilePaths[i]).fileName()), static_cast<int>(i));
    }
    setupGrid->addWidget(m_comboRefRun, 1, 1);

    // Spectrum Channel Length
    setupGrid->addWidget(new QLabel(tr("Spectrum Channels:"), this), 1, 2);
    m_comboChannelLength = new QComboBox(this);
    m_comboChannelLength->addItem("65536 (64k channels - HPGe standard)", 65536);
    m_comboChannelLength->addItem("16384 (16k channels)", 16384);
    m_comboChannelLength->addItem("8192 (8k channels)", 8192);
    m_comboChannelLength->addItem("4096 (4k channels)", 4096);
    m_comboChannelLength->addItem("2048 (2k channels)", 2048);
    setupGrid->addWidget(m_comboChannelLength, 1, 3);

    // Detector range
    setupGrid->addWidget(new QLabel(tr("Detector Range:"), this), 2, 0);
    QHBoxLayout *detLayout = new QHBoxLayout();
    m_spinDetStart = new QSpinBox(this);
    m_spinDetStart->setRange(0, 255);
    m_spinDetStart->setValue(0);
    m_spinDetEnd = new QSpinBox(this);
    m_spinDetEnd->setRange(0, 255);

    // Auto-detect number of detectors from first file size
    int defaultEndDet = 24;
    if (!m_runFilePaths.empty()) {
        qint64 fSize = QFileInfo(m_runFilePaths.front()).size();
        if (fSize % (65536 * 4) == 0) {
            defaultEndDet = std::max(0, static_cast<int>(fSize / (65536 * 4)) - 1);
        } else if (fSize % (4096 * 4) == 0) {
            defaultEndDet = std::max(0, static_cast<int>(fSize / (4096 * 4)) - 1);
        }
    }
    m_spinDetEnd->setValue(defaultEndDet);

    detLayout->addWidget(new QLabel(tr("From:"), this));
    detLayout->addWidget(m_spinDetStart);
    detLayout->addWidget(new QLabel(tr("To:"), this));
    detLayout->addWidget(m_spinDetEnd);
    setupGrid->addLayout(detLayout, 2, 1);

    // Calibration Model & Drift Tolerance
    setupGrid->addWidget(new QLabel(tr("Calibration Model:"), this), 2, 2);
    QHBoxLayout *modelLayout = new QHBoxLayout();
    m_chkQuadratic = new QCheckBox(tr("Quadratic (a0 + a1*ch + a2*ch^2)"), this);
    m_chkQuadratic->setChecked(false); // Linear is standard for HPGe
    modelLayout->addWidget(m_chkQuadratic);

    m_spinDriftTolerance = new QDoubleSpinBox(this);
    m_spinDriftTolerance->setRange(2.0, 100.0);
    m_spinDriftTolerance->setValue(20.0);
    m_spinDriftTolerance->setSuffix(" ch");
    modelLayout->addWidget(new QLabel(tr("Max Drift:"), this));
    modelLayout->addWidget(m_spinDriftTolerance);
    setupGrid->addLayout(modelLayout, 2, 3);

    mainLayout->addWidget(grpSetup);

    // MIDDLE: Anchors Table & Presets
    QGroupBox *grpAnchors = new QGroupBox(tr("2. In-Beam Anchor Lines (Centroid Trajectory Tracking)"), this);
    QVBoxLayout *anchorVBox = new QVBoxLayout(grpAnchors);

    QHBoxLayout *presetBar = new QHBoxLayout();
    presetBar->addWidget(new QLabel(tr("Quick Presets:"), this));

    QPushButton *btn239 = new QPushButton(tr("+ 238.97 keV"), this);
    QPushButton *btn511 = new QPushButton(tr("+ 511.0 keV"), this);
    QPushButton *btn1055 = new QPushButton(tr("+ 1055.0 keV"), this);
    QPushButton *btn1460 = new QPushButton(tr("+ 1460.8 keV"), this);
    QPushButton *btn2614 = new QPushButton(tr("+ 2614.5 keV"), this);
    QPushButton *btnAddCustom = new QPushButton(tr("+ Add Custom..."), this);
    QPushButton *btnRemoveAnchor = new QPushButton(tr("- Remove Selected"), this);

    connect(btn239, &QPushButton::clicked, this, [this]() { onAddPresetAnchor(238.97, 15.0, 25.0); });
    connect(btn511, &QPushButton::clicked, this, [this]() { onAddPresetAnchor(511.0, 15.0, 20.0); });
    connect(btn1055, &QPushButton::clicked, this, [this]() { onAddPresetAnchor(1055.0, 15.0, 20.0); });
    connect(btn1460, &QPushButton::clicked, this, [this]() { onAddPresetAnchor(1460.8, 15.0, 10.0); });
    connect(btn2614, &QPushButton::clicked, this, [this]() { onAddPresetAnchor(2614.5, 20.0, 8.0); });
    connect(btnAddCustom, &QPushButton::clicked, this, &RunByRunManager::onAddAnchorClicked);
    connect(btnRemoveAnchor, &QPushButton::clicked, this, &RunByRunManager::onRemoveAnchorClicked);

    presetBar->addWidget(btn239);
    presetBar->addWidget(btn511);
    presetBar->addWidget(btn1055);
    presetBar->addWidget(btn1460);
    presetBar->addWidget(btn2614);
    presetBar->addWidget(btnAddCustom);
    presetBar->addStretch(1);
    presetBar->addWidget(btnRemoveAnchor);
    anchorVBox->addLayout(presetBar);

    m_tableAnchors = new QTableWidget(this);
    m_tableAnchors->setColumnCount(5);
    m_tableAnchors->setHorizontalHeaderLabels({
        tr("Physical Energy (keV)"),
        tr("Initial Channel (ch)"),
        tr("Search Window (+- ch)"),
        tr("Min Peak Counts"),
        tr("Expected FWHM (ch)")
    });
    m_tableAnchors->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableAnchors->setMaximumHeight(115);
    anchorVBox->addWidget(m_tableAnchors);
    populateAnchorTable();

    mainLayout->addWidget(grpAnchors);

    // EXECUTION BAR
    QHBoxLayout *execBar = new QHBoxLayout();
    m_btnStart = new QPushButton(tr("▶ Start Run-by-Run Calibration"), this);
    m_btnStart->setStyleSheet("font-weight: bold; background-color: #2e7d32; color: white; padding: 6px 14px;");
    m_btnCancel = new QPushButton(tr("⬛ Cancel"), this);
    m_btnCancel->setEnabled(false);

    m_btnExportCal = new QPushButton(tr("💾 Export Legacy Xtrackn .cal"), this);
    m_btnExportCal->setEnabled(false);
    m_btnExportUcal = new QPushButton(tr("📊 Export Extended .ucal (Uncertainties)"), this);
    m_btnExportUcal->setEnabled(false);

    connect(m_btnStart, &QPushButton::clicked, this, &RunByRunManager::onStartCalibrationClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &RunByRunManager::onCancelCalibrationClicked);
    connect(m_btnExportCal, &QPushButton::clicked, this, &RunByRunManager::onExportCalClicked);
    connect(m_btnExportUcal, &QPushButton::clicked, this, &RunByRunManager::onExportUcalClicked);

    execBar->addWidget(m_btnStart);
    execBar->addWidget(m_btnCancel);
    execBar->addSpacing(20);
    execBar->addWidget(m_btnExportCal);
    execBar->addWidget(m_btnExportUcal);
    execBar->addStretch(1);
    mainLayout->addLayout(execBar);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_lblStatusText = new QLabel(tr("Ready. Configure anchor lines and click 'Start Run-by-Run Calibration'."), this);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_lblStatusText);

    // SPLITTER: 2D Grid (Left) + Drift Trends Table & View (Right)
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left: 2D Status Grid
    QGroupBox *grpMatrix = new QGroupBox(tr("3. 2D Run × Detector Alignment Matrix (Double-click cell to inspect)"), this);
    QVBoxLayout *matLayout = new QVBoxLayout(grpMatrix);
    m_gridMatrix = new QTableWidget(this);
    m_gridMatrix->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_gridMatrix, &QTableWidget::cellDoubleClicked, this, &RunByRunManager::onCellDoubleClicked);
    matLayout->addWidget(m_gridMatrix);
    splitter->addWidget(grpMatrix);

    // Right: Detector Drift Inspector
    QGroupBox *grpPlot = new QGroupBox(tr("4. Detector Drift Inspection"), this);
    QVBoxLayout *plotLayout = new QVBoxLayout(grpPlot);

    QHBoxLayout *plotSelectBar = new QHBoxLayout();
    plotSelectBar->addWidget(new QLabel(tr("Inspect Detector:"), this));
    m_comboPlotDet = new QComboBox(this);
    connect(m_comboPlotDet, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunByRunManager::onDetectorPlotSelectionChanged);
    plotSelectBar->addWidget(m_comboPlotDet, 1);
    plotLayout->addLayout(plotSelectBar);

    m_tablePlotData = new QTableWidget(this);
    m_tablePlotData->setColumnCount(7);
    m_tablePlotData->setHorizontalHeaderLabels({
        tr("Run #"), tr("Run File"), tr("Gain a1 (keV/ch)"), tr("Offset a0 (keV)"),
        tr("Residual (keV)"), tr("Chi2/ndf"), tr("Status")
    });
    m_tablePlotData->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    plotLayout->addWidget(m_tablePlotData);
    splitter->addWidget(grpPlot);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter, 1);
}

//==============================================================================
// populateAnchorTable
//==============================================================================
void RunByRunManager::populateAnchorTable()
{
    m_tableAnchors->setRowCount(static_cast<int>(m_anchors.size()));
    for (int i = 0; i < static_cast<int>(m_anchors.size()); ++i) {
        const auto &a = m_anchors[i];
        m_tableAnchors->setItem(i, 0, new QTableWidgetItem(QString::number(a.physicalEnergy, 'f', 2)));
        m_tableAnchors->setItem(i, 1, new QTableWidgetItem(QString::number(a.initialChannel, 'f', 1)));
        m_tableAnchors->setItem(i, 2, new QTableWidgetItem(QString::number(a.searchWindowCh, 'f', 1)));
        m_tableAnchors->setItem(i, 3, new QTableWidgetItem(QString::number(a.minCounts, 'f', 0)));
        m_tableAnchors->setItem(i, 4, new QTableWidgetItem(QString::number(a.expectedFwhmCh, 'f', 1)));
    }
}

void RunByRunManager::onAddPresetAnchor(double energy, double windowCh, double minCounts)
{
    for (const auto &a : m_anchors) {
        if (std::abs(a.physicalEnergy - energy) < 0.5) return; // Already present
    }
    m_anchors.push_back({ energy, energy, windowCh, minCounts, 5.0 });
    populateAnchorTable();
}

void RunByRunManager::onBrowseRunsClicked()
{
    QString selected = QFileDialog::getOpenFileName(this, tr("Select a Sequence Run File"),
                                                    m_runFilePaths.empty() ? QString() : m_runFilePaths.front(),
                                                    tr("Run Files (*.*)"));
    if (selected.isEmpty()) return;

    QFileInfo fi(selected);
    QDir dir = fi.dir();
    QString fileName = fi.fileName();
    int lastDot = fileName.lastIndexOf('.');
    QString prefix = (lastDot > 0) ? fileName.left(lastDot + 1) : fileName;
    m_commonPrefix = prefix;
    m_runFilePaths.clear();

    QStringList entries = dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort);
    struct RunSortItem { QString path; int runNum; };
    std::vector<RunSortItem> foundRuns;

    for (const auto &entry : entries) {
        if (entry.startsWith(prefix, Qt::CaseInsensitive)) {
            QString rest = entry.mid(prefix.length());
            bool ok = false;
            int r = rest.toInt(&ok);
            if (ok) {
                foundRuns.push_back({dir.filePath(entry), r});
            }
        }
    }

    std::sort(foundRuns.begin(), foundRuns.end(), [](const RunSortItem &a, const RunSortItem &b) {
        return a.runNum < b.runNum;
    });

    for (const auto &item : foundRuns) {
        m_runFilePaths.push_back(item.path);
    }

    if (!m_runFilePaths.empty()) {
        m_lblRunsSummary->setText(tr("Found <b>%1</b> runs matching prefix '<b>%2</b>' in %3 (%4 to %5)")
                                      .arg(m_runFilePaths.size())
                                      .arg(m_commonPrefix)
                                      .arg(dir.dirName())
                                      .arg(QFileInfo(m_runFilePaths.front()).fileName())
                                      .arg(QFileInfo(m_runFilePaths.back()).fileName()));

        m_comboRefRun->clear();
        for (size_t i = 0; i < m_runFilePaths.size(); ++i) {
            m_comboRefRun->addItem(QString("[%1] %2").arg(i + 1).arg(QFileInfo(m_runFilePaths[i]).fileName()), static_cast<int>(i));
        }

        qint64 fSize = QFileInfo(m_runFilePaths.front()).size();
        if (fSize % (65536 * 4) == 0) {
            m_spinDetEnd->setValue(std::max(0, static_cast<int>(fSize / (65536 * 4)) - 1));
            m_comboChannelLength->setCurrentIndex(0); // 65536
        } else if (fSize % (4096 * 4) == 0) {
            m_spinDetEnd->setValue(std::max(0, static_cast<int>(fSize / (4096 * 4)) - 1));
            m_comboChannelLength->setCurrentIndex(3); // 4096
        }
    } else {
        m_lblRunsSummary->setText(tr("<font color='red'>No sequence run files detected for prefix '%1'.</font>").arg(prefix));
    }
}

void RunByRunManager::onAddAnchorClicked()
{
    m_anchors.push_back({ 1000.0, 1000.0, 15.0, 15.0, 5.0 });
    populateAnchorTable();
}

void RunByRunManager::onRemoveAnchorClicked()
{
    int row = m_tableAnchors->currentRow();
    if (row >= 0 && row < static_cast<int>(m_anchors.size())) {
        m_anchors.erase(m_anchors.begin() + row);
        populateAnchorTable();
    }
}

//==============================================================================
// onStartCalibrationClicked
//==============================================================================
void RunByRunManager::onStartCalibrationClicked()
{
    if (m_runFilePaths.empty()) {
        QMessageBox::warning(this, tr("No Runs"), tr("No sequence run files were detected."));
        return;
    }

    // Read back anchors from table
    m_anchors.clear();
    for (int r = 0; r < m_tableAnchors->rowCount(); ++r) {
        AnchorPeakDef a;
        a.physicalEnergy = m_tableAnchors->item(r, 0) ? m_tableAnchors->item(r, 0)->text().toDouble() : 0.0;
        a.initialChannel = m_tableAnchors->item(r, 1) ? m_tableAnchors->item(r, 1)->text().toDouble() : 0.0;
        a.searchWindowCh = m_tableAnchors->item(r, 2) ? m_tableAnchors->item(r, 2)->text().toDouble() : 15.0;
        a.minCounts = m_tableAnchors->item(r, 3) ? m_tableAnchors->item(r, 3)->text().toDouble() : 15.0;
        a.expectedFwhmCh = m_tableAnchors->item(r, 4) ? m_tableAnchors->item(r, 4)->text().toDouble() : 5.0;
        if (a.physicalEnergy > 0.0) {
            m_anchors.push_back(a);
        }
    }

    if (m_anchors.size() < 2) {
        QMessageBox::warning(this, tr("Insufficient Anchors"),
                             tr("Please specify at least 2 anchor lines to calculate energy calibration slope and offset."));
        return;
    }

    const int dStart = m_spinDetStart->value();
    const int dEnd = m_spinDetEnd->value();
    if (dStart > dEnd) {
        QMessageBox::warning(this, tr("Invalid Detectors"), tr("Start detector cannot be greater than end detector."));
        return;
    }

    const int numDets = dEnd - dStart + 1;
    const int numRuns = static_cast<int>(m_runFilePaths.size());

    // Initialize 2D Grid
    m_gridMatrix->clear();
    m_gridMatrix->setRowCount(numRuns);
    m_gridMatrix->setColumnCount(numDets);

    QStringList colHeaders;
    for (int d = dStart; d <= dEnd; ++d) {
        colHeaders << QString("Det #%1").arg(d);
    }
    m_gridMatrix->setHorizontalHeaderLabels(colHeaders);

    QStringList rowHeaders;
    for (int r = 0; r < numRuns; ++r) {
        rowHeaders << QFileInfo(m_runFilePaths[r]).fileName();
    }
    m_gridMatrix->setVerticalHeaderLabels(rowHeaders);

    for (int r = 0; r < numRuns; ++r) {
        for (int c = 0; c < numDets; ++c) {
            QTableWidgetItem *item = new QTableWidgetItem("Pending");
            item->setTextAlignment(Qt::AlignCenter);
            item->setBackground(QBrush(QColor("#37474f")));
            m_gridMatrix->setItem(r, c, item);
        }
    }

    // Populate detector inspector combobox
    m_comboPlotDet->clear();
    for (int d = dStart; d <= dEnd; ++d) {
        m_comboPlotDet->addItem(QString("Detector #%1").arg(d), d);
    }

    // Configure Engine
    RunByRunConfig cfg;
    cfg.runFilePaths = m_runFilePaths;
    cfg.referenceRunIndex = m_comboRefRun->currentIndex();
    cfg.detectorStart = dStart;
    cfg.detectorEnd = dEnd;
    cfg.spectrumChannels = m_comboChannelLength->currentData().toInt();
    cfg.spectrumFormat = SpectrumFormat::LongInt32;
    cfg.anchors = m_anchors;
    cfg.useQuadratic = m_chkQuadratic->isChecked();
    cfg.maxAllowedDriftCh = m_spinDriftTolerance->value();

    m_completedTrajectories.clear();
    m_isRunning = true;
    m_btnStart->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_btnExportCal->setEnabled(false);
    m_btnExportUcal->setEnabled(false);
    m_progressBar->setValue(0);

    m_engine = std::make_unique<RunByRunEngine>();
    m_engine->setConfig(cfg);

    m_workerThread = new QThread(this);
    m_engine->moveToThread(m_workerThread);

    connect(m_engine.get(), &RunByRunEngine::detectorStarted, this, &RunByRunManager::onDetectorStarted);
    connect(m_engine.get(), &RunByRunEngine::runProcessed, this, &RunByRunManager::onRunProcessed);
    connect(m_engine.get(), &RunByRunEngine::detectorFinished, this, &RunByRunManager::onDetectorFinished);
    connect(m_engine.get(), &RunByRunEngine::progressUpdated, this, &RunByRunManager::onProgressUpdated);
    connect(m_engine.get(), &RunByRunEngine::batchFinished, this, &RunByRunManager::onBatchFinished);

    connect(m_workerThread, &QThread::started, m_engine.get(), &RunByRunEngine::run);

    m_workerThread->start();
}

void RunByRunManager::onCancelCalibrationClicked()
{
    if (m_engine) {
        m_engine->requestStop();
        m_lblStatusText->setText(tr("Cancelling calibration run..."));
    }
}

void RunByRunManager::onDetectorStarted(int detId, int totalDetectors)
{
    m_lblStatusText->setText(tr("Processing Detector #%1 of %2 through all runs...").arg(detId).arg(totalDetectors));
}

void RunByRunManager::onRunProcessed(int detId, int runIndex, int totalRuns, const RunCalibResult &result)
{
    Q_UNUSED(totalRuns);
    const int col = detId - m_spinDetStart->value();
    const int row = runIndex;
    if (row < 0 || row >= m_gridMatrix->rowCount() || col < 0 || col >= m_gridMatrix->columnCount()) return;

    QTableWidgetItem *item = m_gridMatrix->item(row, col);
    if (!item) {
        item = new QTableWidgetItem();
        m_gridMatrix->setItem(row, col, item);
    }

    if (result.status == RunCalibStatus::Success) {
        item->setText(QString::number(result.a1, 'f', 6));
        item->setBackground(QBrush(QColor("#2e7d32"))); // Green
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2\na0: %3 keV\na1: %4 keV/ch\ns_res: %5 keV\nchi2/ndf: %6")
                             .arg(result.runFileName).arg(detId)
                             .arg(result.a0, 0, 'f', 4)
                             .arg(result.a1, 0, 'f', 6)
                             .arg(result.sRes, 0, 'f', 4)
                             .arg(result.chi2NDF, 0, 'f', 2));
    } else {
        item->setText(tr("FAIL"));
        item->setBackground(QBrush(QColor("#c62828"))); // Red
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2\nFAILURE: %3")
                             .arg(result.runFileName).arg(detId).arg(result.failureReason));
    }
}

void RunByRunManager::onDetectorFinished(int detId, int successCount, int failureCount)
{
    Q_UNUSED(detId);
    Q_UNUSED(successCount);
    Q_UNUSED(failureCount);
}

void RunByRunManager::onProgressUpdated(int percent, const QString &statusText)
{
    m_progressBar->setValue(percent);
    m_lblStatusText->setText(statusText);
}

void RunByRunManager::onBatchFinished(bool cancelled, int totalSuccess, int totalFailures)
{
    m_isRunning = false;
    m_btnStart->setEnabled(true);
    m_btnCancel->setEnabled(false);
    m_btnExportCal->setEnabled(true);
    m_btnExportUcal->setEnabled(true);

    if (m_engine) {
        m_completedTrajectories = m_engine->getTrajectories();
    }

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }

    if (cancelled) {
        m_lblStatusText->setText(tr("Calibration cancelled by user. Partial results saved."));
    } else {
        m_lblStatusText->setText(tr("Batch complete! Successfully calibrated: %1. Flagged issues: %2.")
                                     .arg(totalSuccess).arg(totalFailures));
        m_progressBar->setValue(100);

        // Update plot for currently selected detector
        if (m_comboPlotDet->count() > 0) {
            updatePlot(m_comboPlotDet->currentData().toInt());
        }

        // PROMPT USER AT THE END IF ANY ISSUES OCCURRED
        if (totalFailures > 0) {
            showIssueReportModal(totalSuccess, totalFailures);
        }
    }
}

//==============================================================================
// showIssueReportModal: Prompts the user when runs/detectors have issues
//==============================================================================
void RunByRunManager::showIssueReportModal(int totalSuccess, int totalFailures)
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Run-by-Run Calibration Issues Report"));
    dlg.resize(750, 480);
    dlg.setFont(Design::getDialogFont());
    dlg.setStyleSheet(Design::getDialogStyleSheet());

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    vbox->setSpacing(8);

    QLabel *lblHeader = new QLabel(&dlg);
    lblHeader->setText(QString("<h3>⚠️ Calibration Completed with %1 Flagged Issues (%2 Successful)</h3>"
                               "<p>The following run/detector spectra encountered issues (no silent interpolation was performed):</p>")
                           .arg(totalFailures).arg(totalSuccess));
    vbox->addWidget(lblHeader);

    QTableWidget *tableIssues = new QTableWidget(&dlg);
    tableIssues->setColumnCount(4);
    tableIssues->setHorizontalHeaderLabels({ tr("Detector"), tr("Run #"), tr("Run File"), tr("Diagnostic Reason") });
    tableIssues->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableIssues->horizontalHeader()->setStretchLastSection(true);

    int row = 0;
    for (const auto &traj : m_completedTrajectories) {
        for (const auto &r : traj.runResults) {
            if (r.status != RunCalibStatus::Success) {
                tableIssues->insertRow(row);
                tableIssues->setItem(row, 0, new QTableWidgetItem(QString::number(r.detectorId)));
                tableIssues->setItem(row, 1, new QTableWidgetItem(QString::number(r.runNumber)));
                tableIssues->setItem(row, 2, new QTableWidgetItem(r.runFileName));
                QTableWidgetItem *rItem = new QTableWidgetItem(r.failureReason);
                rItem->setForeground(QBrush(QColor("#ff5252")));
                tableIssues->setItem(row, 3, rItem);
                row++;
            }
        }
    }
    vbox->addWidget(tableIssues, 1);

    QHBoxLayout *btnBox = new QHBoxLayout();
    QPushButton *btnInspect = new QPushButton(tr("Inspect Selected in Main Matrix"), &dlg);
    QPushButton *btnClose = new QPushButton(tr("Close Report"), &dlg);
    btnBox->addWidget(btnInspect);
    btnBox->addStretch(1);
    btnBox->addWidget(btnClose);
    vbox->addLayout(btnBox);

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(btnInspect, &QPushButton::clicked, [&]() {
        int r = tableIssues->currentRow();
        if (r >= 0) {
            int detId = tableIssues->item(r, 0)->text().toInt();
            QString runName = tableIssues->item(r, 2)->text();
            for (int rowIdx = 0; rowIdx < m_gridMatrix->rowCount(); ++rowIdx) {
                if (m_gridMatrix->verticalHeaderItem(rowIdx)->text() == runName) {
                    int colIdx = detId - m_spinDetStart->value();
                    m_gridMatrix->setCurrentCell(rowIdx, colIdx);
                    m_gridMatrix->scrollToItem(m_gridMatrix->item(rowIdx, colIdx));
                    break;
                }
            }
        }
        dlg.accept();
    });

    dlg.exec();
}

//==============================================================================
// updatePlot
//==============================================================================
void RunByRunManager::updatePlot(int detId)
{
    m_tablePlotData->setRowCount(0);

    const DetectorTrajectory *targetTraj = nullptr;
    for (const auto &traj : m_completedTrajectories) {
        if (traj.detectorId == detId) {
            targetTraj = &traj;
            break;
        }
    }
    if (!targetTraj) return;

    m_tablePlotData->setRowCount(static_cast<int>(targetTraj->runResults.size()));
    for (int r = 0; r < static_cast<int>(targetTraj->runResults.size()); ++r) {
        const auto &res = targetTraj->runResults[r];
        m_tablePlotData->setItem(r, 0, new QTableWidgetItem(QString::number(res.runNumber)));
        m_tablePlotData->setItem(r, 1, new QTableWidgetItem(res.runFileName));

        if (res.status == RunCalibStatus::Success) {
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem(QString::number(res.a1, 'f', 6)));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem(QString::number(res.a0, 'f', 4)));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem(QString::number(res.sRes, 'f', 4)));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem(QString::number(res.chi2NDF, 'f', 2)));
            QTableWidgetItem *st = new QTableWidgetItem("OK");
            st->setForeground(QBrush(QColor("#4caf50")));
            m_tablePlotData->setItem(r, 6, st);
        } else {
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem("-"));
            QTableWidgetItem *st = new QTableWidgetItem("FAILED");
            st->setForeground(QBrush(QColor("#f44336")));
            st->setToolTip(res.failureReason);
            m_tablePlotData->setItem(r, 6, st);
        }
    }
}

void RunByRunManager::onDetectorPlotSelectionChanged(int detIdx)
{
    Q_UNUSED(detIdx);
    if (m_comboPlotDet->count() > 0) {
        updatePlot(m_comboPlotDet->currentData().toInt());
    }
}

//==============================================================================
// onCellDoubleClicked: Jump directly to the spectrum in NuTrackN
//==============================================================================
void RunByRunManager::onCellDoubleClicked(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_runFilePaths.size())) return;
    const int detId = col + m_spinDetStart->value();
    const QString filePath = m_runFilePaths[row];

    if (m_mainCanvas) {
        // Load target run and detector index into active canvas
        std::vector<double> specData;
        QString err;
        const int chLen = m_comboChannelLength->currentData().toInt();
        if (ReadSpectrumData(filePath.toStdString(), SpectrumFormat::LongInt32, chLen, detId, specData, &err) && !specData.empty()) {
            m_mainCanvas->loadSpectrumDataToPad(specData, QString("%1#%2").arg(QFileInfo(filePath).fileName()).arg(detId), false);
            CommandPrompt::getInstance()->appendPlainText(
                QString("Inspecting run %1 [Detector #%2] from RunByRunManager\n")
                    .arg(QFileInfo(filePath).fileName()).arg(detId));
        }
    }
}

//==============================================================================
// onExportCalClicked: Strict legacy Xtrackn format
//==============================================================================
void RunByRunManager::onExportCalClicked()
{
    if (m_completedTrajectories.empty()) {
        QMessageBox::warning(this, tr("No Data"), tr("No calibration results to export."));
        return;
    }

    std::vector<RunCalibResult> allResults;
    for (const auto &traj : m_completedTrajectories) {
        for (const auto &r : traj.runResults) {
            allResults.push_back(r);
        }
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Export Legacy Xtrackn .cal"));
    msgBox.setText(tr("How would you like to export the legacy Xtrackn .cal files?"));
    QPushButton *btnMaster = msgBox.addButton(tr("Single Master .cal File"), QMessageBox::ActionRole);
    QPushButton *btnPerRun = msgBox.addButton(tr("Directory of Per-Run Files (<run>.cal)"), QMessageBox::ActionRole);
    QPushButton *btnCancel = msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == btnCancel) return;

    if (msgBox.clickedButton() == btnMaster) {
        QString savePath = QFileDialog::getSaveFileName(this, tr("Save Master Xtrackn Calibration File"),
                                                        "run_calibrations.cal", tr("Xtrackn Calibration (*.cal);;All Files (*.*)"));
        if (!savePath.isEmpty()) {
            QString err;
            if (SaveXtracknCalFile(savePath, allResults, &err)) {
                QMessageBox::information(this, tr("Export Successful"),
                                         tr("Successfully saved %1 calibration records to:\n%2")
                                             .arg(allResults.size()).arg(savePath));
            } else {
                QMessageBox::critical(this, tr("Export Error"), err);
            }
        }
    } else if (msgBox.clickedButton() == btnPerRun) {
        QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select Output Directory for Per-Run .cal Files"));
        if (!dirPath.isEmpty()) {
            QString err;
            if (SaveXtracknPerRunCalFiles(dirPath, allResults, &err)) {
                QMessageBox::information(this, tr("Export Successful"),
                                         tr("Successfully generated per-run .cal files in directory:\n%1").arg(dirPath));
            } else {
                QMessageBox::critical(this, tr("Export Error"), err);
            }
        }
    }
}

//==============================================================================
// onExportUcalClicked: NuTrackN Extended format with uncertainties
//==============================================================================
void RunByRunManager::onExportUcalClicked()
{
    if (m_completedTrajectories.empty()) {
        QMessageBox::warning(this, tr("No Data"), tr("No calibration results to export."));
        return;
    }

    std::vector<RunCalibResult> allResults;
    for (const auto &traj : m_completedTrajectories) {
        for (const auto &r : traj.runResults) {
            allResults.push_back(r);
        }
    }

    QString savePath = QFileDialog::getSaveFileName(this, tr("Save NuTrackN Extended Calibration (.ucal)"),
                                                    "run_calibrations.ucal", tr("NuTrackN Uncertainty Calib (*.ucal);;All Files (*.*)"));
    if (!savePath.isEmpty()) {
        QString err;
        if (SaveNuTrackNUcalFile(savePath, allResults, &err)) {
            QMessageBox::information(this, tr("Export Successful"),
                                     tr("Successfully saved %1 uncertainty calibration records to:\n%2")
                                         .arg(allResults.size()).arg(savePath));
        } else {
            QMessageBox::critical(this, tr("Export Error"), err);
        }
    }
}
