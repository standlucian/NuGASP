#include "canvas.h"
#include "tracknhistogram.h"
#include "Design.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>

//==============================================================================
// QZoomHUD Implementation
//==============================================================================
// Semi-transparent HUD overlay appearing in the top-right corner of the canvas
// when CTRL is held down. Previews +/- 25 channels around the cursor with a
// target reticle, counts, and calibrated energy (if available).
//==============================================================================
QZoomHUD::QZoomHUD(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint),
      m_targetBin(0),
      m_targetCounts(0.0),
      m_targetEnergy(-1.0),
      m_isCalibrated(false),
      m_startBin(0),
      m_endBin(0),
      m_maxCount(1.0),
      m_hasFit(false)
{
    setFixedSize(320, 195);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    hide();
}

void QZoomHUD::updateData(TH1F *hist, int targetBin, double energy, bool isCalibrated,
                         const std::vector<double> &fitCurve,
                         const std::vector<double> &bkgCurve,
                         bool hasFit,
                         const QString &fitInfo)
{
    if (!hist || targetBin < 1 || targetBin > hist->GetNbinsX()) {
        m_targetBin = 0;
        m_counts.clear();
        m_fitCurve.clear();
        m_bkgCurve.clear();
        m_hasFit = false;
        m_fitInfo.clear();
        update();
        return;
    }

    m_targetBin = targetBin;
    m_targetCounts = hist->GetBinContent(targetBin);
    m_targetEnergy = energy;
    m_isCalibrated = isCalibrated;

    const int nBins = hist->GetNbinsX();
    const int halfRange = 25; // +/- 25 channels around target
    m_startBin = std::max(1, targetBin - halfRange);
    m_endBin = std::min(nBins, targetBin + halfRange);

    m_counts.clear();
    m_maxCount = 1.0;
    for (int b = m_startBin; b <= m_endBin; ++b) {
        double c = hist->GetBinContent(b);
        m_counts.push_back(c);
        if (c > m_maxCount) {
            m_maxCount = c;
        }
    }

    m_fitCurve = fitCurve;
    m_bkgCurve = bkgCurve;
    m_hasFit = hasFit;
    m_fitInfo = fitInfo;

    // Ensure m_maxCount accommodates the peak of the fit curve so it is never clipped
    for (double fv : m_fitCurve) {
        if (fv > m_maxCount) {
            m_maxCount = fv;
        }
    }

    update();
}

