#include "RunByRunManager.h"
#include "canvas.h"
#include "Design.h"
#include "calib.h"
#include "tracknhistogram.h"

#include "TH1F.h"
#include "TF1.h"
#include "TLine.h"
#include "TFitResult.h"
#include "TVirtualPad.h"

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
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>

//==============================================================================
// DetectorDriftPlotWidget Implementation
//==============================================================================
DetectorDriftPlotWidget::DetectorDriftPlotWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DetectorDriftPlotWidget::setData(int detectorId, const std::vector<DriftPlotPoint> &points)
{
    m_detectorId = detectorId;
    m_points = points;
    m_hoveredIndex = -1;
    update();
}

void DetectorDriftPlotWidget::clearData()
{
    m_detectorId = -1;
    m_points.clear();
    m_hoveredIndex = -1;
    update();
}

void DetectorDriftPlotWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hoveredIndex = -1;
    QToolTip::hideText();
    update();
}

void DetectorDriftPlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_points.empty()) {
        m_hoveredIndex = -1;
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int w = width();
    const int ml = 65, mr = 20;
    const double plotW = std::max(10.0, static_cast<double>(w - ml - mr));
    const int nPts = static_cast<int>(m_points.size());

    int closestIdx = -1;
    double minDx = 1e9;

    for (int i = 0; i < nPts; ++i) {
        double px = (nPts > 1) ? (ml + (static_cast<double>(i) / (nPts - 1)) * plotW)
                               : (ml + plotW / 2.0);
        double dx = std::abs(px - event->pos().x());
        if (dx < minDx) {
            minDx = dx;
            closestIdx = i;
        }
    }

    const double hoverTol = std::max(10.0, plotW / std::max(1.0, 2.0 * nPts));
    if (closestIdx >= 0 && minDx <= hoverTol) {
        if (m_hoveredIndex != closestIdx) {
            m_hoveredIndex = closestIdx;
            update();
        }

        const auto &pt = m_points[m_hoveredIndex];
        QString statusHtml;
        if (pt.isFallback) {
            statusHtml = QString("<font color='#ffa726'><b>Fallback from Run %1</b></font>").arg(pt.fallbackSourceRun);
        } else if (pt.isValid) {
            statusHtml = "<font color='#4caf50'><b>Fit Success</b></font>";
        } else {
            statusHtml = "<font color='#ef5350'><b>Failed / Uncalibrated</b></font>";
        }

        QString tip = QString("<b>Run #%1:</b> %2<br/>"
                             "<b>Detector:</b> #%3<br/>"
                             "<b>Gain a1:</b> %4 keV/ch<br/>"
                             "<b>Reduced &chi;&sup2;/ndf:</b> %5<br/>"
                             "<b>Status:</b> %6<br/>"
                             "<span style='color:#90caf9;'><i>Double-click to inspect spectrum and peak fits</i></span>")
                         .arg(pt.runNumber)
                         .arg(pt.runFileName)
                         .arg(m_detectorId)
                         .arg(pt.isValid ? QString::number(pt.gain, 'f', 6) : "-")
                         .arg((pt.chi2ndf > 0.0) ? QString::number(pt.chi2ndf, 'f', 2) : "-")
                         .arg(statusHtml);

        QToolTip::showText(event->globalPos(), tip, this);
    } else {
        if (m_hoveredIndex != -1) {
            m_hoveredIndex = -1;
            QToolTip::hideText();
            update();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void DetectorDriftPlotWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_points.empty()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const int w = width();
    const int ml = 65, mr = 20;
    const double plotW = std::max(10.0, static_cast<double>(w - ml - mr));
    const int nPts = static_cast<int>(m_points.size());

    int closestIdx = -1;
    double minDx = 1e9;

    for (int i = 0; i < nPts; ++i) {
        double px = (nPts > 1) ? (ml + (static_cast<double>(i) / (nPts - 1)) * plotW)
                               : (ml + plotW / 2.0);
        double dx = std::abs(px - event->pos().x());
        if (dx < minDx) {
            minDx = dx;
            closestIdx = i;
        }
    }

    if (closestIdx >= 0 && closestIdx < nPts) {
        emit runPointDoubleClicked(m_points[closestIdx].runIndex);
    }

    QWidget::mouseDoubleClickEvent(event);
}

void DetectorDriftPlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Background
    p.fillRect(rect(), QColor("#1e2227"));

    if (m_points.empty()) {
        p.setPen(QColor("#78909c"));
        p.setFont(QFont("Arial", 10));
        p.drawText(rect(), Qt::AlignCenter, tr("No drift data available. Load sequence runs above."));
        return;
    }

    const int ml = 65, mr = 20, mt = 22, mb = 28, gap = 24;
    const double availH = h - mt - mb - gap;
    if (availH < 40.0) return;

    const double subH = availH / 2.0;
    const QRectF rGain(ml, mt, w - ml - mr, subH);
    const QRectF rChi(ml, mt + subH + gap, w - ml - mr, subH);

    // Subplot backgrounds & frames
    p.fillRect(rGain, QColor("#14181c"));
    p.fillRect(rChi, QColor("#14181c"));
    p.setPen(QPen(QColor("#37474f"), 1));
    p.drawRect(rGain);
    p.drawRect(rChi);

    // Determine Gain & Chi2 ranges
    double minGain = 1e9, maxGain = -1e9;
    double minChi = 0.0, maxChi = 0.0;
    int validCount = 0;

    for (const auto &pt : m_points) {
        if (pt.isValid) {
            minGain = std::min(minGain, pt.gain);
            maxGain = std::max(maxGain, pt.gain);
            validCount++;
        }
        if (pt.chi2ndf > 0.0) {
            maxChi = std::max(maxChi, pt.chi2ndf);
        }
    }

    if (validCount == 0) {
        minGain = 0.0;
        maxGain = 1.0;
        maxChi = 5.0;
    } else {
        if (maxGain <= minGain || (maxGain - minGain) < 1e-6) {
            double center = (minGain > 0.0) ? minGain : 1.0;
            minGain = center * 0.95;
            maxGain = center * 1.05;
        } else {
            double span = maxGain - minGain;
            minGain -= span * 0.12;
            maxGain += span * 0.12;
        }
        maxChi = std::max(maxChi * 1.25, 2.5);
    }

    // Grid lines & labels for Gain (Top Plot)
    QFont tickFont("Monospace", 8);
    p.setFont(tickFont);

    const int numTicks = 3;
    for (int t = 0; t <= numTicks; ++t) {
        double frac = static_cast<double>(t) / numTicks;
        double y = rGain.bottom() - frac * rGain.height();
        double val = minGain + frac * (maxGain - minGain);

        p.setPen(QPen(QColor("#263238"), 1, Qt::DashLine));
        p.drawLine(QPointF(rGain.left(), y), QPointF(rGain.right(), y));

        p.setPen(QColor("#90a4ae"));
        p.drawText(QRectF(0, y - 8, ml - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 5));
    }

    // Grid lines & labels for Chi2/ndf (Bottom Plot)
    for (int t = 0; t <= numTicks; ++t) {
        double frac = static_cast<double>(t) / numTicks;
        double y = rChi.bottom() - frac * rChi.height();
        double val = minChi + frac * (maxChi - minChi);

        p.setPen(QPen(QColor("#263238"), 1, Qt::DashLine));
        p.drawLine(QPointF(rChi.left(), y), QPointF(rChi.right(), y));

        p.setPen(QColor("#90a4ae"));
        p.drawText(QRectF(0, y - 8, ml - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 2));
    }

    // Title for Gain Plot
    QFont titleFont("Arial", 9, QFont::Bold);
    p.setFont(titleFont);
    p.setPen(QColor("#81c784")); // Light green
    p.drawText(rGain.left() + 4, rGain.top() - 6,
               QString("Gain a1 (keV/ch) — Detector #%1").arg(m_detectorId >= 0 ? QString::number(m_detectorId) : "-"));

    // Title for Chi2 Plot
    p.setPen(QColor("#4fc3f7")); // Light cyan
    p.drawText(rChi.left() + 4, rChi.top() - 6, "Reduced \u03c7\u00b2 / ndf");

    if (validCount == 0) {
        p.setPen(QColor("#b0bec5"));
        p.setFont(QFont("Arial", 9, QFont::Normal));
        p.drawText(rGain, Qt::AlignCenter, tr("Calibration pending. Click 'Start Calibration' or double-click to test fit."));
        p.drawText(rChi, Qt::AlignCenter, tr("No \u03c7\u00b2 metrics calculated yet."));
    }

    const int nPts = static_cast<int>(m_points.size());
    const double plotW = rGain.width();

    auto getX = [&](int idx) -> double {
        if (nPts <= 1) return rGain.left() + plotW / 2.0;
        return rGain.left() + (static_cast<double>(idx) / (nPts - 1)) * plotW;
    };

    auto getYGain = [&](double val) -> double {
        double norm = (val - minGain) / (maxGain - minGain);
        return rGain.bottom() - norm * rGain.height();
    };

    auto getYChi = [&](double val) -> double {
        double norm = (val - minChi) / (maxChi - minChi);
        return rChi.bottom() - norm * rChi.height();
    };

    // Draw connecting lines for Gain
    if (validCount > 1) {
        QPainterPath gainPath;
        bool inSegment = false;
        for (int i = 0; i < nPts; ++i) {
            const auto &pt = m_points[i];
            if (pt.isValid) {
                double px = getX(i);
                double py = getYGain(pt.gain);
                if (!inSegment) {
                    gainPath.moveTo(px, py);
                    inSegment = true;
                } else {
                    gainPath.lineTo(px, py);
                }
            } else {
                inSegment = false;
            }
        }
        p.setPen(QPen(QColor("#4caf50"), 2));
        p.drawPath(gainPath);

        // Draw connecting lines for Chi2
        QPainterPath chiPath;
        inSegment = false;
        for (int i = 0; i < nPts; ++i) {
            const auto &pt = m_points[i];
            if (pt.isValid) {
                double px = getX(i);
                double py = getYChi(pt.chi2ndf);
                if (!inSegment) {
                    chiPath.moveTo(px, py);
                    inSegment = true;
                } else {
                    chiPath.lineTo(px, py);
                }
            } else {
                inSegment = false;
            }
        }
        p.setPen(QPen(QColor("#00e5ff"), 2));
        p.drawPath(chiPath);
    }

    // Hover crosshair line
    if (m_hoveredIndex >= 0 && m_hoveredIndex < nPts) {
        double hx = getX(m_hoveredIndex);
        p.setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
        p.drawLine(QPointF(hx, rGain.top()), QPointF(hx, rChi.bottom()));
    }

    // Draw data points
    for (int i = 0; i < nPts; ++i) {
        const auto &pt = m_points[i];
        double px = getX(i);
        bool isHovered = (i == m_hoveredIndex);
        double rad = isHovered ? 6.0 : 4.0;

        QColor ptColor;
        if (pt.isFallback) {
            ptColor = QColor("#ffa726"); // Amber
        } else if (pt.isValid) {
            ptColor = QColor("#4caf50"); // Green
        } else if (pt.status == RunCalibStatus::BadChi2) {
            ptColor = QColor("#f57c00"); // Orange for Intervene
        } else {
            ptColor = QColor("#ef5350"); // Red for Fail
        }

        // Gain point
        double pyGain = pt.isValid ? getYGain(pt.gain) : (rGain.bottom() - 10.0);
        p.setPen(QPen(ptColor.lighter(130), 1.5));
        p.setBrush(ptColor);
        p.drawEllipse(QPointF(px, pyGain), rad, rad);

        // Chi2 point
        double pyChi = (pt.chi2ndf > 0.0) ? getYChi(pt.chi2ndf) : (rChi.bottom() - 10.0);
        p.setPen(QPen(ptColor.lighter(130), 1.5));
        p.setBrush(ptColor);
        p.drawEllipse(QPointF(px, pyChi), rad, rad);
    }

    // X-axis Run ticks & labels at bottom of rChi
    p.setFont(tickFont);
    p.setPen(QColor("#78909c"));
    const int step = std::max(1, nPts / 12);
    for (int i = 0; i < nPts; i += step) {
        double px = getX(i);
        p.drawLine(QPointF(px, rChi.bottom()), QPointF(px, rChi.bottom() + 4));
        p.drawText(QRectF(px - 25, rChi.bottom() + 6, 50, 16), Qt::AlignCenter,
                   QString("R%1").arg(m_points[i].runNumber));
    }
}

//==============================================================================
// RunByRunManager Constructor & Destructor
//==============================================================================
RunByRunManager::RunByRunManager(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Run-by-Run Energy Calibration Manager (In-Beam Drift Alignment)"));
    resize(1120, 760);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    // Default reference anchors for in-beam HPGe arrays
    m_anchors = {
        { 238.97,  0.05, 239.0, 15.0, 25.0, 2.5 },
        { 511.00,  0.05, 511.0, 15.0, 20.0, 3.5 },
        { 1055.00, 0.10, 1055.0, 15.0, 20.0, 2.5 }
    };

    // Register custom types for Qt cross-thread queued signal delivery
    qRegisterMetaType<RunCalibResult>("RunCalibResult");
    qRegisterMetaType<RunCalibStatus>("RunCalibStatus");
    qRegisterMetaType<std::vector<RunCalibResult>>("std::vector<RunCalibResult>");

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

    // Spectrum Channel Length
    setupGrid->addWidget(new QLabel(tr("Spectrum Channels:"), this), 1, 0);
    m_comboChannelLength = new QComboBox(this);
    m_comboChannelLength->addItem("65536 (64k channels - HPGe standard)", 65536);
    m_comboChannelLength->addItem("16384 (16k channels)", 16384);
    m_comboChannelLength->addItem("8192 (8k channels)", 8192);
    m_comboChannelLength->addItem("4096 (4k channels)", 4096);
    m_comboChannelLength->addItem("2048 (2k channels)", 2048);
    setupGrid->addWidget(m_comboChannelLength, 1, 1);

    // Calibration Model & Drift Tolerance
    setupGrid->addWidget(new QLabel(tr("Calibration Model:"), this), 1, 2);
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
    setupGrid->addLayout(modelLayout, 1, 3);

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

    setupGrid->addWidget(new QLabel(tr("Max \u03c7\u00b2/ndf:"), this), 2, 2);
    m_spinMaxChi2 = new QDoubleSpinBox(this);
    m_spinMaxChi2->setRange(0.5, 100.0);
    m_spinMaxChi2->setSingleStep(0.5);
    m_spinMaxChi2->setValue(5.0);
    m_spinMaxChi2->setDecimals(1);
    m_spinMaxChi2->setToolTip(tr("Maximum allowed reduced chi-square (\u03c7\u00b2/ndf) for automatic calibration acceptance. Fits exceeding this value will be flagged for review."));
    setupGrid->addWidget(m_spinMaxChi2, 2, 3);

    mainLayout->addWidget(grpSetup);

    // MIDDLE: Anchors Table & Configuration
    QGroupBox *grpAnchors = new QGroupBox(tr("2. In-Beam Anchor Lines (Centroid Trajectory Tracking)"), this);
    QVBoxLayout *anchorVBox = new QVBoxLayout(grpAnchors);
    anchorVBox->setSpacing(6);

    // Top action bar above table
    QHBoxLayout *topActionBar = new QHBoxLayout();
    QPushButton *btnAddCustom = new QPushButton(tr("➕ Add Line"), this);
    QPushButton *btnReadFromFile = new QPushButton(tr("📂 Read from File..."), this);
    QPushButton *btnRemoveAnchor = new QPushButton(tr("➖ Remove Selected"), this);
    QPushButton *btnClearAnchors = new QPushButton(tr("🗑️ Clear All"), this);

    connect(btnAddCustom, &QPushButton::clicked, this, &RunByRunManager::onAddAnchorClicked);
    connect(btnReadFromFile, &QPushButton::clicked, this, &RunByRunManager::onReadPeaksFromFileClicked);
    connect(btnRemoveAnchor, &QPushButton::clicked, this, &RunByRunManager::onRemoveAnchorClicked);
    connect(btnClearAnchors, &QPushButton::clicked, this, &RunByRunManager::onClearAnchorsClicked);

    topActionBar->addWidget(btnAddCustom);
    topActionBar->addWidget(btnReadFromFile);
    topActionBar->addWidget(btnRemoveAnchor);
    topActionBar->addWidget(btnClearAnchors);
    topActionBar->addStretch(1);
    anchorVBox->addLayout(topActionBar);

    m_tableAnchors = new QTableWidget(this);
    m_tableAnchors->setColumnCount(6);
    m_tableAnchors->setHorizontalHeaderLabels({
        tr("Physical Energy (keV) [Edit]"),
        tr("Energy Error (keV) [Edit]"),
        tr("Initial Channel (ch)"),
        tr("Search Window (± ch)"),
        tr("Min Peak Counts"),
        tr("Expected FWHM (ch)")
    });
    m_tableAnchors->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableAnchors->setMaximumHeight(130);
    anchorVBox->addWidget(m_tableAnchors);
    populateAnchorTable();

    // Bottom action bar below table (Separate dedicated button)
    QHBoxLayout *bottomActionBar = new QHBoxLayout();
    QPushButton *btnGrabParams = new QPushButton(tr("🎯 Grab Parameters from Reference Run / Detector..."), this);
    btnGrabParams->setStyleSheet("font-weight: bold; background-color: #0277bd; color: white; padding: 6px 14px;");
    btnGrabParams->setToolTip(tr("Automatically fits each anchor line in a selected reference spectrum to measure peak channel, FWHM, 2.5× search window, and 1/20th count threshold."));
    connect(btnGrabParams, &QPushButton::clicked, this, &RunByRunManager::onGrabParametersClicked);

    bottomActionBar->addWidget(btnGrabParams);
    bottomActionBar->addStretch(1);
    anchorVBox->addLayout(bottomActionBar);

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
    m_gridMatrix->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gridMatrix->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gridMatrix, &QTableWidget::cellDoubleClicked, this, &RunByRunManager::onCellDoubleClicked);
    connect(m_gridMatrix, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTableWidgetItem *item = m_gridMatrix->itemAt(pos);
        if (!item) return;
        int r = item->row();
        int c = item->column();
        QMenu menu(this);
        QAction *actInspect = menu.addAction(tr("🔍 Inspect Spectrum on Canvas"));
        QAction *actIntervene = menu.addAction(tr("🛠️ Intervene & Discriminate Fits..."));
        QAction *chosen = menu.exec(m_gridMatrix->viewport()->mapToGlobal(pos));
        if (chosen == actInspect) {
            onCellDoubleClicked(r, c);
        } else if (chosen == actIntervene) {
            openInterventionDialog(r, c);
        }
    });
    matLayout->addWidget(m_gridMatrix);
    splitter->addWidget(grpMatrix);

    // Right: Detector Drift Inspector (Plot on top, Diagnostics Table on bottom)
    QGroupBox *grpPlot = new QGroupBox(tr("4. Detector Drift Inspection (Graph & Diagnostics)"), this);
    QVBoxLayout *plotLayout = new QVBoxLayout(grpPlot);

    QHBoxLayout *plotSelectBar = new QHBoxLayout();
    plotSelectBar->addWidget(new QLabel(tr("Inspect Detector:"), this));
    m_comboPlotDet = new QComboBox(this);
    connect(m_comboPlotDet, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunByRunManager::onDetectorPlotSelectionChanged);
    plotSelectBar->addWidget(m_comboPlotDet, 1);
    plotLayout->addLayout(plotSelectBar);

    QSplitter *vSplitter = new QSplitter(Qt::Vertical, grpPlot);

    m_plotWidget = new DetectorDriftPlotWidget(grpPlot);
    connect(m_plotWidget, &DetectorDriftPlotWidget::runPointDoubleClicked, this, [this](int runIdx) {
        int detCol = m_comboPlotDet->currentIndex();
        if (detCol >= 0 && runIdx >= 0) {
            onCellDoubleClicked(runIdx, detCol);
        }
    });
    vSplitter->addWidget(m_plotWidget);

    m_tablePlotData = new QTableWidget(this);
    m_tablePlotData->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tablePlotData->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tablePlotData->setColumnCount(7);
    m_tablePlotData->setHorizontalHeaderLabels({
        tr("Run #"), tr("Run File"), tr("Gain a1 (keV/ch)"), tr("Offset a0 (keV)"),
        tr("Residual (keV)"), tr("Chi2/ndf"), tr("Status")
    });
    m_tablePlotData->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(m_tablePlotData, &QTableWidget::cellDoubleClicked, this, [this](int r, int /*col*/) {
        int detCol = m_comboPlotDet->currentIndex();
        if (detCol >= 0 && r >= 0) {
            QTableWidgetItem *item = m_tablePlotData->item(r, 0);
            int runIdx = item ? item->data(Qt::UserRole).toInt() : r;
            onCellDoubleClicked(runIdx, detCol);
        }
    });
    connect(m_tablePlotData, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTableWidgetItem *item = m_tablePlotData->itemAt(pos);
        if (!item) return;
        int r = item->row();
        int detCol = m_comboPlotDet->currentIndex();
        if (detCol < 0 || r < 0) return;
        QTableWidgetItem *rItem = m_tablePlotData->item(r, 0);
        int runIdx = rItem ? rItem->data(Qt::UserRole).toInt() : r;
        QMenu menu(this);
        QAction *actInspect = menu.addAction(tr("🔍 Inspect Spectrum on Canvas"));
        QAction *actIntervene = menu.addAction(tr("🛠️ Intervene & Discriminate Fits..."));
        QAction *chosen = menu.exec(m_tablePlotData->viewport()->mapToGlobal(pos));
        if (chosen == actInspect) {
            onCellDoubleClicked(runIdx, detCol);
        } else if (chosen == actIntervene) {
            openInterventionDialog(runIdx, detCol);
        }
    });
    vSplitter->addWidget(m_tablePlotData);
    vSplitter->setStretchFactor(0, 3);
    vSplitter->setStretchFactor(1, 2);

    plotLayout->addWidget(vSplitter, 1);
    splitter->addWidget(grpPlot);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter, 1);

    // Initial population of detector inspector dropdown
    for (int d = m_spinDetStart->value(); d <= m_spinDetEnd->value(); ++d) {
        m_comboPlotDet->addItem(QString("Detector #%1").arg(d), d);
    }
    if (m_comboPlotDet->count() > 0) {
        updatePlot(m_comboPlotDet->currentData().toInt());
    }

    auto updateDetCombo = [this]() {
        int curDet = m_comboPlotDet->currentData().toInt();
        m_comboPlotDet->blockSignals(true);
        m_comboPlotDet->clear();
        for (int d = m_spinDetStart->value(); d <= m_spinDetEnd->value(); ++d) {
            m_comboPlotDet->addItem(QString("Detector #%1").arg(d), d);
        }
        int idx = m_comboPlotDet->findData(curDet);
        if (idx >= 0) {
            m_comboPlotDet->setCurrentIndex(idx);
        } else if (m_comboPlotDet->count() > 0) {
            m_comboPlotDet->setCurrentIndex(0);
        }
        m_comboPlotDet->blockSignals(false);
        if (m_comboPlotDet->count() > 0) {
            updatePlot(m_comboPlotDet->currentData().toInt());
        }
    };
    connect(m_spinDetStart, QOverload<int>::of(&QSpinBox::valueChanged), this, updateDetCombo);
    connect(m_spinDetEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, updateDetCombo);
}

