#include "calib.h"
#include "Design.h"
#include "tracknhistogram.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QString>
#include <QScrollArea>

//==============================================================================
// Persistent energy memory retained across calibration dialog sessions
//==============================================================================
static std::vector<double> s_rememberedEnergies;
static int s_lastUsedPoints = 2; // Defaults to 2 on first use after program start

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
