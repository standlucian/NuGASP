#include "AutoCalibDialog.h"
#include "canvas.h"
#include "PeakFit.h"
#include "tracknhistogram.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include "Design.h"
#include <algorithm>
#include <cmath>

AutoCalibDialog::AutoCalibDialog(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Automatic Energy Calibration (AK)"));
    resize(680, 620);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    initIsotopes();
    setupUI();
}

void AutoCalibDialog::initIsotopes()
{
    // Legacy Xtrackn 11 standard sources from trackn.F:5031 (INPUTRENE)
    m_isotopes = {
        { "60Co  (1173.2, 1332.5 keV)",
          { 1173.238, 1332.513 }, { 0.015, 0.018 } },

        { "152Eu (121.8, 244.7, 344.3, 778.9, 964.1, 1112.1, 1408.0 keV)",
          { 121.7817, 244.6975, 344.2785, 411.1165, 443.965, 778.9045, 867.378, 964.079, 1085.869, 1089.737, 1112.076, 1299.142, 1408.013 },
          { 0.0003, 0.0008, 0.0017, 0.008, 0.006, 0.009, 0.030, 0.034, 0.034, 0.034, 0.070, 0.035, 0.035 } },

        { "137Cs (661.66 keV)",
          { 661.661 }, { 0.003 } },

        { "133Ba (53.2, 79.6, 81.0, 160.6, 223.1, 276.4, 302.9, 356.0, 383.9 keV)",
          { 53.156, 79.623, 80.999, 160.609, 223.116, 276.404, 302.858, 356.014, 383.859 },
          { 0.005, 0.005, 0.004, 0.025, 0.035, 0.007, 0.005, 0.009, 0.009 } },

        { "88Y   (898.0, 1836.1, 2734.1 keV)",
          { 898.045, 1836.062, 2734.087 }, { 0.012, 0.025, 0.030 } },

        { "22Na  (511.0, 1274.5 keV)",
          { 511.006, 1274.545 }, { 0.001, 0.017 } },

        { "56Co  (846.8, 1238.3, ..., 3547.9 keV)",
          { 846.771, 977.373, 1037.840, 1175.102, 1238.282, 1360.215, 1771.351, 2015.181, 2034.755, 2598.459, 3009.596, 3201.962, 3253.416, 3272.990, 3451.152, 3547.925 },
          { 0.004, 0.004, 0.006, 0.016, 0.017, 0.07, 0.026, 0.028, 0.029, 0.033, 0.046, 0.046, 0.045, 0.045, 0.047, 0.061 } },

        { "57Co  (14.4, 122.1, 136.5 keV)",
          { 14.4130, 122.0614, 136.4743 }, { 0.0003, 0.0001, 0.0003 } },

        { "134Cs (475.4, 563.3, 569.3, 604.7, 795.8, 801.9, 1038.5, 1167.9, 1365.2 keV)",
          { 475.36, 563.27, 569.30, 604.68, 795.78, 801.86, 1038.53, 1167.89, 1365.17 },
          { 0.05, 0.05, 0.03, 0.02, 0.02, 0.03, 0.05, 0.06, 0.10 } },

        { "226Ra (186.2, 242.0, 295.2, 351.9, 609.3, ..., 2447.8 keV)",
          { 186.211, 241.981, 295.213, 351.921, 609.312, 768.356, 934.061, 1120.287, 1238.110, 1377.669, 1509.228, 1729.595, 1764.494, 1847.420, 2118.551, 2204.215, 2447.810 },
          { 0.010, 0.008, 0.008, 0.008, 0.007, 0.010, 0.012, 0.010, 0.012, 0.012, 0.015, 0.015, 0.014, 0.025, 0.030, 0.040, 0.100 } },

        { "241Am (26.3, 59.5 keV)",
          { 26.345, 59.537 }, { 0.001, 0.001 } }
    };
}

void AutoCalibDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. Reference Energies Selection (INPUTRENE)
    QGroupBox *grpRef = new QGroupBox(tr("1. Reference Energies (INPUTRENE)"), this);
    QVBoxLayout *refLayout = new QVBoxLayout(grpRef);
    refLayout->setSpacing(6);

    QHBoxLayout *rowSource = new QHBoxLayout();
    m_radioSource = new QRadioButton(tr("Standard Source:"), grpRef);
    m_radioSource->setChecked(true);
    m_comboSources = new QComboBox(grpRef);
    for (const auto &iso : m_isotopes) {
        m_comboSources->addItem(iso.name);
    }
    rowSource->addWidget(m_radioSource);
    rowSource->addWidget(m_comboSources, 1);
    refLayout->addLayout(rowSource);

    QHBoxLayout *rowManual = new QHBoxLayout();
    m_radioManual = new QRadioButton(tr("Manual Energies:"), grpRef);
    m_editManualEnergies = new QLineEdit(grpRef);
    m_editManualEnergies->setPlaceholderText(tr("e.g. 1173.2, 1332.5 (comma or space separated)"));
    rowManual->addWidget(m_radioManual);
    rowManual->addWidget(m_editManualEnergies, 1);
    refLayout->addLayout(rowManual);

    QHBoxLayout *rowFile = new QHBoxLayout();
    m_radioFile = new QRadioButton(tr("From File:"), grpRef);
    m_editFilePath = new QLineEdit(grpRef);
    m_editFilePath->setPlaceholderText(tr("Select energy list file..."));
    QPushButton *btnBrowse = new QPushButton(tr("Browse..."), grpRef);
    connect(btnBrowse, &QPushButton::clicked, this, &AutoCalibDialog::onBrowseFile);
    rowFile->addWidget(m_radioFile);
    rowFile->addWidget(m_editFilePath, 1);
    rowFile->addWidget(btnBrowse);
    refLayout->addLayout(rowFile);

    QButtonGroup *sourceGrp = new QButtonGroup(this);
    sourceGrp->addButton(m_radioSource);
    sourceGrp->addButton(m_radioManual);
    sourceGrp->addButton(m_radioFile);

    connect(m_radioSource, &QRadioButton::toggled, this, &AutoCalibDialog::onSourceTypeChanged);
    connect(m_radioManual, &QRadioButton::toggled, this, &AutoCalibDialog::onSourceTypeChanged);
    connect(m_radioFile, &QRadioButton::toggled, this, &AutoCalibDialog::onSourceTypeChanged);

    mainLayout->addWidget(grpRef);

    // 2. Gain & Search Constraints
    QGroupBox *grpConstraints = new QGroupBox(tr("2. Calibration Gain Window & Search Limits"), this);
    QGridLayout *constGrid = new QGridLayout(grpConstraints);
    constGrid->setSpacing(6);

    QLabel *lblMinSlope = new QLabel(tr("Min Slope (keV/ch):"), grpConstraints);
    m_spinMinSlope = new QDoubleSpinBox(grpConstraints);
    m_spinMinSlope->setRange(0.01, 10.0);
    m_spinMinSlope->setValue(0.20);
    m_spinMinSlope->setDecimals(3);

    QLabel *lblMaxSlope = new QLabel(tr("Max Slope (keV/ch):"), grpConstraints);
    m_spinMaxSlope = new QDoubleSpinBox(grpConstraints);
    m_spinMaxSlope->setRange(0.01, 20.0);
    m_spinMaxSlope->setValue(2.00);
    m_spinMaxSlope->setDecimals(3);

    QLabel *lblMaxOffset = new QLabel(tr("Max |Offset| (keV):"), grpConstraints);
    m_spinMaxOffset = new QDoubleSpinBox(grpConstraints);
    m_spinMaxOffset->setRange(0.0, 1000.0);
    m_spinMaxOffset->setValue(150.0);
    m_spinMaxOffset->setDecimals(1);

    QLabel *lblSigma = new QLabel(tr("Peak Sigma (ch):"), grpConstraints);
    m_spinSigma = new QDoubleSpinBox(grpConstraints);
    m_spinSigma->setRange(0.5, 50.0);
    m_spinSigma->setValue(2.5);

    QLabel *lblThresh = new QLabel(tr("Peak Threshold:"), grpConstraints);
    m_spinThreshold = new QDoubleSpinBox(grpConstraints);
    m_spinThreshold->setRange(0.001, 0.9);
    m_spinThreshold->setValue(0.05);
    m_spinThreshold->setDecimals(3);

    constGrid->addWidget(lblMinSlope, 0, 0);
    constGrid->addWidget(m_spinMinSlope, 0, 1);
    constGrid->addWidget(lblMaxSlope, 0, 2);
    constGrid->addWidget(m_spinMaxSlope, 0, 3);
    constGrid->addWidget(lblMaxOffset, 0, 4);
    constGrid->addWidget(m_spinMaxOffset, 0, 5);

    constGrid->addWidget(lblSigma, 1, 0);
    constGrid->addWidget(m_spinSigma, 1, 1);
    constGrid->addWidget(lblThresh, 1, 2);
    constGrid->addWidget(m_spinThreshold, 1, 3);

    mainLayout->addWidget(grpConstraints);

    // 3. Peak Matches Preview Table
    QGroupBox *grpMatches = new QGroupBox(tr("3. Detected Peaks & Matched Reference Lines"), this);
    QVBoxLayout *matchesLayout = new QVBoxLayout(grpMatches);

    m_tableMatches = new QTableWidget(grpMatches);
    m_tableMatches->setColumnCount(5);
    m_tableMatches->setHorizontalHeaderLabels({tr("Peak #"), tr("Channel"), tr("Counts"), tr("Matched Energy (keV)"), tr("Residual (keV)")});
    m_tableMatches->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableMatches->verticalHeader()->setVisible(false);
    matchesLayout->addWidget(m_tableMatches);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *btnSearch = new QPushButton(tr("Search & Match Peaks"), grpMatches);
    btnSearch->setStyleSheet("QPushButton { background-color: #264f78; border-color: #3880c0; }");
    connect(btnSearch, &QPushButton::clicked, this, &AutoCalibDialog::runSearchAndMatch);

    m_lblResultStatus = new QLabel(tr("Ready to search peaks and match reference energies."), grpMatches);
    m_lblResultStatus->setStyleSheet("color: #4ec9b0; font-weight: bold;");

    actionLayout->addWidget(btnSearch);
    actionLayout->addSpacing(10);
    actionLayout->addWidget(m_lblResultStatus, 1);
    matchesLayout->addLayout(actionLayout);

    mainLayout->addWidget(grpMatches);

    // Dialog bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Apply Calibration"), this);
    m_btnApply->setEnabled(false);
    m_btnApply->setStyleSheet("QPushButton { background-color: #0e639c; } QPushButton:hover { background-color: #1177bb; }");
    connect(m_btnApply, &QPushButton::clicked, this, &AutoCalibDialog::applyCalibration);

    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch(1);
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    onSourceTypeChanged();
}

