#include "DisplayParamsDialog.h"
#include "canvas.h"
#include "Design.h"
#include "TH1F.h"
#include "TVirtualPad.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include "Design.h"
#include <QRadioButton>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>

DisplayParamsDialog::DisplayParamsDialog(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Display Parameters (DD)"));
    resize(520, 520);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();
    loadCurrentValues();
}

void DisplayParamsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. Vertical Autoscale Headroom
    QGroupBox *grpHeadroom = new QGroupBox(tr("Autoscale Vertical Headroom (FY / Up Arrow)"), this);
    QGridLayout *gridHeadroom = new QGridLayout(grpHeadroom);
    gridHeadroom->setSpacing(8);

    QLabel *lblLinear = new QLabel(tr("Linear Headroom (%):"), grpHeadroom);
    m_spinLinearHeadroom = new QDoubleSpinBox(grpHeadroom);
    m_spinLinearHeadroom->setRange(0.0, 100.0);
    m_spinLinearHeadroom->setSingleStep(5.0);
    m_spinLinearHeadroom->setDecimals(1);
    m_spinLinearHeadroom->setSuffix(" %");

    QLabel *lblLog = new QLabel(tr("Logarithmic Headroom (%):"), grpHeadroom);
    m_spinLogHeadroom = new QDoubleSpinBox(grpHeadroom);
    m_spinLogHeadroom->setRange(0.0, 200.0);
    m_spinLogHeadroom->setSingleStep(10.0);
    m_spinLogHeadroom->setDecimals(1);
    m_spinLogHeadroom->setSuffix(" %");

    gridHeadroom->addWidget(lblLinear, 0, 0);
    gridHeadroom->addWidget(m_spinLinearHeadroom, 0, 1);
    gridHeadroom->addWidget(lblLog, 1, 0);
    gridHeadroom->addWidget(m_spinLogHeadroom, 1, 1);
    mainLayout->addWidget(grpHeadroom);

    // 2. Grid & Zoom Settings
    QGroupBox *grpDisplay = new QGroupBox(tr("Canvas Grid & Zoom Navigation"), this);
    QGridLayout *gridDisplay = new QGridLayout(grpDisplay);
    gridDisplay->setSpacing(8);

    m_chkGridX = new QCheckBox(tr("Show X Grid"), grpDisplay);
    QLabel *lblGridDivX = new QLabel(tr("X Grid Size (Divisions):"), grpDisplay);
    m_spinGridDivX = new QSpinBox(grpDisplay);
    m_spinGridDivX->setRange(2, 100);
    m_spinGridDivX->setValue(10);
    m_spinGridDivX->setToolTip(tr("Number of grid intervals along the X axis"));

    m_chkGridY = new QCheckBox(tr("Show Y Grid"), grpDisplay);
    QLabel *lblGridDivY = new QLabel(tr("Y Grid Size (Divisions):"), grpDisplay);
    m_spinGridDivY = new QSpinBox(grpDisplay);
    m_spinGridDivY->setRange(2, 100);
    m_spinGridDivY->setValue(10);
    m_spinGridDivY->setToolTip(tr("Number of grid intervals along the Y axis"));

    connect(m_chkGridX, &QCheckBox::toggled, m_spinGridDivX, &QSpinBox::setEnabled);
    connect(m_chkGridY, &QCheckBox::toggled, m_spinGridDivY, &QSpinBox::setEnabled);

    QLabel *lblGridWidth = new QLabel(tr("Grid Line Width:"), grpDisplay);
    m_spinGridWidth = new QSpinBox(grpDisplay);
    m_spinGridWidth->setRange(1, 5);
    m_spinGridWidth->setSuffix(" px");
    m_spinGridWidth->setValue(1);

    QLabel *lblGridStyle = new QLabel(tr("Grid Style:"), grpDisplay);
    m_comboGridStyle = new QComboBox(grpDisplay);
    m_comboGridStyle->addItem(tr("Dashed"), 2);
    m_comboGridStyle->addItem(tr("Dotted"), 3);
    m_comboGridStyle->addItem(tr("Solid"), 1);

    QLabel *lblZoom = new QLabel(tr("Default Zoom Width:"), grpDisplay);
    m_spinZoomWidth = new QSpinBox(grpDisplay);
    m_spinZoomWidth->setRange(10, 10240);
    m_spinZoomWidth->setSingleStep(50);
    m_spinZoomWidth->setSuffix(" ch");

    gridDisplay->addWidget(m_chkGridX, 0, 0);
    gridDisplay->addWidget(lblGridDivX, 0, 1);
    gridDisplay->addWidget(m_spinGridDivX, 0, 2);

    gridDisplay->addWidget(m_chkGridY, 1, 0);
    gridDisplay->addWidget(lblGridDivY, 1, 1);
    gridDisplay->addWidget(m_spinGridDivY, 1, 2);

    gridDisplay->addWidget(lblGridWidth, 2, 0);
    gridDisplay->addWidget(m_spinGridWidth, 2, 1);
    gridDisplay->addWidget(lblGridStyle, 2, 2);
    gridDisplay->addWidget(m_comboGridStyle, 2, 3);

    gridDisplay->addWidget(lblZoom, 3, 0);
    gridDisplay->addWidget(m_spinZoomWidth, 3, 1, 1, 3);
    mainLayout->addWidget(grpDisplay);

    // 3. Y-Axis Scale Mode (Linear vs Log)
    QGroupBox *grpScale = new QGroupBox(tr("Y-Axis Scale Function (FUNCCMD)"), this);
    QHBoxLayout *scaleLayout = new QHBoxLayout(grpScale);
    m_radioLinearY = new QRadioButton(tr("Normal (Linear)"), grpScale);
    m_radioLogY = new QRadioButton(tr("Logarithmic"), grpScale);
    QButtonGroup *scaleGroup = new QButtonGroup(this);
    scaleGroup->addButton(m_radioLinearY);
    scaleGroup->addButton(m_radioLogY);
    scaleLayout->addWidget(m_radioLinearY);
    scaleLayout->addWidget(m_radioLogY);
    scaleLayout->addStretch(1);
    mainLayout->addWidget(grpScale);

    // 4. Current Axis Range (View / Override)
    QGroupBox *grpRange = new QGroupBox(tr("Current Visible Axis Limits"), this);
    QGridLayout *gridRange = new QGridLayout(grpRange);
    gridRange->setSpacing(6);

    QLabel *lblXMin = new QLabel(tr("X Min:"), grpRange);
    m_spinXMin = new QDoubleSpinBox(grpRange);
    m_spinXMin->setRange(0.0, 100000.0);
    m_spinXMin->setDecimals(1);

    QLabel *lblXMax = new QLabel(tr("X Max:"), grpRange);
    m_spinXMax = new QDoubleSpinBox(grpRange);
    m_spinXMax->setRange(1.0, 100000.0);
    m_spinXMax->setDecimals(1);

    QLabel *lblYMin = new QLabel(tr("Y Min:"), grpRange);
    m_spinYMin = new QDoubleSpinBox(grpRange);
    m_spinYMin->setRange(0.0, 1e9);
    m_spinYMin->setDecimals(1);

    QLabel *lblYMax = new QLabel(tr("Y Max:"), grpRange);
    m_spinYMax = new QDoubleSpinBox(grpRange);
    m_spinYMax->setRange(1.0, 1e9);
    m_spinYMax->setDecimals(1);

    gridRange->addWidget(lblXMin, 0, 0);
    gridRange->addWidget(m_spinXMin, 0, 1);
    gridRange->addWidget(lblXMax, 0, 2);
    gridRange->addWidget(m_spinXMax, 0, 3);
    gridRange->addWidget(lblYMin, 1, 0);
    gridRange->addWidget(m_spinYMin, 1, 1);
    gridRange->addWidget(lblYMax, 1, 2);
    gridRange->addWidget(m_spinYMax, 1, 3);
    mainLayout->addWidget(grpRange);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnApply = new QPushButton(tr("Apply"), this);
    QPushButton *btnOk = new QPushButton(tr("OK"), this);
    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnOk->setStyleSheet("QPushButton { background-color: #0e639c; } QPushButton:hover { background-color: #1177bb; }");

    connect(btnApply, &QPushButton::clicked, this, &DisplayParamsDialog::applySettings);
    connect(btnOk, &QPushButton::clicked, this, &DisplayParamsDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch(1);
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    connect(m_radioLogY, &QRadioButton::toggled, this, [this](bool isLog) {
        if (isLog) {
            m_spinYMin->setRange(0.0001, 1e9);
            if (m_spinYMin->value() < 0.1) {
                m_spinYMin->setValue(0.5);
            }
        } else {
            m_spinYMin->setRange(0.0, 1e9);
        }
    });

    mainLayout->addLayout(btnLayout);
}

