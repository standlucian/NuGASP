#include "IntegralDialog.h"
#include "canvas.h"
#include "Integral.h"
#include "tracknhistogram.h"
#include "Design.h"

#include <QGridLayout>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>
#include <QString>
#include <QCloseEvent>
#include <cmath>

IntegralDialog::IntegralDialog(QMainCanvas *canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas), m_isUpdating(false)
{
    setupUI();
    setWindowTitle("Integration Parameters");
    setAttribute(Qt::WA_ShowWithoutActivating); // Don't steal focus
    
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());
}

IntegralDialog::~IntegralDialog()
{
}

void IntegralDialog::setupUI()
{
    QGridLayout *mainLayout = new QGridLayout(this);

    // Background Left
    m_bgLeftGroup = new QGroupBox("Background Left");
    QGridLayout *bgLeftLayout = new QGridLayout(m_bgLeftGroup);
    bgLeftLayout->setAlignment(Qt::AlignBottom);
    m_bgLeftStart = new QSpinBox(); m_bgLeftStart->setRange(0, 100000);
    m_bgLeftEnd = new QSpinBox();   m_bgLeftEnd->setRange(0, 100000);
    bgLeftLayout->addWidget(new QLabel("Start:"), 0, 0);
    bgLeftLayout->addWidget(m_bgLeftStart, 0, 1);
    bgLeftLayout->addWidget(new QLabel("End:"), 1, 0);
    bgLeftLayout->addWidget(m_bgLeftEnd, 1, 1);

    // Integration Region
    m_intGroup = new QGroupBox("Integration Region");
    QGridLayout *intLayout = new QGridLayout(m_intGroup);
    intLayout->setAlignment(Qt::AlignBottom);
    m_peakCombo = new QComboBox(m_intGroup);
    m_peakCombo->setVisible(false);
    m_intStart = new QSpinBox(); m_intStart->setRange(0, 100000);
    m_intEnd = new QSpinBox();   m_intEnd->setRange(0, 100000);
    intLayout->addWidget(m_peakCombo, 0, 0, 1, 2);
    intLayout->addWidget(new QLabel("Start:"), 1, 0);
    intLayout->addWidget(m_intStart, 1, 1);
    intLayout->addWidget(new QLabel("End:"), 2, 0);
    intLayout->addWidget(m_intEnd, 2, 1);

    // Background Right
    m_bgRightGroup = new QGroupBox("Background Right");
    QGridLayout *bgRightLayout = new QGridLayout(m_bgRightGroup);
    bgRightLayout->setAlignment(Qt::AlignBottom);
    m_bgRightCombo = new QComboBox(m_bgRightGroup);
    m_bgRightCombo->setVisible(false);
    m_bgRightStart = new QSpinBox(); m_bgRightStart->setRange(0, 100000);
    m_bgRightEnd = new QSpinBox();   m_bgRightEnd->setRange(0, 100000);
    bgRightLayout->addWidget(m_bgRightCombo, 0, 0, 1, 2);
    bgRightLayout->addWidget(new QLabel("Start:"), 1, 0);
    bgRightLayout->addWidget(m_bgRightStart, 1, 1);
    bgRightLayout->addWidget(new QLabel("End:"), 2, 0);
    bgRightLayout->addWidget(m_bgRightEnd, 2, 1);

    mainLayout->addWidget(m_bgLeftGroup, 0, 0);
    mainLayout->addWidget(m_intGroup, 0, 1);
    mainLayout->addWidget(m_bgRightGroup, 0, 2);

    // Background Regression Method
    QLabel *lblMethod = new QLabel("Background Method:", this);
    m_bgMethodCombo = new QComboBox(this);
    m_bgMethodCombo->addItem("OLS (Unbiased Default)", static_cast<int>(BackgroundFitMethod::OLS));
    m_bgMethodCombo->addItem("2-Pass IRLS (Model WLS)", static_cast<int>(BackgroundFitMethod::ModelWLS));
    m_bgMethodCombo->addItem("Poisson WLS (1/y)", static_cast<int>(BackgroundFitMethod::PoissonWLS));
    mainLayout->addWidget(lblMethod, 1, 0);
    mainLayout->addWidget(m_bgMethodCombo, 1, 1, 1, 2);

    // Results Group
    QGroupBox *resGroup = new QGroupBox("Results");
    QGridLayout *resLayout = new QGridLayout(resGroup);
    m_grossAreaLabel = new QLabel("-");
    m_bgAreaLabel = new QLabel("-");
    m_netAreaLabel = new QLabel("-");
    m_centroidLabel = new QLabel("-");
    m_fwhmTitleLabel = new QLabel("FWHM:");
    m_fwhmLabel = new QLabel("-");

    resLayout->addWidget(new QLabel("Gross Area:"), 0, 0); resLayout->addWidget(m_grossAreaLabel, 0, 1);
    resLayout->addWidget(new QLabel("Background:"), 1, 0); resLayout->addWidget(m_bgAreaLabel, 1, 1);
    resLayout->addWidget(new QLabel("Net Area:"), 2, 0);   resLayout->addWidget(m_netAreaLabel, 2, 1);
    resLayout->addWidget(new QLabel("Centroid:"), 3, 0);   resLayout->addWidget(m_centroidLabel, 3, 1);
    resLayout->addWidget(m_fwhmTitleLabel, 4, 0);          resLayout->addWidget(m_fwhmLabel, 4, 1);

    mainLayout->addWidget(resGroup, 2, 0, 1, 3);

    connect(m_peakCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isUpdating || !m_canvas || idx < 0) return;
        const std::size_t pairOffset = static_cast<std::size_t>(idx) * 2;
        if (pairOffset + 1 >= m_canvas->integral_markers.size()) return;
        m_selectedPeakIdx = idx;
        m_isUpdating = true;
        m_intStart->setValue(m_canvas->integral_markers[pairOffset]);
        m_intEnd->setValue(m_canvas->integral_markers[pairOffset + 1]);
        m_isUpdating = false;

        TH1F *hist = m_canvas->HijF[m_canvas->SelectedElement_i][m_canvas->SelectedElement_j];
        if (!hist) return;
        Double_t slope = 0.0, addition = 0.0;
        std::vector<IntegratedPeak> peaks;
        integral_function(hist, m_canvas->integral_markers, m_canvas->background_markers,
                          slope, addition, &peaks, true, m_canvas->getBackgroundFitMethod());
        updateResultLabels(peaks);
    });

    connect(m_bgRightCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isUpdating || !m_canvas || idx < 0) return;
        const int bgPairIdx = m_bgRightCombo->itemData(idx).toInt();
        const std::size_t pairOffset = static_cast<std::size_t>(bgPairIdx) * 2;
        if (pairOffset + 1 >= m_canvas->background_markers.size()) return;
        m_selectedRightBgIdx = bgPairIdx;
        m_isUpdating = true;
        m_bgRightStart->setValue(m_canvas->background_markers[pairOffset]);
        m_bgRightEnd->setValue(m_canvas->background_markers[pairOffset + 1]);
        m_isUpdating = false;
    });

    connect(m_bgMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isUpdating || !m_canvas || idx < 0) return;
        m_canvas->setBackgroundFitMethod(
            static_cast<BackgroundFitMethod>(m_bgMethodCombo->itemData(idx).toInt()));
        recalculate();
    });

    connect(m_bgLeftStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgLeftEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_intStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_intEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgRightStart, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
    connect(m_bgRightEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, &IntegralDialog::recalculate);
}