void QZoomHUD::paintEvent(QPaintEvent *)
{
    if (m_counts.empty() || m_targetBin <= 0) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // 1. Semi-transparent dark rounded container
    QRectF bgRect(1.0, 1.0, w - 2.0, h - 2.0);
    painter.setPen(QPen(QColor(51, 153, 255, 220), 1.5));
    painter.setBrush(QColor(22, 24, 29));
    painter.drawRoundedRect(bgRect, 8.0, 8.0);

    // 2. Top Header / Target readout
    painter.setPen(QColor(255, 255, 255, 220));
    QFont headerFont = Design::getGraphFont();
    headerFont.setPointSize(10);
    headerFont.setBold(true);
    painter.setFont(headerFont);
    painter.drawText(QRectF(10, 8, 140, 20), Qt::AlignLeft | Qt::AlignVCenter, "🎯 AutoFit Target");

    // Fit status badge in top right of header
    if (m_hasFit && !m_fitInfo.isEmpty()) {
        QFont badgeFont = Design::getGraphFont();
        badgeFont.setPointSize(8);
        badgeFont.setBold(true);
        painter.setFont(badgeFont);
        QFontMetrics fm(badgeFont);
        int badgeTextW = fm.horizontalAdvance(m_fitInfo);
        int badgeW = badgeTextW + 14;
        int badgeH = 18;
        QRectF badgeRect(w - badgeW - 12, 8, badgeW, badgeH);

        painter.setPen(QPen(QColor(255, 170, 0, 220), 1.2));
        painter.setBrush(QColor(255, 150, 0, 45));
        painter.drawRoundedRect(badgeRect, 4.0, 4.0);
        painter.setPen(QColor(255, 195, 50));
        painter.drawText(badgeRect, Qt::AlignCenter, m_fitInfo);
    }

    QFont readoutFont = Design::getGraphFont();
    readoutFont.setPointSize(9);
    painter.setFont(readoutFont);
    QString readout;
    if (m_isCalibrated && m_targetEnergy > 0.0) {
        readout = QString("Ch %1 (%2 keV) | %3 cts")
                      .arg(m_targetBin)
                      .arg(m_targetEnergy, 0, 'f', 2)
                      .arg(static_cast<long long>(std::round(m_targetCounts)));
    } else {
        readout = QString("Ch %1 | %2 cts")
                      .arg(m_targetBin)
                      .arg(static_cast<long long>(std::round(m_targetCounts)));
    }
    painter.setPen(QColor(0, 240, 255));
    painter.drawText(QRectF(10, 28, w - 20, 18), Qt::AlignLeft | Qt::AlignVCenter, readout);

    // 3. Plot Area
    const qreal plotLeft = 14.0;
    const qreal plotRight = w - 14.0;
    const qreal plotTop = 50.0;
    const qreal plotBottom = h - 26.0;
    const qreal plotW = plotRight - plotLeft;
    const qreal plotH = plotBottom - plotTop;

    // Dark grid background
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.setBrush(QColor(10, 12, 16, 200));
    QRectF plotBox(plotLeft, plotTop, plotW, plotH);
    painter.drawRect(plotBox);

    // Horizontal guide line at 50%
    painter.drawLine(QPointF(plotLeft, plotTop + plotH * 0.5), QPointF(plotRight, plotTop + plotH * 0.5));

    // 4. Render spectrum bars / polygon
    const int countBins = static_cast<int>(m_counts.size());
    if (countBins > 1 && m_maxCount > 0.0) {
        const qreal binW = plotW / static_cast<qreal>(countBins);

        QPolygonF poly;
        poly << QPointF(plotLeft, plotBottom);

        for (int i = 0; i < countBins; ++i) {
            qreal x1 = plotLeft + i * binW;
            qreal x2 = plotLeft + (i + 1) * binW;
            qreal normH = (m_counts[i] / m_maxCount) * (plotH - 4.0);
            qreal yTop = plotBottom - normH;

            poly << QPointF(x1, yTop);
            poly << QPointF(x2, yTop);
        }
        poly << QPointF(plotRight, plotBottom);

        // Fill area under spectrum
        QLinearGradient grad(0, plotTop, 0, plotBottom);
        grad.setColorAt(0.0, QColor(0, 200, 255, 120));
        grad.setColorAt(1.0, QColor(0, 100, 200, 25));
        painter.setPen(Qt::NoPen);
        painter.setBrush(grad);
        painter.drawPolygon(poly);

        // Outline spectrum trace
        painter.setPen(QPen(QColor(0, 255, 255, 240), 1.5));
        for (int i = 0; i < countBins; ++i) {
            qreal x1 = plotLeft + i * binW;
            qreal x2 = plotLeft + (i + 1) * binW;
            qreal normH = (m_counts[i] / m_maxCount) * (plotH - 4.0);
            qreal yTop = plotBottom - normH;
            painter.drawLine(QPointF(x1, yTop), QPointF(x2, yTop));
            if (i + 1 < countBins) {
                qreal nextNormH = (m_counts[i + 1] / m_maxCount) * (plotH - 4.0);
                qreal nextYTop = plotBottom - nextNormH;
                painter.drawLine(QPointF(x2, yTop), QPointF(x2, nextYTop));
            }
        }

        // 5. Render Background Line (if present)
        if (m_hasFit && !m_bkgCurve.empty()) {
            const int nSamples = static_cast<int>(m_bkgCurve.size());
            QPolygonF bkgPoly;
            painter.setPen(QPen(QColor(255, 210, 110, 180), 1.3, Qt::DashLine));
            for (int k = 0; k < nSamples; ++k) {
                if (m_bkgCurve[k] >= 0.0) {
                    qreal frac = static_cast<qreal>(k) / static_cast<qreal>(nSamples - 1);
                    qreal xVal = plotLeft + frac * plotW;
                    qreal normH = (m_bkgCurve[k] / m_maxCount) * (plotH - 4.0);
                    qreal yVal = plotBottom - normH;
                    bkgPoly << QPointF(xVal, yVal);
                } else if (!bkgPoly.isEmpty()) {
                    if (bkgPoly.size() >= 2) {
                        painter.drawPolyline(bkgPoly);
                    }
                    bkgPoly.clear();
                }
            }
            if (bkgPoly.size() >= 2) {
                painter.drawPolyline(bkgPoly);
            }
        }

        // 6. Render Fit Shape Curve (Gaussian + Background)
        if (m_hasFit && !m_fitCurve.empty()) {
            const int nSamples = static_cast<int>(m_fitCurve.size());
            QPolygonF fitPoly;
            painter.setPen(QPen(QColor(255, 160, 20, 255), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            for (int k = 0; k < nSamples; ++k) {
                if (m_fitCurve[k] >= 0.0) {
                    qreal frac = static_cast<qreal>(k) / static_cast<qreal>(nSamples - 1);
                    qreal xVal = plotLeft + frac * plotW;
                    qreal normH = (m_fitCurve[k] / m_maxCount) * (plotH - 4.0);
                    qreal yVal = plotBottom - normH;
                    fitPoly << QPointF(xVal, yVal);
                } else if (!fitPoly.isEmpty()) {
                    if (fitPoly.size() >= 2) {
                        painter.drawPolyline(fitPoly);
                    }
                    fitPoly.clear();
                }
            }
            if (fitPoly.size() >= 2) {
                painter.drawPolyline(fitPoly);
            }
        }

        // 7. Target Reticle (cursor crosshair)
        const int targetIndex = m_targetBin - m_startBin;
        if (targetIndex >= 0 && targetIndex < countBins) {
            qreal reticleX = plotLeft + (targetIndex + 0.5) * binW;
            painter.setPen(QPen(QColor(255, 70, 70, 240), 1.5, Qt::DashLine));
            painter.drawLine(QPointF(reticleX, plotTop), QPointF(reticleX, plotBottom));

            // Small red triangle marker at the top of the reticle
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 70, 70, 250));
            QPolygonF triangle;
            triangle << QPointF(reticleX, plotTop + 7.0)
                     << QPointF(reticleX - 4.0, plotTop)
                     << QPointF(reticleX + 4.0, plotTop);
            painter.drawPolygon(triangle);
        }
    }

    // 8. X-axis channel bounds labels
    QFont axisFont = Design::getGraphFont();
    axisFont.setPointSize(8);
    painter.setFont(axisFont);
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRectF(plotLeft, plotBottom + 2, 60, 18), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_startBin));
    painter.drawText(QRectF(plotRight - 60, plotBottom + 2, 60, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(m_endBin));

    // Center bin label in bright amber
    painter.setPen(QColor(255, 200, 80));
    painter.drawText(QRectF(plotLeft, plotBottom + 2, plotW, 18), Qt::AlignCenter, QString("[%1]").arg(m_targetBin));
}