//==============================================================================
// populateAnchorTable & syncAnchorsFromTable
//==============================================================================
void RunByRunManager::syncAnchorsFromTable()
{
    if (!m_tableAnchors) return;
    std::vector<AnchorPeakDef> updated;
    for (int r = 0; r < m_tableAnchors->rowCount(); ++r) {
        AnchorPeakDef a;
        a.physicalEnergy = m_tableAnchors->item(r, 0) ? m_tableAnchors->item(r, 0)->text().toDouble() : 0.0;
        a.energyError    = m_tableAnchors->item(r, 1) ? m_tableAnchors->item(r, 1)->text().toDouble() : 0.05;
        a.initialChannel = m_tableAnchors->item(r, 2) ? m_tableAnchors->item(r, 2)->text().toDouble() : 0.0;
        a.searchWindowCh = m_tableAnchors->item(r, 3) ? m_tableAnchors->item(r, 3)->text().toDouble() : 15.0;
        a.minCounts      = m_tableAnchors->item(r, 4) ? m_tableAnchors->item(r, 4)->text().toDouble() : 15.0;
        a.expectedFwhmCh = m_tableAnchors->item(r, 5) ? m_tableAnchors->item(r, 5)->text().toDouble() : 5.0;
        if (a.physicalEnergy > 0.0) {
            updated.push_back(a);
        }
    }
    m_anchors = updated;
}

