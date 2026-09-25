#include "EfficiencyDialog.h"
#include "canvas.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include "Design.h"

EfficiencyDialog::EfficiencyDialog(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Detector Efficiency Setup (DE)"));
    resize(520, 520);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();
    loadCurrentConfig();
}

void EfficiencyDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. Correction Mode Selection (matching Xtrackn effcor.F)
    QGroupBox *grpMode = new QGroupBox(tr("Efficiency Correction Mode"), this);
    QHBoxLayout *modeLayout = new QHBoxLayout(grpMode);
    m_radioNone = new QRadioButton(tr("None (No Correction)"), grpMode);
    m_radioPoly = new QRadioButton(tr("Polynomial: ln(Eff) = f(ln E)"), grpMode);
    m_radioSpec = new QRadioButton(tr("Efficiency Spectrum File"), grpMode);

    QButtonGroup *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_radioNone);
    modeGroup->addButton(m_radioPoly);
    modeGroup->addButton(m_radioSpec);

    modeLayout->addWidget(m_radioNone);
    modeLayout->addWidget(m_radioPoly);
    modeLayout->addWidget(m_radioSpec);
    mainLayout->addWidget(grpMode);

    connect(m_radioNone, &QRadioButton::toggled, this, &EfficiencyDialog::onTypeChanged);
    connect(m_radioPoly, &QRadioButton::toggled, this, &EfficiencyDialog::onTypeChanged);
    connect(m_radioSpec, &QRadioButton::toggled, this, &EfficiencyDialog::onTypeChanged);

    // 2. Polynomial Coefficients Group
    m_grpPoly = new QGroupBox(tr("Polynomial Coefficients: ln(Eff) = sum( A[i] * (ln E)^i )"), this);
    QVBoxLayout *polyLayout = new QVBoxLayout(m_grpPoly);
    polyLayout->setSpacing(6);

    QGridLayout *coeffGrid = new QGridLayout();
    coeffGrid->setSpacing(6);
    m_spinCoeffs.resize(6);
    for (int i = 0; i < 6; ++i) {
        QLabel *lbl = new QLabel(QString("A[%1] (order %2):").arg(i).arg(i), m_grpPoly);
        QDoubleSpinBox *spin = new QDoubleSpinBox(m_grpPoly);
        spin->setRange(-1e6, 1e6);
        spin->setDecimals(6);
        spin->setSingleStep(0.01);
        coeffGrid->addWidget(lbl, i / 2, (i % 2) * 2);
        coeffGrid->addWidget(spin, i / 2, (i % 2) * 2 + 1);
        m_spinCoeffs[i] = spin;
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EfficiencyDialog::updateTestCalculation);
    }
    polyLayout->addLayout(coeffGrid);

    // File buttons for Polynomial
    QHBoxLayout *polyBtnLayout = new QHBoxLayout();
    QPushButton *btnLoadEff = new QPushButton(tr("Load from .eff..."), m_grpPoly);
    QPushButton *btnSaveEff = new QPushButton(tr("Save to .eff..."), m_grpPoly);
    connect(btnLoadEff, &QPushButton::clicked, this, &EfficiencyDialog::onLoadFile);
    connect(btnSaveEff, &QPushButton::clicked, this, &EfficiencyDialog::onSaveFile);
    polyBtnLayout->addWidget(btnLoadEff);
    polyBtnLayout->addWidget(btnSaveEff);
    polyBtnLayout->addStretch(1);
    polyLayout->addLayout(polyBtnLayout);

    // Interactive Test Calculation
    QHBoxLayout *testLayout = new QHBoxLayout();
    QLabel *lblTest = new QLabel(tr("Test Energy (keV):"), m_grpPoly);
    m_spinTestEnergy = new QDoubleSpinBox(m_grpPoly);
    m_spinTestEnergy->setRange(1.0, 10000.0);
    m_spinTestEnergy->setValue(1332.5);
    m_spinTestEnergy->setDecimals(1);
    m_lblTestResult = new QLabel(tr("Calculated Eff: -"), m_grpPoly);
    m_lblTestResult->setStyleSheet("color: #4ec9b0; font-weight: bold; font-size: 13px;");
    connect(m_spinTestEnergy, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EfficiencyDialog::updateTestCalculation);

    testLayout->addWidget(lblTest);
    testLayout->addWidget(m_spinTestEnergy);
    testLayout->addSpacing(12);
    testLayout->addWidget(m_lblTestResult);
    testLayout->addStretch(1);
    polyLayout->addLayout(testLayout);

    mainLayout->addWidget(m_grpPoly);

    // 3. Efficiency Spectrum Group
    m_grpSpec = new QGroupBox(tr("Spectrum Lookup (1 keV/ch)"), this);
    QHBoxLayout *specLayout = new QHBoxLayout(m_grpSpec);
    m_editSpecPath = new QLineEdit(m_grpSpec);
    m_editSpecPath->setPlaceholderText(tr("Path to efficiency spectrum (.spe, .txt)..."));
    QPushButton *btnBrowse = new QPushButton(tr("Browse..."), m_grpSpec);
    connect(btnBrowse, &QPushButton::clicked, this, &EfficiencyDialog::onBrowseSpectrum);
    specLayout->addWidget(m_editSpecPath);
    specLayout->addWidget(btnBrowse);
    mainLayout->addWidget(m_grpSpec);

    // 4. Scaling Factor
    QHBoxLayout *scaleLayout = new QHBoxLayout();
    QLabel *lblScale = new QLabel(tr("Efficiency Scaling Factor:"), this);
    m_spinScaling = new QDoubleSpinBox(this);
    m_spinScaling->setRange(1e-6, 1e6);
    m_spinScaling->setValue(1.0);
    m_spinScaling->setDecimals(4);
    scaleLayout->addWidget(lblScale);
    scaleLayout->addWidget(m_spinScaling);
    scaleLayout->addStretch(1);
    mainLayout->addLayout(scaleLayout);

    // Dialog Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnApply = new QPushButton(tr("Apply"), this);
    QPushButton *btnOk = new QPushButton(tr("OK"), this);
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnOk->setStyleSheet("QPushButton { background-color: #0e639c; } QPushButton:hover { background-color: #1177bb; }");

    connect(btnApply, &QPushButton::clicked, this, &EfficiencyDialog::applySettings);
    connect(btnOk, &QPushButton::clicked, this, &EfficiencyDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch(1);
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    onTypeChanged();
}

