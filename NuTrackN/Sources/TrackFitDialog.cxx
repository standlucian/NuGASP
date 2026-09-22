#include "TrackFitDialog.h"
#include "canvas.h"
#include "tracknhistogram.h"
#include "calib.h"
#include "Design.h"

#include "TF1.h"
#include "TCanvas.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QTabWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QMouseEvent>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>

//==============================================================================
// PeakFitTileWidget Implementation
//==============================================================================
PeakFitTileWidget::PeakFitTileWidget(int peakIndex, bool isInteractive, QWidget *parent)
    : QWidget(parent),
      m_peakIndex(peakIndex),
      m_isInteractive(isInteractive),
      m_hasData(false)
{
    if (m_isInteractive) {
        setCursor(Qt::PointingHandCursor);
        setMinimumSize(270, 190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        setMinimumSize(320, 230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
}

void PeakFitTileWidget::setPeakData(const TrackFitPeakResult &res, int peakIndex)
{
    m_res = res;
    m_peakIndex = peakIndex;
    m_hasData = true;
    update();
}

void PeakFitTileWidget::clearData()
{
    m_hasData = false;
    m_res = TrackFitPeakResult();
    update();
}

void PeakFitTileWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_isInteractive && event->button() == Qt::LeftButton && m_peakIndex >= 0) {
        emit tileClicked(m_peakIndex);
    }
    QWidget::mousePressEvent(event);
}

void PeakFitTileWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal w = width();
    const qreal h = height();

    // 1. Container background & border
    QRectF bgRect(1.0, 1.0, w - 2.0, h - 2.0);
    if (!m_hasData) {
        painter.setPen(QPen(QColor("#3e3e42"), 1.0));
        painter.setBrush(QColor("#15161a"));
        painter.drawRoundedRect(bgRect, 6.0, 6.0);
        painter.setPen(QColor("#666666"));
        painter.setFont(QFont("sans-serif", 10, QFont::Normal));
        painter.drawText(bgRect, Qt::AlignCenter, "No peak selected / No fit data");
        return;
    }

    const bool isIncluded = m_res.isIncluded;
    const bool isFitted = (m_res.fittedCentroid > 0.0);

    QColor borderColor = !isIncluded ? QColor("#444444") : (isFitted ? QColor("#007acc") : QColor("#555555"));
    QColor bgColor = !isIncluded ? QColor("#14161a") : QColor("#181b22");

    painter.setPen(QPen(borderColor, 1.2));
    painter.setBrush(bgColor);
    painter.drawRoundedRect(bgRect, 6.0, 6.0);

    // 2. Header: Reference Energy & Badge
    QFont headerFont("sans-serif", 9, QFont::Bold);
    painter.setFont(headerFont);
    painter.setPen(isIncluded ? QColor("#00ffff") : QColor("#888888"));

    QString titleText = QString("Ref: %1 keV").arg(m_res.refEnergy, 0, 'f', 2);
    painter.drawText(QRectF(10, 6, w * 0.55, 20), Qt::AlignLeft | Qt::AlignVCenter, titleText);

    // Badge in top-right
    QString badgeText;
    QColor badgeBg, badgePen, badgeTextCol;
    if (!isIncluded) {
        badgeText = "EXCLUDED";
        badgeBg = QColor(60, 60, 60, 180);
        badgePen = QColor("#555555");
        badgeTextCol = QColor("#aaaaaa");
    } else if (isFitted) {
        badgeText = QString("ΔE: %1%2 keV")
                        .arg(m_res.residualEnergy >= 0 ? "+" : "")
                        .arg(m_res.residualEnergy, 0, 'f', 2);
        double absRes = std::abs(m_res.residualEnergy);
        if (absRes < 0.25) {
            badgeBg = QColor(22, 56, 43, 200);
            badgePen = QColor("#2e7d32");
            badgeTextCol = QColor("#4ec9b0");
        } else if (absRes < 0.75) {
            badgeBg = QColor(61, 50, 22, 200);
            badgePen = QColor("#f57f17");
            badgeTextCol = QColor("#f0c674");
        } else {
            badgeBg = QColor(61, 22, 22, 200);
            badgePen = QColor("#c62828");
            badgeTextCol = QColor("#f48771");
        }
    } else {
        badgeText = m_res.status;
        badgeBg = QColor(50, 25, 25, 200);
        badgePen = QColor("#803030");
        badgeTextCol = QColor("#f48771");
    }

    QFont badgeFont("sans-serif", 8, QFont::Bold);
    painter.setFont(badgeFont);
    QFontMetrics fm(badgeFont);
    int badgeW = fm.horizontalAdvance(badgeText) + 12;
    QRectF badgeRect(w - badgeW - 8, 6, badgeW, 18);
    painter.setPen(badgePen);
    painter.setBrush(badgeBg);
    painter.drawRoundedRect(badgeRect, 3.0, 3.0);
    painter.setPen(badgeTextCol);
    painter.drawText(badgeRect, Qt::AlignCenter, badgeText);

    // 3. Plot Area boundaries
    const qreal plotLeft = 10.0;
    const qreal plotRight = w - 10.0;
    const qreal plotTop = 30.0;
    const qreal plotBottom = m_showFooter ? (h - 22.0) : (h - 10.0);
    const qreal plotW = plotRight - plotLeft;
    const qreal plotH = plotBottom - plotTop;

    // Dark plot canvas
    painter.setPen(QPen(QColor(255, 255, 255, 25), 1.0));
    painter.setBrush(QColor(12, 14, 18));
    QRectF plotBox(plotLeft, plotTop, plotW, plotH);
    painter.drawRect(plotBox);

    // Subtle 50% horizontal line
    painter.drawLine(QPointF(plotLeft, plotTop + plotH * 0.5), QPointF(plotRight, plotTop + plotH * 0.5));

    // 4. Render Raw Spectrum
    const int countBins = static_cast<int>(m_res.localCounts.size());
    if (countBins > 1) {
        double maxCount = 1.0;
        for (double c : m_res.localCounts) {
            if (c > maxCount) maxCount = c;
        }
        if (isFitted && (m_res.fitAmplitude + m_res.fitBaselineOffset) > maxCount) {
            maxCount = (m_res.fitAmplitude + m_res.fitBaselineOffset) * 1.05;
        }

        const qreal binW = plotW / static_cast<qreal>(countBins);

        QPolygonF poly;
        poly << QPointF(plotLeft, plotBottom);
        for (int i = 0; i < countBins; ++i) {
            qreal x1 = plotLeft + i * binW;
            qreal x2 = plotLeft + (i + 1) * binW;
            qreal normH = (m_res.localCounts[i] / maxCount) * (plotH - 4.0);
            qreal yTop = plotBottom - normH;
            poly << QPointF(x1, yTop);
            poly << QPointF(x2, yTop);
        }
        poly << QPointF(plotRight, plotBottom);

        // Fill area under spectrum
        QLinearGradient grad(0, plotTop, 0, plotBottom);
        if (isIncluded) {
            grad.setColorAt(0.0, QColor(0, 200, 255, 110));
            grad.setColorAt(1.0, QColor(0, 100, 200, 20));
        } else {
            grad.setColorAt(0.0, QColor(100, 100, 100, 80));
            grad.setColorAt(1.0, QColor(50, 50, 50, 15));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(grad);
        painter.drawPolygon(poly);

        // Stepped outline
        painter.setPen(QPen(isIncluded ? QColor(0, 255, 255, 220) : QColor(130, 130, 130, 180), 1.2));
        for (int i = 0; i < countBins; ++i) {
            qreal x1 = plotLeft + i * binW;
            qreal x2 = plotLeft + (i + 1) * binW;
            qreal normH = (m_res.localCounts[i] / maxCount) * (plotH - 4.0);
            qreal yTop = plotBottom - normH;
            painter.drawLine(QPointF(x1, yTop), QPointF(x2, yTop));
            if (i + 1 < countBins) {
                qreal nextNormH = (m_res.localCounts[i + 1] / maxCount) * (plotH - 4.0);
                qreal nextYTop = plotBottom - nextNormH;
                painter.drawLine(QPointF(x2, yTop), QPointF(x2, nextYTop));
            }
        }

        // 5. Render Fitted Gaussian Curve + Baseline
        if (isFitted && m_res.fitSigma > 0.0) {
            QPen curvePen(isIncluded ? QColor(255, 200, 59) : QColor(160, 140, 70), isIncluded ? 2.0 : 1.2);
            if (!isIncluded) curvePen.setStyle(Qt::DashLine);
            painter.setPen(curvePen);

            const double chMin = m_res.fitStartBin - 1.0;
            const int numCurveSteps = static_cast<int>(plotW);
            QPolygonF curvePoints;
            for (int s = 0; s <= numCurveSteps; ++s) {
                qreal px = plotLeft + s;
                double ch = chMin + (px - plotLeft) / binW;
                double diff = (ch - m_res.fittedCentroid) / m_res.fitSigma;
                double gausVal = m_res.fitAmplitude * std::exp(-0.5 * diff * diff);
                double bkgVal = std::max(0.0, m_res.fitBaselineOffset + m_res.fitBaselineSlope * (ch - m_res.fittedCentroid));
                double totalFit = gausVal + bkgVal;
                qreal py = plotBottom - (totalFit / maxCount) * (plotH - 4.0);
                py = std::max(plotTop - 2.0, std::min(plotBottom + 2.0, py));
                curvePoints << QPointF(px, py);
            }
            painter.drawPolyline(curvePoints);

            // Centroid dashed vertical line
            if (m_res.fittedCentroid >= chMin && m_res.fittedCentroid <= m_res.fitEndBin) {
                qreal pxCent = plotLeft + (m_res.fittedCentroid - chMin) * binW;
                QPen centPen(QColor(255, 200, 59, 180), 1.0, Qt::DashLine);
                painter.setPen(centPen);
                painter.drawLine(QPointF(pxCent, plotTop), QPointF(pxCent, plotBottom));
            }
        }
    }

    // 6. Footer Readout
    if (m_showFooter) {
        QFont footerFont("sans-serif", 8, QFont::Normal);
        painter.setFont(footerFont);
        painter.setPen(QColor("#cccccc"));

        QString footerLeft;
        if (isFitted) {
            footerLeft = QString("Ch: %1 | FWHM: %2 keV")
                             .arg(m_res.fittedCentroid, 0, 'f', 1)
                             .arg(m_res.fwhmEnergy, 0, 'f', 2);
        } else {
            footerLeft = QString("Pred Ch: %1").arg(m_res.expectedChannel, 0, 'f', 1);
        }
        painter.drawText(QRectF(10, h - 20, w * 0.6, 16), Qt::AlignLeft | Qt::AlignVCenter, footerLeft);

        QString footerRight = (m_res.netArea > 0.0) ? QString("Area: %1").arg(m_res.netArea, 0, 'f', 0) : "";
        painter.drawText(QRectF(w * 0.6, h - 20, w * 0.4 - 10, 16), Qt::AlignRight | Qt::AlignVCenter, footerRight);
    }
}

//==============================================================================
// TrackFitMath Namespace - Shared Calibration Regression & Evaluation
//==============================================================================
namespace TrackFitMath {

bool solvePolynomial(const std::vector<double> &chs,
                     const std::vector<double> &ens,
                     int order,
                     std::vector<double> &outCoeffs)
{
    const int N = static_cast<int>(chs.size());
    const int nPar = order + 1;
    if (N < nPar) return false;

    // Normal Equations: M * A = Y
    std::vector<std::vector<double>> M(nPar, std::vector<double>(nPar, 0.0));
    std::vector<double> Y(nPar, 0.0);

    for (int i = 0; i < N; ++i) {
        const double c = chs[i];
        const double e = ens[i];
        std::vector<double> cp(2 * nPar, 1.0);
        for (int p = 1; p < 2 * nPar; ++p) {
            cp[p] = cp[p - 1] * c;
        }

        for (int j = 0; j < nPar; ++j) {
            Y[j] += e * cp[j];
            for (int k = 0; k < nPar; ++k) {
                M[j][k] += cp[j + k];
            }
        }
    }

    // Gaussian elimination with partial pivoting
    std::vector<double> coeffs(nPar, 0.0);
    for (int i = 0; i < nPar; ++i) {
        int maxRow = i;
        for (int k = i + 1; k < nPar; ++k) {
            if (std::abs(M[k][i]) > std::abs(M[maxRow][i])) {
                maxRow = k;
            }
        }
        std::swap(M[i], M[maxRow]);
        std::swap(Y[i], Y[maxRow]);

        if (std::abs(M[i][i]) < 1e-18) {
            return false;
        }

        for (int k = i + 1; k < nPar; ++k) {
            double factor = M[k][i] / M[i][i];
            Y[k] -= factor * Y[i];
            for (int j = i; j < nPar; ++j) {
                M[k][j] -= factor * M[i][j];
            }
        }
    }

    // Back-substitution
    for (int i = nPar - 1; i >= 0; --i) {
        double sum = Y[i];
        for (int j = i + 1; j < nPar; ++j) {
            sum -= M[i][j] * coeffs[j];
        }
        coeffs[i] = sum / M[i][i];
    }

    outCoeffs = coeffs;
    return true;
}

void evaluateResiduals(std::vector<TrackFitPeakResult> &peaks,
                       const std::vector<double> &coeffs,
                       double &outRmsResidualKeV)
{
    const int nPar = static_cast<int>(coeffs.size());
    double sumSqRes = 0.0;
    int evaluatedCount = 0;

    for (auto &res : peaks) {
        if (res.fittedCentroid > 0.0) {
            double c = res.fittedCentroid;
            double eCalc = 0.0;
            double cp = 1.0;
            for (int p = 0; p < nPar; ++p) {
                eCalc += coeffs[p] * cp;
                cp *= c;
            }
            res.calcEnergy = eCalc;
            res.residualEnergy = eCalc - res.refEnergy;

            double slopeAtCh = (nPar > 1) ? coeffs[1] : 1.0;
            if (nPar > 2) slopeAtCh += 2.0 * coeffs[2] * c;
            if (nPar > 3) slopeAtCh += 3.0 * coeffs[3] * c * c;
            res.fwhmEnergy = std::abs(slopeAtCh * res.fwhmChannel);

            if (res.isIncluded) {
                sumSqRes += res.residualEnergy * res.residualEnergy;
                evaluatedCount++;
            }
        } else {
            res.calcEnergy = 0.0;
            res.residualEnergy = 0.0;
            res.fwhmEnergy = 0.0;
        }
    }

    outRmsResidualKeV = (evaluatedCount > 0) ? std::sqrt(sumSqRes / evaluatedCount) : 0.0;
}

void fitFwhmRelation(const std::vector<TrackFitPeakResult> &peaks,
                     double &outIntercept, double &outSlope)
{
    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    int fwhmPts = 0;
    for (const auto &res : peaks) {
        if (res.isIncluded && res.fwhmEnergy > 0.1 && res.refEnergy > 0.0) {
            sumX += res.refEnergy;
            sumY += res.fwhmEnergy;
            sumXX += res.refEnergy * res.refEnergy;
            sumXY += res.refEnergy * res.fwhmEnergy;
            fwhmPts++;
        }
    }
    if (fwhmPts >= 2) {
        double denom = fwhmPts * sumXX - sumX * sumX;
        if (std::abs(denom) > 1e-9) {
            outSlope = (fwhmPts * sumXY - sumX * sumY) / denom;
            outIntercept = (sumY - outSlope * sumX) / fwhmPts;
        }
    }
}

QString formatEquation(const std::vector<double> &coeffs, int order, int pointCount)
{
    const int nPar = static_cast<int>(coeffs.size());
    QString eqStr = "<b>Energy Calibration:</b> <span style='color:#00ffff;'>E(ch) = ";
    for (int p = 0; p < nPar; ++p) {
        if (p > 0) eqStr += (coeffs[p] >= 0 ? " + " : " - ");
        else if (coeffs[p] < 0) eqStr += "-";
        double absVal = std::abs(coeffs[p]);
        eqStr += QString::number(absVal, 'g', 6);
        if (p == 1) eqStr += "&middot;ch";
        else if (p == 2) eqStr += "&middot;ch&sup2;";
        else if (p == 3) eqStr += "&middot;ch&sup3;";
    }
    eqStr += QString("</span> &nbsp; (Order %1 fit across %2 points)").arg(order).arg(pointCount);
    return eqStr;
}

} // namespace TrackFitMath

//==============================================================================
// TrackFitInspectionDialog Implementation
//==============================================================================
TrackFitInspectionDialog::TrackFitInspectionDialog(const std::vector<TrackFitPeakResult> &peakResults,
                                                   int polyOrder,
                                                   QWidget *parent)
    : QDialog(parent),
      m_peaks(peakResults),
      m_polyOrder(std::max(1, std::min(3, polyOrder))),
      m_rmsResidual(0.0),
      m_fwhmIntercept(0.0),
      m_fwhmSlope(0.0),
      m_accepted(false)
{
    setWindowTitle("AutoTrace - Visual Fit Inspection (Xtrackn)");
    resize(1180, 800);
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #ffffff; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 8px; font-weight: bold; color: #00ffff; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #e0e0e0; font-size: 13px; }"
        "QComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 4px 8px; font-size: 13px; }"
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
        "QPushButton:pressed { background-color: #007acc; }"
        "QScrollArea { background-color: #16181d; border: 1px solid #333333; border-radius: 4px; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. Top Header Bar
    QGroupBox *headerBox = new QGroupBox("Inspection & Recalibration", this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerBox);
    headerLayout->setContentsMargins(10, 8, 10, 8);

    m_lblSummary = new QLabel(headerBox);
    headerLayout->addWidget(m_lblSummary);

    headerLayout->addStretch(1);

    QLabel *lblOrder = new QLabel("Polynomial Order:", headerBox);
    m_comboPolyOrder = new QComboBox(headerBox);
    m_comboPolyOrder->addItems({"Order 1 (Linear)", "Order 2 (Quadratic)", "Order 3 (Cubic)"});
    m_comboPolyOrder->setCurrentIndex(m_polyOrder - 1);
    connect(m_comboPolyOrder, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackFitInspectionDialog::onPolyOrderChanged);

    headerLayout->addWidget(lblOrder);
    headerLayout->addWidget(m_comboPolyOrder);
    headerLayout->addSpacing(15);

    QLabel *lblTip = new QLabel("💡 <i>Click any tile to toggle Include/Exclude</i>", headerBox);
    lblTip->setStyleSheet("color: #88d49e;");
    headerLayout->addWidget(lblTip);

    mainLayout->addWidget(headerBox);

    // 2. Center Scroll Area with Grid of Tiles
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    QWidget *gridContainer = new QWidget(scrollArea);
    gridContainer->setStyleSheet("background-color: #16181d;");
    QGridLayout *gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(10);
    gridLayout->setContentsMargins(10, 10, 10, 10);

    const int numPeaks = static_cast<int>(m_peaks.size());
    const int cols = 3;
    m_tiles.resize(numPeaks, nullptr);

    for (int i = 0; i < numPeaks; ++i) {
        PeakFitTileWidget *tile = new PeakFitTileWidget(i, true, gridContainer);
        tile->setMinimumSize(340, 220);
        tile->setPeakData(m_peaks[i], i);
        connect(tile, &PeakFitTileWidget::tileClicked, this, &TrackFitInspectionDialog::onTileClicked);
        gridLayout->addWidget(tile, i / cols, i % cols);
        m_tiles[i] = tile;
    }

    gridContainer->setLayout(gridLayout);
    scrollArea->setWidget(gridContainer);
    mainLayout->addWidget(scrollArea, 1);

    // 3. Bottom Footer Bar: Equation & Buttons
    QGroupBox *footerBox = new QGroupBox("Calibration Results", this);
    QVBoxLayout *footerLayout = new QVBoxLayout(footerBox);
    footerLayout->setContentsMargins(10, 8, 10, 8);
    footerLayout->setSpacing(6);

    QHBoxLayout *eqRow = new QHBoxLayout();
    m_lblEquation = new QLabel(footerBox);
    m_lblRms = new QLabel(footerBox);
    m_lblRms->setStyleSheet("font-weight: bold; color: #4ec9b0; font-size: 13px;");
    eqRow->addWidget(m_lblEquation, 1);
    eqRow->addWidget(m_lblRms);
    footerLayout->addLayout(eqRow);

    QHBoxLayout *fwhmRow = new QHBoxLayout();
    m_lblFwhm = new QLabel(footerBox);
    fwhmRow->addWidget(m_lblFwhm);
    footerLayout->addLayout(fwhmRow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);

    QPushButton *btnAccept = new QPushButton("✓ Accept Calibration", footerBox);
    btnAccept->setStyleSheet(
        "QPushButton { background-color: #007acc; color: #ffffff; border: 1px solid #0098ff; "
        "border-radius: 4px; padding: 7px 22px; font-weight: bold; font-size: 13px; } "
        "QPushButton:hover { background-color: #118ad4; }"
    );
    connect(btnAccept, &QPushButton::clicked, this, &TrackFitInspectionDialog::onAcceptCalibration);

    QPushButton *btnClose = new QPushButton("Close / Return", footerBox);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addWidget(btnAccept);
    btnRow->addWidget(btnClose);
    footerLayout->addLayout(btnRow);

    mainLayout->addWidget(footerBox);

    // Initial calculation and UI update
    recalculateRegression();
}

void TrackFitInspectionDialog::onTileClicked(int peakIndex)
{
    if (peakIndex >= 0 && peakIndex < static_cast<int>(m_peaks.size())) {
        m_peaks[peakIndex].isIncluded = !m_peaks[peakIndex].isIncluded;
        recalculateRegression();
    }
}

void TrackFitInspectionDialog::onPolyOrderChanged(int orderIndex)
{
    m_polyOrder = orderIndex + 1;
    recalculateRegression();
}

void TrackFitInspectionDialog::onAcceptCalibration()
{
    m_accepted = true;
    accept();
}

void TrackFitInspectionDialog::recalculateRegression()
{
    std::vector<double> chs, ens;
    int matchedCount = 0;
    for (const auto &res : m_peaks) {
        if (res.fittedCentroid > 0.0) {
            matchedCount++;
            if (res.isIncluded && res.refEnergy > 0.0) {
                chs.push_back(res.fittedCentroid);
                ens.push_back(res.refEnergy);
            }
        }
    }

    const int N = static_cast<int>(chs.size());
    const int effectiveOrder = std::min(m_polyOrder, std::max(1, N - 1));

    bool ok = false;
    if (N >= 2) {
        ok = TrackFitMath::solvePolynomial(chs, ens, effectiveOrder, m_calibCoeffs);
    }

    if (ok) {
        TrackFitMath::evaluateResiduals(m_peaks, m_calibCoeffs, m_rmsResidual);
        TrackFitMath::fitFwhmRelation(m_peaks, m_fwhmIntercept, m_fwhmSlope);

        m_lblEquation->setText(TrackFitMath::formatEquation(m_calibCoeffs, effectiveOrder, N));
        m_lblRms->setText(QString("RMS Residual: &plusmn;%1 keV").arg(m_rmsResidual, 0, 'f', 4));
        m_lblFwhm->setText(QString("<b>Resolution:</b> <i>FWHM(E) = %1 + %2 &middot; E (keV)</i>")
                              .arg(m_fwhmIntercept, 0, 'f', 3).arg(m_fwhmSlope, 0, 'g', 4));
    } else {
        m_calibCoeffs.clear();
        m_rmsResidual = 0.0;
        m_lblEquation->setText("<b>Energy Calibration:</b> <span style='color:#f48771;'>Need at least 2 valid included peaks.</span>");
        m_lblRms->setText("RMS Residual: -");
        m_lblFwhm->setText("<b>Resolution:</b> <i>FWHM(E) = -</i>");
    }

    m_lblSummary->setText(QString("<b>Fitted Peaks:</b> <span style='color:#4ec9b0;'>%1 / %2</span> matched &nbsp;|&nbsp; <b>Active in Fit:</b> <span style='color:#00ffff;'>%3</span>")
                              .arg(matchedCount).arg(m_peaks.size()).arg(N));

    updateHeaderAndTiles();
}

void TrackFitInspectionDialog::updateHeaderAndTiles()
{
    for (size_t i = 0; i < m_tiles.size() && i < m_peaks.size(); ++i) {
        if (m_tiles[i]) {
            m_tiles[i]->setPeakData(m_peaks[i], static_cast<int>(i));
        }
    }
}

//==============================================================================
// TrackFitDialog Constructor
//==============================================================================
TrackFitDialog::TrackFitDialog(QMainCanvas *mainCanvas,
                               TracknHistogram *activeHistogram,
                               int currentDetId,
                               QWidget *parent)
    : QDialog(parent),
      m_mainCanvas(mainCanvas),
      m_activeHist(activeHistogram),
      m_currentDetId(currentDetId),
      m_selectedPeakIndex(0)
{
    setWindowTitle("AutoTrace / TrackFit Calibration Manager (DT)");
    resize(1080, 720);
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #ffffff; }"
        "QTabWidget::pane { border: 1px solid #3e3e42; background: #252526; border-radius: 4px; }"
        "QTabBar::tab { background: #2d2d30; color: #cccccc; padding: 8px 20px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; font-size: 13px; }"
        "QTabBar::tab:selected { background: #007acc; color: #ffffff; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 10px; font-weight: bold; color: #00ffff; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #e0e0e0; font-size: 13px; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 4px 8px; font-size: 13px; }"
        "QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #007acc; }"
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
        "QPushButton:pressed { background-color: #007acc; }"
        "QTableWidget { background-color: #252526; color: #ffffff; gridline-color: #3e3e42; selection-background-color: #094771; font-size: 13px; }"
        "QHeaderView::section { background-color: #2d2d30; color: #cccccc; padding: 6px; border: 1px solid #3e3e42; font-weight: bold; font-size: 12px; }"
        "QScrollArea { background-color: #1e1e1e; border: none; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // --- Header Box: Target spectrum info & current status ---
    QGroupBox *headerBox = new QGroupBox("Target Spectrum & Calibration", this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerBox);
    headerLayout->setContentsMargins(10, 8, 10, 8);

    const int sel_i = m_mainCanvas ? m_mainCanvas->SelectedElement_i : 1;
    const int sel_j = m_mainCanvas ? m_mainCanvas->SelectedElement_j : 1;
    const int specIdx = m_mainCanvas ? m_mainCanvas->getCurrentSpectrumIndex() : 0;

    QString statusText;
    if (m_activeHist && m_activeHist->IsCalibrated()) {
        statusText = "<span style='color:#4ec9b0;'>CALIBRATED</span>";
    } else {
        statusText = "<span style='color:#f48771;'>UNCALIBRATED</span>";
    }

    QLabel *lblPadInfo = new QLabel(
        QString("<b>Pad:</b> (%1, %2) &nbsp;|&nbsp; <b>Spectrum Index:</b> #%3 &nbsp;|&nbsp; <b>Current Status:</b> %4")
            .arg(sel_i).arg(sel_j).arg(specIdx).arg(statusText),
        headerBox);
    headerLayout->addWidget(lblPadInfo);
    headerLayout->addStretch(1);
    mainLayout->addWidget(headerBox);

    // --- Tab Widget ---
    QTabWidget *tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget, 1);

    // =========================================================================
    // TAB 1: Table & Live Peak Inspector
    // =========================================================================
    QWidget *tabTable = new QWidget();
    QHBoxLayout *tabTableLayout = new QHBoxLayout(tabTable);
    tabTableLayout->setSpacing(10);
    tabTableLayout->setContentsMargins(8, 8, 8, 8);

    // Left pane: Controls & Table
    QWidget *leftPane = new QWidget(tabTable);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setSpacing(8);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Controls box
    QGroupBox *controlsBox = new QGroupBox("Calibration Source & Search Settings", leftPane);
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsBox);
    controlsLayout->setSpacing(6);
    controlsLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *row1 = new QHBoxLayout();
    QLabel *lblPreset = new QLabel("Source:", controlsBox);
    m_comboPreset = new QComboBox(controlsBox);
    m_comboPreset->setMinimumWidth(240);

    QPushButton *btnAddLine = new QPushButton("+ Add Line...", controlsBox);
    btnAddLine->setToolTip("Add custom gamma energy (keV)");
    QPushButton *btnRemoveLine = new QPushButton("- Remove Selected", controlsBox);

    row1->addWidget(lblPreset);
    row1->addWidget(m_comboPreset, 1);
    row1->addWidget(btnAddLine);
    row1->addWidget(btnRemoveLine);
    controlsLayout->addLayout(row1);

    QHBoxLayout *row2 = new QHBoxLayout();
    QLabel *lblGain = new QLabel("Initial Gain:", controlsBox);
    m_spinInitGain = new QDoubleSpinBox(controlsBox);
    m_spinInitGain->setRange(0.001, 100.0);
    m_spinInitGain->setDecimals(5);
    double initialGain = (m_activeHist && m_activeHist->IsCalibrated()) ? m_activeHist->GetCalibA1() : 0.50;
    m_spinInitGain->setValue(initialGain);
    m_spinInitGain->setFixedWidth(90);

    QLabel *lblOffset = new QLabel("Offset:", controlsBox);
    m_spinInitOffset = new QDoubleSpinBox(controlsBox);
    m_spinInitOffset->setRange(-1000.0, 1000.0);
    m_spinInitOffset->setDecimals(4);
    double initialOffset = (m_activeHist && m_activeHist->IsCalibrated()) ? m_activeHist->GetCalibA0() : 0.0;
    m_spinInitOffset->setValue(initialOffset);
    m_spinInitOffset->setFixedWidth(80);

    QLabel *lblWindow = new QLabel("Window (±ch):", controlsBox);
    m_spinSearchWindow = new QSpinBox(controlsBox);
    m_spinSearchWindow->setRange(5, 200);
    m_spinSearchWindow->setValue(25);
    m_spinSearchWindow->setFixedWidth(65);

    QLabel *lblFitWidth = new QLabel("Fit Width:", controlsBox);
    m_spinFitWidth = new QDoubleSpinBox(controlsBox);
    m_spinFitWidth->setRange(1.2, 6.0);
    m_spinFitWidth->setSingleStep(0.2);
    m_spinFitWidth->setDecimals(1);
    m_spinFitWidth->setValue(2.5);
    m_spinFitWidth->setFixedWidth(65);

    QPushButton *btnRunAutoTrace = new QPushButton("▶ Auto-Trace & Fit", controlsBox);
    btnRunAutoTrace->setStyleSheet(
        "QPushButton { background-color: #007acc; color: #ffffff; border: 1px solid #0098ff; "
        "border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 13px; } "
        "QPushButton:hover { background-color: #118ad4; }"
    );

    QPushButton *btnViewAllFits = new QPushButton("🔍 All Fits...", controlsBox);
    btnViewAllFits->setToolTip("Open dedicated visual inspection window showing all fitted peaks");
    btnViewAllFits->setStyleSheet(
        "QPushButton { background-color: #2e7d32; color: #ffffff; border: 1px solid #43a047; "
        "border-radius: 4px; padding: 6px 12px; font-weight: bold; font-size: 13px; } "
        "QPushButton:hover { background-color: #388e3c; }"
    );

    row2->addWidget(lblGain);
    row2->addWidget(m_spinInitGain);
    row2->addSpacing(6);
    row2->addWidget(lblOffset);
    row2->addWidget(m_spinInitOffset);
    row2->addSpacing(6);
    row2->addWidget(lblWindow);
    row2->addWidget(m_spinSearchWindow);
    row2->addSpacing(6);
    row2->addWidget(lblFitWidth);
    row2->addWidget(m_spinFitWidth);
    row2->addStretch(1);
    row2->addWidget(btnRunAutoTrace);
    row2->addSpacing(4);
    row2->addWidget(btnViewAllFits);
    controlsLayout->addLayout(row2);
    leftLayout->addWidget(controlsBox);

    // Table
    m_tablePeaks = new QTableWidget(leftPane);
    m_tablePeaks->setColumnCount(10);
    m_tablePeaks->setHorizontalHeaderLabels({
        "Use", "Ref (keV)", "Pred. Ch", "Centroid (ch)",
        "Err", "FWHM (keV)", "Area", "Recal (keV)", "ΔE (keV)", "Status"
    });
    m_tablePeaks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tablePeaks->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tablePeaks->verticalHeader()->setVisible(false);
    m_tablePeaks->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_tablePeaks, 1);
    tabTableLayout->addWidget(leftPane, 6);

    // Right pane: Live Peak Inspector
    QGroupBox *inspectorBox = new QGroupBox("🔍 Live Peak Fit Inspector", tabTable);
    QVBoxLayout *inspectorLayout = new QVBoxLayout(inspectorBox);
    inspectorLayout->setSpacing(8);
    inspectorLayout->setContentsMargins(8, 8, 8, 8);

    m_inspectorTile = new PeakFitTileWidget(-1, false, inspectorBox);
    m_inspectorTile->setShowFooter(false);
    inspectorLayout->addWidget(m_inspectorTile, 1);

    m_lblInspectorDetails = new QLabel(inspectorBox);
    m_lblInspectorDetails->setStyleSheet("color: #cccccc; font-size: 12px; background: #16181d; padding: 6px; border-radius: 4px;");
    m_lblInspectorDetails->setWordWrap(true);
    inspectorLayout->addWidget(m_lblInspectorDetails);

    m_btnToggleInclude = new QPushButton("Toggle Exclude / Include Peak", inspectorBox);
    m_btnToggleInclude->setStyleSheet("background-color: #3e3e42; font-size: 12px; padding: 6px 12px;");
    inspectorLayout->addWidget(m_btnToggleInclude);

    tabTableLayout->addWidget(inspectorBox, 4);

    tabWidget->addTab(tabTable, "📋 Table & Live Inspector");

    // =========================================================================
    // TAB 2: Multi-Peak Fit Grid (Xtrackn View)
    // =========================================================================
    QWidget *tabGrid = new QWidget();
    QVBoxLayout *tabGridLayout = new QVBoxLayout(tabGrid);
    tabGridLayout->setSpacing(6);
    tabGridLayout->setContentsMargins(8, 8, 8, 8);

    m_lblGridSummary = new QLabel("All Fitted Peaks Grid (Xtrackn View) &mdash; Click any tile to toggle its inclusion in the calibration polynomial:", tabGrid);
    m_lblGridSummary->setStyleSheet("color: #9cdcfe; font-size: 13px; font-weight: bold;");
    tabGridLayout->addWidget(m_lblGridSummary);

    m_gridScrollArea = new QScrollArea(tabGrid);
    m_gridScrollArea->setWidgetResizable(true);
    m_gridContainer = new QWidget(m_gridScrollArea);
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setSpacing(10);
    m_gridLayout->setContentsMargins(6, 6, 6, 6);
    m_gridScrollArea->setWidget(m_gridContainer);
    tabGridLayout->addWidget(m_gridScrollArea, 1);

    tabWidget->addTab(tabGrid, "👁 Multi-Peak Fit Grid (Xtrackn View)");

    // --- Regression Summary & Order Controls ---
    QGroupBox *summaryBox = new QGroupBox("Recalibration Results & Polynomial Equation", this);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryBox);
    summaryLayout->setSpacing(5);
    summaryLayout->setContentsMargins(10, 8, 10, 8);

    QHBoxLayout *sumRow1 = new QHBoxLayout();
    QLabel *lblOrder = new QLabel("Polynomial Order:", summaryBox);
    m_comboPolyOrder = new QComboBox(summaryBox);
    m_comboPolyOrder->addItems({"1st Order (Linear: A0, A1)",
                                "2nd Order (Quadratic: A0, A1, A2)",
                                "3rd Order (Cubic: A0, A1, A2, A3)"});
    m_comboPolyOrder->setCurrentIndex(1); // Default to quadratic

    m_lblRmsResidual = new QLabel("RMS Residual: - keV", summaryBox);
    m_lblRmsResidual->setStyleSheet("color: #4ec9b0; font-weight: bold; font-size: 13px;");

    sumRow1->addWidget(lblOrder);
    sumRow1->addWidget(m_comboPolyOrder);
    sumRow1->addSpacing(20);
    sumRow1->addWidget(m_lblRmsResidual);
    sumRow1->addStretch(1);
    summaryLayout->addLayout(sumRow1);

    m_lblEquation = new QLabel("<b>Energy Calibration:</b> <i>Run Auto-Trace to compute calibration.</i>", summaryBox);
    summaryLayout->addWidget(m_lblEquation);

    m_lblFwhmEquation = new QLabel("<b>Energy Resolution (FWHM):</b> <i>FWHM(E) = -</i>", summaryBox);
    m_lblFwhmEquation->setStyleSheet("color: #cccccc; font-size: 12px;");
    summaryLayout->addWidget(m_lblFwhmEquation);

    mainLayout->addWidget(summaryBox);

    // --- Bottom Action Buttons ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    QPushButton *btnApplyActive = new QPushButton("✓ Apply to Active Spectrum", this);
    btnApplyActive->setStyleSheet(
        "QPushButton { background-color: #007acc; color: #ffffff; border: 1px solid #0098ff; "
        "border-radius: 4px; padding: 7px 18px; font-weight: bold; font-size: 13px; min-height: 28px; }"
        "QPushButton:hover { background-color: #118ad4; }"
    );

    QPushButton *btnApplyAll = new QPushButton("Apply to All Open Spectra", this);
    btnApplyAll->setStyleSheet(
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; "
        "border-radius: 4px; padding: 7px 16px; font-weight: bold; font-size: 13px; min-height: 28px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
    );

    QPushButton *btnSave = new QPushButton("Save...", this);
    btnSave->setToolTip("Save calibrated parameters to a .cal or .mcal file.");

    QPushButton *btnClose = new QPushButton("Close", this);

    actionLayout->addWidget(btnApplyActive);
    actionLayout->addWidget(btnApplyAll);
    actionLayout->addWidget(btnSave);
    actionLayout->addStretch(1);
    actionLayout->addWidget(btnClose);
    mainLayout->addLayout(actionLayout);

    // --- Connections ---
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackFitDialog::onSourcePresetChanged);
    connect(btnAddLine, &QPushButton::clicked, this, &TrackFitDialog::onAddCustomLine);
    connect(btnRemoveLine, &QPushButton::clicked, this, &TrackFitDialog::onRemoveSelectedLine);
    connect(btnRunAutoTrace, &QPushButton::clicked, this, &TrackFitDialog::onAutoTraceClicked);
    connect(btnViewAllFits, &QPushButton::clicked, this, &TrackFitDialog::onViewAllFitsClicked);
    connect(m_comboPolyOrder, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackFitDialog::onPolyOrderChanged);
    connect(m_tablePeaks, &QTableWidget::itemChanged, this, &TrackFitDialog::onTableItemChanged);
    connect(m_tablePeaks, &QTableWidget::itemSelectionChanged, this, &TrackFitDialog::onTableSelectionChanged);
    connect(m_btnToggleInclude, &QPushButton::clicked, this, &TrackFitDialog::onToggleCurrentInclude);
    connect(btnApplyActive, &QPushButton::clicked, this, &TrackFitDialog::onApplyActiveClicked);
    connect(btnApplyAll, &QPushButton::clicked, this, &TrackFitDialog::onApplyAllClicked);
    connect(btnSave, &QPushButton::clicked, this, &TrackFitDialog::onSaveClicked);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    // Initialize presets
    initSourcePresets();
}