void RunByRunManager::populateAnchorTable()
{
    m_tableAnchors->setRowCount(static_cast<int>(m_anchors.size()));
    for (int i = 0; i < static_cast<int>(m_anchors.size()); ++i) {
        const auto &a = m_anchors[i];

        // Column 0: Physical Energy (keV) - User-editable
        QTableWidgetItem *itemEnergy = new QTableWidgetItem(QString::number(a.physicalEnergy, 'f', 2));
        itemEnergy->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        itemEnergy->setTextAlignment(Qt::AlignCenter);
        m_tableAnchors->setItem(i, 0, itemEnergy);

        // Column 1: Energy Error (keV) - User-editable
        QTableWidgetItem *itemErr = new QTableWidgetItem(QString::number(a.energyError, 'f', 3));
        itemErr->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        itemErr->setTextAlignment(Qt::AlignCenter);
        m_tableAnchors->setItem(i, 1, itemErr);

        // Column 2: Initial Channel (ch) - Non-editable (grabbed from run)
        QTableWidgetItem *itemCh = new QTableWidgetItem(a.initialChannel > 0.0 ? QString::number(a.initialChannel, 'f', 1) : tr("(auto)"));
        itemCh->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        itemCh->setTextAlignment(Qt::AlignCenter);
        itemCh->setForeground(QBrush(QColor("#64b5f6"))); // Cyan/Blue hint
        m_tableAnchors->setItem(i, 2, itemCh);

        // Column 3: Search Window (± ch) - Non-editable
        QTableWidgetItem *itemWin = new QTableWidgetItem(QString::number(a.searchWindowCh, 'f', 1));
        itemWin->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        itemWin->setTextAlignment(Qt::AlignCenter);
        m_tableAnchors->setItem(i, 3, itemWin);

        // Column 4: Min Peak Counts - Non-editable
        QTableWidgetItem *itemCounts = new QTableWidgetItem(QString::number(a.minCounts, 'f', 0));
        itemCounts->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        itemCounts->setTextAlignment(Qt::AlignCenter);
        m_tableAnchors->setItem(i, 4, itemCounts);

        // Column 5: Expected FWHM (ch) - Non-editable
        QTableWidgetItem *itemFwhm = new QTableWidgetItem(QString::number(a.expectedFwhmCh, 'f', 1));
        itemFwhm->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        itemFwhm->setTextAlignment(Qt::AlignCenter);
        m_tableAnchors->setItem(i, 5, itemFwhm);
    }
}

void RunByRunManager::onBrowseRunsClicked()
{
    QStringList selectedFiles = QFileDialog::getOpenFileNames(this, tr("Select Sequence Run Files"),
                                                              m_runFilePaths.empty() ? QString() : m_runFilePaths.front(),
                                                              tr("All Files (*);;Run Files (*.*)"));
    if (selectedFiles.isEmpty()) return;

    m_runFilePaths.clear();

    if (selectedFiles.size() == 1) {
        // Single file selected: check if it belongs to a sequence prefix in that folder
        QFileInfo fi(selectedFiles.front());
        QDir dir = fi.dir();
        QString fileName = fi.fileName();
        int lastDot = fileName.lastIndexOf('.');
        QString prefix = (lastDot > 0) ? fileName.left(lastDot + 1) : fileName;
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

        if (foundRuns.size() > 1) {
            std::sort(foundRuns.begin(), foundRuns.end(), [](const RunSortItem &a, const RunSortItem &b) {
                return a.runNum < b.runNum;
            });
            for (const auto &item : foundRuns) {
                m_runFilePaths.push_back(item.path);
            }
        } else {
            m_runFilePaths.push_back(selectedFiles.front());
        }
    } else {
        // Multiple files selected by user: sort naturally by numeric file order
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(selectedFiles.begin(), selectedFiles.end(), [&collator](const QString &a, const QString &b) {
            return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
        });

        for (const auto &p : selectedFiles) {
            m_runFilePaths.push_back(p);
        }

        QFileInfo fi(m_runFilePaths.front());
        int lastDot = fi.fileName().lastIndexOf('.');
        m_commonPrefix = (lastDot > 0) ? fi.fileName().left(lastDot + 1) : "";
    }

    if (!m_runFilePaths.empty()) {
        m_lblRunsSummary->setText(tr("Loaded <b>%1</b> runs from %2 (%3 to %4)")
                                      .arg(m_runFilePaths.size())
                                      .arg(QFileInfo(m_runFilePaths.front()).dir().dirName())
                                      .arg(QFileInfo(m_runFilePaths.front()).fileName())
                                      .arg(QFileInfo(m_runFilePaths.back()).fileName()));

        qint64 fSize = QFileInfo(m_runFilePaths.front()).size();
        if (fSize % (65536 * 4) == 0) {
            m_spinDetEnd->setValue(std::max(0, static_cast<int>(fSize / (65536 * 4)) - 1));
            m_comboChannelLength->setCurrentIndex(0); // 65536
        } else if (fSize % (4096 * 4) == 0) {
            m_spinDetEnd->setValue(std::max(0, static_cast<int>(fSize / (4096 * 4)) - 1));
            m_comboChannelLength->setCurrentIndex(3); // 4096
        }

        if (m_comboPlotDet && m_comboPlotDet->count() > 0) {
            updatePlot(m_comboPlotDet->currentData().toInt());
        }
    } else {
        m_lblRunsSummary->setText(tr("<font color='red'>No files selected.</font>"));
    }
}

void RunByRunManager::onAddAnchorClicked()
{
    syncAnchorsFromTable();
    m_anchors.push_back({ 1000.0, 0.05, 0.0, 15.0, 15.0, 3.5 });
    populateAnchorTable();
    if (m_tableAnchors->rowCount() > 0) {
        int lastRow = m_tableAnchors->rowCount() - 1;
        m_tableAnchors->setCurrentCell(lastRow, 0);
        m_tableAnchors->editItem(m_tableAnchors->item(lastRow, 0));
    }
}

void RunByRunManager::onReadPeaksFromFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Read Anchor Peaks from File"),
        QString(),
        tr("Peak Files (*.txt *.dat *.peaks *.csv);;All Files (*.*)"));
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("File Open Error"),
                              tr("Could not open file for reading:\n%1").arg(filePath));
        return;
    }

    QTextStream in(&file);
    std::vector<AnchorPeakDef> parsedPeaks;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        // Comments starting with #, //, !, ;, %
        if (line.startsWith('#') || line.startsWith("//") || line.startsWith('!') ||
            line.startsWith(';') || line.startsWith('%')) {
            continue;
        }

        // Split by whitespace or comma/semicolon delimiters
        QStringList tokens = line.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
        if (tokens.empty()) continue;

        bool okE = false;
        double energy = tokens[0].toDouble(&okE);
        if (!okE || energy <= 0.0) continue;

        double energyErr = 0.05; // Default error
        if (tokens.size() >= 2) {
            bool okErr = false;
            double errVal = tokens[1].toDouble(&okErr);
            if (okErr && errVal >= 0.0) {
                energyErr = errVal;
            }
        }

        AnchorPeakDef a;
        a.physicalEnergy = energy;
        a.energyError    = energyErr;
        a.initialChannel = 0.0;
        a.searchWindowCh = 15.0;
        a.minCounts      = 20.0;
        a.expectedFwhmCh = 3.5;
        parsedPeaks.push_back(a);
    }

    file.close();

    if (parsedPeaks.empty()) {
        QMessageBox::warning(this, tr("No Valid Peaks Found"),
                             tr("No valid peaks could be parsed from:\n%1\n\nExpected simple format (one per line):\n<Energy_keV> [Energy_Error_keV]").arg(filePath));
        return;
    }

    syncAnchorsFromTable();
    if (!m_anchors.empty()) {
        QMessageBox promptBox(this);
        promptBox.setWindowTitle(tr("Load Peaks from File"));
        promptBox.setText(tr("Found %1 peak line(s) in file.\nExisting table currently contains %2 peak(s).")
                              .arg(parsedPeaks.size()).arg(m_anchors.size()));
        promptBox.setInformativeText(tr("Do you want to replace existing peaks or append these peaks?"));
        QPushButton *btnReplace = promptBox.addButton(tr("Replace Existing"), QMessageBox::ActionRole);
        QPushButton *btnAppend = promptBox.addButton(tr("Append"), QMessageBox::ActionRole);
        QPushButton *btnCancel = promptBox.addButton(QMessageBox::Cancel);
        promptBox.exec();

        if (promptBox.clickedButton() == btnCancel) {
            return;
        } else if (promptBox.clickedButton() == btnReplace) {
            m_anchors = parsedPeaks;
        } else if (promptBox.clickedButton() == btnAppend) {
            m_anchors.insert(m_anchors.end(), parsedPeaks.begin(), parsedPeaks.end());
        }
    } else {
        m_anchors = parsedPeaks;
    }

    populateAnchorTable();

    QMessageBox::information(this, tr("Peaks Loaded"),
                             tr("Successfully loaded %1 anchor peak(s) from:\n%2\n\nYou can now click 'Grab Parameters from Reference Run' to auto-measure channels and FWHM.")
                                 .arg(parsedPeaks.size()).arg(QFileInfo(filePath).fileName()));
}

void RunByRunManager::onRemoveAnchorClicked()
{
    syncAnchorsFromTable();
    int row = m_tableAnchors->currentRow();
    if (row >= 0 && row < static_cast<int>(m_anchors.size())) {
        m_anchors.erase(m_anchors.begin() + row);
        populateAnchorTable();
    }
}

void RunByRunManager::onClearAnchorsClicked()
{
    m_anchors.clear();
    populateAnchorTable();
}

