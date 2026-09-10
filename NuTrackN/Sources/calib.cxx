#include "calib.h"
#include "Design.h"
#include "tracknhistogram.h"

#include <iostream>
#include <vector>

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QString>

//==============================================================================
// TwoPointCalibration
//==============================================================================
// Computes linear energy calibration parameters using two reference peaks:
//
//   E(ch) = A(0) + A(1) * ch
//
// where:
//   A(1) = slope     = (E2 - E1) / (ch2 - ch1)   [keV / channel]
//   A(0) = intercept = E2 - A(1) * ch2           [keV]
//
// Inputs:
//   puncte_calib2p : List of marker channels selected by user clicks on peaks.
//                    The last two elements (n-2, n-1) are taken as reference points.
//   energie1       : True gamma energy corresponding to the first peak channel (ch1).
//   energie2       : True gamma energy corresponding to the second peak channel (ch2).
//==============================================================================
void TwoPointCalibration(const std::vector<Float_t>& puncte_calib2p,
                         double energie1,
                         double energie2)
{
    const std::size_t n = puncte_calib2p.size();

    // At least two marker points must have been placed on the spectrum
    if (n < 2) {
        CommandPrompt::getInstance()->appendPlainText(
            "Two-point calibration requires at least two points.");
        return;
    }

    // Extract the two most recently placed reference peak positions
    const double ch1 = puncte_calib2p[n - 2];
    const double ch2 = puncte_calib2p[n - 1];

    // Guard against identical channel positions (would cause division by zero)
    if (std::abs(ch2 - ch1) < 1e-6) {
        CommandPrompt::getInstance()->appendPlainText(
            "Error: Calibration channel points cannot be identical.");
        return;
    }

    // Compute linear calibration coefficients:
    //   slope m = delta(E) / delta(channel)
    //   intercept a = E2 - m * ch2
    const double m = (energie2 - energie1) / (ch2 - ch1);
    const double a = energie2 - m * ch2;

    // Format output string matching legacy GASP / Xtrackn style: A(0)=..., A(1)=...
    const QString report = QString("Calibration coef :   A(0)= %1   A(1)= %2")
                               .arg(a, 0, 'f', 3)
                               .arg(m, 0, 'f', 3);

    // Print to both the GUI embedded terminal console and standard output
    CommandPrompt::getInstance()->appendPlainText(report);
    std::cout << report.toStdString() << std::endl;
}

//==============================================================================
// runTwoPointCalibrationDialog
//==============================================================================
// Launches an interactive modal dialog prompting the user to input the known
// physical energies (e.g. 1173.2 keV and 1332.5 keV for 60Co) corresponding
// to the two selected peak channels.
//
// Once submitted, validates user inputs, runs TwoPointCalibration, and updates
// the active histogram's internal calibration model (if provided).
//==============================================================================
void runTwoPointCalibrationDialog(QWidget *parent,
                                  const std::vector<Float_t> &puncte_calib2p,
                                  TracknHistogram *activeHistogram)
{
    // Verify that at least two markers have been defined on the canvas
    if (puncte_calib2p.size() < 2) {
        const QString msg = "Two markers are needed to calibrate in two points.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        return;
    }

    // Read the last two marker positions (channels)
    const Float_t ch1 = puncte_calib2p[puncte_calib2p.size() - 2];
    const Float_t ch2 = puncte_calib2p[puncte_calib2p.size() - 1];

    // Construct the Qt modal dialog with styled dark background
    QDialog dialog(parent);
    dialog.setWindowTitle("Two-Point Energy Calibration");
    dialog.setStyleSheet("background-color: #2b2b2b; color: #ffffff;");
    QFormLayout form(&dialog);

    // First peak energy input field, showing its channel coordinate in the label
    QLineEdit *energy1LineEdit = new QLineEdit(&dialog);
    energy1LineEdit->setStyleSheet("background-color: #ffffff; color: #000000;");
    form.addRow(QString::number(ch1, 'f', 2) + " ch (First Energy):", energy1LineEdit);

    // Second peak energy input field, showing its channel coordinate in the label
    QLineEdit *energy2LineEdit = new QLineEdit(&dialog);
    energy2LineEdit->setStyleSheet("background-color: #ffffff; color: #000000;");
    form.addRow(QString::number(ch2, 'f', 2) + " ch (Second Energy):", energy2LineEdit);

    // Add standard OK / Cancel confirmation buttons
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Execute modal dialog and process user input
    if (dialog.exec() == QDialog::Accepted) {
        bool ok1 = false;
        bool ok2 = false;
        const double e1 = energy1LineEdit->text().toDouble(&ok1);
        const double e2 = energy2LineEdit->text().toDouble(&ok2);

        // Validation: energies must parse as valid numbers and must be positive
        if (ok1 && ok2 && e1 > 0.0 && e2 > 0.0) {
            // Compute coefficients and print report
            TwoPointCalibration(puncte_calib2p, e1, e2);

            // Apply calibration parameters directly to the active TracknHistogram instance
            if (activeHistogram && std::abs(ch2 - ch1) > 1e-6) {
                const double slope = (e2 - e1) / (ch2 - ch1);
                const double intercept = e2 - slope * ch2;
                activeHistogram->SetCalibration(intercept, slope, 0.0);
            }
        } else {
            // Inform user about input errors via terminal and stdout
            if (ok1 && ok2 && (e1 <= 0.0 || e2 <= 0.0)) {
                const QString msg = "The energy values must be strictly positive.\n";
                std::cout << msg.toStdString();
                CommandPrompt::getInstance()->appendPlainText(msg);
            } else {
                const QString msg = "Both energy fields must be filled with valid numbers.\n";
                std::cout << msg.toStdString();
                CommandPrompt::getInstance()->appendPlainText(msg);
            }
        }
    }
}
