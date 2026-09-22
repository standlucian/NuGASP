#include "calib.h"
#include "Design.h"
#include "canvas.h"
#include "tracknhistogram.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QString>
#include <QScrollArea>
#include <QTabWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFile>

//==============================================================================
// Persistent energy memory retained across calibration dialog sessions
//==============================================================================
static std::vector<double> s_rememberedEnergies;
static int s_lastUsedPoints = 2; // Defaults to 2 on first use after program start
static QString s_lastCalibrationFilePath;
static std::vector<CalibDetector> s_cachedDetectors;

//==============================================================================
// ParseCalibrationFile
//==============================================================================
// Parses energy calibration files in:
//   1. GASP multi-detector .mcal format (group, detId, nSeg, [order, c0..c_ord-1, chMax]...)
//   2. Multi-detector table format (detId, a0, a1, [a2], [a3]...)
//   3. Single detector polynomial format (a0, a1, [a2], [a3]...)
//==============================================================================
bool ParseCalibrationFile(const QString &filePath,
                          std::vector<CalibDetector> &outDetectors,
                          QString &outFormatInfo)
{
    outDetectors.clear();
    std::ifstream file(filePath.toStdString());
    if (!file.is_open()) {
        outFormatInfo = "Cannot open file.";
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == '!' ||
            (trimmed.size() >= 2 && trimmed.substr(0, 2) == "//")) {
            continue;
        }
        lines.push_back(trimmed);
    }

    if (lines.empty()) {
        outFormatInfo = "File contains no valid calibration data.";
        return false;
    }

    // Format 1: GASP multi-detector piecewise polynomial format
    bool isGaspMcal = true;
    std::vector<CalibDetector> mcalDets;
    for (const auto &ln : lines) {
        std::istringstream iss(ln);
        CalibDetector det;
        if (!(iss >> det.group >> det.detectorId >> det.numSegments)) {
            isGaspMcal = false;
            break;
        }
        if (det.numSegments <= 0 || det.numSegments > 20) {
            isGaspMcal = false;
            break;
        }
        for (int s = 0; s < det.numSegments; ++s) {
            int order = 0;
            if (!(iss >> order) || order <= 0 || order > 10) {
                isGaspMcal = false;
                break;
            }
            CalibSegment seg;
            for (int k = 0; k < order; ++k) {
                double c = 0.0;
                if (!(iss >> c)) {
                    isGaspMcal = false;
                    break;
                }
                seg.coeffs.push_back(c);
            }
            if (!isGaspMcal) break;
            if (!(iss >> seg.maxChannel)) {
                isGaspMcal = false;
                break;
            }
            det.segments.push_back(seg);
        }
        if (!isGaspMcal || det.segments.size() != static_cast<size_t>(det.numSegments)) {
            isGaspMcal = false;
            break;
        }
        mcalDets.push_back(det);
    }

    if (isGaspMcal && !mcalDets.empty()) {
        outDetectors = mcalDets;
        outFormatInfo = QString("GASP Multi-detector format (%1 detectors, piecewise)").arg(mcalDets.size());
        return true;
    }

    // Format 2: Simple multi-detector table: "detId a0 a1 [a2] [a3]..."
    bool isTable = true;
    std::vector<CalibDetector> tableDets;
    for (const auto &ln : lines) {
        std::istringstream iss(ln);
        std::vector<double> tokens;
        double val;
        while (iss >> val) tokens.push_back(val);
        if (tokens.size() >= 3) {
            CalibDetector det;
            det.group = 1;
            det.detectorId = static_cast<int>(tokens[0]);
            det.numSegments = 1;
            CalibSegment seg;
            seg.maxChannel = 1e9;
            for (size_t i = 1; i < tokens.size(); ++i) {
                seg.coeffs.push_back(tokens[i]);
            }
            det.segments.push_back(seg);
            tableDets.push_back(det);
        } else {
            isTable = false;
            break;
        }
    }

    if (isTable && !tableDets.empty()) {
        outDetectors = tableDets;
        outFormatInfo = QString("Multi-detector table (%1 detectors)").arg(tableDets.size());
        return true;
    }

    // Format 3: Single detector polynomial: "a0 a1 [a2] [a3]..."
    std::istringstream iss(lines[0]);
    std::vector<double> tokens;
    double val;
    while (iss >> val) tokens.push_back(val);
    if (tokens.size() >= 2) {
        CalibDetector det;
        det.group = 1;
        det.detectorId = 0;
        det.numSegments = 1;
        CalibSegment seg;
        seg.maxChannel = 1e9;
        for (double c : tokens) seg.coeffs.push_back(c);
        det.segments.push_back(seg);
        outDetectors.push_back(det);
        outFormatInfo = QString("Single detector polynomial (%1 coeffs)").arg(tokens.size());
        return true;
    }

    outFormatInfo = "Unrecognized calibration file format.";
    return false;
}

//==============================================================================
// SaveCalibrationFile
//==============================================================================
bool SaveCalibrationFile(const QString &filePath,
                         const std::vector<CalibDetector> &detectors,
                         bool isMcalFormat)
{
    std::ofstream out(filePath.toStdString());
    if (!out.is_open()) return false;

    if (isMcalFormat) {
        out << "! GASPware Multi-detector Calibration File\n";
        out << "! Format: group detId numSegments [order a0 a1 a2 a3 chMax]...\n";
        for (const auto &det : detectors) {
            out << det.group << " " << det.detectorId << " " << det.segments.size();
            for (const auto &seg : det.segments) {
                out << " " << seg.coeffs.size();
                for (double c : seg.coeffs) {
                    out << " " << std::scientific << std::setprecision(6) << c;
                }
                out << " " << std::fixed << std::setprecision(0) << seg.maxChannel;
            }
            out << "\n";
        }
    } else {
        out << "# Energy Calibration Coefficients\n";
        for (const auto &det : detectors) {
            if (detectors.size() > 1) {
                out << det.detectorId << " ";
            }
            for (const auto &seg : det.segments) {
                for (double c : seg.coeffs) {
                    out << std::scientific << std::setprecision(6) << c << " ";
                }
            }
            out << "\n";
        }
    }
    return true;
}