//==============================================================================
// initSourcePresets
//==============================================================================
void TrackFitDialog::initSourcePresets()
{
    m_comboPreset->blockSignals(true);
    m_comboPreset->addItem("152Eu (121.8, 244.7, 344.3, 778.9, 964.1, 1112.1, 1408.0 keV)", 0);
    m_comboPreset->addItem("60Co (1173.24, 1332.51 keV)", 1);
    m_comboPreset->addItem("137Cs (661.66 keV)", 2);
    m_comboPreset->addItem("133Ba (81.0, 160.6, 276.4, 302.9, 356.0, 383.9 keV)", 3);
    m_comboPreset->addItem("22Na (511.01, 1274.55 keV)", 4);
    m_comboPreset->addItem("56Co (846.8, 1238.3, 1771.4, 2034.8, 2598.5, 3253.4 keV)", 5);
    m_comboPreset->addItem("226Ra (186.2, 295.2, 351.9, 609.3, 1120.3, 1764.5 keV)", 6);
    m_comboPreset->addItem("241Am (26.35, 59.54 keV)", 7);
    m_comboPreset->addItem("Custom / User-Defined...", 8);
    m_comboPreset->blockSignals(false);

    // Default to 152Eu
    onSourcePresetChanged(0);
}

//==============================================================================
// onSourcePresetChanged
//==============================================================================
void TrackFitDialog::onSourcePresetChanged(int index)
{
    std::vector<ReferenceGamma> lines;
    switch (index) {
        case 0: // 152Eu
            lines = {
                {121.78, "152Eu 121.78", true},
                {244.70, "152Eu 244.70", true},
                {344.28, "152Eu 344.28", true},
                {778.90, "152Eu 778.90", true},
                {867.38, "152Eu 867.38", false},
                {964.08, "152Eu 964.08", true},
                {1085.87, "152Eu 1085.87", false},
                {1112.08, "152Eu 1112.08", true},
                {1408.01, "152Eu 1408.01", true}
            };
            break;
        case 1: // 60Co
            lines = {
                {1173.24, "60Co 1173.24", true},
                {1332.51, "60Co 1332.51", true}
            };
            break;
        case 2: // 137Cs
            lines = {
                {661.66, "137Cs 661.66", true}
            };
            break;
        case 3: // 133Ba
            lines = {
                {81.00, "133Ba 81.00", true},
                {160.61, "133Ba 160.61", true},
                {223.12, "133Ba 223.12", false},
                {276.40, "133Ba 276.40", true},
                {302.86, "133Ba 302.86", true},
                {356.01, "133Ba 356.01", true},
                {383.86, "133Ba 383.86", true}
            };
            break;
        case 4: // 22Na
            lines = {
                {511.006, "22Na 511.01", true},
                {1274.545, "22Na 1274.55", true}
            };
            break;
        case 5: // 56Co
            lines = {
                {846.77, "56Co 846.77", true},
                {1037.84, "56Co 1037.84", true},
                {1238.28, "56Co 1238.28", true},
                {1771.35, "56Co 1771.35", true},
                {2034.76, "56Co 2034.76", true},
                {2598.46, "56Co 2598.46", true},
                {3253.42, "56Co 3253.42", true}
            };
            break;
        case 6: // 226Ra
            lines = {
                {186.21, "226Ra 186.21", true},
                {295.21, "226Ra 295.21", true},
                {351.92, "226Ra 351.92", true},
                {609.31, "226Ra 609.31", true},
                {1120.29, "226Ra 1120.29", true},
                {1764.49, "226Ra 1764.49", true}
            };
            break;
        case 7: // 241Am
            lines = {
                {26.35, "241Am 26.35", true},
                {59.54, "241Am 59.54", true}
            };
            break;
        default:
            return;
    }

    updateReferenceList(lines);
}

