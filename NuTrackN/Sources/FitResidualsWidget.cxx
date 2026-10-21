#include "FitResidualsWidget.h"
#include "canvas.h"
#include "tracknhistogram.h"
#include "Design.h"

#include <TH1F.h>
#include <TF1.h>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>
#include <algorithm>

//==============================================================================
// QFitResidualsWidget Constructor
//==============================================================================
QFitResidualsWidget::QFitResidualsWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(110);
    setMaximumHeight(350);
    setMouseTracking(true);
    setupUI();
}

void QFitResidualsWidget::setupUI()
{
    QVBoxLayout *mainVBox = new QVBoxLayout(this);
    mainVBox->setContentsMargins(0, 0, 0, 0);
    mainVBox->setSpacing(0);

    // Header bar
    m_headerWidget = new QWidget(this);
    m_headerWidget->setFixedHeight(26);
    m_headerWidget->setStyleSheet("background-color: #21252b; border-bottom: 1px solid #3e4451;");

    QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 2, 8, 2);
    headerLayout->setSpacing(10);

    m_lblTitle = new QLabel(tr("📊 Fit Residuals"), m_headerWidget);
    m_lblTitle->setStyleSheet("color: #61afef; font-weight: bold; font-size: 11px;");
    headerLayout->addWidget(m_lblTitle);

    m_lblMetrics = new QLabel(m_headerWidget);
    m_lblMetrics->setStyleSheet("color: #abb2bf; font-family: monospace; font-size: 11px;");
    headerLayout->addWidget(m_lblMetrics, 1);

    m_btnMode = new QPushButton(tr("Mode: Pulls (σ)"), m_headerWidget);
    m_btnMode->setFixedHeight(20);
    m_btnMode->setStyleSheet(
        "QPushButton {"
        "  background-color: #2c313a; color: #98c379; border: 1px solid #3e4451;"
        "  border-radius: 3px; font-size: 10px; font-weight: bold; padding: 1px 8px;"
        "}"
        "QPushButton:hover { background-color: #3e4451; color: #ffffff; }"
    );
    connect(m_btnMode, &QPushButton::clicked, this, [this]() {
        if (m_mode == DisplayMode::Pulls) {
            setDisplayMode(DisplayMode::RawCounts);
        } else {
            setDisplayMode(DisplayMode::Pulls);
        }
    });
    headerLayout->addWidget(m_btnMode);

    m_btnClose = new QPushButton("✕", m_headerWidget);
    m_btnClose->setFixedSize(18, 18);
    m_btnClose->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent; color: #abb2bf; border: none;"
        "  font-weight: bold; font-size: 11px;"
        "}"
        "QPushButton:hover { color: #e06c75; }"
    );
    connect(m_btnClose, &QPushButton::clicked, this, [this]() {
        hide();
        emit closed();
    });
    headerLayout->addWidget(m_btnClose);

    mainVBox->addWidget(m_headerWidget);
    mainVBox->addStretch(1);
}

void QFitResidualsWidget::setDisplayMode(DisplayMode mode)
{
    m_mode = mode;
    if (m_btnMode) {
        m_btnMode->setText(m_mode == DisplayMode::Pulls ? tr("Mode: Pulls (σ)") : tr("Mode: Counts (cts)"));
        m_btnMode->setStyleSheet(
            m_mode == DisplayMode::Pulls
                ? "QPushButton { background-color: #2c313a; color: #98c379; border: 1px solid #3e4451; border-radius: 3px; font-size: 10px; font-weight: bold; padding: 1px 8px; }"
                : "QPushButton { background-color: #2c313a; color: #61afef; border: 1px solid #3e4451; border-radius: 3px; font-size: 10px; font-weight: bold; padding: 1px 8px; }"
        );
    }
    update();
}

void QFitResidualsWidget::updateHeaderReadout()
{
    if (m_lblTitle) {
        m_lblTitle->setText(QString("📊 %1").arg(m_fitTitle.isEmpty() ? tr("Fit Residuals") : m_fitTitle));
    }
    if (m_lblMetrics) {
        const QString redChi2Str = (m_redChi2 >= 1e4) ? QString::number(m_redChi2, 'g', 4) : QString::number(m_redChi2, 'f', 2);
        m_lblMetrics->setText(
            QString("χ²/ndf: <b style='color:#98c379;'>%1</b> | RMS: <b style='color:#61afef;'>%2 cts</b> | Max Pull: <b style='color:#e5c07b;'>%3σ</b> (%4 bins)")
                .arg(redChi2Str)
                .arg(m_rmsResidual, 0, 'f', 1)
                .arg(m_maxAbsPull, 0, 'f', 1)
                .arg(m_points.size())
        );
    }
}