void DisplayParamsDialog::loadCurrentValues()
{
    if (!m_mainCanvas) return;

    m_spinLinearHeadroom->setValue(m_mainCanvas->m_autoscaleHeadroomLinear);
    m_spinLogHeadroom->setValue(m_mainCanvas->m_autoscaleHeadroomLog);
    m_spinZoomWidth->setValue(m_mainCanvas->m_defaultZoomWidth);
    m_spinGridDivX->setValue(m_mainCanvas->m_gridDivisionsX);
    m_spinGridDivY->setValue(m_mainCanvas->m_gridDivisionsY);
    m_spinGridWidth->setValue(m_mainCanvas->m_gridLineWidth);
    int styleIdx = m_comboGridStyle->findData(m_mainCanvas->m_gridLineStyle);
    if (styleIdx >= 0) m_comboGridStyle->setCurrentIndex(styleIdx);

    TVirtualPad *pad = nullptr;
    if (m_mainCanvas->getRootCanvas() && m_mainCanvas->getRootCanvas()->getCanvas()) {
        if (m_mainCanvas->maxElement_i > 1 || m_mainCanvas->maxElement_j > 1) {
            const int padIndex = (m_mainCanvas->SelectedElement_i - 1) * m_mainCanvas->maxElement_j + m_mainCanvas->SelectedElement_j;
            pad = m_mainCanvas->getRootCanvas()->getCanvas()->GetPad(padIndex);
        }
        if (!pad) pad = m_mainCanvas->getRootCanvas()->getCanvas();
    }

    if (pad) {
        m_chkGridX->setChecked(pad->GetGridx() != 0);
        m_chkGridY->setChecked(pad->GetGridy() != 0);
        if (pad->GetLogy() != 0) {
            m_radioLogY->setChecked(true);
        } else {
            m_radioLinearY->setChecked(true);
        }
    } else {
        m_radioLinearY->setChecked(true);
    }
    m_spinGridDivX->setEnabled(m_chkGridX->isChecked());
    m_spinGridDivY->setEnabled(m_chkGridY->isChecked());

    TH1F *hist = m_mainCanvas->HijF[m_mainCanvas->SelectedElement_i][m_mainCanvas->SelectedElement_j];
    if (hist && hist->GetXaxis()) {
        const int first = hist->GetXaxis()->GetFirst();
        const int last  = hist->GetXaxis()->GetLast();
        m_spinXMin->setValue(hist->GetXaxis()->GetBinLowEdge(first));
        m_spinXMax->setValue(hist->GetXaxis()->GetBinUpEdge(last));
        double curYMin = hist->GetMinimum();
        if (m_radioLogY->isChecked()) {
            m_spinYMin->setRange(0.0001, 1e9);
            if (curYMin < 0.1) curYMin = 0.5;
        } else {
            m_spinYMin->setRange(0.0, 1e9);
            if (curYMin < 0.0) curYMin = 0.0;
        }
        m_spinYMin->setValue(curYMin);
        m_spinYMax->setValue(hist->GetMaximum());
    }
}