void RunByRunManager::onGrabParametersClicked()
{
    syncAnchorsFromTable();
    if (m_anchors.empty()) {
        QMessageBox::information(this, tr("No Anchors"), tr("Please add at least one anchor line to the table first."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Grab Parameters from Reference Spectrum"));
    dlg.setFont(Design::getDialogFont());
    dlg.setStyleSheet(Design::getDialogStyleSheet());
    dlg.resize(540, 240);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(8);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(6);

    // 1. Reference Run File
    grid->addWidget(new QLabel(tr("Reference Run:"), &dlg), 0, 0);
    QComboBox *comboRun = new QComboBox(&dlg);
    for (size_t i = 0; i < m_runFilePaths.size(); ++i) {
        comboRun->addItem(QString("[%1] %2").arg(i + 1).arg(QFileInfo(m_runFilePaths[i]).fileName()), m_runFilePaths[i]);
    }
    if (m_mainCanvas && !m_mainCanvas->getCurrentSpectrumFile().isEmpty()) {
        QString cur = m_mainCanvas->getCurrentSpectrumFile();
        comboRun->insertItem(0, QString("Canvas Spectrum (%1)").arg(QFileInfo(cur).fileName()), cur);
        comboRun->setCurrentIndex(0);
    }
    grid->addWidget(comboRun, 0, 1);

    // 2. Reference Detector
    grid->addWidget(new QLabel(tr("Reference Detector ID:"), &dlg), 1, 0);
    QSpinBox *spinDet = new QSpinBox(&dlg);
    spinDet->setRange(0, 255);
    spinDet->setValue(m_spinDetStart ? m_spinDetStart->value() : 0);
    grid->addWidget(spinDet, 1, 1);

    // 3. Min Counts Fraction
    grid->addWidget(new QLabel(tr("Min Counts Threshold:"), &dlg), 2, 0);
    QDoubleSpinBox *spinCountFrac = new QDoubleSpinBox(&dlg);
    spinCountFrac->setRange(0.5, 100.0);
    spinCountFrac->setValue(5.0); // 5% = 1/20th
    spinCountFrac->setSuffix(" % of peak area (1/20th)");
    grid->addWidget(spinCountFrac, 2, 1);

    // 4. Search Window Multiplier
    grid->addWidget(new QLabel(tr("Search Window:"), &dlg), 3, 0);
    QDoubleSpinBox *spinWinMult = new QDoubleSpinBox(&dlg);
    spinWinMult->setRange(1.0, 10.0);
    spinWinMult->setValue(2.5);
    spinWinMult->setSuffix(" × FWHM");
    grid->addWidget(spinWinMult, 3, 1);

    layout->addLayout(grid);

    QLabel *lblHelp = new QLabel(tr("<i>Measures centroid channel, FWHM, search window (2.5× FWHM), and count threshold (1/20th area) "
                                    "for each anchor line in the reference spectrum.</i>"), &dlg);
    lblHelp->setWordWrap(true);
    layout->addWidget(lblHelp);

    QHBoxLayout *btnBox = new QHBoxLayout();
    QPushButton *btnExec = new QPushButton(tr("🎯 Measure & Apply to Table"), &dlg);
    btnExec->setStyleSheet("font-weight: bold; background-color: #1976d2; color: white;");
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &dlg);
    btnBox->addStretch(1);
    btnBox->addWidget(btnExec);
    btnBox->addWidget(btnCancel);
    layout->addLayout(btnBox);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnExec, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    QString refPath = comboRun->currentData().toString();
    int refDet = spinDet->value();
    double countFrac = spinCountFrac->value() / 100.0;
    double winMult = spinWinMult->value();

    if (refPath.isEmpty()) {
        QMessageBox::warning(this, tr("No File"), tr("Please select a valid reference run spectrum."));
        return;
    }

    std::vector<double> refSpectrum;
    QString err;
    int channels = m_comboChannelLength ? m_comboChannelLength->currentData().toInt() : 65536;

    if (!ReadSpectrumData(refPath.toStdString(), SpectrumFormat::LongInt32, channels, refDet, refSpectrum, &err) || refSpectrum.empty()) {
        QMessageBox::critical(this, tr("Spectrum Read Failed"),
                              tr("Could not read spectrum from:\n%1\nError: %2").arg(refPath).arg(err.isEmpty() ? "Empty spectrum" : err));
        return;
    }

    int updatedCount = 0;
    for (auto &anchor : m_anchors) {
        double searchCenter = (anchor.initialChannel > 0.0) ? anchor.initialChannel : anchor.physicalEnergy;
        double searchWin = std::max(25.0, anchor.searchWindowCh);
        FittedAnchor fa = RunByRunEngine::fitAnchorPeak(refSpectrum, searchCenter, searchWin, 5.0, anchor.physicalEnergy);
        if (fa.isValid) {
            anchor.initialChannel = std::round(fa.centroidCh * 10.0) / 10.0;
            anchor.expectedFwhmCh = std::max(0.8, std::round(fa.fwhmCh * 10.0) / 10.0);
            anchor.searchWindowCh = std::max(10.0, std::round(fa.fwhmCh * winMult * 10.0) / 10.0);
            anchor.minCounts = std::max(5.0, std::round(fa.area * countFrac));
            updatedCount++;
        }
    }

    populateAnchorTable();

    QMessageBox::information(this, tr("Parameters Updated"),
                             tr("Successfully grabbed and updated parameters for %1 of %2 anchor lines using %3 (Detector #%4).")
                                 .arg(updatedCount).arg(m_anchors.size()).arg(QFileInfo(refPath).fileName()).arg(refDet));
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
    syncAnchorsFromTable();

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
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
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
    cfg.detectorStart = dStart;
    cfg.detectorEnd = dEnd;
    cfg.spectrumChannels = m_comboChannelLength->currentData().toInt();
    cfg.spectrumFormat = SpectrumFormat::LongInt32;
    cfg.anchors = m_anchors;
    cfg.useQuadratic = m_chkQuadratic->isChecked();
    cfg.maxAllowedDriftCh = m_spinDriftTolerance->value();
    cfg.maxAllowedChi2NDF = m_spinMaxChi2 ? m_spinMaxChi2->value() : 5.0;

    m_completedTrajectories.clear();
    m_isRunning = true;
    m_btnStart->setEnabled(false);
    m_btnCancel->setEnabled(true);
    m_btnExportCal->setEnabled(false);
    m_btnExportUcal->setEnabled(false);
    m_progressBar->setValue(0);

    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_engine) m_engine->requestStop();
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }

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
    connect(m_engine.get(), &RunByRunEngine::batchFinished, m_workerThread, &QThread::quit);

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

    if (result.status == RunCalibStatus::Success && !result.isFallbackFromPrevious) {
        item->setText("GOOD");
        item->setBackground(QBrush(QColor("#2e7d32"))); // Green
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2\na0: %3 keV\na1: %4 keV/ch\ns_res: %5 keV\nchi2/ndf: %6")
                             .arg(result.runFileName).arg(detId)
                             .arg(result.a0, 0, 'f', 4)
                             .arg(result.a1, 0, 'f', 6)
                             .arg(result.sRes, 0, 'f', 4)
                             .arg(result.chi2NDF, 0, 'f', 2));
    } else if (result.isFallbackFromPrevious) {
        item->setText(QString("~%1 (R%2)").arg(result.a1, 0, 'f', 4).arg(result.fallbackSourceRun));
        item->setBackground(QBrush(QColor("#e65100"))); // Amber / Orange
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2 [FALLBACK: Adopted Run %3]\na0: %4 keV\na1: %5 keV/ch\nNote: %6")
                             .arg(result.runFileName).arg(detId).arg(result.fallbackSourceRun)
                             .arg(result.a0, 0, 'f', 4)
                             .arg(result.a1, 0, 'f', 6)
                             .arg(result.failureReason));
    } else if (result.status == RunCalibStatus::BadChi2) {
        item->setText(tr("INTERVENE"));
        item->setBackground(QBrush(QColor("#f57c00"))); // Vibrant Amber / Orange
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2 [INTERVENE]\nReason: %3\n\u03c7\u00b2/ndf: %4\nDouble-click to open spectrum and intervene.")
                             .arg(result.runFileName).arg(detId)
                             .arg(result.failureReason)
                             .arg(result.chi2NDF > 0.0 ? QString::number(result.chi2NDF, 'f', 2) : "-"));
    } else {
        item->setText(tr("FAIL"));
        item->setBackground(QBrush(QColor("#c62828"))); // Red for genuine failures (no peaks found, low stats, etc.)
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2 [FAIL]\nReason: %3\nDouble-click to inspect spectrum.")
                             .arg(result.runFileName).arg(detId)
                             .arg(result.failureReason.isEmpty() ? "No peaks found or fit failed" : result.failureReason));
    }

    m_gridMatrix->viewport()->update();
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
    QPushButton *btnIntervene = new QPushButton(tr("🛠️ Intervene & Fix Fits..."), &dlg);
    btnIntervene->setStyleSheet("font-weight: bold; background-color: #0277bd; color: white; padding: 6px 14px;");
    QPushButton *btnInspect = new QPushButton(tr("🔍 Inspect on Matrix"), &dlg);
    QPushButton *btnClose = new QPushButton(tr("Close Report"), &dlg);
    btnBox->addWidget(btnIntervene);
    btnBox->addWidget(btnInspect);
    btnBox->addStretch(1);
    btnBox->addWidget(btnClose);
    vbox->addLayout(btnBox);

    auto triggerInterventionForSelectedRow = [&]() {
        int r = tableIssues->currentRow();
        if (r >= 0) {
            int detId = tableIssues->item(r, 0)->text().toInt();
            QString runName = tableIssues->item(r, 2)->text();
            for (int rowIdx = 0; rowIdx < static_cast<int>(m_runFilePaths.size()); ++rowIdx) {
                if (QFileInfo(m_runFilePaths[rowIdx]).fileName() == runName) {
                    int colIdx = detId - (m_spinDetStart ? m_spinDetStart->value() : 0);
                    openInterventionDialog(rowIdx, colIdx);
                    break;
                }
            }
        }
    };

    connect(btnIntervene, &QPushButton::clicked, triggerInterventionForSelectedRow);
    connect(tableIssues, &QTableWidget::cellDoubleClicked, [=](int, int) {
        triggerInterventionForSelectedRow();
    });

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(btnInspect, &QPushButton::clicked, [&]() {
        int r = tableIssues->currentRow();
        if (r >= 0) {
            int detId = tableIssues->item(r, 0)->text().toInt();
            QString runName = tableIssues->item(r, 2)->text();
            for (int rowIdx = 0; rowIdx < m_gridMatrix->rowCount(); ++rowIdx) {
                if (m_gridMatrix->verticalHeaderItem(rowIdx)->text() == runName) {
                    int colIdx = detId - (m_spinDetStart ? m_spinDetStart->value() : 0);
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

    if (!targetTraj) {
        // Calibration not yet run, but show all discovered runs for inspection
        const int numRuns = static_cast<int>(m_runFilePaths.size());
        m_tablePlotData->setRowCount(numRuns);
        std::vector<DriftPlotPoint> uncalibPoints;
        uncalibPoints.reserve(numRuns);

        for (int r = 0; r < numRuns; ++r) {
            QFileInfo fi(m_runFilePaths[r]);
            QTableWidgetItem *itemRun = new QTableWidgetItem(QString::number(r + 1));
            itemRun->setData(Qt::UserRole, r);
            itemRun->setTextAlignment(Qt::AlignCenter);
            m_tablePlotData->setItem(r, 0, itemRun);

            QTableWidgetItem *itemFile = new QTableWidgetItem(fi.fileName());
            m_tablePlotData->setItem(r, 1, itemFile);

            for (int c = 2; c <= 5; ++c) {
                QTableWidgetItem *it = new QTableWidgetItem("-");
                it->setTextAlignment(Qt::AlignCenter);
                m_tablePlotData->setItem(r, c, it);
            }

            QTableWidgetItem *st = new QTableWidgetItem(tr("Ready (Uncalibrated)"));
            st->setForeground(QBrush(QColor("#90a4ae")));
            st->setTextAlignment(Qt::AlignCenter);
            m_tablePlotData->setItem(r, 6, st);

            DriftPlotPoint pt;
            pt.runIndex = r;
            pt.runNumber = r + 1;
            pt.runFileName = fi.fileName();
            pt.gain = 0.0;
            pt.chi2ndf = 0.0;
            pt.status = RunCalibStatus::Success;
            pt.isFallback = false;
            pt.isValid = false;
            uncalibPoints.push_back(pt);
        }

        if (m_plotWidget) {
            m_plotWidget->setData(detId, uncalibPoints);
        }
        return;
    }

    const int numRuns = static_cast<int>(targetTraj->runResults.size());
    m_tablePlotData->setRowCount(numRuns);
    std::vector<DriftPlotPoint> plotPoints;
    plotPoints.reserve(numRuns);

    for (int r = 0; r < numRuns; ++r) {
        const auto &res = targetTraj->runResults[r];

        QTableWidgetItem *itemRun = new QTableWidgetItem(QString::number(res.runNumber));
        itemRun->setData(Qt::UserRole, r);
        itemRun->setTextAlignment(Qt::AlignCenter);
        m_tablePlotData->setItem(r, 0, itemRun);

        m_tablePlotData->setItem(r, 1, new QTableWidgetItem(res.runFileName));

        DriftPlotPoint pt;
        pt.runIndex = r;
        pt.runNumber = res.runNumber;
        pt.runFileName = res.runFileName;
        pt.gain = res.a1;
        pt.chi2ndf = res.chi2NDF;
        pt.status = res.status;
        pt.isFallback = res.isFallbackFromPrevious;
        pt.fallbackSourceRun = res.fallbackSourceRun;

        if (res.status == RunCalibStatus::Success && !res.isFallbackFromPrevious) {
            pt.isValid = true;
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem(QString::number(res.a1, 'f', 6)));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem(QString::number(res.a0, 'f', 4)));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem(QString::number(res.sRes, 'f', 4)));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem(QString::number(res.chi2NDF, 'f', 2)));
            QTableWidgetItem *st = new QTableWidgetItem("OK");
            st->setForeground(QBrush(QColor("#4caf50")));
            st->setTextAlignment(Qt::AlignCenter);
            m_tablePlotData->setItem(r, 6, st);
        } else if (res.isFallbackFromPrevious) {
            pt.isValid = true;
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem(QString::number(res.a1, 'f', 6)));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem(QString::number(res.a0, 'f', 4)));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem(QString::number(res.sRes, 'f', 4)));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem(QString::number(res.chi2NDF, 'f', 2)));
            QTableWidgetItem *st = new QTableWidgetItem(QString("FALLBACK (R%1)").arg(res.fallbackSourceRun));
            st->setForeground(QBrush(QColor("#ffa726")));
            st->setTextAlignment(Qt::AlignCenter);
            st->setToolTip(res.failureReason);
            m_tablePlotData->setItem(r, 6, st);
        } else if (res.status == RunCalibStatus::BadChi2) {
            pt.isValid = false;
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem(res.chi2NDF > 0.0 ? QString::number(res.chi2NDF, 'f', 2) : "-"));
            QTableWidgetItem *st = new QTableWidgetItem("INTERVENE");
            st->setForeground(QBrush(QColor("#ffa726")));
            st->setTextAlignment(Qt::AlignCenter);
            st->setToolTip(res.failureReason.isEmpty() ? tr("Double-click to inspect and intervene") : res.failureReason);
            m_tablePlotData->setItem(r, 6, st);
        } else {
            pt.isValid = false;
            m_tablePlotData->setItem(r, 2, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 3, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 4, new QTableWidgetItem("-"));
            m_tablePlotData->setItem(r, 5, new QTableWidgetItem("-"));
            QTableWidgetItem *st = new QTableWidgetItem("FAIL");
            st->setForeground(QBrush(QColor("#ef5350")));
            st->setTextAlignment(Qt::AlignCenter);
            st->setToolTip(res.failureReason.isEmpty() ? tr("No peaks found or fit failed") : res.failureReason);
            m_tablePlotData->setItem(r, 6, st);
        }

        plotPoints.push_back(pt);
    }

    if (m_plotWidget) {
        m_plotWidget->setData(detId, plotPoints);
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
// inspectSpectrumOnCanvas: Loads spectrum into active pad, fits peaks & auto-zooms
//==============================================================================
void RunByRunManager::inspectSpectrumOnCanvas(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_runFilePaths.size())) return;
    const int detStart = m_spinDetStart ? m_spinDetStart->value() : 0;
    const int detId = col + detStart;
    const QString filePath = m_runFilePaths[row];
    if (!m_mainCanvas) return;

    syncAnchorsFromTable();

    // 1. Read spectrum data for target run and detector
    std::vector<double> specData;
    QString err;
    const int chLen = m_comboChannelLength->currentData().toInt();
    if (!ReadSpectrumData(filePath.toStdString(), SpectrumFormat::LongInt32, chLen, detId, specData, &err) || specData.empty()) {
        QMessageBox::warning(this, tr("Spectrum Read Error"),
                             tr("Could not read spectrum for detector %1 from:\n%2\n\n%3")
                                 .arg(detId).arg(filePath).arg(err));
        return;
    }

    // 2. Load spectrum into active pad
    const QString specTitle = QString("%1#%2").arg(QFileInfo(filePath).fileName()).arg(detId);
    m_mainCanvas->loadSpectrumDataToPad(specData, specTitle, false);

    const int sel_i = m_mainCanvas->SelectedElement_i;
    const int sel_j = m_mainCanvas->SelectedElement_j;
    TH1F *hist = m_mainCanvas->HijF[sel_i][sel_j];
    if (!hist) return;

    TVirtualPad *pad = nullptr;
    if (m_mainCanvas->canvas && m_mainCanvas->canvas->getCanvas()) {
        pad = (m_mainCanvas->maxElement_i > 1 || m_mainCanvas->maxElement_j > 1)
            ? m_mainCanvas->canvas->getCanvas()->GetPad((sel_i - 1) * m_mainCanvas->maxElement_j + sel_j)
            : m_mainCanvas->canvas->getCanvas();
    }
    if (!pad) pad = gPad;

    // 3. Clear previous visual markers, fit functions, and labels on this pad
    m_mainCanvas->clearDrawnObjects();
    if (pad && pad->GetListOfPrimitives()) {
        for (auto *obj : m_mainCanvas->autoFitMarkers[sel_i][sel_j]) {
            if (obj) {
                pad->GetListOfPrimitives()->Remove(obj);
            }
        }
    }
    for (auto *obj : m_mainCanvas->autoFitMarkers[sel_i][sel_j]) {
        delete obj;
    }
    m_mainCanvas->autoFitMarkers[sel_i][sel_j].clear();
    m_mainCanvas->gaussCenters[sel_i][sel_j].clear();
    m_mainCanvas->gaussCentersHeight[sel_i][sel_j].clear();

    // 4. Fit anchor peaks and draw fit functions
    std::vector<double> peakPositions;
    int fitCount = 0;
    const int totalBins = static_cast<int>(specData.size());

    for (size_t aIdx = 0; aIdx < m_anchors.size(); ++aIdx) {
        const auto &a = m_anchors[aIdx];

        // Determine best search center: check if this run already has trajectory calibration
        double centerCh = (a.initialChannel > 0.0) ? a.initialChannel : a.physicalEnergy;
        for (const auto &traj : m_completedTrajectories) {
            if (traj.detectorId == detId) {
                if (row >= 0 && row < static_cast<int>(traj.runResults.size())) {
                    const auto &runRes = traj.runResults[row];
                    for (const auto &fa : runRes.fittedAnchors) {
                        if (std::abs(fa.energy - a.physicalEnergy) < 1.0 && fa.isValid && fa.centroidCh > 0.0) {
                            centerCh = fa.centroidCh;
                            break;
                        }
                    }
                }
                break;
            }
        }

        const double windowCh = (a.searchWindowCh > 0.0) ? a.searchWindowCh : 15.0;
        const double minCounts = (a.minCounts > 0.0) ? a.minCounts : 20.0;
        int lowCh = std::max(1, static_cast<int>(std::floor(centerCh - windowCh)));
        int highCh = std::min(totalBins - 2, static_cast<int>(std::ceil(centerCh + windowCh)));

        int maxBin = lowCh;
        double maxVal = (lowCh < totalBins) ? specData[lowCh] : 0.0;
        if (lowCh < highCh) {
            for (int ch = lowCh; ch <= highCh; ++ch) {
                if (specData[ch] > maxVal) {
                    maxVal = specData[ch];
                    maxBin = ch;
                }
            }
        }

        // Fallback to base initial channel if peak not distinct around moving centroid
        double baseCentroid = (a.initialChannel > 0.0) ? a.initialChannel : a.physicalEnergy;
        double curBase = (lowCh < highCh) ? (specData[lowCh] + specData[highCh]) / 2.0 : 0.0;
        if ((maxVal <= 0.0 || (maxVal - curBase) < minCounts) &&
            std::abs(centerCh - baseCentroid) > 0.5) {
            int bLow = std::max(1, static_cast<int>(std::floor(baseCentroid - windowCh)));
            int bHigh = std::min(totalBins - 2, static_cast<int>(std::ceil(baseCentroid + windowCh)));
            if (bLow < bHigh) {
                int bMaxBin = bLow;
                double bMaxVal = specData[bLow];
                for (int ch = bLow; ch <= bHigh; ++ch) {
                    if (specData[ch] > bMaxVal) {
                        bMaxVal = specData[ch];
                        bMaxBin = ch;
                    }
                }
                double bBaseVal = (specData[bLow] + specData[bHigh]) / 2.0;
                if ((bMaxVal - bBaseVal) >= minCounts) {
                    maxBin = bMaxBin;
                    maxVal = bMaxVal;
                    centerCh = baseCentroid;
                    lowCh = bLow;
                    highCh = bHigh;
                }
            }
        }

        double baseVal = (lowCh < highCh) ? (specData[lowCh] + specData[highCh]) / 2.0 : 0.0;
        double netHeight = maxVal - baseVal;

        bool fitSuccess = false;
        double fittedCentroid = centerCh;

        if (netHeight >= 5.0 && maxVal > 0.0) {
            const int fitHalfWidth = std::clamp(static_cast<int>(windowCh * 0.85), 5, 35);
            const int fitMin = std::max(1, maxBin - fitHalfWidth);
            const int fitMax = std::min(totalBins - 1, maxBin + fitHalfWidth);
            const int nFitBins = fitMax - fitMin + 1;

            if (nFitBins >= 5) {
                TH1F hTemp(Form("hAnchorTemp_%d_%d", detId, static_cast<int>(aIdx)), "Anchor",
                           nFitBins, fitMin - 0.5, fitMax + 0.5);
                hTemp.SetDirectory(nullptr);
                for (int i = 0; i < nFitBins; ++i) {
                    const int ch = fitMin + i;
                    hTemp.SetBinContent(i + 1, specData[ch]);
                    hTemp.SetBinError(i + 1, std::max(1.0, std::sqrt(std::max(0.0, specData[ch]))));
                }

                // Centered formula: [0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])
                // Parameter 3 is the local baseline AT the centroid, avoiding huge/negative extrapolations to channel 0
                TF1 fGaus(Form("fAnchorGaus_%d_%d", detId, static_cast<int>(aIdx)),
                          "[0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])", fitMin - 0.5, fitMax + 0.5);
                const double bkgSlope = (specData[fitMax] - specData[fitMin]) / static_cast<double>(nFitBins);
                const double estAmpl = std::max(1.0, maxVal - baseVal);
                const double estMean = static_cast<double>(maxBin);
                const double estSigma = (a.expectedFwhmCh > 0.0) ? (a.expectedFwhmCh / 2.35482) : 2.0;

                fGaus.SetParameter(0, estAmpl);
                fGaus.SetParameter(1, estMean);
                fGaus.SetParameter(2, estSigma);
                fGaus.SetParameter(3, baseVal);
                fGaus.SetParameter(4, bkgSlope);

                fGaus.SetParLimits(1, fitMin, fitMax);
                fGaus.SetParLimits(2, 0.4, 30.0);
                fGaus.SetParLimits(0, 0.0, maxVal * 3.0);

                TFitResultPtr fitRes = hTemp.Fit(&fGaus, "Q0NS", "", fitMin - 0.5, fitMax + 0.5);
                if (fitRes.Get() && fitRes->IsValid()) {
                    const double meanVal = fGaus.GetParameter(1);
                    const double sigmaVal = std::abs(fGaus.GetParameter(2));
                    const double fwhmVal = 2.35482 * sigmaVal;
                    const double fitChi2 = fitRes->Chi2() / std::max(1, static_cast<int>(fitRes->Ndf()));

                    if (meanVal >= lowCh && meanVal <= highCh && fwhmVal >= 0.5 && fwhmVal <= 50.0 && fitChi2 <= 50.0) {
                        fitSuccess = true;
                        fittedCentroid = meanVal;

                        if (pad) pad->cd();

                        // Fit function (Gaussian + Linear background) in vibrant red
                        TF1 *fFit = new TF1(Form("rbr_fit_%d_%d", detId, static_cast<int>(aIdx)),
                                            "[0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])", fitMin - 0.5, fitMax + 0.5);
                        fFit->SetParameters(fGaus.GetParameters());
                        fFit->SetLineColor(kRed);
                        fFit->SetLineWidth(2);
                        fFit->Draw("same");
                        m_mainCanvas->autoFitMarkers[sel_i][sel_j].push_back(fFit);

                        // Baseline background function in dashed blue: [0] + [1]*(x-[2])
                        TF1 *fBkg = new TF1(Form("rbr_background_%d_%d", detId, static_cast<int>(aIdx)),
                                            "[0] + [1]*(x-[2])", fitMin - 0.5, fitMax + 0.5);
                        fBkg->SetParameter(0, fGaus.GetParameter(3));
                        fBkg->SetParameter(1, fGaus.GetParameter(4));
                        fBkg->SetParameter(2, fGaus.GetParameter(1));
                        fBkg->SetLineColor(kBlue);
                        fBkg->SetLineWidth(1);
                        fBkg->SetLineStyle(2);
                        fBkg->Draw("same");
                        m_mainCanvas->autoFitMarkers[sel_i][sel_j].push_back(fBkg);

                        const double peakHeight = hist->GetBinContent(hist->FindBin(meanVal));
                        m_mainCanvas->gaussCenters[sel_i][sel_j].push_back(meanVal);
                        m_mainCanvas->gaussCentersHeight[sel_i][sel_j].push_back(peakHeight);
                        fitCount++;
                    }
                }
            }
        }

        peakPositions.push_back(fitSuccess ? fittedCentroid : centerCh);
    }

    // 5. Auto-zoom to the area of our peaks +-10% to the sides
    if (!peakPositions.empty()) {
        const double minPeakX = *std::min_element(peakPositions.begin(), peakPositions.end());
        const double maxPeakX = *std::max_element(peakPositions.begin(), peakPositions.end());
        const double span = maxPeakX - minPeakX;
        // +-10% to the sides of peak span (or 10% of peak position for single peak)
        const double margin = (span > 20.0) ? (0.10 * span) : std::max(30.0, 0.10 * maxPeakX);
        const double totalBinsD = static_cast<double>(hist->GetNbinsX());
        const double zLow = std::max(0.0, minPeakX - margin);
        const double zHigh = std::min(totalBinsD, maxPeakX + margin);

        if (zLow < zHigh) {
            hist->GetXaxis()->SetRangeUser(zLow, zHigh);
            m_mainCanvas->adjustYAxisToVisibleMax(hist);
        }
    }

    // 6. If calibration was already completed for this run & detector, apply it for energy readouts
    TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(hist);
    for (const auto &traj : m_completedTrajectories) {
        if (traj.detectorId == detId) {
            if (row >= 0 && row < static_cast<int>(traj.runResults.size())) {
                const auto &runRes = traj.runResults[row];
                if (tHist && (runRes.status == RunCalibStatus::Success || runRes.isFallbackFromPrevious)) {
                    tHist->SetCalibration(runRes.a0, runRes.a1, runRes.a2);
                }
            }
            break;
        }
    }

    // 7. Render peak centroid labels and update canvas
    m_mainCanvas->renderPeakLabels(sel_i, sel_j);
    m_mainCanvas->updateAxisStatusLabels();

    if (pad) {
        pad->Modified();
        pad->Update();
    }
    if (m_mainCanvas->canvas && m_mainCanvas->canvas->getCanvas()) {
        m_mainCanvas->canvas->getCanvas()->Modified();
        m_mainCanvas->canvas->getCanvas()->Update();
    }

    CommandPrompt::getInstance()->appendPlainText(
        QString("Inspecting run %1 [Detector #%2]: auto-zoomed to peak region, displayed %3 peak fit(s)\n")
            .arg(QFileInfo(filePath).fileName()).arg(detId).arg(fitCount));
}