//==============================================================================
// LinearCalibration
//==============================================================================
// Computes first-order linear energy calibration coefficients A0 (intercept)
// and A1 (slope) using linear least-squares regression over N data points:
//
//   E(ch) = A(0) + A(1) * ch
//
// Formulas:
//   meanX = sum(x_i) / N,   meanY = sum(y_i) / N
//   Sxx = sum( (x_i - meanX)^2 ),   Sxy = sum( (x_i - meanX)*(y_i - meanY) )
//   A(1) = Sxy / Sxx
//   A(0) = meanY - A(1) * meanX
//==============================================================================
void LinearCalibration(const std::vector<double>& channels, const std::vector<double>& energies)
{
    const std::size_t N = channels.size();
    if (N < 2 || energies.size() != N) {
        CommandPrompt::getInstance()->appendPlainText(
            "Calibration requires at least two points.");
        return;
    }

    double sumX = 0.0, sumY = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        sumX += channels[i];
        sumY += energies[i];
    }
    const double meanX = sumX / N;
    const double meanY = sumY / N;

    double Sxx = 0.0, Sxy = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        const double dx = channels[i] - meanX;
        const double dy = energies[i] - meanY;
        Sxx += dx * dx;
        Sxy += dx * dy;
    }

    if (std::abs(Sxx) < 1e-9) {
        CommandPrompt::getInstance()->appendPlainText(
            "Error: Calibration channel points cannot be identical.");
        return;
    }

    const double slope = Sxy / Sxx;
    const double intercept = meanY - slope * meanX;

    const QString report = QString("Calibration coef :   A(0)= %1   A(1)= %2  (from %3 points, 1st order)")
                               .arg(intercept, 0, 'f', 3)
                               .arg(slope, 0, 'f', 3)
                               .arg(N);

    CommandPrompt::getInstance()->appendPlainText(report);
    std::cout << report.toStdString() << std::endl;
}

//==============================================================================
// TwoPointCalibration (legacy wrapper)
//==============================================================================
void TwoPointCalibration(const std::vector<Float_t>& puncte_calib2p,
                         double energie1,
                         double energie2)
{
    const std::size_t n = puncte_calib2p.size();
    if (n < 2) {
        CommandPrompt::getInstance()->appendPlainText(
            "Two-point calibration requires at least two points.");
        return;
    }

    const std::vector<double> chs = {
        static_cast<double>(puncte_calib2p[n - 2]),
        static_cast<double>(puncte_calib2p[n - 1])
    };
    const std::vector<double> ens = { energie1, energie2 };
    LinearCalibration(chs, ens);
}