void AutoCalibDialog::onSourceTypeChanged()
{
    m_comboSources->setEnabled(m_radioSource->isChecked());
    m_editManualEnergies->setEnabled(m_radioManual->isChecked());
    m_editFilePath->setEnabled(m_radioFile->isChecked());
}

void AutoCalibDialog::onSourceSelected(int)
{
    // Ready
}

void AutoCalibDialog::onBrowseFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Energy Reference File"), "",
        tr("Text Files (*.txt *.dat *.ener);;All Files (*)"));
    if (!fileName.isEmpty()) {
        m_editFilePath->setText(fileName);
    }
}

std::vector<double> AutoCalibDialog::getSelectedReferenceEnergies() const
{
    std::vector<double> energies;

    if (m_radioSource->isChecked()) {
        const int idx = m_comboSources->currentIndex();
        if (idx >= 0 && idx < static_cast<int>(m_isotopes.size())) {
            energies = m_isotopes[idx].energies;
        }
    } else if (m_radioManual->isChecked()) {
        QString txt = m_editManualEnergies->text();
        QStringList parts = txt.split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            bool ok = false;
            double v = p.toDouble(&ok);
            if (ok && v > 0.0) {
                energies.push_back(v);
            }
        }
    } else if (m_radioFile->isChecked()) {
        QFile file(m_editFilePath->text());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith("#")) continue;
                bool ok = false;
                double v = line.toDouble(&ok);
                if (ok && v > 0.0) {
                    energies.push_back(v);
                }
            }
        }
    }

    std::sort(energies.begin(), energies.end());
    return energies;
}