//==============================================================================
// onCellDoubleClicked: Inspects spectrum on canvas, and if cell needs intervention,
// immediately opens the intervention dialog
//==============================================================================
void RunByRunManager::onCellDoubleClicked(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_runFilePaths.size())) return;
    const int detStart = m_spinDetStart ? m_spinDetStart->value() : 0;
    const int detId = col + detStart;

    // 1. Automatically load spectrum and auto-zoom to peak area on canvas
    inspectSpectrumOnCanvas(row, col);

    // 2. If flagged or needs intervention, immediately open intervention dialog
    bool needsIntervention = false;
    QTableWidgetItem *item = m_gridMatrix->item(row, col);
    if (item && (item->text() == tr("INTERVENE") || item->text() == "INTERVENE")) {
        needsIntervention = true;
    } else {
        for (const auto &traj : m_completedTrajectories) {
            if (traj.detectorId == detId) {
                if (row >= 0 && row < static_cast<int>(traj.runResults.size())) {
                    if (traj.runResults[row].status != RunCalibStatus::Success) {
                        needsIntervention = true;
                    }
                }
                break;
            }
        }
    }

    if (needsIntervention) {
        openInterventionDialog(row, col);
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

//==============================================================================
// openInterventionDialog: Modal for manual peak inspection, tuning, refitting,
// independent baseline/slope background tuning, parameter locking, and discrimination.
//==============================================================================
void RunByRunManager::openInterventionDialog(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_runFilePaths.size())) return;
    const int detStart = m_spinDetStart ? m_spinDetStart->value() : 0;
    const int detId = col + detStart;
    const QString filePath = m_runFilePaths[row];
    const QString runName = QFileInfo(filePath).fileName();

    syncAnchorsFromTable();
    if (m_anchors.empty()) {
        QMessageBox::warning(this, tr("No Anchors"), tr("No anchor lines are defined. Please set up anchor lines first."));
        return;
    }

    // Automatically load spectrum and auto-zoom to peak area on main canvas
    inspectSpectrumOnCanvas(row, col);

    // Read spectrum for this run and detector
    std::vector<double> specData;
    QString err;
    const int chLen = m_comboChannelLength ? m_comboChannelLength->currentData().toInt() : 65536;
    if (!ReadSpectrumData(filePath.toStdString(), SpectrumFormat::LongInt32, chLen, detId, specData, &err) || specData.empty()) {
        QMessageBox::warning(this, tr("Spectrum Read Error"),
                             tr("Could not read spectrum for detector %1 from:\n%2\n\n%3")
                                 .arg(detId).arg(filePath).arg(err));
        return;
    }

    // Find current trajectory and result if they exist
    DetectorTrajectory *targetTraj = nullptr;
    for (auto &traj : m_completedTrajectories) {
        if (traj.detectorId == detId) {
            targetTraj = &traj;
            break;
        }
    }

    RunCalibResult curRes;
    if (targetTraj && row < static_cast<int>(targetTraj->runResults.size())) {
        curRes = targetTraj->runResults[row];
    } else {
        curRes.detectorId = detId;
        curRes.runNumber = row + 1;
        curRes.runFileName = runName;
        curRes.fullFilePath = filePath;
        curRes.status = RunCalibStatus::BadChi2;
    }

    const double maxChi2 = m_spinMaxChi2 ? m_spinMaxChi2->value() : 5.0;
    const bool useQuadratic = m_chkQuadratic ? m_chkQuadratic->isChecked() : false;

    // Create Modal Dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Intervene & Discriminate Fits — Run: %1, Detector #%2").arg(runName).arg(detId));
    dlg.resize(1020, 620);
    dlg.setFont(Design::getDialogFont());
    dlg.setStyleSheet(Design::getDialogStyleSheet());

    QVBoxLayout *mainVBox = new QVBoxLayout(&dlg);
    mainVBox->setSpacing(10);

    // 1. Header info banner
    QGroupBox *grpInfo = new QGroupBox(tr("Current Status & Metrics"), &dlg);
    QHBoxLayout *infoLayout = new QHBoxLayout(grpInfo);
    QLabel *lblCurStatus = new QLabel(&dlg);
    QString statusColor = (curRes.status == RunCalibStatus::Success && !curRes.isFallbackFromPrevious) ? "#4caf50"
                         : (curRes.isFallbackFromPrevious ? "#ff9800"
                         : (curRes.status == RunCalibStatus::BadChi2 ? "#ffa726" : "#f44336"));
    QString statusName = (curRes.status == RunCalibStatus::Success && !curRes.isFallbackFromPrevious) ? "GOOD"
                        : (curRes.isFallbackFromPrevious ? QString("FALLBACK (Adopted R%1)").arg(curRes.fallbackSourceRun)
                        : (curRes.status == RunCalibStatus::BadChi2 ? "INTERVENE (High \u03c7\u00b2)" : "FAILED (No Peak / Stats)"));
    lblCurStatus->setText(QString("<b>Current Status:</b> <span style='color:%1; font-weight:bold;'>%2</span> &nbsp;|&nbsp; "
                                  "<b>Gain (a1):</b> %3 keV/ch &nbsp;|&nbsp; "
                                  "<b>Offset (a0):</b> %4 keV &nbsp;|&nbsp; "
                                  "<b>\u03c7\u00b2/ndf:</b> %5 &nbsp;|&nbsp; "
                                  "<b>Cutoff:</b> \u2264 %6")
                              .arg(statusColor)
                              .arg(statusName)
                              .arg(curRes.a1 > 0.0 ? QString::number(curRes.a1, 'f', 6) : "-")
                              .arg(curRes.a0 != 0.0 ? QString::number(curRes.a0, 'f', 4) : "-")
                              .arg(curRes.chi2NDF > 0.0 ? QString::number(curRes.chi2NDF, 'f', 2) : "-")
                              .arg(QString::number(maxChi2, 'f', 1)));
    infoLayout->addWidget(lblCurStatus);
    mainVBox->addWidget(grpInfo);

    // 2. Interactive Peak Table with Independent Baseline & Slope Background and Parameter Locks
    QGroupBox *grpPeaks = new QGroupBox(tr("Peak Anchors, Independent Background (Baseline & Slope) & Parameter Locks"), &dlg);
    QVBoxLayout *peaksVBox = new QVBoxLayout(grpPeaks);

    QTableWidget *tbl = new QTableWidget(&dlg);
    tbl->setColumnCount(9);
    tbl->setHorizontalHeaderLabels({
        tr("Use"),
        tr("Energy (keV)"),
        tr("Centroid (ch) 🔒"),
        tr("Shift (\u0394 ch)"),
        tr("Window (\u00b1 ch)"),
        tr("FWHM (ch) 🔒"),
        tr("Baseline (cts) 🔒"),
        tr("Slope (cts/ch) 🔒"),
        tr("Action")
    });
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setStretchLastSection(true);

    const int nAnchors = static_cast<int>(m_anchors.size());
    tbl->setRowCount(nAnchors);

    struct PeakRowWidgets {
        QCheckBox *chkUse{nullptr};
        double energy{0.0};
        double energyError{0.05};
        double nominalCentroid{0.0};

        QDoubleSpinBox *spinCentroid{nullptr};
        QPushButton *btnFixCentroid{nullptr};

        QLabel *lblShift{nullptr};

        QDoubleSpinBox *spinWindow{nullptr};

        QDoubleSpinBox *spinFwhm{nullptr};
        QPushButton *btnFixFwhm{nullptr};

        QDoubleSpinBox *spinBkg{nullptr};
        QPushButton *btnFixBkg{nullptr};

        QDoubleSpinBox *spinSlope{nullptr};
        QPushButton *btnFixSlope{nullptr};

        QPushButton *btnRefit{nullptr};

        TLine *guideLine{nullptr};
        TF1 *fitFunc{nullptr};
        TF1 *bkgFunc{nullptr};
    };
    std::vector<PeakRowWidgets> rowWidgets(nAnchors);

    auto makeLockButton = [](const QString &paramName, QWidget *parent) -> QPushButton* {
        QPushButton *btn = new QPushButton("🔓", parent);
        btn->setCheckable(true);
        btn->setFixedWidth(28);
        btn->setFixedHeight(24);
        btn->setToolTip(QString(QObject::tr("Lock/Fix %1 during peak refit")).arg(paramName));
        btn->setStyleSheet(
            "QPushButton { background-color: #37474f; color: #cfd8dc; border-radius: 3px; font-size: 10pt; padding: 0px; border: 1px solid #455a64; }"
            "QPushButton:hover { background-color: #455a64; }"
            "QPushButton:checked { background-color: #f57c00; color: #ffffff; border: 1px solid #ffb74d; }"
        );
        QObject::connect(btn, &QPushButton::toggled, btn, [btn](bool checked) {
            btn->setText(checked ? "🔒" : "🔓");
        });
        return btn;
    };

    for (int i = 0; i < nAnchors; ++i) {
        const auto &anc = m_anchors[i];
        rowWidgets[i].energy = anc.physicalEnergy;
        rowWidgets[i].energyError = anc.energyError;

        double nominalCh = (anc.initialChannel > 0.0) ? anc.initialChannel : anc.physicalEnergy;
        rowWidgets[i].nominalCentroid = nominalCh;

        double initCentroid = nominalCh;
        double initFwhm = (anc.expectedFwhmCh > 0.0) ? anc.expectedFwhmCh : 3.5;
        double initWin = (anc.searchWindowCh > 0.0) ? anc.searchWindowCh : 15.0;
        double initBkg = 10.0;
        double initSlope = 0.0;
        bool isUsed = true;

        for (const auto &fa : curRes.fittedAnchors) {
            if (std::abs(fa.energy - anc.physicalEnergy) < 1.0) {
                if (fa.isValid && fa.centroidCh > 0.0) {
                    initCentroid = fa.centroidCh;
                    initFwhm = fa.fwhmCh;
                    if (fa.background > 0.0) {
                        initBkg = fa.background;
                    }
                    initSlope = fa.bkgSlope;
                }
                isUsed = fa.isValid;
                break;
            }
        }

        // Estimate background from spectrum edges if not yet known
        if (initBkg <= 10.0 && !specData.empty()) {
            int lowB = std::max(0, static_cast<int>(initCentroid - initWin));
            int highB = std::min(static_cast<int>(specData.size()) - 1, static_cast<int>(initCentroid + initWin));
            if (lowB < highB) {
                initBkg = std::max(0.0, (specData[lowB] + specData[highB]) / 2.0);
                initSlope = (specData[highB] - specData[lowB]) / static_cast<double>(highB - lowB);
            }
        }

        // Col 0: Checkbox
        QWidget *chkWidget = new QWidget(tbl);
        QHBoxLayout *chkLayout = new QHBoxLayout(chkWidget);
        chkLayout->setContentsMargins(0, 0, 0, 0);
        chkLayout->setAlignment(Qt::AlignCenter);
        QCheckBox *chk = new QCheckBox(chkWidget);
        chk->setChecked(isUsed);
        chkLayout->addWidget(chk);
        rowWidgets[i].chkUse = chk;
        tbl->setCellWidget(i, 0, chkWidget);

        // Col 1: Energy label
        QTableWidgetItem *eItem = new QTableWidgetItem(QString("%1 \u00b1 %2").arg(anc.physicalEnergy, 0, 'f', 2).arg(anc.energyError, 0, 'f', 2));
        eItem->setFlags(Qt::ItemIsEnabled);
        eItem->setTextAlignment(Qt::AlignCenter);
        tbl->setItem(i, 1, eItem);

        // Col 2: Centroid SpinBox + Lock Button
        QWidget *wCentroid = new QWidget(tbl);
        QHBoxLayout *lCentroid = new QHBoxLayout(wCentroid);
        lCentroid->setContentsMargins(2, 2, 2, 2);
        lCentroid->setSpacing(4);
        QDoubleSpinBox *spinC = new QDoubleSpinBox(wCentroid);
        spinC->setRange(1.0, chLen - 1);
        spinC->setDecimals(2);
        spinC->setSingleStep(0.1);
        spinC->setValue(initCentroid);
        QPushButton *btnFixC = makeLockButton(tr("Centroid"), wCentroid);
        lCentroid->addWidget(spinC, 1);
        lCentroid->addWidget(btnFixC);
        rowWidgets[i].spinCentroid = spinC;
        rowWidgets[i].btnFixCentroid = btnFixC;
        tbl->setCellWidget(i, 2, wCentroid);

        // Col 3: Shift Indicator Badge
        QLabel *lblShift = new QLabel(tbl);
        lblShift->setAlignment(Qt::AlignCenter);
        double delta = initCentroid - nominalCh;
        QString sign = (delta > 0.005) ? "+" : "";
        lblShift->setText(QString("\u0394 %1%2 ch").arg(sign).arg(delta, 0, 'f', 2));
        lblShift->setStyleSheet("color: #90a4ae; font-weight: bold; padding: 2px 6px;");
        rowWidgets[i].lblShift = lblShift;
        tbl->setCellWidget(i, 3, lblShift);

        // Col 4: Search Window SpinBox
        QDoubleSpinBox *spinW = new QDoubleSpinBox(tbl);
        spinW->setRange(2.0, 200.0);
        spinW->setDecimals(1);
        spinW->setSingleStep(1.0);
        spinW->setValue(initWin);
        spinW->setToolTip(tr("Search and fit half-window in channels (\u00b1 \u0394ch) around centroid."));
        rowWidgets[i].spinWindow = spinW;
        tbl->setCellWidget(i, 4, spinW);

        // Col 5: FWHM SpinBox + Lock Button
        QWidget *wFwhm = new QWidget(tbl);
        QHBoxLayout *lFwhm = new QHBoxLayout(wFwhm);
        lFwhm->setContentsMargins(2, 2, 2, 2);
        lFwhm->setSpacing(4);
        QDoubleSpinBox *spinF = new QDoubleSpinBox(wFwhm);
        spinF->setRange(0.2, 50.0);
        spinF->setDecimals(2);
        spinF->setSingleStep(0.1);
        spinF->setValue(initFwhm);
        QPushButton *btnFixF = makeLockButton(tr("FWHM"), wFwhm);
        lFwhm->addWidget(spinF, 1);
        lFwhm->addWidget(btnFixF);
        rowWidgets[i].spinFwhm = spinF;
        rowWidgets[i].btnFixFwhm = btnFixF;
        tbl->setCellWidget(i, 5, wFwhm);

        // Col 6: Baseline Background SpinBox + Lock Button
        QWidget *wBkg = new QWidget(tbl);
        QHBoxLayout *lBkg = new QHBoxLayout(wBkg);
        lBkg->setContentsMargins(2, 2, 2, 2);
        lBkg->setSpacing(4);
        QDoubleSpinBox *spinB = new QDoubleSpinBox(wBkg);
        spinB->setRange(0.0, 10000000.0);
        spinB->setDecimals(1);
        spinB->setSingleStep(1.0);
        spinB->setValue(initBkg);
        spinB->setToolTip(tr("Baseline offset (cts) directly under peak centroid."));
        QPushButton *btnFixB = makeLockButton(tr("Baseline"), wBkg);
        lBkg->addWidget(spinB, 1);
        lBkg->addWidget(btnFixB);
        rowWidgets[i].spinBkg = spinB;
        rowWidgets[i].btnFixBkg = btnFixB;
        tbl->setCellWidget(i, 6, wBkg);

        // Col 7: Background Slope SpinBox + Lock Button
        QWidget *wSlope = new QWidget(tbl);
        QHBoxLayout *lSlope = new QHBoxLayout(wSlope);
        lSlope->setContentsMargins(2, 2, 2, 2);
        lSlope->setSpacing(4);
        QDoubleSpinBox *spinS = new QDoubleSpinBox(wSlope);
        spinS->setRange(-1000.0, 1000.0);
        spinS->setDecimals(3);
        spinS->setSingleStep(0.01);
        spinS->setValue(initSlope);
        spinS->setToolTip(tr("Background slope (cts/ch) across the peak fitting region."));
        QPushButton *btnFixS = makeLockButton(tr("Slope"), wSlope);
        lSlope->addWidget(spinS, 1);
        lSlope->addWidget(btnFixS);
        rowWidgets[i].spinSlope = spinS;
        rowWidgets[i].btnFixSlope = btnFixS;
        tbl->setCellWidget(i, 7, wSlope);

        // Col 8: Refit button with high-contrast, fully readable theme-compliant styling
        QPushButton *btnRefit = new QPushButton(tr("🔄 Refit Peak"), tbl);
        btnRefit->setStyleSheet(
            "QPushButton {"
            "  background-color: #0277bd;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  border-radius: 3px;"
            "  padding: 4px 10px;"
            "  border: 1px solid #0288d1;"
            "}"
            "QPushButton:hover { background-color: #039be5; }"
            "QPushButton:pressed { background-color: #01579b; }"
        );
        rowWidgets[i].btnRefit = btnRefit;
        tbl->setCellWidget(i, 8, btnRefit);
    }

    peaksVBox->addWidget(tbl);
    mainVBox->addWidget(grpPeaks);

    // 3. Live Recalibration & Preview Panel
    QGroupBox *grpPreview = new QGroupBox(tr("Live Recalibration Preview"), &dlg);
    QHBoxLayout *prevLayout = new QHBoxLayout(grpPreview);
    QLabel *lblPreviewStatus = new QLabel(&dlg);
    lblPreviewStatus->setFont(QFont("Arial", 10, QFont::Bold));
    prevLayout->addWidget(lblPreviewStatus);
    mainVBox->addWidget(grpPreview);

    RunCalibResult liveCalibResult;

    // Helper to update on-screen visual ROOT canvas marker guideline, fit curve, and baseline
    auto updateCanvasPeakOverlay = [&](int peakIdx) {
        if (peakIdx < 0 || peakIdx >= nAnchors) return;
        if (!m_mainCanvas) return;

        double cCh = rowWidgets[peakIdx].spinCentroid->value();
        double fCh = rowWidgets[peakIdx].spinFwhm->value();
        double bkgVal = rowWidgets[peakIdx].spinBkg->value();
        double slopeVal = rowWidgets[peakIdx].spinSlope->value();
        double nomCh = rowWidgets[peakIdx].nominalCentroid;
        double wCh = rowWidgets[peakIdx].spinWindow ? rowWidgets[peakIdx].spinWindow->value() : std::clamp(fCh * 3.5, 12.0, 45.0);

        // 1. Update table shift badge
        double delta = cCh - nomCh;
        QString sign = (delta > 0.005) ? "+" : "";
        rowWidgets[peakIdx].lblShift->setText(QString("\u0394 %1%2 ch").arg(sign).arg(delta, 0, 'f', 2));
        if (std::abs(delta) < 0.01) {
            rowWidgets[peakIdx].lblShift->setStyleSheet("color: #90a4ae; font-weight: bold; padding: 2px 6px;");
        } else if (std::abs(delta) < 2.0) {
            rowWidgets[peakIdx].lblShift->setStyleSheet("color: #81c784; font-weight: bold; padding: 2px 6px; background-color: rgba(76, 175, 80, 0.18); border-radius: 3px;");
        } else if (std::abs(delta) < 5.0) {
            rowWidgets[peakIdx].lblShift->setStyleSheet("color: #ffb74d; font-weight: bold; padding: 2px 6px; background-color: rgba(255, 152, 0, 0.18); border-radius: 3px;");
        } else {
            rowWidgets[peakIdx].lblShift->setStyleSheet("color: #ef5350; font-weight: bold; padding: 2px 6px; background-color: rgba(244, 67, 54, 0.18); border-radius: 3px;");
        }

        // 2. Active ROOT pad & spectrum histogram
        const int s_i = m_mainCanvas->SelectedElement_i;
        const int s_j = m_mainCanvas->SelectedElement_j;
        TH1F *h = m_mainCanvas->HijF[s_i][s_j];
        if (!h) return;

        TVirtualPad *activePad = nullptr;
        if (m_mainCanvas->canvas && m_mainCanvas->canvas->getCanvas()) {
            activePad = (m_mainCanvas->maxElement_i > 1 || m_mainCanvas->maxElement_j > 1)
                ? m_mainCanvas->canvas->getCanvas()->GetPad((s_i - 1) * m_mainCanvas->maxElement_j + s_j)
                : m_mainCanvas->canvas->getCanvas();
        }
        if (!activePad) activePad = gPad;
        if (!activePad) return;

        activePad->cd();

        // 3. Visual Guideline TLine at current centroid
        TLine *guideLine = rowWidgets[peakIdx].guideLine;
        if (!guideLine) {
            guideLine = new TLine();
            guideLine->SetLineColor(kYellow + 1);
            guideLine->SetLineWidth(2);
            guideLine->SetLineStyle(7); // Dashed bright yellow line
            guideLine->Draw();
            rowWidgets[peakIdx].guideLine = guideLine;
            m_mainCanvas->autoFitMarkers[s_i][s_j].push_back(guideLine);
        }
        double yMax = h->GetMaximum();
        guideLine->SetX1(cCh);
        guideLine->SetX2(cCh);
        guideLine->SetY1(0.0);
        guideLine->SetY2(yMax * 1.05);

        // 4. Gaussian Fit Function TF1: [0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])
        int binC = h->FindBin(cCh);
        double binCounts = (binC >= 1 && binC <= h->GetNbinsX()) ? h->GetBinContent(binC) : 0.0;
        double ampl = std::max(1.0, binCounts - bkgVal);
        double sigma = std::max(0.2, fCh / 2.35482);
        double xMin = std::max(0.0, cCh - wCh);
        double xMax = std::min(static_cast<double>(h->GetNbinsX()), cCh + wCh);

        TF1 *fFit = rowWidgets[peakIdx].fitFunc;
        if (!fFit) {
            fFit = dynamic_cast<TF1*>(activePad->GetPrimitive(Form("rbr_fit_%d_%d", detId, peakIdx)));
        }
        if (!fFit) {
            fFit = new TF1(Form("rbr_fit_%d_%d", detId, peakIdx),
                           "[0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])", xMin, xMax);
            fFit->SetLineColor(kRed);
            fFit->SetLineWidth(2);
            fFit->Draw("same");
            m_mainCanvas->autoFitMarkers[s_i][s_j].push_back(fFit);
        }
        rowWidgets[peakIdx].fitFunc = fFit;
        fFit->SetRange(xMin, xMax);
        fFit->SetParameter(0, ampl);
        fFit->SetParameter(1, cCh);
        fFit->SetParameter(2, sigma);
        fFit->SetParameter(3, bkgVal);
        fFit->SetParameter(4, slopeVal);

        // 5. Baseline Background Function TF1: [0] + [1]*(x-[2])
        TF1 *fBkg = rowWidgets[peakIdx].bkgFunc;
        if (!fBkg) {
            fBkg = dynamic_cast<TF1*>(activePad->GetPrimitive(Form("rbr_background_%d_%d", detId, peakIdx)));
        }
        if (!fBkg) {
            fBkg = new TF1(Form("rbr_background_%d_%d", detId, peakIdx),
                           "[0] + [1]*(x-[2])", xMin, xMax);
            fBkg->SetLineColor(kBlue);
            fBkg->SetLineWidth(1);
            fBkg->SetLineStyle(2);
            fBkg->Draw("same");
            m_mainCanvas->autoFitMarkers[s_i][s_j].push_back(fBkg);
        }
        rowWidgets[peakIdx].bkgFunc = fBkg;
        fBkg->SetRange(xMin, xMax);
        fBkg->SetParameter(0, bkgVal);
        fBkg->SetParameter(1, slopeVal);
        fBkg->SetParameter(2, cCh);

        activePad->Modified();
        activePad->Update();
        if (m_mainCanvas->canvas && m_mainCanvas->canvas->getCanvas()) {
            m_mainCanvas->canvas->getCanvas()->Modified();
            m_mainCanvas->canvas->getCanvas()->Update();
        }
    };

    auto recalcLivePreview = [&]() {
        std::vector<FittedAnchor> activeAnchors;
        for (int i = 0; i < nAnchors; ++i) {
            if (rowWidgets[i].chkUse && rowWidgets[i].chkUse->isChecked()) {
                FittedAnchor fa;
                fa.energy = rowWidgets[i].energy;
                fa.energyErr = rowWidgets[i].energyError;
                fa.centroidCh = rowWidgets[i].spinCentroid->value();
                fa.fwhmCh = rowWidgets[i].spinFwhm->value();
                fa.background = rowWidgets[i].spinBkg->value();
                fa.bkgSlope = rowWidgets[i].spinSlope->value();
                fa.isValid = true;
                activeAnchors.push_back(fa);
            }
        }

        if (activeAnchors.size() < 2) {
            lblPreviewStatus->setText(tr("<span style='color:#f44336;'>⚠️ Insufficient peaks selected (%1). At least 2 required.</span>").arg(activeAnchors.size()));
            liveCalibResult = RunCalibResult();
            liveCalibResult.status = RunCalibStatus::BadChi2;
            return;
        }

        RunCalibResult res;
        res.detectorId = detId;
        res.runNumber = row + 1;
        res.runFileName = runName;
        res.fullFilePath = filePath;
        res.fittedAnchors = activeAnchors;

        bool ok = RunByRunEngine::computeCalibration(activeAnchors, useQuadratic, res, maxChi2);
        liveCalibResult = res;

        QString chi2Str = QString::number(res.chi2NDF, 'f', 2);
        QString gainStr = QString::number(res.a1, 'f', 6);
        QString offStr = QString::number(res.a0, 'f', 4);
        QString resStr = QString::number(res.sRes, 'f', 4);

        if (ok && res.status == RunCalibStatus::Success) {
            lblPreviewStatus->setText(QString("<span style='color:#4caf50;'>✅ ACCEPTABLE FIT: a1 = %1 keV/ch | a0 = %2 keV | s_res = %3 keV | <b>\u03c7\u00b2/ndf = %4</b> (\u2264 %5 cutoff)</span>")
                                          .arg(gainStr).arg(offStr).arg(resStr).arg(chi2Str).arg(maxChi2, 0, 'f', 1));
        } else {
            lblPreviewStatus->setText(QString("<span style='color:#ff9800;'>⚠️ HIGH \u03c7\u00b2 FIT: a1 = %1 keV/ch | a0 = %2 keV | s_res = %3 keV | <b>\u03c7\u00b2/ndf = %4</b> (> %5 cutoff)</span>")
                                          .arg(gainStr).arg(offStr).arg(resStr).arg(chi2Str).arg(maxChi2, 0, 'f', 1));
        }
    };

    // Connect spinboxes and checkboxes to live recalc and on-screen canvas feedback
    for (int i = 0; i < nAnchors; ++i) {
        int peakIdx = i;

        connect(rowWidgets[i].chkUse, &QCheckBox::toggled, &dlg, [=]() {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        connect(rowWidgets[i].spinCentroid, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [=](double) {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        connect(rowWidgets[i].spinWindow, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [=](double) {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        connect(rowWidgets[i].spinFwhm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [=](double) {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        connect(rowWidgets[i].spinBkg, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [=](double) {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        connect(rowWidgets[i].spinSlope, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [=](double) {
            recalcLivePreview();
            updateCanvasPeakOverlay(peakIdx);
        });

        // Refit button action
        connect(rowWidgets[i].btnRefit, &QPushButton::clicked, &dlg, [&, peakIdx]() {
            double cCh = rowWidgets[peakIdx].spinCentroid->value();
            double fCh = rowWidgets[peakIdx].spinFwhm->value();
            double bkgVal = rowWidgets[peakIdx].spinBkg->value();
            double slopeVal = rowWidgets[peakIdx].spinSlope->value();
            double wCh = rowWidgets[peakIdx].spinWindow ? rowWidgets[peakIdx].spinWindow->value() : std::clamp(fCh * 3.5, 12.0, 45.0);
            double en = rowWidgets[peakIdx].energy;

            FitFixedParams fixed;
            fixed.fixCentroid = rowWidgets[peakIdx].btnFixCentroid && rowWidgets[peakIdx].btnFixCentroid->isChecked();
            fixed.fixedCentroid = cCh;

            fixed.fixFwhm = rowWidgets[peakIdx].btnFixFwhm && rowWidgets[peakIdx].btnFixFwhm->isChecked();
            fixed.fixedFwhm = fCh;

            fixed.fixBaseline = rowWidgets[peakIdx].btnFixBkg && rowWidgets[peakIdx].btnFixBkg->isChecked();
            fixed.fixedBaseline = bkgVal;

            fixed.fixSlope = rowWidgets[peakIdx].btnFixSlope && rowWidgets[peakIdx].btnFixSlope->isChecked();
            fixed.fixedSlope = slopeVal;

            FittedAnchor fa = RunByRunEngine::fitAnchorPeakWithFixed(specData, cCh, wCh, 5.0, en, fixed);
            if (fa.isValid) {
                if (!fixed.fixCentroid) rowWidgets[peakIdx].spinCentroid->setValue(fa.centroidCh);
                if (!fixed.fixFwhm) rowWidgets[peakIdx].spinFwhm->setValue(std::max(0.4, fa.fwhmCh));
                if (!fixed.fixBaseline && fa.background > 0.0) rowWidgets[peakIdx].spinBkg->setValue(fa.background);
                if (!fixed.fixSlope) rowWidgets[peakIdx].spinSlope->setValue(fa.bkgSlope);

                rowWidgets[peakIdx].chkUse->setChecked(true);
                recalcLivePreview();
                updateCanvasPeakOverlay(peakIdx);

                QString lockMsg;
                if (fixed.fixCentroid || fixed.fixFwhm || fixed.fixBaseline || fixed.fixSlope) {
                    lockMsg = tr(" (respecting locked parameters)");
                }
                QToolTip::showText(rowWidgets[peakIdx].btnRefit->mapToGlobal(QPoint(0, 0)),
                                   tr("Fitted successfully%1:\nCentroid = %2 ch, FWHM = %3 ch\nBaseline = %4 cts, Slope = %5 cts/ch")
                                       .arg(lockMsg)
                                       .arg(rowWidgets[peakIdx].spinCentroid->value(), 0, 'f', 2)
                                       .arg(rowWidgets[peakIdx].spinFwhm->value(), 0, 'f', 2)
                                       .arg(rowWidgets[peakIdx].spinBkg->value(), 0, 'f', 1)
                                       .arg(rowWidgets[peakIdx].spinSlope->value(), 0, 'f', 3));
            } else {
                QMessageBox::information(&dlg, tr("Refit Notice"),
                                         tr("Could not converge Gaussian fit around channel %1. You can nudge parameters manually or lock specific values.")
                                             .arg(cCh, 0, 'f', 1));
            }
        });
    }

    recalcLivePreview();

    // 4. Action Buttons (Option A, Option B, Option C, Inspect on Canvas, Cancel)
    QHBoxLayout *actionLayout = new QHBoxLayout();

    QPushButton *btnCanvas = new QPushButton(tr("🔍 Inspect Spectrum on Canvas"), &dlg);
    btnCanvas->setStyleSheet("font-weight: bold; background-color: #0277bd; color: white; padding: 6px 12px;");
    connect(btnCanvas, &QPushButton::clicked, &dlg, [&]() {
        inspectSpectrumOnCanvas(row, col);
    });
    actionLayout->addWidget(btnCanvas);
    actionLayout->addStretch(1);

    QPushButton *btnOptionB = new QPushButton(tr("✅ Apply Adjusted Fit (Mark GOOD)"), &dlg);
    btnOptionB->setStyleSheet("font-weight: bold; background-color: #2e7d32; color: white; padding: 6px 14px;");

    QPushButton *btnOptionA = new QPushButton(tr("⚠️ Force Accept As-Is"), &dlg);
    btnOptionA->setStyleSheet("font-weight: bold; background-color: #f57c00; color: white; padding: 6px 12px;");

    QPushButton *btnOptionC = new QPushButton(tr("❌ Reject (Make it FAIL)"), &dlg);
    btnOptionC->setStyleSheet("font-weight: bold; background-color: #c62828; color: white; padding: 6px 12px;");

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), &dlg);
    btnCancel->setStyleSheet("font-weight: bold; background-color: #455a64; color: white; padding: 6px 12px;");

    actionLayout->addWidget(btnOptionB);
    actionLayout->addWidget(btnOptionA);
    actionLayout->addWidget(btnOptionC);
    actionLayout->addWidget(btnCancel);
    mainVBox->addLayout(actionLayout);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    // Apply Option B
    connect(btnOptionB, &QPushButton::clicked, &dlg, [&]() {
        recalcLivePreview();
        if (liveCalibResult.fittedAnchors.size() < 2 || liveCalibResult.a1 <= 0.0) {
            QMessageBox::warning(&dlg, tr("Invalid Calibration"),
                                 tr("Cannot apply calibration: at least 2 valid peak fits with positive slope are required."));
            return;
        }

        liveCalibResult.status = RunCalibStatus::Success;
        liveCalibResult.isFallbackFromPrevious = false;
        liveCalibResult.failureReason.clear();

        // Update or insert into trajectories
        if (!targetTraj) {
            DetectorTrajectory newTraj;
            newTraj.detectorId = detId;
            newTraj.runResults.resize(m_runFilePaths.size());
            for (size_t rIdx = 0; rIdx < m_runFilePaths.size(); ++rIdx) {
                newTraj.runResults[rIdx].runNumber = static_cast<int>(rIdx + 1);
                newTraj.runResults[rIdx].runFileName = QFileInfo(m_runFilePaths[rIdx]).fileName();
                newTraj.runResults[rIdx].fullFilePath = m_runFilePaths[rIdx];
                newTraj.runResults[rIdx].detectorId = detId;
                newTraj.runResults[rIdx].status = RunCalibStatus::BadChi2;
            }
            m_completedTrajectories.push_back(newTraj);
            targetTraj = &m_completedTrajectories.back();
        }

        if (row < static_cast<int>(targetTraj->runResults.size())) {
            targetTraj->runResults[row] = liveCalibResult;
        }

        // Update last valid centroids
        targetTraj->lastValidCentroids.clear();
        for (const auto &fa : liveCalibResult.fittedAnchors) {
            targetTraj->lastValidCentroids.push_back(fa.centroidCh);
        }

        // Update Matrix Cell
        QTableWidgetItem *item = m_gridMatrix->item(row, col);
        if (!item) {
            item = new QTableWidgetItem();
            m_gridMatrix->setItem(row, col, item);
        }
        item->setText("GOOD");
        item->setBackground(QBrush(QColor("#2e7d32")));
        item->setForeground(QBrush(Qt::white));
        item->setToolTip(QString("Run: %1 | Det: %2 [MANUALLY ADJUSTED]\na0: %3 keV\na1: %4 keV/ch\ns_res: %5 keV\nchi2/ndf: %6")
                             .arg(runName).arg(detId)
                             .arg(liveCalibResult.a0, 0, 'f', 4)
                             .arg(liveCalibResult.a1, 0, 'f', 6)
                             .arg(liveCalibResult.sRes, 0, 'f', 4)
                             .arg(liveCalibResult.chi2NDF, 0, 'f', 2));

        // Refresh canvas with updated calibration & peak fits
        inspectSpectrumOnCanvas(row, col);

        // Refresh plot if detector selected
        if (m_comboPlotDet && m_comboPlotDet->currentData().toInt() == detId) {
            updatePlot(detId);
        }

        dlg.accept();
    });

    // Apply Option A: Force accept
    connect(btnOptionA, &QPushButton::clicked, &dlg, [&]() {
        if (!targetTraj) {
            QMessageBox::warning(&dlg, tr("No Data"), tr("No existing fit parameters found to accept."));
            return;
        }
        if (row < static_cast<int>(targetTraj->runResults.size())) {
            targetTraj->runResults[row].status = RunCalibStatus::Success;
            targetTraj->runResults[row].isFallbackFromPrevious = false;
            targetTraj->runResults[row].failureReason = "Force accepted by user";

            QTableWidgetItem *item = m_gridMatrix->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                m_gridMatrix->setItem(row, col, item);
            }
            item->setText("GOOD");
            item->setBackground(QBrush(QColor("#2e7d32")));
            item->setForeground(QBrush(Qt::white));
            item->setToolTip(QString("Run: %1 | Det: %2 [FORCE ACCEPTED]\na0: %3 keV\na1: %4 keV/ch\ns_res: %5 keV\nchi2/ndf: %6")
                                 .arg(runName).arg(detId)
                                 .arg(targetTraj->runResults[row].a0, 0, 'f', 4)
                                 .arg(targetTraj->runResults[row].a1, 0, 'f', 6)
                                 .arg(targetTraj->runResults[row].sRes, 0, 'f', 4)
                                 .arg(targetTraj->runResults[row].chi2NDF, 0, 'f', 2));

            if (m_comboPlotDet && m_comboPlotDet->currentData().toInt() == detId) {
                updatePlot(detId);
            }
        }
        dlg.accept();
    });

    // Apply Option C: Reject entirely
    connect(btnOptionC, &QPushButton::clicked, &dlg, [&]() {
        if (targetTraj && row < static_cast<int>(targetTraj->runResults.size())) {
            targetTraj->runResults[row].status = RunCalibStatus::NoPeakInWindow;
            targetTraj->runResults[row].failureReason = "Manually rejected by user";
            targetTraj->runResults[row].a0 = 0.0;
            targetTraj->runResults[row].a1 = 0.0;
            targetTraj->runResults[row].a2 = 0.0;

            QTableWidgetItem *item = m_gridMatrix->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                m_gridMatrix->setItem(row, col, item);
            }
            item->setText("FAIL");
            item->setBackground(QBrush(QColor("#c62828")));
            item->setForeground(QBrush(Qt::white));
            item->setToolTip(QString("Run: %1 | Det: %2\nFAILURE: Manually rejected by user (Chi2/ndf was %3)")
                                 .arg(runName).arg(detId).arg(curRes.chi2NDF, 0, 'f', 2));

            if (m_comboPlotDet && m_comboPlotDet->currentData().toInt() == detId) {
                updatePlot(detId);
            }
        }
        dlg.accept();
    });

    dlg.exec();
}