void QFitResidualsWidget::updateResiduals(TH1F *hist, TF1 *fitFunc, double xMin, double xMax,
                                         const QString &title, double chi2, int ndf)
{
    if (!hist || !fitFunc || xMin >= xMax) {
        clearResiduals();
        return;
    }

    m_fitTitle = title.isEmpty() ? tr("Peak Fit Residuals") : title;
    m_chi2 = chi2;
    m_ndf = std::max(1, ndf);
    m_redChi2 = m_chi2 / m_ndf;
    m_xMin = xMin;
    m_xMax = xMax;

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    m_isCalibrated = (trackHist && trackHist->IsCalibrated());

    m_points.clear();
    m_maxAbsPull = 0.0;
    m_maxAbsRaw = 0.0;
    double sumSq = 0.0;
    double sumRel = 0.0;

    const int bStart = std::max(1, hist->FindBin(xMin));
    const int bEnd = std::min(hist->GetNbinsX(), hist->FindBin(xMax));

    for (int b = bStart; b <= bEnd; ++b) {
        const double ch = hist->GetBinCenter(b);
        if (ch < xMin || ch > xMax) continue;

        const double data = hist->GetBinContent(b);
        const double fitVal = fitFunc->Eval(ch);
        const double raw = data - fitVal;
        const double err = std::max(1.0, std::sqrt(std::max(0.0, data)));
        const double pull = raw / err;

        ResidualPoint pt;
        pt.channel = ch;
        pt.energy = m_isCalibrated ? trackHist->ChannelToEnergy(ch) : -1.0;
        pt.dataCount = data;
        pt.fitCount = fitVal;
        pt.rawResidual = raw;
        pt.error = err;
        pt.pull = pull;

        m_points.push_back(pt);

        m_maxAbsPull = std::max(m_maxAbsPull, std::abs(pull));
        m_maxAbsRaw = std::max(m_maxAbsRaw, std::abs(raw));
        sumSq += raw * raw;
        if (data > 0.0) sumRel += std::abs(raw) / data;
    }

    const int n = static_cast<int>(m_points.size());
    m_rmsResidual = (n > 0) ? std::sqrt(sumSq / n) : 0.0;
    m_meanRelPct = (n > 0) ? (sumRel / n * 100.0) : 0.0;

    updateHeaderReadout();
    show();
    update();
}

void QFitResidualsWidget::clearResiduals()
{
    m_points.clear();
    m_fitTitle.clear();
    m_maxAbsPull = 0.0;
    m_maxAbsRaw = 0.0;
    m_hoveredIndex = -1;
    updateHeaderReadout();
    update();
}

void QFitResidualsWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_points.empty() || !m_plotRect.contains(event->pos())) {
        m_hoveredIndex = -1;
        QToolTip::hideText();
        update();
        return;
    }

    const double mouseX = event->pos().x();
    const double plotW = m_plotRect.width();
    const double frac = std::clamp((mouseX - m_plotRect.left()) / plotW, 0.0, 1.0);
    const double targetCh = m_xMin + frac * (m_xMax - m_xMin);

    int bestIdx = -1;
    double bestDist = 1e9;
    for (size_t i = 0; i < m_points.size(); ++i) {
        double d = std::abs(m_points[i].channel - targetCh);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestIdx != m_hoveredIndex) {
        m_hoveredIndex = bestIdx;
        if (bestIdx >= 0 && bestIdx < static_cast<int>(m_points.size())) {
            const auto &pt = m_points[bestIdx];
            QString tipText;
            if (pt.energy > 0.0) {
                tipText = QString("Ch %1 (%2 keV)\nData: %3 cts\nFit: %4 cts\nResidual: %5%6 cts\nPull: %7%8 σ")
                              .arg(pt.channel, 0, 'f', 1)
                              .arg(pt.energy, 0, 'f', 2)
                              .arg(pt.dataCount, 0, 'f', 0)
                              .arg(pt.fitCount, 0, 'f', 1)
                              .arg(pt.rawResidual > 0 ? "+" : "")
                              .arg(pt.rawResidual, 0, 'f', 1)
                              .arg(pt.pull > 0 ? "+" : "")
                              .arg(pt.pull, 0, 'f', 2);
            } else {
                tipText = QString("Ch %1\nData: %2 cts\nFit: %3 cts\nResidual: %4%5 cts\nPull: %6%7 σ")
                              .arg(pt.channel, 0, 'f', 1)
                              .arg(pt.dataCount, 0, 'f', 0)
                              .arg(pt.fitCount, 0, 'f', 1)
                              .arg(pt.rawResidual > 0 ? "+" : "")
                              .arg(pt.rawResidual, 0, 'f', 1)
                              .arg(pt.pull > 0 ? "+" : "")
                              .arg(pt.pull, 0, 'f', 2);
            }
            QToolTip::showText(event->globalPos(), tipText, this);
        }
        update();
    }
}