//==============================================================================
// updateReferenceList
//==============================================================================
void TrackFitDialog::updateReferenceList(const std::vector<ReferenceGamma> &lines)
{
    m_currentRefLines = lines;
    m_peakResults.clear();

    const double gain = m_spinInitGain->value();
    const double offset = m_spinInitOffset->value();

    for (const auto &ref : m_currentRefLines) {
        TrackFitPeakResult res;
        res.refEnergy = ref.energy;
        res.isIncluded = ref.enabled;
        res.expectedChannel = (gain > 1e-9) ? (ref.energy - offset) / gain : 0.0;
        res.status = "Pending";
        m_peakResults.push_back(res);
    }

    refreshTableDisplay();
    refreshGridTiles();
    updateInspectorView(0);
}

//==============================================================================
// onAddCustomLine
//==============================================================================
void TrackFitDialog::onAddCustomLine()
{
    bool ok = false;
    double energy = QInputDialog::getDouble(this, "Add Reference Gamma Line",
                                            "Gamma Energy (keV):", 500.0, 1.0, 20000.0, 2, &ok);
    if (!ok || energy <= 0.0) return;

    ReferenceGamma ref;
    ref.energy = energy;
    ref.label = QString("Custom %1").arg(energy, 0, 'f', 2);
    ref.enabled = true;

    m_currentRefLines.push_back(ref);
    std::sort(m_currentRefLines.begin(), m_currentRefLines.end(),
              [](const ReferenceGamma &a, const ReferenceGamma &b) { return a.energy < b.energy; });

    updateReferenceList(m_currentRefLines);
    m_comboPreset->setCurrentIndex(8); // Custom
}