void DisplayParamsDialog::applySettings()
{
    if (!m_mainCanvas) return;

    m_mainCanvas->m_autoscaleHeadroomLinear = m_spinLinearHeadroom->value();
    m_mainCanvas->m_autoscaleHeadroomLog    = m_spinLogHeadroom->value();
    m_mainCanvas->m_defaultZoomWidth        = m_spinZoomWidth->value();
    m_mainCanvas->m_gridDivisionsX          = m_spinGridDivX->value();
    m_mainCanvas->m_gridDivisionsY          = m_spinGridDivY->value();
    m_mainCanvas->m_gridLineWidth           = m_spinGridWidth->value();
    m_mainCanvas->m_gridLineStyle           = m_comboGridStyle->currentData().toInt();

    const bool wantLog = m_radioLogY->isChecked();
    double y0 = m_spinYMin->value();
    double y1 = m_spinYMax->value();

    if (wantLog) {
        if (y0 <= 0.0) y0 = 0.5;
        if (y1 <= y0) y1 = y0 + 10.0;
    }

    TH1F *hist = m_mainCanvas->HijF[m_mainCanvas->SelectedElement_i][m_mainCanvas->SelectedElement_j];
    if (hist && hist->GetXaxis()) {
        const double x0 = m_spinXMin->value();
        const double x1 = m_spinXMax->value();

        if (x1 > x0) {
            hist->GetXaxis()->SetRangeUser(x0, x1);
        }
        if (y1 > y0) {
            hist->SetMinimum(y0);
            hist->SetMaximum(y1);
            hist->GetYaxis()->SetRangeUser(y0, y1);
        }

        for (TH1F *overlay : m_mainCanvas->HijC[m_mainCanvas->SelectedElement_i][m_mainCanvas->SelectedElement_j]) {
            if (!overlay || overlay == hist) continue;
            overlay->SetMinimum(y0);
            overlay->SetMaximum(y1);
        }
    }

    TVirtualPad *activePad = nullptr;
    if (m_mainCanvas->getRootCanvas() && m_mainCanvas->getRootCanvas()->getCanvas()) {
        if (m_mainCanvas->maxElement_i > 1 || m_mainCanvas->maxElement_j > 1) {
            const int padIndex = (m_mainCanvas->SelectedElement_i - 1) * m_mainCanvas->maxElement_j + m_mainCanvas->SelectedElement_j;
            activePad = m_mainCanvas->getRootCanvas()->getCanvas()->GetPad(padIndex);
        }
        if (!activePad) activePad = m_mainCanvas->getRootCanvas()->getCanvas();
    }

    gStyle->SetGridColor(kGray + 2);
    gStyle->SetGridStyle(m_mainCanvas->m_gridLineStyle);
    gStyle->SetGridWidth(m_mainCanvas->m_gridLineWidth);

    for (int z = 1; z <= m_mainCanvas->maxElement_i; ++z) {
        for (int g = 1; g <= m_mainCanvas->maxElement_j; ++g) {
            TH1F *h = m_mainCanvas->HijF[z][g];
            if (h && h->GetXaxis()) {
                h->GetXaxis()->SetNdivisions(m_mainCanvas->m_gridDivisionsX, kTRUE);
                h->GetXaxis()->SetLabelSize(0);
                h->GetXaxis()->SetTickLength(0);
                h->GetYaxis()->SetNdivisions(m_mainCanvas->m_gridDivisionsY, kTRUE);
                h->GetYaxis()->SetLabelSize(0);
                h->GetYaxis()->SetTickLength(0);
            }
            TVirtualPad *p = nullptr;
            if (m_mainCanvas->maxElement_i > 1 || m_mainCanvas->maxElement_j > 1) {
                p = m_mainCanvas->getRootCanvas()->getCanvas()->GetPad((z - 1) * m_mainCanvas->maxElement_j + g);
            } else {
                p = m_mainCanvas->getRootCanvas()->getCanvas();
            }
            if (p) {
                p->cd();
                p->SetGridx(m_chkGridX->isChecked() ? 1 : 0);
                p->SetGridy(m_chkGridY->isChecked() ? 1 : 0);
                if (p == activePad && (p->GetLogy() != 0) != wantLog) {
                    p->SetLogy(wantLog ? 1 : 0);
                }
                if (m_chkGridX->isChecked() || m_chkGridY->isChecked()) {
                    p->RedrawAxis("g");
                }
                p->Modified();
                p->Update();
            }
        }
    }

    if (m_mainCanvas->getRootCanvas() && m_mainCanvas->getRootCanvas()->getCanvas()) {
        m_mainCanvas->getRootCanvas()->getCanvas()->Modified();
        m_mainCanvas->getRootCanvas()->getCanvas()->Update();
    }
    m_mainCanvas->updateAxisStatusLabels();

    CommandPrompt::getInstance()->appendPlainText(
        QString("Display Parameters updated (DD): Linear Headroom=%1%, Log Headroom=%2%, Grid=[%3,%4] (Divisions: X=%5, Y=%6, Width=%7px), ZoomWidth=%8 ch\n")
            .arg(m_mainCanvas->m_autoscaleHeadroomLinear, 0, 'f', 1)
            .arg(m_mainCanvas->m_autoscaleHeadroomLog, 0, 'f', 1)
            .arg(m_chkGridX->isChecked() ? "X" : "-")
            .arg(m_chkGridY->isChecked() ? "Y" : "-")
            .arg(m_mainCanvas->m_gridDivisionsX)
            .arg(m_mainCanvas->m_gridDivisionsY)
            .arg(m_mainCanvas->m_gridLineWidth)
            .arg(m_mainCanvas->m_defaultZoomWidth));
}

void DisplayParamsDialog::onAccept()
{
    applySettings();
    accept();
}
