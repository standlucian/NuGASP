#include "IntegralDialog.h"
#include "canvas.h"
#include "Integral.h"
#include "tracknhistogram.h"

#include <QGridLayout>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QString>

IntegralDialog::IntegralDialog(QMainCanvas *canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas), m_isUpdating(false)
{
    setupUI();
    setWindowTitle("Integration Parameters");
    setAttribute(Qt::WA_ShowWithoutActivating); // Don't steal focus
    
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #ffffff; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 10px; font-weight: bold; color: #00ffff; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #e0e0e0; font-size: 13px; }"
        "QSpinBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 4px 8px; font-size: 13px; }"
        "QSpinBox:focus { border: 1px solid #007acc; }"
    );
}

IntegralDialog::~IntegralDialog()
{
}

void IntegralDialog::setupUI()
{
    QGridLayout *mainLayout = new QGridLayout(this);

    // Background Left
    QGroupBox *bgLeftGroup = new QGroupBox("Background Left");
    QGridLayout *bgLeftLayout = new QGridLayout(bgLeftGroup);
    m_bgLeftStart = new QSpinBox(); m_bgLeftStart->setRange(1, 100000);
    m_bgLeftEnd = new QSpinBox();   m_bgLeftEnd->setRange(1, 100000);
    bgLeftLayout->addWidget(new QLabel("Start:"), 0, 0);
    bgLeftLayout->addWidget(m_bgLeftStart, 0, 1);
    bgLeftLayout->addWidget(new QLabel("End:"), 1, 0);
    bgLeftLayout->addWidget(m_bgLeftEnd, 1, 1);

    // Integration Region
    QGroupBox *intGroup = new QGroupBox("Integration Region");
    QGridLayout *intLayout = new QGridLayout(intGroup);
    m_intStart = new QSpinBox(); m_intStart->setRange(1, 100000);
    m_intEnd = new QSpinBox();   m_intEnd->setRange(1, 100000);
    intLayout->addWidget(new QLabel("Start:"), 0, 0);
    intLayout->addWidget(m_intStart, 0, 1);
    intLayout->addWidget(new QLabel("End:"), 1, 0);
    intLayout->addWidget(m_intEnd, 1, 1);

    // Background Right
    QGroupBox *bgRightGroup = new QGroupBox("Background Right");
    QGridLayout *bgRightLayout = new QGridLayout(bgRightGroup);
    m_bgRightStart = new QSpinBox(); m_bgRightStart->setRange(1, 100000);
    m_bgRightEnd = new QSpinBox();   m_bgRightEnd->setRange(1, 100000);
    bgRightLayout->addWidget(new QLabel("Start:"), 0, 0);
    bgRightLayout->addWidget(m_bgRightStart, 0, 1);
    bgRightLayout->addWidget(new QLabel("End:"), 1, 0);
    bgRightLayout->addWidget(m_bgRightEnd, 1, 1);

    mainLayout->addWidget(bgLeftGroup, 0, 0);
    mainLayout->addWidget(intGroup, 0, 1);
    mainLayout->addWidget(bgRightGroup, 0, 2);

    // Results Group
    QGroupBox *resGroup = new QGroupBox("Results");
    QGridLayout *resLayout = new QGridLayout(resGroup);
    m_grossAreaLabel = new QLabel("-");
    m_bgAreaLabel = new QLabel("-");
    m_netAreaLabel = new QLabel("-");
    m_centroidLabel = new QLabel("-");
    m_fwhmLabel = new QLabel("-");

    resLayout->addWidget(new QLabel("Gross Area:"), 0, 0); resLayout->addWidget(m_grossAreaLabel, 0, 1);
    resLayout->addWidget(new QLabel("Background:"), 1, 0); resLayout->addWidget(m_bgAreaLabel, 1, 1);
    resLayout->addWidget(new QLabel("Net Area:"), 2, 0);   resLayout->addWidget(m_netAreaLabel, 2, 1);
    resLayout->addWidget(new QLabel("Centroid:"), 3, 0);   resLayout->addWidget(m_centroidLabel, 3, 1);
    resLayout->addWidget(new QLabel("FWHM (ch):"), 4, 0);  resLayout->addWidget(m_fwhmLabel, 4, 1);

    mainLayout->addWidget(resGroup, 1, 0, 1, 3);

    connect(m_bgLeftStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgLeftEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_intStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_intEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgRightStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgRightEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
}

void IntegralDialog::updateFromCanvas()
{
    m_isUpdating = true;
    
    // Clear first to prevent bad signals
    m_bgLeftStart->setValue(1);
    m_bgLeftEnd->setValue(1);
    m_bgRightStart->setValue(1);
    m_bgRightEnd->setValue(1);
    m_intStart->setValue(1);
    m_intEnd->setValue(1);

    if (m_canvas->background_markers.size() >= 4) {
        m_bgLeftStart->setValue(m_canvas->background_markers[0]);
        m_bgLeftEnd->setValue(m_canvas->background_markers[1]);
        m_bgRightStart->setValue(m_canvas->background_markers[2]);
        m_bgRightEnd->setValue(m_canvas->background_markers[3]);
    }
    
    if (m_canvas->integral_markers.size() >= 2) {
        m_intStart->setValue(m_canvas->integral_markers[0]);
        m_intEnd->setValue(m_canvas->integral_markers[1]);
    }

    m_isUpdating = false;
    recalculate();
}

void IntegralDialog::recalculate()
{
    if (m_isUpdating) return;

    // Update canvas markers
    m_canvas->background_markers.clear();
    m_canvas->background_markers.push_back(m_bgLeftStart->value());
    m_canvas->background_markers.push_back(m_bgLeftEnd->value());
    m_canvas->background_markers.push_back(m_bgRightStart->value());
    m_canvas->background_markers.push_back(m_bgRightEnd->value());

    m_canvas->integral_markers.clear();
    m_canvas->integral_markers.push_back(m_intStart->value());
    m_canvas->integral_markers.push_back(m_intEnd->value());

    TH1F *hist = m_canvas->HijF[m_canvas->SelectedElement_i][m_canvas->SelectedElement_j];
    if (!hist) return;

    // Call integral function
    Double_t slope = 0, addition = 0;
    std::vector<IntegratedPeak> peaks;
    integral_function(hist, m_canvas->integral_markers, m_canvas->background_markers, slope, addition, &peaks);

    if (!peaks.empty()) {
        const auto &p = peaks.front();
        m_grossAreaLabel->setText(QString::number(p.grossArea, 'f', 1) + " +- " + QString::number(p.grossAreaError, 'f', 1));
        m_bgAreaLabel->setText(QString::number(p.bgArea, 'f', 1) + " +- " + QString::number(p.bgAreaError, 'f', 1));
        m_netAreaLabel->setText(QString::number(p.area, 'f', 1) + " +- " + QString::number(p.areaError, 'f', 1));
        m_centroidLabel->setText(QString::number(p.centroid, 'f', 2) + " +- " + QString::number(p.centroidError, 'f', 2));
        m_fwhmLabel->setText(QString::number(p.fwhm, 'f', 2));
    } else {
        m_grossAreaLabel->setText("-");
        m_bgAreaLabel->setText("-");
        m_netAreaLabel->setText("-");
        m_centroidLabel->setText("-");
        m_fwhmLabel->setText("-");
    }

    // Redraw
    m_canvas->clearDrawnObjects();
    m_canvas->showBackgroundMarkers();
    m_canvas->showIntegralMarkers();
    m_canvas->areaFunctionWithBackground(false);
}