//==============================================================================
// onRemoveSelectedLine
//==============================================================================
void TrackFitDialog::onRemoveSelectedLine()
{
    int row = m_tablePeaks->currentRow();
    if (row < 0 || row >= static_cast<int>(m_currentRefLines.size())) return;

    m_currentRefLines.erase(m_currentRefLines.begin() + row);
    updateReferenceList(m_currentRefLines);
    m_comboPreset->setCurrentIndex(8); // Custom
}

//==============================================================================
// refreshTableDisplay
//==============================================================================
void TrackFitDialog::refreshTableDisplay()
{
    m_tablePeaks->blockSignals(true);
    m_tablePeaks->setRowCount(static_cast<int>(m_peakResults.size()));

    for (int r = 0; r < static_cast<int>(m_peakResults.size()); ++r) {
        const auto &res = m_peakResults[r];

        // Column 0: Checkbox
        QTableWidgetItem *chkItem = new QTableWidgetItem();
        chkItem->setCheckState(res.isIncluded ? Qt::Checked : Qt::Unchecked);
        chkItem->setTextAlignment(Qt::AlignCenter);
        m_tablePeaks->setItem(r, 0, chkItem);

        // Column 1: Ref Energy (keV)
        QTableWidgetItem *itemRef = new QTableWidgetItem(QString::number(res.refEnergy, 'f', 2));
        itemRef->setTextAlignment(Qt::AlignCenter);
        itemRef->setFlags(itemRef->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 1, itemRef);

        // Column 2: Pred. Ch
        QTableWidgetItem *itemPred = new QTableWidgetItem(QString::number(res.expectedChannel, 'f', 1));
        itemPred->setTextAlignment(Qt::AlignCenter);
        itemPred->setFlags(itemPred->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 2, itemPred);

        // Column 3: Centroid (ch)
        QString cStr = (res.fittedCentroid > 0.0) ? QString::number(res.fittedCentroid, 'f', 2) : "-";
        QTableWidgetItem *itemCentroid = new QTableWidgetItem(cStr);
        itemCentroid->setTextAlignment(Qt::AlignCenter);
        itemCentroid->setFlags(itemCentroid->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 3, itemCentroid);

        // Column 4: Centroid Err
        QString errStr = (res.centroidErr > 0.0) ? QString::number(res.centroidErr, 'f', 3) : "-";
        QTableWidgetItem *itemErr = new QTableWidgetItem(errStr);
        itemErr->setTextAlignment(Qt::AlignCenter);
        itemErr->setFlags(itemErr->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 4, itemErr);

        // Column 5: FWHM (keV)
        QString fwhmStr = (res.fwhmEnergy > 0.0) ? QString::number(res.fwhmEnergy, 'f', 2) :
                          (res.fwhmChannel > 0.0) ? QString("%1 ch").arg(res.fwhmChannel, 0, 'f', 1) : "-";
        QTableWidgetItem *itemFwhm = new QTableWidgetItem(fwhmStr);
        itemFwhm->setTextAlignment(Qt::AlignCenter);
        itemFwhm->setFlags(itemFwhm->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 5, itemFwhm);

        // Column 6: Net Area
        QString areaStr = (res.netArea > 0.0) ? QString::number(res.netArea, 'f', 0) : "-";
        QTableWidgetItem *itemArea = new QTableWidgetItem(areaStr);
        itemArea->setTextAlignment(Qt::AlignCenter);
        itemArea->setFlags(itemArea->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 6, itemArea);

        // Column 7: Recalib (keV)
        QString calcStr = (res.calcEnergy > 0.0) ? QString::number(res.calcEnergy, 'f', 2) : "-";
        QTableWidgetItem *itemCalc = new QTableWidgetItem(calcStr);
        itemCalc->setTextAlignment(Qt::AlignCenter);
        itemCalc->setFlags(itemCalc->flags() & ~Qt::ItemIsEditable);
        m_tablePeaks->setItem(r, 7, itemCalc);

        // Column 8: Residual Delta E (keV)
        QString resStr = (res.fittedCentroid > 0.0 && res.calcEnergy > 0.0) ?
                             QString("%1%2").arg(res.residualEnergy >= 0 ? "+" : "").arg(res.residualEnergy, 0, 'f', 3) : "-";
        QTableWidgetItem *itemRes = new QTableWidgetItem(resStr);
        itemRes->setTextAlignment(Qt::AlignCenter);
        itemRes->setFlags(itemRes->flags() & ~Qt::ItemIsEditable);
        if (std::abs(res.residualEnergy) > 1.0) {
            itemRes->setForeground(QColor("#f48771"));
        } else if (res.fittedCentroid > 0.0) {
            itemRes->setForeground(QColor("#4ec9b0"));
        }
        m_tablePeaks->setItem(r, 8, itemRes);

        // Column 9: Status
        QTableWidgetItem *itemStat = new QTableWidgetItem(res.status);
        itemStat->setTextAlignment(Qt::AlignCenter);
        itemStat->setFlags(itemStat->flags() & ~Qt::ItemIsEditable);
        if (res.status.startsWith("✓")) {
            itemStat->setForeground(QColor("#4ec9b0"));
        } else if (res.status.contains("Low") || res.status.contains("Out")) {
            itemStat->setForeground(QColor("#f48771"));
        }
        m_tablePeaks->setItem(r, 9, itemStat);
    }

    m_tablePeaks->blockSignals(false);
}