void IntegralDialog::updateResultLabels(const std::vector<IntegratedPeak> &peaks)
{
    auto roundUp = [](double err, int decimals) -> double {
        if (err <= 0.0) return 0.0;
        const double scale = std::pow(10.0, decimals);
        return std::ceil(err * scale) / scale;
    };

    if (!peaks.empty()) {
        const std::size_t peakIdx = (m_selectedPeakIdx >= 0 && static_cast<std::size_t>(m_selectedPeakIdx) < peaks.size())
            ? static_cast<std::size_t>(m_selectedPeakIdx)
            : 0;
        const auto &p = peaks[peakIdx];
        m_grossAreaLabel->setText(QString::number(p.grossArea, 'f', 1) + " +- " + QString::number(roundUp(p.grossAreaError, 1), 'f', 1));
        m_bgAreaLabel->setText(QString::number(p.bgArea, 'f', 1) + " +- " + QString::number(roundUp(p.bgAreaError, 1), 'f', 1));
        m_netAreaLabel->setText(QString::number(p.area, 'f', 1) + " +- " + QString::number(roundUp(p.areaError, 1), 'f', 1));
        if (p.isCalibrated) {
            m_centroidLabel->setText(
                QString("%1 +- %2 keV (ch %3)")
                    .arg(p.energy, 0, 'f', 2)
                    .arg(roundUp(p.energyError, 2), 0, 'f', 2)
                    .arg(p.centroid, 0, 'f', 2));
            m_fwhmLabel->setText(
                QString("%1 +- %2 keV (%3 ch)")
                    .arg(p.energyFwhm, 0, 'f', 2)
                    .arg(roundUp(p.energyFwhmError, 2), 0, 'f', 2)
                    .arg(p.fwhm, 0, 'f', 2));
        } else {
            m_centroidLabel->setText(QString::number(p.centroid, 'f', 2) + " +- " + QString::number(roundUp(p.centroidError, 2), 'f', 2) + " ch");
            m_fwhmLabel->setText(QString::number(p.fwhm, 'f', 2) + " +- " + QString::number(roundUp(p.fwhmError, 2), 'f', 2) + " ch");
        }
    } else {
        m_grossAreaLabel->setText("-");
        m_bgAreaLabel->setText("-");
        m_netAreaLabel->setText("-");
        m_centroidLabel->setText("-");
        m_fwhmLabel->setText("-");
    }
}