void AutoCalibDialog::runSearchAndMatch()
{
    if (!m_mainCanvas) return;
    TracknHistogram *hist = m_mainCanvas->getActiveTracknHistogram();
    if (!hist) {
        QMessageBox::warning(this, tr("Error"), tr("No active histogram found."));
        return;
    }

    std::vector<double> refEnergies = getSelectedReferenceEnergies();
    if (refEnergies.size() < 2) {
        QMessageBox::warning(this, tr("Reference Energies"),
            tr("At least 2 reference energies are required for calibration."));
        return;
    }

    const double sigma     = m_spinSigma->value();
    const double threshold = m_spinThreshold->value();

    // 1. Peak search matching Xtrackn peaksearch(0)
    std::vector<DetectedPeak> detected = findPeaksWithTSpectrum(hist, sigma, threshold, false);
    if (detected.size() < 2) {
        m_lblResultStatus->setText(tr("Insufficient peaks detected (< 2). Try lowering threshold or adjusting sigma."));
        m_lblResultStatus->setStyleSheet("color: #f48771; font-weight: bold;");
        m_btnApply->setEnabled(false);
        return;
    }

    // Sort detected peaks by channel
    std::sort(detected.begin(), detected.end(), [](const DetectedPeak &a, const DetectedPeak &b) {
        return a.channel < b.channel;
    });

    const double k1Min   = m_spinMinSlope->value();
    const double k1Max   = m_spinMaxSlope->value();
    const double k0Max   = m_spinMaxOffset->value();

    const std::size_t nRene  = refEnergies.size();
    const std::size_t nPeaks = detected.size();

    // 2. Combinatorial Point-Matching (matching legacy autoECALIBRATION lines 7791-7826)
    int maxFound = 0;
    double bestA0 = 0.0;
    double bestA1 = 1.0;

    std::vector<std::pair<int, int>> bestMatches; // <peakIdx, reneIdx>

    for (std::size_t i1 = 0; i1 < nRene - 1; ++i1) {
        for (std::size_t i2 = nRene - 1; i2 > i1; --i2) {
            for (std::size_t j1 = 0; j1 < nPeaks - 1; ++j1) {
                for (std::size_t j2 = nPeaks - 1; j2 > j1; --j2) {
                    const double dCh = detected[j2].channel - detected[j1].channel;
                    if (dCh <= 0.0) continue;

                    const double a1 = (refEnergies[i2] - refEnergies[i1]) / dCh;
                    if (a1 < k1Min || a1 > k1Max) continue;

                    const double a0 = refEnergies[i1] - a1 * detected[j1].channel;
                    if (std::abs(a0) > k0Max) continue;

                    // Test all other reference energies under trial calibration (a0, a1)
                    int nFound = 0;
                    std::vector<std::pair<int, int>> curMatches;

                    for (std::size_t r = 0; r < nRene; ++r) {
                        const double expCh = (refEnergies[r] - a0) / a1;
                        // Find closest detected peak
                        int closestPeak = -1;
                        double minDiff = 1e9;
                        for (std::size_t p = 0; p < nPeaks; ++p) {
                            const double diff = std::abs(detected[p].channel - expCh);
                            if (diff < minDiff) {
                                minDiff = diff;
                                closestPeak = static_cast<int>(p);
                            }
                        }

                        // Tolerance: within 3 * sigma
                        if (closestPeak >= 0 && minDiff <= std::max(3.0, sigma * 2.0)) {
                            nFound++;
                            curMatches.push_back({ closestPeak, static_cast<int>(r) });
                        }
                    }

                    if (nFound > maxFound) {
                        maxFound = nFound;
                        bestA0 = a0;
                        bestA1 = a1;
                        bestMatches = curMatches;
                    }
                }
            }
        }
    }

    if (maxFound < 2) {
        m_lblResultStatus->setText(tr("No consistent calibration pattern found within gain limits."));
        m_lblResultStatus->setStyleSheet("color: #f48771; font-weight: bold;");
        m_btnApply->setEnabled(false);
        return;
    }

    // 3. Least-squares linear regression over matched pairs (matching Curfit lines 7856-7864)
    double sumX = 0, sumY = 0, sumXX = 0, sumXY = 0;
    const int N = static_cast<int>(bestMatches.size());
    for (const auto &match : bestMatches) {
        const double x = detected[match.first].channel;
        const double y = refEnergies[match.second];
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }
    const double delta = N * sumXX - sumX * sumX;
    if (std::abs(delta) > 1e-12) {
        bestA0 = (sumXX * sumY - sumX * sumXY) / delta;
        bestA1 = (N * sumXY - sumX * sumY) / delta;
    }

    m_bestA0 = bestA0;
    m_bestA1 = bestA1;
    m_bestA2 = 0.0;
    m_matchedCount = maxFound;

    // 4. Populate preview table
    m_tableMatches->setRowCount(static_cast<int>(detected.size()));
    for (int i = 0; i < static_cast<int>(detected.size()); ++i) {
        m_tableMatches->setItem(i, 0, new QTableWidgetItem(QString("#%1").arg(i + 1)));
        m_tableMatches->setItem(i, 1, new QTableWidgetItem(QString::number(detected[i].channel, 'f', 2)));
        m_tableMatches->setItem(i, 2, new QTableWidgetItem(QString::number(detected[i].height, 'f', 0)));

        auto it = std::find_if(bestMatches.begin(), bestMatches.end(), [i](const std::pair<int, int> &m) {
            return m.first == i;
        });

        if (it != bestMatches.end()) {
            const double refE = refEnergies[it->second];
            const double fitE = bestA0 + bestA1 * detected[i].channel;
            const double res  = fitE - refE;
            QTableWidgetItem *itemE = new QTableWidgetItem(QString::number(refE, 'f', 2));
            itemE->setForeground(QBrush(QColor("#4ec9b0")));
            m_tableMatches->setItem(i, 3, itemE);

            QTableWidgetItem *itemRes = new QTableWidgetItem(QString("%1%2").arg(res >= 0 ? "+" : "").arg(res, 0, 'f', 2));
            itemRes->setForeground(QBrush(QColor("#9cdcfe")));
            m_tableMatches->setItem(i, 4, itemRes);
        } else {
            m_tableMatches->setItem(i, 3, new QTableWidgetItem("-"));
            m_tableMatches->setItem(i, 4, new QTableWidgetItem("-"));
        }
    }

    m_lblResultStatus->setText(QString("Fit: E = %1 + %2 * Ch (matched %3 of %4 lines)")
        .arg(bestA0, 0, 'f', 2)
        .arg(bestA1, 0, 'f', 4)
        .arg(maxFound)
        .arg(refEnergies.size()));
    m_lblResultStatus->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    m_btnApply->setEnabled(true);
}

void AutoCalibDialog::applyCalibration()
{
    if (!m_mainCanvas) return;
    TracknHistogram *hist = m_mainCanvas->getActiveTracknHistogram();
    if (!hist) return;

    hist->SetCalibration(m_bestA0, m_bestA1, m_bestA2);

    const int sel_i = m_mainCanvas->SelectedElement_i;
    const int sel_j = m_mainCanvas->SelectedElement_j;

    m_mainCanvas->renderPeakLabels(sel_i, sel_j);
    m_mainCanvas->renderPeakSearchLabels(sel_i, sel_j);
    m_mainCanvas->updateAxisStatusLabels();

    if (m_mainCanvas->getRootCanvas() && m_mainCanvas->getRootCanvas()->getCanvas()) {
        m_mainCanvas->getRootCanvas()->getCanvas()->Modified();
        m_mainCanvas->getRootCanvas()->getCanvas()->Update();
    }

    CommandPrompt::getInstance()->appendPlainText(
        QString("Auto-Calibration applied (AK): E = %1 + %2 * Ch (%3 peaks matched)\n")
            .arg(m_bestA0, 0, 'f', 2)
            .arg(m_bestA1, 0, 'f', 4)
            .arg(m_matchedCount));

    accept();
}