//==============================================================================
// runTwoPointCalibrationDialog
//==============================================================================
// Launches an interactive modal dialog allowing the user to select how many
// points to calibrate on (N >= 2). As the point count changes, the dialog
// automatically resizes and dynamically populates input rows for each point.
//
// Defaults to 2 points on the first use after program launch, and subsequently
// defaults to the previously used point count.
//
// Channels are pre-populated from the user's marked peaks, while energies are
// pre-filled from memory when available. Computes a first-order (linear)
// calibration fit over all N points.
//==============================================================================
void runTwoPointCalibrationDialog(QWidget *parent,
                                  const std::vector<Float_t> &puncte_calib2p,
                                  TracknHistogram *activeHistogram)
{
    if (puncte_calib2p.size() < 2) {
        const QString msg = "At least two markers are needed to perform energy calibration.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        return;
    }

    const int availableMarkers = static_cast<int>(puncte_calib2p.size());
    // Default to 2 on first use after launch, then default to the previously used value (capped at available markers)
    const int initialPoints = std::min(availableMarkers, std::max(2, s_lastUsedPoints));

    // Construct the Qt modal dialog
    QDialog dialog(parent);
    dialog.setWindowTitle("Energy Calibration (1st Order)");
    dialog.setStyleSheet(
        "QDialog { background-color: #2b2b2b; color: #ffffff; }"
        "QLabel { color: #ffffff; font-size: 19px; }"
        "QSpinBox { background-color: #ffffff; color: #000000; font-size: 19px; font-weight: bold; border-radius: 3px; padding: 4px 8px; }"
        "QLineEdit { background-color: #ffffff; color: #000000; font-size: 19px; border: 1px solid #707070; border-radius: 3px; padding: 4px 8px; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Top control: How many points should be used? (limited to available markers)
    QHBoxLayout *topControlLayout = new QHBoxLayout();
    QLabel *lblHowMany = new QLabel("How many points should be used?", &dialog);
    lblHowMany->setStyleSheet("font-weight: bold; font-size: 19px;");

    QSpinBox *pointsSpinBox = new QSpinBox(&dialog);
    pointsSpinBox->setRange(2, availableMarkers);
    pointsSpinBox->setValue(initialPoints);
    pointsSpinBox->setFixedWidth(90);
    pointsSpinBox->setToolTip(QString("Max points: %1 (from currently marked/fitted peaks)").arg(availableMarkers));

    topControlLayout->addWidget(lblHowMany);
    topControlLayout->addWidget(pointsSpinBox);
    topControlLayout->addStretch(1);
    mainLayout->addLayout(topControlLayout);

    // Container widget holding the point rows
    QWidget *rowsContainer = new QWidget(&dialog);
    QVBoxLayout *rowsLayout = new QVBoxLayout(rowsContainer);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(8);
    mainLayout->addWidget(rowsContainer);

    // Row representation
    struct PointRowWidgets {
        QWidget   *widget = nullptr;
        QLabel    *idxLabel = nullptr;
        QLineEdit *chEdit = nullptr;
        QLineEdit *enEdit = nullptr;
    };
    std::vector<PointRowWidgets> rows;

    // Lambda to update and resize row fields dynamically
    auto updateRows = [&](int nPoints) {
        // Collect the last nPoints from puncte_calib2p and sort ascending by channel
        std::vector<Float_t> sortedMarkers;
        if (availableMarkers >= nPoints) {
            sortedMarkers.assign(puncte_calib2p.end() - nPoints, puncte_calib2p.end());
        } else {
            sortedMarkers = puncte_calib2p;
        }
        std::sort(sortedMarkers.begin(), sortedMarkers.end());

        // Allocate new row widgets if necessary
        while (static_cast<int>(rows.size()) < nPoints) {
            const int idx = static_cast<int>(rows.size());
            QWidget *rowWidget = new QWidget(rowsContainer);
            QHBoxLayout *rowHBox = new QHBoxLayout(rowWidget);
            rowHBox->setContentsMargins(0, 3, 0, 3);
            rowHBox->setSpacing(10);

            QLabel *idxLabel = new QLabel(QString("Point %1:").arg(idx + 1), rowWidget);
            idxLabel->setFixedWidth(85);
            idxLabel->setStyleSheet("color: #00ffff; font-weight: bold;");

            QLabel *chLabel = new QLabel("Channel:", rowWidget);
            chLabel->setStyleSheet("color: #dddddd;");
            QLineEdit *chEdit = new QLineEdit(rowWidget);
            chEdit->setFixedWidth(140);
            chEdit->setReadOnly(true);
            chEdit->setFocusPolicy(Qt::NoFocus);
            chEdit->setStyleSheet("background-color: #383838; color: #9cdcfe; font-weight: bold; border: 1px solid #555555; border-radius: 3px; padding: 4px 8px;");

            QLabel *enLabel = new QLabel("Energy (keV):", rowWidget);
            enLabel->setStyleSheet("color: #dddddd;");
            QLineEdit *enEdit = new QLineEdit(rowWidget);
            enEdit->setFixedWidth(170);

            rowHBox->addWidget(idxLabel);
            rowHBox->addWidget(chLabel);
            rowHBox->addWidget(chEdit);
            rowHBox->addWidget(enLabel);
            rowHBox->addWidget(enEdit);
            rowHBox->addStretch(1);

            rowsLayout->addWidget(rowWidget);
            rows.push_back({rowWidget, idxLabel, chEdit, enEdit});
        }

        // Update values and visibility
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            if (i < nPoints) {
                rows[i].widget->show();
                rows[i].idxLabel->setText(QString("Point %1:").arg(i + 1));

                // Pre-fill channel from sorted markers if available
                if (i < static_cast<int>(sortedMarkers.size())) {
                    rows[i].chEdit->setText(QString::number(sortedMarkers[i], 'f', 2));
                }

                // Pre-fill energy from persistent memory if available
                if (i < static_cast<int>(s_rememberedEnergies.size()) && s_rememberedEnergies[i] > 0.0) {
                    rows[i].enEdit->setText(QString::number(s_rememberedEnergies[i], 'f', 2));
                }
            } else {
                rows[i].widget->hide();
            }
        }

        // Automatically resize the dialog to match the updated row set
        dialog.adjustSize();
    };

    // Connect spinbox value change to auto-update and resize
    QObject::connect(pointsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [&](int val) {
        s_lastUsedPoints = val;
        updateRows(val);
    });

    // Initial build of rows
    updateRows(initialPoints);

    // Standard OK / Cancel buttons
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    buttonBox.setStyleSheet(
        "QPushButton { background-color: #4a4a4a; color: #ffffff; border: 1px solid #707070; "
        "border-radius: 4px; padding: 6px 24px; font-weight: bold; font-size: 19px; min-height: 28px; } "
        "QPushButton:hover { background-color: #5a5a5a; }"
    );
    mainLayout->addWidget(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Focus first energy field
    if (!rows.empty() && rows[0].enEdit) {
        rows[0].enEdit->setFocus();
        rows[0].enEdit->selectAll();
    }

    // Execute dialog and process calibration
    if (dialog.exec() == QDialog::Accepted) {
        const int nPoints = pointsSpinBox->value();
        std::vector<double> channels;
        std::vector<double> energies;
        bool allValid = true;

        for (int i = 0; i < nPoints; ++i) {
            bool okCh = false, okEn = false;
            const double ch = rows[i].chEdit->text().toDouble(&okCh);
            const double en = rows[i].enEdit->text().toDouble(&okEn);

            if (!okCh || !okEn || en <= 0.0) {
                allValid = false;
                break;
            }
            channels.push_back(ch);
            energies.push_back(en);
        }

        if (!allValid) {
            const QString msg = "Error: All channel and energy fields must be filled with valid numbers (energies must be > 0).\n";
            std::cout << msg.toStdString();
            CommandPrompt::getInstance()->appendPlainText(msg);
            return;
        }

        // Perform linear regression across all N points
        LinearCalibration(channels, energies);

        // Store entered energies in memory for subsequent calibrations
        s_rememberedEnergies = energies;
        s_lastUsedPoints = nPoints;

        // Compute slope and intercept and apply to active histogram
        if (activeHistogram) {
            double sumX = 0.0, sumY = 0.0;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                sumX += channels[i];
                sumY += energies[i];
            }
            const double meanX = sumX / channels.size();
            const double meanY = sumY / channels.size();

            double Sxx = 0.0, Sxy = 0.0;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                const double dx = channels[i] - meanX;
                const double dy = energies[i] - meanY;
                Sxx += dx * dx;
                Sxy += dx * dy;
            }

            if (std::abs(Sxx) > 1e-9) {
                const double slope = Sxy / Sxx;
                const double intercept = meanY - slope * meanX;
                activeHistogram->SetCalibration(intercept, slope, 0.0);
            }
        }
    }
}