void IntegralDialog::updateFromCanvas()
{
    if (!m_canvas) return;
    m_isUpdating = true;

    const int numBgPairs = static_cast<int>(m_canvas->background_markers.size() / 2);
    const int numPeaks = static_cast<int>(m_canvas->integral_markers.size() / 2);

    m_hasLeftBg = (numBgPairs >= 1);
    m_hasRightBg = (numBgPairs >= 2);
    if (m_bgLeftGroup) m_bgLeftGroup->setEnabled(m_hasLeftBg);
    if (m_bgRightGroup) m_bgRightGroup->setEnabled(m_hasRightBg);

    if (m_selectedRightBgIdx < 1 || m_selectedRightBgIdx >= numBgPairs) {
        m_selectedRightBgIdx = 1;
    }
    if (m_bgRightCombo) {
        m_bgRightCombo->clear();
        for (int p = 1; p < numBgPairs; ++p) {
            m_bgRightCombo->addItem(
                QString("Bg #%1 (%2-%3)")
                    .arg(p + 1)
                    .arg(m_canvas->background_markers[2 * p])
                    .arg(m_canvas->background_markers[2 * p + 1]),
                p);
        }
        m_bgRightCombo->setVisible(numBgPairs > 2);
        const int bgComboIdx = m_bgRightCombo->findData(m_selectedRightBgIdx);
        if (bgComboIdx >= 0) m_bgRightCombo->setCurrentIndex(bgComboIdx);
    }

    m_bgLeftStart->setValue(m_hasLeftBg ? m_canvas->background_markers[0] : 0);
    m_bgLeftEnd->setValue(m_hasLeftBg ? m_canvas->background_markers[1] : 0);
    m_bgRightStart->setValue(m_hasRightBg ? m_canvas->background_markers[2 * m_selectedRightBgIdx] : 0);
    m_bgRightEnd->setValue(m_hasRightBg ? m_canvas->background_markers[2 * m_selectedRightBgIdx + 1] : 0);

    const bool hasInt = (numPeaks >= 1);
    if (m_intGroup) m_intGroup->setEnabled(hasInt);
    if (m_selectedPeakIdx < 0 || m_selectedPeakIdx >= numPeaks) {
        m_selectedPeakIdx = 0;
    }
    if (m_peakCombo) {
        m_peakCombo->clear();
        for (int p = 0; p < numPeaks; ++p) {
            m_peakCombo->addItem(
                QString("Peak #%1 (%2-%3)")
                    .arg(p + 1)
                    .arg(m_canvas->integral_markers[2 * p])
                    .arg(m_canvas->integral_markers[2 * p + 1]),
                p);
        }
        m_peakCombo->setVisible(numPeaks > 1);
        if (m_selectedPeakIdx >= 0 && m_selectedPeakIdx < m_peakCombo->count()) {
            m_peakCombo->setCurrentIndex(m_selectedPeakIdx);
        }
    }
    m_intStart->setValue(hasInt ? m_canvas->integral_markers[2 * m_selectedPeakIdx] : 0);
    m_intEnd->setValue(hasInt ? m_canvas->integral_markers[2 * m_selectedPeakIdx + 1] : 0);

    if (m_bgMethodCombo) {
        const int methodIdx = m_bgMethodCombo->findData(static_cast<int>(m_canvas->getBackgroundFitMethod()));
        if (methodIdx >= 0) m_bgMethodCombo->setCurrentIndex(methodIdx);
    }

    m_isUpdating = false;
    m_hasUserModified = false;

    TH1F *hist = m_canvas->HijF[m_canvas->SelectedElement_i][m_canvas->SelectedElement_j];
    if (!hist) return;

    Double_t slope = 0.0, addition = 0.0;
    std::vector<IntegratedPeak> peaks;
    integral_function(hist, m_canvas->integral_markers, m_canvas->background_markers,
                      slope, addition, &peaks, true, m_canvas->getBackgroundFitMethod());
    updateResultLabels(peaks);
}

