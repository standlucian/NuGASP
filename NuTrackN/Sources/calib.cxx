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

    const double ch1 = puncte_calib2p[n - 2];
    const double ch2 = puncte_calib2p[n - 1];

    if (std::abs(ch2 - ch1) < 1e-6) {
        CommandPrompt::getInstance()->appendPlainText(
            "Error: Calibration channel points cannot be identical.");
        return;
    }

    const double m = (energie2 - energie1) / (ch2 - ch1);
    const double a = energie2 - m * ch2;

    const QString report = QString("Calibration coef :   A(0)= %1   A(1)= %2")
                               .arg(a, 0, 'f', 3)
                               .arg(m, 0, 'f', 3);

    CommandPrompt::getInstance()->appendPlainText(report);
    std::cout << report.toStdString() << std::endl;
}

void runTwoPointCalibrationDialog(QWidget *parent,
                                  const std::vector<Float_t> &puncte_calib2p,
                                  TracknHistogram *activeHistogram)
{
    if (puncte_calib2p.size() < 2) {
        const QString msg = "Two markers are needed to calibrate in two points.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
        return;
    }

    const Float_t ch1 = puncte_calib2p[puncte_calib2p.size() - 2];
    const Float_t ch2 = puncte_calib2p[puncte_calib2p.size() - 1];

    QDialog dialog(parent);
    dialog.setWindowTitle("Two-Point Energy Calibration");
    dialog.setStyleSheet("background-color: #2b2b2b; color: #ffffff;");
    QFormLayout form(&dialog);

    QLineEdit *energy1LineEdit = new QLineEdit(&dialog);
    energy1LineEdit->setStyleSheet("background-color: #ffffff; color: #000000;");
    form.addRow(QString::number(ch1, 'f', 2) + " ch (First Energy):", energy1LineEdit);

    QLineEdit *energy2LineEdit = new QLineEdit(&dialog);
    energy2LineEdit->setStyleSheet("background-color: #ffffff; color: #000000;");
    form.addRow(QString::number(ch2, 'f', 2) + " ch (Second Energy):", energy2LineEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        bool ok1 = false;
        bool ok2 = false;
        const double e1 = energy1LineEdit->text().toDouble(&ok1);
        const double e2 = energy2LineEdit->text().toDouble(&ok2);

        if (ok1 && ok2 && e1 > 0.0 && e2 > 0.0) {
            TwoPointCalibration(puncte_calib2p, e1, e2);

            // Apply calibration directly to the active spectrum if provided
            if (activeHistogram && std::abs(ch2 - ch1) > 1e-6) {
                const double slope = (e2 - e1) / (ch2 - ch1);
                const double intercept = e2 - slope * ch2;
                activeHistogram->SetCalibration(intercept, slope, 0.0);
            }
        } else {
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