//==============================================================================
// runEnergyCalibrationDialog
//==============================================================================
// Full Energy Calibration Manager (EnCal):
// Supports GASP multi-detector .mcal, simple .cal tables, manual polynomial entry,
// and reference marker regression.
//==============================================================================
void runEnergyCalibrationDialog(QMainCanvas *mainCanvas, int currentDetId)
{
    if (!mainCanvas) return;
    const int sel_i = mainCanvas->SelectedElement_i;
    const int sel_j = mainCanvas->SelectedElement_j;
    TracknHistogram *activeHist = mainCanvas->getActiveTracknHistogram();
    if (!activeHist) {
        QMessageBox::warning(mainCanvas, "Energy Calibration", "No active spectrum loaded in the selected pad.");
        return;
    }

    QDialog dialog(mainCanvas);
    dialog.setWindowTitle("Energy Calibration Manager (EnCal)");
    dialog.resize(820, 600);
    dialog.setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #ffffff; }"
        "QTabWidget::pane { border: 1px solid #3e3e42; background: #252526; border-radius: 4px; }"
        "QTabBar::tab { background: #2d2d30; color: #cccccc; padding: 8px 18px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; font-size: 13px; }"
        "QTabBar::tab:selected { background: #007acc; color: #ffffff; }"
        "QLabel { color: #e0e0e0; font-size: 13px; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 5px 8px; font-size: 13px; }"
        "QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #007acc; }"
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
        "QPushButton:pressed { background-color: #007acc; }"
        "QTableWidget { background-color: #252526; color: #ffffff; gridline-color: #3e3e42; selection-background-color: #094771; font-size: 13px; }"
        "QHeaderView::section { background-color: #2d2d30; color: #cccccc; padding: 6px; border: 1px solid #3e3e42; font-weight: bold; font-size: 12px; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 12px; font-weight: bold; color: #00ffff; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // --- Header Section: Active Pad Info & Current Calibration Status ---
    QGroupBox *headerBox = new QGroupBox("Target Spectrum & Calibration Status", &dialog);
    QVBoxLayout *headerLayout = new QVBoxLayout(headerBox);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(8);

    QHBoxLayout *headerRow1 = new QHBoxLayout();
    QLabel *lblTarget = new QLabel(
        QString("<b>Pad Location:</b> Row %1, Col %2 &nbsp;|&nbsp; <b>Current Spectrum Index:</b> #%3")
            .arg(sel_i).arg(sel_j).arg(mainCanvas->getCurrentSpectrumIndex()),
        headerBox);
    lblTarget->setStyleSheet("color: #9cdcfe; font-size: 13px;");
    headerRow1->addWidget(lblTarget);
    headerRow1->addStretch(1);

    QPushButton *btnDisableCalib = new QPushButton("Disable Calibration", headerBox);
    btnDisableCalib->setStyleSheet(
        "QPushButton { background-color: #3a2020; color: #f48771; border: 1px solid #803030; border-radius: 4px; padding: 5px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #502525; border-color: #a04040; }"
    );
    btnDisableCalib->setToolTip("Removes all calibration parameters and reverts readouts to raw channel numbers.");
    headerRow1->addWidget(btnDisableCalib);
    headerLayout->addLayout(headerRow1);

    QLabel *lblStatus = new QLabel(headerBox);
    headerLayout->addWidget(lblStatus);
    mainLayout->addWidget(headerBox);

    auto updateStatusLabel = [&]() {
        if (activeHist->IsCalibrated()) {
            const auto &segs = activeHist->GetCalibrationSegments();
            if (segs.size() > 1) {
                lblStatus->setText(QString("<b>Status:</b> <span style='color:#4ec9b0;'>CALIBRATED</span> &mdash; Piecewise polynomial (%1 segments loaded).")
                    .arg(segs.size()));
            } else {
                lblStatus->setText("<b>Status:</b> <span style='color:#4ec9b0;'>CALIBRATED</span> &mdash; Single segment: Polynomial");
            }
        } else {
            lblStatus->setText("<b>Status:</b> <span style='color:#f48771;'>UNCALIBRATED</span> &mdash; Displaying raw spectrum channels.");
        }
    };
    updateStatusLabel();

    QObject::connect(btnDisableCalib, &QPushButton::clicked, [&]() {
        activeHist->ClearCalibration();
        mainCanvas->renderPeakLabels(sel_i, sel_j);
        mainCanvas->renderPeakSearchLabels(sel_i, sel_j);
        mainCanvas->updateAxisStatusLabels();
        mainCanvas->getRootCanvas()->getCanvas()->Update();
        updateStatusLabel();
        CommandPrompt::getInstance()->appendPlainText("Calibration disabled for active spectrum.\n");
    });

    // --- Tab Widget ---
    QTabWidget *tabWidget = new QTabWidget(&dialog);
    mainLayout->addWidget(tabWidget, 1);

    // ==========================================
    // TAB 1: Load from File (.mcal / .cal)
    // ==========================================
    QWidget *tabFile = new QWidget();
    QVBoxLayout *tabFileLayout = new QVBoxLayout(tabFile);
    tabFileLayout->setSpacing(12);
    tabFileLayout->setContentsMargins(12, 12, 12, 12);

    QHBoxLayout *filePickerRow = new QHBoxLayout();
    QLabel *lblFilePrompt = new QLabel("Calibration File:", tabFile);
    QLineEdit *editFilePath = new QLineEdit(tabFile);
    editFilePath->setReadOnly(true);
    editFilePath->setPlaceholderText("Select a .mcal or .cal energy calibration file...");
    QPushButton *btnBrowse = new QPushButton("Browse...", tabFile);
    filePickerRow->addWidget(lblFilePrompt);
    filePickerRow->addWidget(editFilePath, 1);
    filePickerRow->addWidget(btnBrowse);
    tabFileLayout->addLayout(filePickerRow);

    QLabel *lblFormatInfo = new QLabel(tabFile);
    lblFormatInfo->setStyleSheet("color: #4ec9b0; font-style: italic;");
    tabFileLayout->addWidget(lblFormatInfo);

    QHBoxLayout *detPickerRow = new QHBoxLayout();
    QLabel *lblDetSelect = new QLabel("Select Detector Calibration:", tabFile);
    QComboBox *comboDetectors = new QComboBox(tabFile);
    comboDetectors->setMinimumWidth(320);
    detPickerRow->addWidget(lblDetSelect);
    detPickerRow->addWidget(comboDetectors, 1);
    tabFileLayout->addLayout(detPickerRow);

    QLabel *lblSegmentsTitle = new QLabel("Calibration Segments for Selected Detector:", tabFile);
    lblSegmentsTitle->setStyleSheet("font-weight: bold; color: #00ffff;");
    tabFileLayout->addWidget(lblSegmentsTitle);

    QTableWidget *tableSegments = new QTableWidget(tabFile);
    tableSegments->setColumnCount(7);
    tableSegments->setHorizontalHeaderLabels({"Segment", "Channel Limit", "Order", "A0 (Offset)", "A1 (Slope)", "A2 (Quad)", "A3 (Cubic)"});
    tableSegments->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableSegments->verticalHeader()->setVisible(false);
    tableSegments->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableSegments->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabFileLayout->addWidget(tableSegments, 1);

    std::vector<CalibDetector> loadedDetectors = s_cachedDetectors;

    auto updateSegmentsTable = [&](const CalibDetector &det) {
        tableSegments->setRowCount(static_cast<int>(det.segments.size()));
        for (int s = 0; s < static_cast<int>(det.segments.size()); ++s) {
            const auto &seg = det.segments[s];
            QTableWidgetItem *itemSeg = new QTableWidgetItem(QString("#%1").arg(s + 1));
            itemSeg->setTextAlignment(Qt::AlignCenter);
            tableSegments->setItem(s, 0, itemSeg);

            QString chLim = (seg.maxChannel >= 1e8) ? "Full Range" : QString::number(seg.maxChannel, 'f', 0);
            QTableWidgetItem *itemCh = new QTableWidgetItem(chLim);
            itemCh->setTextAlignment(Qt::AlignCenter);
            tableSegments->setItem(s, 1, itemCh);

            QTableWidgetItem *itemOrd = new QTableWidgetItem(QString::number(seg.coeffs.size()));
            itemOrd->setTextAlignment(Qt::AlignCenter);
            tableSegments->setItem(s, 2, itemOrd);

            for (int k = 0; k < 4; ++k) {
                QString val = (k < static_cast<int>(seg.coeffs.size())) ? QString::number(seg.coeffs[k], 'g', 6) : "-";
                QTableWidgetItem *itemCoeff = new QTableWidgetItem(val);
                itemCoeff->setTextAlignment(Qt::AlignCenter);
                tableSegments->setItem(s, 3 + k, itemCoeff);
            }
        }
    };

    auto loadFile = [&](const QString &path) {
        QString info;
        std::vector<CalibDetector> dets;
        if (ParseCalibrationFile(path, dets, info)) {
            loadedDetectors = dets;
            s_cachedDetectors = dets;
            s_lastCalibrationFilePath = path;
            editFilePath->setText(path);
            lblFormatInfo->setText("✓ " + info);

            comboDetectors->clear();
            int selectIdx = 0;
            for (int i = 0; i < static_cast<int>(dets.size()); ++i) {
                comboDetectors->addItem(
                    QString("Detector %1 (Group %2, %3 segs)").arg(dets[i].detectorId).arg(dets[i].group).arg(dets[i].segments.size()),
                    dets[i].detectorId);
                if (dets[i].detectorId == currentDetId) {
                    selectIdx = i;
                }
            }
            if (comboDetectors->count() > 0) {
                comboDetectors->setCurrentIndex(selectIdx);
                updateSegmentsTable(dets[selectIdx]);
            }
            return true;
        } else {
            lblFormatInfo->setText("✗ " + info);
            return false;
        }
    };

    QObject::connect(btnBrowse, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(
            &dialog, "Select Energy Calibration File", "",
            "Calibration Files (*.mcal *.cal *.dat *.txt);;All Files (*)");
        if (!path.isEmpty()) {
            loadFile(path);
        }
    });

    QObject::connect(comboDetectors, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(loadedDetectors.size())) {
            updateSegmentsTable(loadedDetectors[idx]);
        }
    });

    // Restore previously opened file if user explicitly selected one earlier in the session
    if (!s_lastCalibrationFilePath.isEmpty() && QFile::exists(s_lastCalibrationFilePath)) {
        loadFile(s_lastCalibrationFilePath);
    } else {
        lblFormatInfo->setText("No calibration file loaded. Click 'Browse...' to select a .mcal or .cal file.");
    }

    tabWidget->addTab(tabFile, "File Calibration (.mcal / .cal)");

    // ==========================================
    // TAB 2: Manual Coefficients
    // ==========================================
    QWidget *tabManual = new QWidget();
    QVBoxLayout *tabManualLayout = new QVBoxLayout(tabManual);
    tabManualLayout->setSpacing(14);
    tabManualLayout->setContentsMargins(16, 16, 16, 16);

    QLabel *lblManualDesc = new QLabel(
        "Direct polynomial calibration equation:<br>"
        "<b>E(ch) = A(0) + A(1)&middot;ch + A(2)&middot;ch&sup2; + A(3)&middot;ch&sup3;</b>", tabManual);
    lblManualDesc->setStyleSheet("color: #9cdcfe; font-size: 13px;");
    tabManualLayout->addWidget(lblManualDesc);

    QFormLayout *formManual = new QFormLayout();
    formManual->setSpacing(10);
    formManual->setLabelAlignment(Qt::AlignRight);

    double initA0 = activeHist->IsCalibrated() ? activeHist->GetCalibA0() : 0.0;
    double initA1 = activeHist->IsCalibrated() ? activeHist->GetCalibA1() : 1.0;
    double initA2 = activeHist->IsCalibrated() ? activeHist->GetCalibA2() : 0.0;
    double initA3 = 0.0;
    if (activeHist->IsCalibrated() && !activeHist->GetCalibrationSegments().empty()) {
        const auto &c = activeHist->GetCalibrationSegments()[0].coeffs;
        if (c.size() > 0) initA0 = c[0];
        if (c.size() > 1) initA1 = c[1];
        if (c.size() > 2) initA2 = c[2];
        if (c.size() > 3) initA3 = c[3];
    }

    QDoubleSpinBox *spinA0 = new QDoubleSpinBox(tabManual);
    spinA0->setRange(-1e7, 1e7);
    spinA0->setDecimals(6);
    spinA0->setValue(initA0);

    QDoubleSpinBox *spinA1 = new QDoubleSpinBox(tabManual);
    spinA1->setRange(-1e7, 1e7);
    spinA1->setDecimals(6);
    spinA1->setValue(initA1);

    QDoubleSpinBox *spinA2 = new QDoubleSpinBox(tabManual);
    spinA2->setRange(-1e7, 1e7);
    spinA2->setDecimals(9);
    spinA2->setValue(initA2);

    QDoubleSpinBox *spinA3 = new QDoubleSpinBox(tabManual);
    spinA3->setRange(-1e7, 1e7);
    spinA3->setDecimals(12);
    spinA3->setValue(initA3);

    formManual->addRow("A(0) Offset (keV):", spinA0);
    formManual->addRow("A(1) Slope (keV/ch):", spinA1);
    formManual->addRow("A(2) Quadratic (keV/ch²):", spinA2);
    formManual->addRow("A(3) Cubic (keV/ch³):", spinA3);
    tabManualLayout->addLayout(formManual);

    // Interactive channel-to-energy calculator
    QGroupBox *boxCalc = new QGroupBox("Interactive Calibration Tester", tabManual);
    QHBoxLayout *calcLayout = new QHBoxLayout(boxCalc);
    calcLayout->setContentsMargins(12, 12, 12, 12);
    QLabel *lblTestCh = new QLabel("Test Channel:", boxCalc);
    QDoubleSpinBox *spinTestCh = new QDoubleSpinBox(boxCalc);
    spinTestCh->setRange(0, 1e7);
    spinTestCh->setValue(1000.0);
    spinTestCh->setDecimals(1);

    QLabel *lblTestResult = new QLabel(boxCalc);
    lblTestResult->setStyleSheet("color: #4ec9b0; font-weight: bold; font-size: 14px;");

    auto updateTestCalculation = [&]() {
        const double ch = spinTestCh->value();
        const double a0 = spinA0->value();
        const double a1 = spinA1->value();
        const double a2 = spinA2->value();
        const double a3 = spinA3->value();
        const double energy = a0 + ch * (a1 + ch * (a2 + ch * a3));
        lblTestResult->setText(QString("Calculated Energy: %1 keV").arg(energy, 0, 'f', 3));
    };

    QObject::connect(spinTestCh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateTestCalculation);
    QObject::connect(spinA0, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateTestCalculation);
    QObject::connect(spinA1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateTestCalculation);
    QObject::connect(spinA2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateTestCalculation);
    QObject::connect(spinA3, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateTestCalculation);
    updateTestCalculation();

    calcLayout->addWidget(lblTestCh);
    calcLayout->addWidget(spinTestCh);
    calcLayout->addSpacing(16);
    calcLayout->addWidget(lblTestResult);
    calcLayout->addStretch(1);
    tabManualLayout->addWidget(boxCalc);
    tabManualLayout->addStretch(1);

    tabWidget->addTab(tabManual, "Manual Polynomial");

    // ==========================================
    // TAB 3: Reference Points Regression (2P / N-Points)
    // ==========================================
    QWidget *tabPoints = new QWidget();
    QVBoxLayout *tabPointsLayout = new QVBoxLayout(tabPoints);
    tabPointsLayout->setSpacing(12);
    tabPointsLayout->setContentsMargins(14, 14, 14, 14);

    std::vector<Float_t> refMarkers = mainCanvas->getPuncteCalib2p();
    if (refMarkers.size() < 2 && mainCanvas->getSpacebarMarkers().size() >= 2) {
        refMarkers.clear();
        for (Double_t sm : mainCanvas->getSpacebarMarkers()) {
            refMarkers.push_back(static_cast<Float_t>(sm));
        }
    }
    if (refMarkers.size() < 2 && mainCanvas->getGaussCenters(sel_i, sel_j).size() >= 2) {
        refMarkers.clear();
        for (Double_t gc : mainCanvas->getGaussCenters(sel_i, sel_j)) {
            refMarkers.push_back(static_cast<Float_t>(gc));
        }
    }

    const int availablePoints = static_cast<int>(refMarkers.size());
    const int initialN = std::min(std::max(2, availablePoints), std::max(2, s_lastUsedPoints));

    QHBoxLayout *pointsTopRow = new QHBoxLayout();
    QLabel *lblNumPoints = new QLabel("Number of Reference Points:", tabPoints);
    QSpinBox *spinNumPoints = new QSpinBox(tabPoints);
    spinNumPoints->setRange(2, std::max(2, availablePoints));
    spinNumPoints->setValue(initialN);
    spinNumPoints->setFixedWidth(80);
    pointsTopRow->addWidget(lblNumPoints);
    pointsTopRow->addWidget(spinNumPoints);
    pointsTopRow->addStretch(1);
    tabPointsLayout->addLayout(pointsTopRow);

    QWidget *pointsRowsWidget = new QWidget(tabPoints);
    QVBoxLayout *pointsRowsLayout = new QVBoxLayout(pointsRowsWidget);
    pointsRowsLayout->setContentsMargins(0, 0, 0, 0);
    pointsRowsLayout->setSpacing(6);
    tabPointsLayout->addWidget(pointsRowsWidget);

    struct PointRow {
        QWidget *container;
        QLineEdit *chEdit;
        QLineEdit *enEdit;
    };
    std::vector<PointRow> pointRows;

    auto updatePointRows = [&](int n) {
        std::vector<Float_t> sorted = refMarkers;
        if (static_cast<int>(sorted.size()) > n) {
            sorted.assign(sorted.end() - n, sorted.end());
        }
        std::sort(sorted.begin(), sorted.end());

        while (static_cast<int>(pointRows.size()) < n) {
            int idx = static_cast<int>(pointRows.size());
            QWidget *w = new QWidget(pointsRowsWidget);
            QHBoxLayout *h = new QHBoxLayout(w);
            h->setContentsMargins(0, 2, 0, 2);
            h->setSpacing(10);

            QLabel *lbl = new QLabel(QString("Point %1:").arg(idx + 1), w);
            lbl->setFixedWidth(70);
            lbl->setStyleSheet("color: #00ffff; font-weight: bold;");

            QLabel *lCh = new QLabel("Channel:", w);
            QLineEdit *eCh = new QLineEdit(w);
            eCh->setFixedWidth(130);

            QLabel *lEn = new QLabel("Energy (keV):", w);
            QLineEdit *eEn = new QLineEdit(w);
            eEn->setFixedWidth(150);

            h->addWidget(lbl);
            h->addWidget(lCh);
            h->addWidget(eCh);
            h->addWidget(lEn);
            h->addWidget(eEn);
            h->addStretch(1);

            pointsRowsLayout->addWidget(w);
            pointRows.push_back({w, eCh, eEn});
        }

        for (int i = 0; i < static_cast<int>(pointRows.size()); ++i) {
            if (i < n) {
                pointRows[i].container->show();
                if (i < static_cast<int>(sorted.size())) {
                    pointRows[i].chEdit->setText(QString::number(sorted[i], 'f', 2));
                }
                if (i < static_cast<int>(s_rememberedEnergies.size()) && s_rememberedEnergies[i] > 0.0) {
                    pointRows[i].enEdit->setText(QString::number(s_rememberedEnergies[i], 'f', 2));
                }
            } else {
                pointRows[i].container->hide();
            }
        }
    };

    updatePointRows(initialN);
    QObject::connect(spinNumPoints, QOverload<int>::of(&QSpinBox::valueChanged), [&](int v) {
        updatePointRows(v);
    });

    QLabel *lblRegResult = new QLabel(tabPoints);
    lblRegResult->setStyleSheet("color: #4ec9b0; font-weight: bold; font-size: 13px;");
    tabPointsLayout->addWidget(lblRegResult);

    QPushButton *btnCalcReg = new QPushButton("Calculate Linear Fit", tabPoints);
    tabPointsLayout->addWidget(btnCalcReg);
    tabPointsLayout->addStretch(1);

    double regSlope = 1.0, regIntercept = 0.0;
    bool regValid = false;

    auto computeRegression = [&]() -> bool {
        const int n = spinNumPoints->value();
        std::vector<double> chs, ens;
        for (int i = 0; i < n; ++i) {
            bool okC = false, okE = false;
            double c = pointRows[i].chEdit->text().toDouble(&okC);
            double e = pointRows[i].enEdit->text().toDouble(&okE);
            if (!okC || !okE || e <= 0.0) {
                lblRegResult->setText("<span style='color:#f48771;'>Error: Invalid channel or energy in row " + QString::number(i + 1) + "</span>");
                return false;
            }
            chs.push_back(c);
            ens.push_back(e);
        }

        double sumX = 0.0, sumY = 0.0;
        for (int i = 0; i < n; ++i) {
            sumX += chs[i];
            sumY += ens[i];
        }
        double meanX = sumX / n;
        double meanY = sumY / n;
        double Sxx = 0.0, Sxy = 0.0;
        for (int i = 0; i < n; ++i) {
            double dx = chs[i] - meanX;
            double dy = ens[i] - meanY;
            Sxx += dx * dx;
            Sxy += dx * dy;
        }
        if (std::abs(Sxx) < 1e-9) {
            lblRegResult->setText("<span style='color:#f48771;'>Error: Channels cannot be identical.</span>");
            return false;
        }
        regSlope = Sxy / Sxx;
        regIntercept = meanY - regSlope * meanX;
        regValid = true;
        s_rememberedEnergies = ens;
        s_lastUsedPoints = n;

        lblRegResult->setText(QString("Fit Result: A(0) = %1 keV,  A(1) = %2 keV/ch  (from %3 points)")
            .arg(regIntercept, 0, 'f', 4).arg(regSlope, 0, 'f', 5).arg(n));
        return true;
    };

    QObject::connect(btnCalcReg, &QPushButton::clicked, [&]() {
        if (computeRegression()) {
            spinA0->setValue(regIntercept);
            spinA1->setValue(regSlope);
            spinA2->setValue(0.0);
            spinA3->setValue(0.0);
        }
    });

    tabWidget->addTab(tabPoints, "Reference Fit (2P / N-Points)");

    // Intelligently select initial tab based on current spectrum calibration & markers
    if (activeHist->IsCalibrated()) {
        const auto &segs = activeHist->GetCalibrationSegments();
        if (segs.size() <= 1) {
            tabWidget->setCurrentIndex(1); // Manual Polynomial for single segment
        } else {
            tabWidget->setCurrentIndex(0); // File tab for multi-segment piecewise
        }
    } else if (availablePoints >= 2) {
        tabWidget->setCurrentIndex(2); // Reference Fit if reference markers exist
    } else if (!loadedDetectors.empty()) {
        tabWidget->setCurrentIndex(0); // File tab if file was previously loaded
    } else {
        tabWidget->setCurrentIndex(1); // Default to Manual Polynomial
    }

    // --- Bottom Action Buttons ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    QPushButton *btnApplyActive = new QPushButton("✓ Apply to Active Spectrum", &dialog);
    btnApplyActive->setStyleSheet(
        "QPushButton { background-color: #007acc; color: #ffffff; border: 1px solid #0098ff; "
        "border-radius: 4px; padding: 7px 18px; font-weight: bold; font-size: 14px; min-height: 28px; }"
        "QPushButton:hover { background-color: #118ad4; }"
    );

    QPushButton *btnApplyAll = new QPushButton("Apply to All Open Spectra", &dialog);
    btnApplyAll->setStyleSheet(
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; "
        "border-radius: 4px; padding: 7px 16px; font-weight: bold; font-size: 13px; min-height: 28px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
    );

    QPushButton *btnSave = new QPushButton("Save...", &dialog);
    btnSave->setToolTip("Save calibration parameters to an .mcal or .cal file.");

    QPushButton *btnClose = new QPushButton("Close", &dialog);

    actionLayout->addWidget(btnApplyActive);
    actionLayout->addWidget(btnApplyAll);
    actionLayout->addWidget(btnSave);
    actionLayout->addStretch(1);
    actionLayout->addWidget(btnClose);
    mainLayout->addLayout(actionLayout);

    // Apply helper: takes calibration segments and applies them to a target pad
    auto applySegmentsToPad = [&](int row, int col, const std::vector<CalibSegment> &segs) {
        if (row < 1 || row > mainCanvas->maxElement_i || col < 1 || col > mainCanvas->maxElement_j) return;
        TracknHistogram *h = dynamic_cast<TracknHistogram*>(mainCanvas->HijF[row][col]);
        if (!h) return;
        h->SetSegmentedCalibration(segs);
        mainCanvas->renderPeakLabels(row, col);
        mainCanvas->renderPeakSearchLabels(row, col);
    };

    // Helper to get currently active tab's segments
    auto getCurrentTabSegments = [&]() -> std::vector<CalibSegment> {
        const int tabIdx = tabWidget->currentIndex();
        std::vector<CalibSegment> segs;
        if (tabIdx == 0) { // File tab
            int comboIdx = comboDetectors->currentIndex();
            if (comboIdx >= 0 && comboIdx < static_cast<int>(loadedDetectors.size())) {
                segs = loadedDetectors[comboIdx].segments;
            }
        } else if (tabIdx == 1) { // Manual tab
            CalibSegment s;
            s.maxChannel = 1e9;
            s.coeffs = { spinA0->value(), spinA1->value(), spinA2->value(), spinA3->value() };
            segs.push_back(s);
        } else if (tabIdx == 2) { // Fit tab
            if (!regValid) {
                computeRegression();
            }
            if (regValid) {
                CalibSegment s;
                s.maxChannel = 1e9;
                s.coeffs = { regIntercept, regSlope, 0.0 };
                segs.push_back(s);
            }
        }
        return segs;
    };

    // Connect Apply to Active Spectrum
    QObject::connect(btnApplyActive, &QPushButton::clicked, [&]() {
        std::vector<CalibSegment> segs = getCurrentTabSegments();
        if (segs.empty()) {
            QMessageBox::warning(&dialog, "EnCal", "No valid calibration segments available to apply.");
            return;
        }
        applySegmentsToPad(sel_i, sel_j, segs);
        mainCanvas->updateAxisStatusLabels();
        mainCanvas->getRootCanvas()->getCanvas()->Update();
        updateStatusLabel();

        QString logMsg = QString("Energy Calibration applied to Pad (%1, %2): %3 segments.\n")
                             .arg(sel_i).arg(sel_j).arg(segs.size());
        CommandPrompt::getInstance()->appendPlainText(logMsg);
        std::cout << logMsg.toStdString();
    });

    // Connect Apply to All Open Spectra
    QObject::connect(btnApplyAll, &QPushButton::clicked, [&]() {
        const int tabIdx = tabWidget->currentIndex();
        if (tabIdx == 0 && loadedDetectors.size() > 1) {
            // Multi-detector file mode: match detector ID to spectrum index or sequential cell!
            for (int z = 1; z <= mainCanvas->maxElement_i; ++z) {
                for (int g = 1; g <= mainCanvas->maxElement_j; ++g) {
                    TracknHistogram *h = dynamic_cast<TracknHistogram*>(mainCanvas->HijF[z][g]);
                    if (!h) continue;
                    // Try to match detector by pad order or index
                    int targetDetId = (z - 1) * mainCanvas->maxElement_j + g - 1;
                    const CalibDetector *matchDet = nullptr;
                    for (const auto &d : loadedDetectors) {
                        if (d.detectorId == targetDetId) {
                            matchDet = &d;
                            break;
                        }
                    }
                    if (!matchDet && !loadedDetectors.empty()) {
                        matchDet = &loadedDetectors[0];
                    }
                    if (matchDet) {
                        applySegmentsToPad(z, g, matchDet->segments);
                    }
                }
            }
            CommandPrompt::getInstance()->appendPlainText(
                QString("Applied multi-detector calibration to all pads from %1.\n").arg(editFilePath->text()));
        } else {
            // Single calibration applied to all pads
            std::vector<CalibSegment> segs = getCurrentTabSegments();
            if (segs.empty()) {
                QMessageBox::warning(&dialog, "EnCal", "No valid calibration segments to apply.");
                return;
            }
            for (int z = 1; z <= mainCanvas->maxElement_i; ++z) {
                for (int g = 1; g <= mainCanvas->maxElement_j; ++g) {
                    applySegmentsToPad(z, g, segs);
                }
            }
            CommandPrompt::getInstance()->appendPlainText("Applied energy calibration to all open pads.\n");
        }

        mainCanvas->updateAxisStatusLabels();
        mainCanvas->getRootCanvas()->getCanvas()->Update();
        updateStatusLabel();
    });

    // Connect Save
    QObject::connect(btnSave, &QPushButton::clicked, [&]() {
        QString savePath = QFileDialog::getSaveFileName(
            &dialog, "Save Calibration File", "calibration.cal",
            "GASP Multi-detector (*.mcal);;Calibration Table (*.cal);;All Files (*)");
        if (savePath.isEmpty()) return;

        bool isMcal = savePath.endsWith(".mcal", Qt::CaseInsensitive);
        std::vector<CalibDetector> detsToSave;
        if (!loadedDetectors.empty() && tabWidget->currentIndex() == 0) {
            detsToSave = loadedDetectors;
        } else {
            CalibDetector det;
            det.group = 1;
            det.detectorId = currentDetId;
            det.segments = getCurrentTabSegments();
            if (det.segments.empty() && activeHist->IsCalibrated()) {
                det.segments = activeHist->GetCalibrationSegments();
            }
            if (det.segments.empty()) {
                QMessageBox::warning(&dialog, "Save Calibration", "No valid calibration parameters available to save.");
                return;
            }
            detsToSave.push_back(det);
        }

        if (SaveCalibrationFile(savePath, detsToSave, isMcal)) {
            QMessageBox::information(&dialog, "Save Calibration", "Calibration successfully saved to:\n" + savePath);
        } else {
            QMessageBox::critical(&dialog, "Save Calibration", "Failed to save calibration file.");
        }
    });

    QObject::connect(btnClose, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}