void IntegralDialog::closeEvent(QCloseEvent *event)
{
    if (m_hasUserModified && m_canvas) {
        m_canvas->areaFunctionWithBackground(false, true, false);
        m_hasUserModified = false;
    }
    QDialog::closeEvent(event);
}

void IntegralDialog::done(int r)
{
    if (m_hasUserModified && m_canvas) {
        m_canvas->areaFunctionWithBackground(false, true, false);
        m_hasUserModified = false;
    }
    QDialog::done(r);
}

void IntegralDialog::recalculate()
{
    if (m_isUpdating || !m_canvas) return;
    m_hasUserModified = true;

    // Update active background pairs while preserving any extra pairs
    if (m_hasLeftBg && m_canvas->background_markers.size() >= 2) {
        m_canvas->background_markers[0] = m_bgLeftStart->value();
        m_canvas->background_markers[1] = m_bgLeftEnd->value();
    }
    const std::size_t rightBgOffset = static_cast<std::size_t>(std::max(1, m_selectedRightBgIdx)) * 2;
    if (m_hasRightBg && rightBgOffset + 1 < m_canvas->background_markers.size()) {
        m_canvas->background_markers[rightBgOffset] = m_bgRightStart->value();
        m_canvas->background_markers[rightBgOffset + 1] = m_bgRightEnd->value();
        if (m_bgRightCombo && m_bgRightCombo->isVisible()) {
            const int curIdx = m_bgRightCombo->currentIndex();
            if (curIdx >= 0) {
                m_bgRightCombo->setItemText(
                    curIdx,
                    QString("Bg #%1 (%2-%3)")
                        .arg(m_selectedRightBgIdx + 1)
                        .arg(m_bgRightStart->value())
                        .arg(m_bgRightEnd->value()));
            }
        }
    }

    const std::size_t peakOffset = static_cast<std::size_t>(std::max(0, m_selectedPeakIdx)) * 2;
    if (peakOffset + 1 < m_canvas->integral_markers.size()) {
        m_canvas->integral_markers[peakOffset] = m_intStart->value();
        m_canvas->integral_markers[peakOffset + 1] = m_intEnd->value();
        if (m_peakCombo && m_peakCombo->isVisible()) {
            const int curIdx = m_peakCombo->currentIndex();
            if (curIdx >= 0) {
                m_peakCombo->setItemText(
                    curIdx,
                    QString("Peak #%1 (%2-%3)")
                        .arg(m_selectedPeakIdx + 1)
                        .arg(m_intStart->value())
                        .arg(m_intEnd->value()));
            }
        }
    }

    TH1F *hist = m_canvas->HijF[m_canvas->SelectedElement_i][m_canvas->SelectedElement_j];
    if (!hist) return;

    // Call integral function quietly for live UI updates
    Double_t slope = 0, addition = 0;
    std::vector<IntegratedPeak> peaks;
    integral_function(hist, m_canvas->integral_markers, m_canvas->background_markers,
                      slope, addition, &peaks, true, m_canvas->getBackgroundFitMethod());
    updateResultLabels(peaks);

    // Redraw quietly without printing duplicate console tables or writing intermediate log rows
    m_canvas->clearDrawnObjects();
    m_canvas->showBackgroundMarkers();
    m_canvas->showIntegralMarkers();
    m_canvas->areaFunctionWithBackground(false, false, true);
}