void QFitResidualsWidget::leaveEvent(QEvent *)
{
    m_hoveredIndex = -1;
    QToolTip::hideText();
    update();
}

void QFitResidualsWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Fill overall background with theme-compliant dark tone
    painter.fillRect(rect(), QColor("#1e1e24"));

    // Margins around the plot canvas
    const qreal marginLeft = 48.0;
    const qreal marginRight = 16.0;
    const qreal marginTop = 30.0;
    const qreal marginBottom = 22.0;

    m_plotRect = QRectF(marginLeft, marginTop, w - marginLeft - marginRight, h - marginTop - marginBottom);
    if (m_plotRect.width() <= 10 || m_plotRect.height() <= 10) return;

    // Dark plot inner frame
    painter.fillRect(m_plotRect, QColor("#16181d"));
    painter.setPen(QPen(QColor("#3e4451"), 1.0));
    painter.drawRect(m_plotRect);

    if (m_points.empty()) {
        painter.setPen(QColor("#5c6370"));
        QFont emptyFont = font();
        emptyFont.setPointSize(10);
        painter.setFont(emptyFont);
        painter.drawText(m_plotRect, Qt::AlignCenter, tr("No fit residuals to display. Perform a peak fit or inspect a spectrum to view pulls."));
        return;
    }

    const qreal plotL = m_plotRect.left();
    const qreal plotR = m_plotRect.right();
    const qreal plotT = m_plotRect.top();
    const qreal plotB = m_plotRect.bottom();
    const qreal plotW = m_plotRect.width();
    const qreal plotH = m_plotRect.height();
    const qreal yZero = plotT + plotH / 2.0;

    // Determine Y limits
    double yLim = 3.5;
    if (m_mode == DisplayMode::Pulls) {
        yLim = std::max(3.5, std::ceil(m_maxAbsPull * 1.15));
    } else {
        yLim = std::max(5.0, m_maxAbsRaw * 1.15);
    }

    auto valToY = [&](double v) -> qreal {
        return yZero - (v / yLim) * (plotH / 2.0);
    };

    auto chToX = [&](double ch) -> qreal {
        if (m_xMax <= m_xMin) return plotL;
        return plotL + ((ch - m_xMin) / (m_xMax - m_xMin)) * plotW;
    };

    // Draw confidence bands for Pulls mode
    if (m_mode == DisplayMode::Pulls) {
        // +/- 1 sigma band (subtle emerald tint)
        qreal yP1 = valToY(1.0);
        qreal yM1 = valToY(-1.0);
        painter.fillRect(QRectF(plotL, yP1, plotW, yM1 - yP1), QColor(152, 195, 121, 20));

        // Dashed lines at +/- 1, 2, 3 sigma
        painter.setPen(QPen(QColor(152, 195, 121, 100), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(plotL, yP1), QPointF(plotR, yP1));
        painter.drawLine(QPointF(plotL, yM1), QPointF(plotR, yM1));

        qreal yP2 = valToY(2.0);
        qreal yM2 = valToY(-2.0);
        painter.setPen(QPen(QColor(229, 192, 123, 100), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(plotL, yP2), QPointF(plotR, yP2));
        painter.drawLine(QPointF(plotL, yM2), QPointF(plotR, yM2));

        if (yLim >= 3.0) {
            qreal yP3 = valToY(3.0);
            qreal yM3 = valToY(-3.0);
            painter.setPen(QPen(QColor(224, 108, 117, 100), 1.0, Qt::DashLine));
            painter.drawLine(QPointF(plotL, yP3), QPointF(plotR, yP3));
            painter.drawLine(QPointF(plotL, yM3), QPointF(plotR, yM3));
        }

        // Y-axis labels on the left margin
        QFont axisFont = font();
        axisFont.setPointSize(8);
        axisFont.setFamily("monospace");
        painter.setFont(axisFont);

        auto drawYLabel = [&](double v, const QString &txt, const QColor &c) {
            qreal y = valToY(v);
            painter.setPen(c);
            painter.drawText(QRectF(2, y - 7, marginLeft - 6, 14), Qt::AlignRight | Qt::AlignVCenter, txt);
        };

        drawYLabel(0.0, "0", QColor("#dcdfe4"));
        drawYLabel(1.0, "+1σ", QColor("#98c379"));
        drawYLabel(-1.0, "-1σ", QColor("#98c379"));
        drawYLabel(2.0, "+2σ", QColor("#e5c07b"));
        drawYLabel(-2.0, "-2σ", QColor("#e5c07b"));
        if (yLim >= 3.0) {
            drawYLabel(3.0, "+3σ", QColor("#e06c75"));
            drawYLabel(-3.0, "-3σ", QColor("#e06c75"));
        }
    } else {
        // Raw counts mode labels
        QFont axisFont = font();
        axisFont.setPointSize(8);
        axisFont.setFamily("monospace");
        painter.setFont(axisFont);

        auto drawYLabel = [&](double v, const QString &txt) {
            qreal y = valToY(v);
            painter.setPen(QColor("#abb2bf"));
            painter.drawText(QRectF(2, y - 7, marginLeft - 6, 14), Qt::AlignRight | Qt::AlignVCenter, txt);
        };
        drawYLabel(0.0, "0");
        drawYLabel(yLim * 0.8, QString("+%1").arg(static_cast<int>(yLim * 0.8)));
        drawYLabel(-yLim * 0.8, QString("-%1").arg(static_cast<int>(yLim * 0.8)));
    }

    // Zero reference line
    painter.setPen(QPen(QColor("#61afef"), 1.2));
    painter.drawLine(QPointF(plotL, yZero), QPointF(plotR, yZero));

    // Draw residual bars / stems
    const double binWidthPix = (m_points.size() > 1) ? (plotW / m_points.size()) : 10.0;
    const qreal barW = std::clamp(binWidthPix * 0.65, 2.0, 12.0);

    for (size_t i = 0; i < m_points.size(); ++i) {
        const auto &pt = m_points[i];
        const qreal x = chToX(pt.channel);
        const double val = (m_mode == DisplayMode::Pulls) ? pt.pull : pt.rawResidual;
        const qreal y = valToY(val);

        QColor barCol;
        if (m_mode == DisplayMode::Pulls) {
            const double absP = std::abs(pt.pull);
            if (absP <= 1.0) {
                barCol = QColor("#98c379"); // emerald
            } else if (absP <= 2.0) {
                barCol = QColor("#e5c07b"); // amber
            } else {
                barCol = QColor("#e06c75"); // coral red
            }
        } else {
            barCol = (pt.rawResidual >= 0.0) ? QColor("#61afef") : QColor("#e06c75");
        }

        if (static_cast<int>(i) == m_hoveredIndex) {
            barCol = QColor("#ffffff");
        }

        // Stem
        painter.setPen(QPen(barCol, std::clamp(barW, 1.5, 4.0)));
        painter.drawLine(QPointF(x, yZero), QPointF(x, y));

        // Dot at value
        painter.setBrush(barCol);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y), 2.5, 2.5);

        // Highlight ring on hover
        if (static_cast<int>(i) == m_hoveredIndex) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor("#ffffff"), 1.5));
            painter.drawEllipse(QPointF(x, y), 5.0, 5.0);
        }
    }

    // Draw X-axis tick labels at bottom
    QFont xFont = font();
    xFont.setPointSize(8);
    xFont.setFamily("monospace");
    painter.setFont(xFont);
    painter.setPen(QColor("#abb2bf"));

    const int nTicks = std::clamp(static_cast<int>(plotW / 80.0), 3, 9);
    for (int t = 0; t <= nTicks; ++t) {
        const double frac = static_cast<double>(t) / nTicks;
        const double ch = m_xMin + frac * (m_xMax - m_xMin);
        const qreal x = plotL + frac * plotW;

        // Tick mark
        painter.drawLine(QPointF(x, plotB), QPointF(x, plotB + 4));

        QString labelText = QString::number(ch, 'f', 1);
        if (m_isCalibrated && !m_points.empty()) {
            TracknHistogram *th = m_mainCanvas ? m_mainCanvas->getActiveTracknHistogram() : nullptr;
            if (th && th->IsCalibrated()) {
                double en = th->ChannelToEnergy(ch);
                labelText = QString("%1\n(%2)").arg(ch, 0, 'f', 0).arg(en, 0, 'f', 1);
            }
        }
        painter.drawText(QRectF(x - 30, plotB + 4, 60, 16), Qt::AlignCenter | Qt::AlignTop, labelText);
    }
}