void EfficiencyDialog::onTypeChanged()
{
    const bool isPoly = m_radioPoly->isChecked();
    const bool isSpec = m_radioSpec->isChecked();

    m_grpPoly->setEnabled(isPoly);
    m_grpSpec->setEnabled(isSpec);
    updateTestCalculation();
}

void EfficiencyDialog::loadCurrentConfig()
{
    if (!m_mainCanvas) return;
    const auto &cfg = m_mainCanvas->getEfficiencyConfig();

    if (cfg.type == EfficiencyType::Polynomial) {
        m_radioPoly->setChecked(true);
    } else if (cfg.type == EfficiencyType::Spectrum) {
        m_radioSpec->setChecked(true);
    } else {
        m_radioNone->setChecked(true);
    }

    for (std::size_t i = 0; i < m_spinCoeffs.size(); ++i) {
        if (i < cfg.coeffs.size()) {
            m_spinCoeffs[i]->setValue(cfg.coeffs[i]);
        } else {
            m_spinCoeffs[i]->setValue(0.0);
        }
    }

    m_editSpecPath->setText(cfg.spectrumFile);
    m_spinScaling->setValue(cfg.scalingFactor > 0.0 ? cfg.scalingFactor : 1.0);
    onTypeChanged();
}

void EfficiencyDialog::updateTestCalculation()
{
    if (!m_radioPoly->isChecked()) {
        m_lblTestResult->setText(tr("Calculated Eff: (Active only in Polynomial mode)"));
        return;
    }

    const double energy = m_spinTestEnergy->value();
    if (energy <= 0.01) {
        m_lblTestResult->setText(tr("Calculated Eff: 1.0000"));
        return;
    }

    const double lnE = std::log(energy);
    double sum = 0.0;
    double term = 1.0;
    for (auto *spin : m_spinCoeffs) {
        sum += spin->value() * term;
        term *= lnE;
    }
    const double eff = std::exp(sum) * m_spinScaling->value();
    m_lblTestResult->setText(QString("Calculated Eff: %1 (%2 %)")
        .arg(eff, 0, 'g', 4)
        .arg(eff * 100.0, 0, 'f', 3));
}

void EfficiencyDialog::onBrowseSpectrum()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Efficiency Spectrum File"), "",
        tr("Spectrum Files (*.spe *.txt *.dat);;All Files (*)"));
    if (!fileName.isEmpty()) {
        m_editSpecPath->setText(fileName);
    }
}

void EfficiencyDialog::onLoadFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Efficiency Polynomial"), "",
        tr("Efficiency Files (*.eff *.txt);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not open efficiency file: %1").arg(fileName));
        return;
    }

    QTextStream in(&file);
    int idx = 0;
    while (!in.atEnd() && idx < static_cast<int>(m_spinCoeffs.size())) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        bool ok = false;
        double val = line.toDouble(&ok);
        if (ok) {
            m_spinCoeffs[idx++]->setValue(val);
        }
    }
    updateTestCalculation();
}

void EfficiencyDialog::onSaveFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Efficiency Polynomial"), "",
        tr("Efficiency Files (*.eff);;Text Files (*.txt)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not write efficiency file: %1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out << "# Efficiency polynomial coefficients ln(Eff) = sum( A[i] * (ln E)^i )\n";
    for (std::size_t i = 0; i < m_spinCoeffs.size(); ++i) {
        out << QString::number(m_spinCoeffs[i]->value(), 'g', 8) << "\n";
    }
}

void EfficiencyDialog::applySettings()
{
    if (!m_mainCanvas) return;

    EfficiencyConfig cfg;
    if (m_radioPoly->isChecked()) {
        cfg.type = EfficiencyType::Polynomial;
    } else if (m_radioSpec->isChecked()) {
        cfg.type = EfficiencyType::Spectrum;
    } else {
        cfg.type = EfficiencyType::None;
    }

    cfg.coeffs.clear();
    for (auto *spin : m_spinCoeffs) {
        cfg.coeffs.push_back(spin->value());
    }
    cfg.spectrumFile = m_editSpecPath->text().trimmed();
    cfg.scalingFactor = m_spinScaling->value();

    m_mainCanvas->setEfficiencyConfig(cfg);

    QString desc;
    if (cfg.type == EfficiencyType::Polynomial) {
        desc = "Polynomial ln(Eff) = sum(A[i]*(ln E)^i)";
    } else if (cfg.type == EfficiencyType::Spectrum) {
        desc = "Spectrum file: " + cfg.spectrumFile;
    } else {
        desc = "Disabled (None)";
    }

    CommandPrompt::getInstance()->appendPlainText(
        QString("Efficiency Correction updated (DE): %1 (Scale: %2)\n")
            .arg(desc).arg(cfg.scalingFactor));
}

void EfficiencyDialog::onAccept()
{
    applySettings();
    accept();
}