//==============================================================================
// onTableSelectionChanged
//==============================================================================
void TrackFitDialog::onTableSelectionChanged()
{
    int row = m_tablePeaks->currentRow();
    if (row >= 0 && row < static_cast<int>(m_peakResults.size())) {
        updateInspectorView(row);
    }
}

//==============================================================================
// updateInspectorView
//==============================================================================
void TrackFitDialog::updateInspectorView(int row)
{
    if (row < 0 || row >= static_cast<int>(m_peakResults.size())) {
        m_inspectorTile->clearData();
        m_lblInspectorDetails->setText("No peak selected.");
        return;
    }

    m_selectedPeakIndex = row;
    const auto &res = m_peakResults[row];
    m_inspectorTile->setPeakData(res, row);

    QString statusColor = res.isIncluded ? (res.fittedCentroid > 0 ? "#4ec9b0" : "#f48771") : "#888888";
    QString details = QString(
        "<b>Peak:</b> %1 keV &nbsp;|&nbsp; <b>Status:</b> <span style='color:%2;'>%3</span><br>"
        "<b>Predicted Ch:</b> %4 &nbsp;|&nbsp; <b>Fitted Centroid:</b> %5<br>"
        "<b>FWHM:</b> %6 keV (%7 ch) &nbsp;|&nbsp; <b>Net Area:</b> %8 counts<br>"
        "<b>Recalibrated Energy:</b> %9 keV &nbsp;|&nbsp; <b>Residual ΔE:</b> %10 keV")
        .arg(res.refEnergy, 0, 'f', 2)
        .arg(statusColor)
        .arg(res.status)
        .arg(res.expectedChannel, 0, 'f', 1)
        .arg(res.fittedCentroid > 0 ? QString("%1 ± %2").arg(res.fittedCentroid, 0, 'f', 2).arg(res.centroidErr, 0, 'f', 3) : "None")
        .arg(res.fwhmEnergy > 0 ? QString::number(res.fwhmEnergy, 'f', 2) : "-")
        .arg(res.fwhmChannel > 0 ? QString::number(res.fwhmChannel, 'f', 1) : "-")
        .arg(res.netArea > 0 ? QString::number(res.netArea, 'f', 0) : "-")
        .arg(res.calcEnergy > 0 ? QString::number(res.calcEnergy, 'f', 2) : "-")
        .arg(res.fittedCentroid > 0 ? QString("%1%2").arg(res.residualEnergy >= 0 ? "+" : "").arg(res.residualEnergy, 0, 'f', 3) : "-");

    m_lblInspectorDetails->setText(details);

    if (res.isIncluded) {
        m_btnToggleInclude->setText("❌ Exclude This Peak From Fit");
        m_btnToggleInclude->setStyleSheet("background-color: #4a2222; color: #f48771; font-weight: bold; border: 1px solid #803030; padding: 6px;");
    } else {
        m_btnToggleInclude->setText("✔ Include This Peak In Fit");
        m_btnToggleInclude->setStyleSheet("background-color: #224a33; color: #4ec9b0; font-weight: bold; border: 1px solid #308040; padding: 6px;");
    }
}

//==============================================================================
// onToggleCurrentInclude
//==============================================================================
void TrackFitDialog::onToggleCurrentInclude()
{
    if (m_selectedPeakIndex >= 0 && m_selectedPeakIndex < static_cast<int>(m_peakResults.size())) {
        onTileClicked(m_selectedPeakIndex);
    }
}

//==============================================================================
// refreshGridTiles
//==============================================================================
void TrackFitDialog::refreshGridTiles()
{
    // Clear existing layout
    for (auto *tile : m_gridTiles) {
        m_gridLayout->removeWidget(tile);
        delete tile;
    }
    m_gridTiles.clear();

    const int nPeaks = static_cast<int>(m_peakResults.size());
    const int cols = 3; // 3 columns grid matching Xtrackn

    int includedCount = 0;
    for (int i = 0; i < nPeaks; ++i) {
        PeakFitTileWidget *tile = new PeakFitTileWidget(i, true, m_gridContainer);
        tile->setPeakData(m_peakResults[i], i);
        connect(tile, &PeakFitTileWidget::tileClicked, this, &TrackFitDialog::onTileClicked);

        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(tile, row, col);
        m_gridTiles.push_back(tile);

        if (m_peakResults[i].isIncluded) includedCount++;
    }

    m_lblGridSummary->setText(
        QString("All Fitted Peaks Grid (Xtrackn View) &mdash; %1 peaks loaded (%2 included in fit). Click any tile to toggle:")
            .arg(nPeaks).arg(includedCount));
}

//==============================================================================
// onTileClicked
//==============================================================================
void TrackFitDialog::onTileClicked(int peakIndex)
{
    if (peakIndex < 0 || peakIndex >= static_cast<int>(m_peakResults.size())) return;

    m_peakResults[peakIndex].isIncluded = !m_peakResults[peakIndex].isIncluded;
    if (!m_peakResults[peakIndex].isIncluded) {
        m_peakResults[peakIndex].status = "Excluded";
    } else if (m_peakResults[peakIndex].fittedCentroid > 0.0) {
        m_peakResults[peakIndex].status = "✓ Matched";
    }

    performPolynomialFit();
    refreshTableDisplay();
    updateInspectorView(peakIndex);

    // Update the specific tile in the grid
    if (peakIndex < static_cast<int>(m_gridTiles.size())) {
        m_gridTiles[peakIndex]->setPeakData(m_peakResults[peakIndex], peakIndex);
    }
}

//==============================================================================
// fitPeakAtChannel
//==============================================================================
bool TrackFitDialog::fitPeakAtChannel(double expectedCh, double searchTolCh, double fitWindowFwhm,
                                      TrackFitPeakResult &outRes)
{
    if (!m_activeHist) return false;

    const int nBins = m_activeHist->GetNbinsX();
    int minScan = std::max(1, static_cast<int>(std::floor(expectedCh - searchTolCh)));
    int maxScan = std::min(nBins, static_cast<int>(std::ceil(expectedCh + searchTolCh)));
    if (minScan >= maxScan) return false;

    // 1. Locate local maximum bin within search window
    int peakBin = minScan;
    double maxVal = m_activeHist->GetBinContent(peakBin);
    for (int b = minScan + 1; b <= maxScan; ++b) {
        double val = m_activeHist->GetBinContent(b);
        if (val > maxVal) {
            maxVal = val;
            peakBin = b;
        }
    }

    if (maxVal <= 0.0) return false;

    double leftVal = m_activeHist->GetBinContent(minScan);
    double rightVal = m_activeHist->GetBinContent(maxScan);
    double baseline = std::min(leftVal, rightVal);
    double amplitude = maxVal - baseline;
    if (amplitude < 5.0) return false;

    // 2. Define fit boundary around peak
    double initialSigma = 2.5;
    double halfFitWidth = std::max(6.0, fitWindowFwhm * initialSigma * 1.5);

    int xLow = std::max(1, static_cast<int>(std::round(peakBin - halfFitWidth)));
    int xHigh = std::min(nBins, static_cast<int>(std::round(peakBin + halfFitWidth)));
    if (xHigh - xLow < 6) return false;

    // Display slice: show 3 times more bins in the visual inspection dialog & inspector
    double halfDispWidth = halfFitWidth * 3.0;
    int dispLow = std::max(1, static_cast<int>(std::round(peakBin - halfDispWidth)));
    int dispHigh = std::min(nBins, static_cast<int>(std::round(peakBin + halfDispWidth)));

    outRes.fitStartBin = dispLow;
    outRes.fitEndBin = dispHigh;
    outRes.localCounts.clear();
    for (int b = dispLow; b <= dispHigh; ++b) {
        outRes.localCounts.push_back(m_activeHist->GetBinContent(b));
    }

    // 3. Construct TF1 Gaussian + linear background centered at peakBin
    // Formula: [0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])
    // Parameter [3] is the baseline level directly AT centroid [1].
    TF1 gfit("trackfit_gaus", "[0]*exp(-0.5*((x-[1])/[2])^2) + [3] + [4]*(x-[1])", xLow, xHigh);
    gfit.SetParameter(0, amplitude);
    gfit.SetParameter(1, static_cast<double>(peakBin));
    gfit.SetParameter(2, initialSigma);
    gfit.SetParameter(3, baseline);
    double initSlope = (rightVal - leftVal) / std::max(1, maxScan - minScan);
    gfit.SetParameter(4, initSlope);

    gfit.SetParLimits(0, 0.0, maxVal * 2.5);
    gfit.SetParLimits(1, xLow, xHigh);
    gfit.SetParLimits(2, 0.6, 25.0);
    gfit.SetParLimits(3, 0.0, maxVal);

    int fitStatus = m_activeHist->Fit(&gfit, "QRN0");
    if (fitStatus != 0) {
        // Fallback to pure Gaussian
        TF1 pureGaus("trackfit_pure_gaus", "gaus", xLow, xHigh);
        pureGaus.SetParameter(0, amplitude);
        pureGaus.SetParameter(1, static_cast<double>(peakBin));
        pureGaus.SetParameter(2, initialSigma);
        pureGaus.SetParLimits(2, 0.6, 25.0);
        if (m_activeHist->Fit(&pureGaus, "QRN0") != 0) {
            return false;
        }
        outRes.fittedCentroid = pureGaus.GetParameter(1);
        outRes.centroidErr = pureGaus.GetParError(1);
        double sig = std::abs(pureGaus.GetParameter(2));
        outRes.fwhmChannel = sig * 2.354820;
        outRes.netArea = pureGaus.GetParameter(0) * sig * std::sqrt(2.0 * M_PI);
        outRes.netAreaErr = std::sqrt(std::max(1.0, outRes.netArea));
        outRes.fitAmplitude = pureGaus.GetParameter(0);
        outRes.fitSigma = sig;
        outRes.fitBaselineOffset = baseline;
        outRes.fitBaselineSlope = 0.0;
        return (outRes.fittedCentroid >= xLow && outRes.fittedCentroid <= xHigh);
    }

    outRes.fittedCentroid = gfit.GetParameter(1);
    outRes.centroidErr = gfit.GetParError(1);
    double sig = std::abs(gfit.GetParameter(2));
    outRes.fwhmChannel = sig * 2.354820;
    outRes.netArea = gfit.GetParameter(0) * sig * std::sqrt(2.0 * M_PI);
    outRes.netAreaErr = std::sqrt(std::max(1.0, outRes.netArea));

    outRes.fitAmplitude = gfit.GetParameter(0);
    outRes.fitSigma = sig;
    outRes.fitBaselineOffset = gfit.GetParameter(3);
    outRes.fitBaselineSlope = gfit.GetParameter(4);

    return (outRes.fittedCentroid >= xLow && outRes.fittedCentroid <= xHigh &&
            outRes.fwhmChannel > 0.8 && outRes.fwhmChannel < 50.0);
}

//==============================================================================
// onAutoTraceClicked
//==============================================================================
void TrackFitDialog::onAutoTraceClicked()
{
    if (!m_activeHist) {
        QMessageBox::warning(this, "AutoTrace", "No active spectrum loaded.");
        return;
    }

    const double initGain = m_spinInitGain->value();
    const double initOffset = m_spinInitOffset->value();
    const double tolCh = m_spinSearchWindow->value();
    const double fitWidth = m_spinFitWidth->value();

    int matchedCount = 0;

    // Pass 1: Search using initial gain & offset
    for (auto &res : m_peakResults) {
        if (!res.isIncluded) {
            res.status = "Excluded";
            continue;
        }

        res.expectedChannel = (initGain > 1e-9) ? (res.refEnergy - initOffset) / initGain : 0.0;
        if (res.expectedChannel < 1.0 || res.expectedChannel > m_activeHist->GetNbinsX()) {
            res.status = "Out of Range";
            res.fittedCentroid = 0.0;
            res.localCounts.clear();
            continue;
        }

        if (fitPeakAtChannel(res.expectedChannel, tolCh, fitWidth, res)) {
            res.status = "✓ Matched";
            matchedCount++;
        } else {
            res.fittedCentroid = 0.0;
            res.centroidErr = 0.0;
            res.fwhmChannel = 0.0;
            res.netArea = 0.0;
            res.status = "Low Counts";
        }
    }

    // Pass 2: Iterative Trajectory Refinement if at least 2 peaks matched
    if (matchedCount >= 2) {
        double sumC = 0.0, sumE = 0.0, sumCC = 0.0, sumCE = 0.0;
        int mN = 0;
        for (const auto &res : m_peakResults) {
            if (res.isIncluded && res.fittedCentroid > 0.0) {
                sumC += res.fittedCentroid;
                sumE += res.refEnergy;
                sumCC += res.fittedCentroid * res.fittedCentroid;
                sumCE += res.fittedCentroid * res.refEnergy;
                mN++;
            }
        }
        double denom = mN * sumCC - sumC * sumC;
        if (std::abs(denom) > 1e-9) {
            double refinedGain = (mN * sumCE - sumC * sumE) / denom;
            double refinedOffset = (sumE - refinedGain * sumC) / mN;

            if (refinedGain > 0.01) {
                // Re-predict and fit any missed peaks with refined gain & offset
                for (auto &res : m_peakResults) {
                    if (!res.isIncluded) continue;
                    double refinedCh = (res.refEnergy - refinedOffset) / refinedGain;
                    if (refinedCh >= 1.0 && refinedCh <= m_activeHist->GetNbinsX()) {
                        res.expectedChannel = refinedCh;
                        if (res.fittedCentroid <= 0.0) {
                            double pass2Tol = std::min(tolCh, 18.0);
                            if (fitPeakAtChannel(refinedCh, pass2Tol, fitWidth, res)) {
                                res.status = "✓ Matched";
                                matchedCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    if (matchedCount < 2) {
        QMessageBox::warning(this, "AutoTrace",
            QString("Only %1 peak(s) were successfully matched. At least 2 are required for calibration.\n"
                    "Try adjusting the Initial Gain or Search Window tolerance.")
                .arg(matchedCount));
    }

    performPolynomialFit();
    refreshTableDisplay();
    refreshGridTiles();
    updateInspectorView(m_selectedPeakIndex);

    // Automatically pop up the dedicated All-Fits Visual Inspection Dialog!
    if (matchedCount > 0) {
        onViewAllFitsClicked();
    }
}

//==============================================================================
// onViewAllFitsClicked
//==============================================================================
void TrackFitDialog::onViewAllFitsClicked()
{
    int polyOrder = m_comboPolyOrder->currentIndex() + 1;
    TrackFitInspectionDialog dlg(m_peakResults, polyOrder, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_peakResults = dlg.getResults();
        m_comboPolyOrder->setCurrentIndex(dlg.getPolyOrder() - 1);
        performPolynomialFit();
        refreshTableDisplay();
        refreshGridTiles();
        updateInspectorView(m_selectedPeakIndex);

        if (dlg.isAccepted()) {
            onApplyActiveClicked();
        }
    } else {
        m_peakResults = dlg.getResults();
        m_comboPolyOrder->setCurrentIndex(dlg.getPolyOrder() - 1);
        performPolynomialFit();
        refreshTableDisplay();
        refreshGridTiles();
        updateInspectorView(m_selectedPeakIndex);
    }
}

//==============================================================================
// performPolynomialFit
//==============================================================================
void TrackFitDialog::performPolynomialFit()
{
    std::vector<double> chs, ens;
    for (const auto &res : m_peakResults) {
        if (res.isIncluded && res.fittedCentroid > 0.0 && res.refEnergy > 0.0) {
            chs.push_back(res.fittedCentroid);
            ens.push_back(res.refEnergy);
        }
    }

    const int N = static_cast<int>(chs.size());
    const int reqOrder = m_comboPolyOrder->currentIndex() + 1; // 1, 2, or 3
    const int order = std::min(reqOrder, std::max(1, N - 1));

    if (N < 2) {
        m_lblEquation->setText("<b>Energy Calibration:</b> <i>Need at least 2 valid matched peaks.</i>");
        m_lblRmsResidual->setText("RMS Residual: -");
        m_lblFwhmEquation->setText("<b>Energy Resolution (FWHM):</b> <i>FWHM(E) = -</i>");
        m_fitSuccess = false;
        return;
    }

    if (!TrackFitMath::solvePolynomial(chs, ens, order, m_calibCoeffs)) {
        m_lblEquation->setText("<span style='color:#f48771;'>Fit Error: Singular matrix.</span>");
        m_fitSuccess = false;
        return;
    }

    m_fitSuccess = true;
    TrackFitMath::evaluateResiduals(m_peakResults, m_calibCoeffs, m_rmsResidualKeV);
    TrackFitMath::fitFwhmRelation(m_peakResults, m_fwhmIntercept, m_fwhmSlope);

    m_lblEquation->setText(TrackFitMath::formatEquation(m_calibCoeffs, order, N));
    m_lblRmsResidual->setText(QString("RMS Residual: %1 keV").arg(m_rmsResidualKeV, 0, 'f', 4));
    m_lblFwhmEquation->setText(
        QString("<b>Energy Resolution (FWHM):</b> <i>FWHM(E) = %1 + %2 &middot; E (keV)</i>")
            .arg(m_fwhmIntercept, 0, 'f', 3).arg(m_fwhmSlope, 0, 'g', 4));
}

//==============================================================================
// onPolyOrderChanged
//==============================================================================
void TrackFitDialog::onPolyOrderChanged(int)
{
    performPolynomialFit();
    refreshTableDisplay();
    refreshGridTiles();
    updateInspectorView(m_selectedPeakIndex);
}

//==============================================================================
// onTableItemChanged
//==============================================================================
void TrackFitDialog::onTableItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 0) return;
    int row = item->row();
    if (row < 0 || row >= static_cast<int>(m_peakResults.size())) return;

    bool checked = (item->checkState() == Qt::Checked);
    m_peakResults[row].isIncluded = checked;
    if (!checked) {
        m_peakResults[row].status = "Excluded";
    } else if (m_peakResults[row].fittedCentroid > 0.0) {
        m_peakResults[row].status = "✓ Matched";
    }

    performPolynomialFit();
    refreshTableDisplay();
    refreshGridTiles();
    updateInspectorView(row);
}

//==============================================================================
// onApplyActiveClicked
//==============================================================================
void TrackFitDialog::onApplyActiveClicked()
{
    if (!m_fitSuccess || m_calibCoeffs.empty()) {
        QMessageBox::warning(this, "Apply Calibration", "No valid calibration parameters available to apply.");
        return;
    }

    CalibSegment seg;
    seg.maxChannel = 1e9;
    seg.coeffs = m_calibCoeffs;

    m_activeHist->SetSegmentedCalibration({seg});

    const int sel_i = m_mainCanvas->SelectedElement_i;
    const int sel_j = m_mainCanvas->SelectedElement_j;
    m_mainCanvas->renderPeakLabels(sel_i, sel_j);
    m_mainCanvas->renderPeakSearchLabels(sel_i, sel_j);
    m_mainCanvas->updateAxisStatusLabels();
    m_mainCanvas->getRootCanvas()->getCanvas()->Update();

    QString logMsg = QString("AutoTrace / TrackFit recalibration applied to pad (%1, %2): RMS error = %3 keV\n")
                         .arg(sel_i).arg(sel_j).arg(m_rmsResidualKeV, 0, 'f', 4);
    CommandPrompt::getInstance()->appendPlainText(logMsg);
    std::cout << logMsg.toStdString();

    QMessageBox::information(this, "AutoTrace / TrackFit", "Calibration successfully applied to active spectrum.");
}

//==============================================================================
// onApplyAllClicked
//==============================================================================
void TrackFitDialog::onApplyAllClicked()
{
    if (!m_fitSuccess || m_calibCoeffs.empty()) {
        QMessageBox::warning(this, "Apply Calibration", "No valid calibration parameters available to apply.");
        return;
    }

    CalibSegment seg;
    seg.maxChannel = 1e9;
    seg.coeffs = m_calibCoeffs;

    for (int z = 1; z <= m_mainCanvas->maxElement_i; ++z) {
        for (int g = 1; g <= m_mainCanvas->maxElement_j; ++g) {
            TracknHistogram *h = dynamic_cast<TracknHistogram*>(m_mainCanvas->HijF[z][g]);
            if (!h) continue;
            h->SetSegmentedCalibration({seg});
            m_mainCanvas->renderPeakLabels(z, g);
            m_mainCanvas->renderPeakSearchLabels(z, g);
        }
    }

    m_mainCanvas->updateAxisStatusLabels();
    m_mainCanvas->getRootCanvas()->getCanvas()->Update();

    CommandPrompt::getInstance()->appendPlainText("AutoTrace / TrackFit calibration applied to all open spectra.\n");
    QMessageBox::information(this, "AutoTrace / TrackFit", "Calibration successfully applied to all open spectra.");
}

//==============================================================================
// onSaveClicked
//==============================================================================
void TrackFitDialog::onSaveClicked()
{
    if (!m_fitSuccess || m_calibCoeffs.empty()) {
        QMessageBox::warning(this, "Save Calibration", "No valid calibration parameters to save.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(
        this, "Save Calibration File", "trackfit_calibration.cal",
        "Calibration Table (*.cal);;GASP Multi-detector (*.mcal);;All Files (*)");
    if (savePath.isEmpty()) return;

    bool isMcal = savePath.endsWith(".mcal", Qt::CaseInsensitive);
    CalibDetector det;
    det.group = 1;
    det.detectorId = m_currentDetId;
    CalibSegment seg;
    seg.maxChannel = 1e9;
    seg.coeffs = m_calibCoeffs;
    det.segments.push_back(seg);

    if (SaveCalibrationFile(savePath, {det}, isMcal)) {
        QMessageBox::information(this, "Save Calibration", "Calibration successfully saved to:\n" + savePath);
    } else {
        QMessageBox::critical(this, "Save Calibration", "Failed to save calibration file.");
    }
}
