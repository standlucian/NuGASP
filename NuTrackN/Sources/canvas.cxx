#include "canvas.h"
#include "Design.h"
#include "PeakFit.h"
#include "tracknhistogram.h"
#include "SpectrumImportDialog.h"
#include "SpectrumExportDialog.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QLinearGradient>

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
    QFont headerFont("sans-serif", 10, QFont::Bold);
    painter.setFont(headerFont);
    painter.drawText(QRectF(10, 8, 140, 20), Qt::AlignLeft | Qt::AlignVCenter, "🎯 AutoFit Target");

    // Fit status badge in top right of header
    if (m_hasFit && !m_fitInfo.isEmpty()) {
        QFont badgeFont("sans-serif", 8, QFont::Bold);
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

    QFont readoutFont("sans-serif", 9, QFont::Normal);
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
    QFont axisFont("sans-serif", 8, QFont::Normal);
    painter.setFont(axisFont);
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRectF(plotLeft, plotBottom + 2, 60, 18), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_startBin));
    painter.drawText(QRectF(plotRight - 60, plotBottom + 2, 60, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(m_endBin));

    // Center bin label in bright amber
    painter.setPen(QColor(255, 200, 80));
    painter.drawText(QRectF(plotLeft, plotBottom + 2, plotW, 18), Qt::AlignCenter, QString("[%1]").arg(m_targetBin));
}

//==============================================================================
// QRootCanvas Constructor
//==============================================================================
// Initializes an embedded CERN ROOT TCanvas inside a Qt QWidget.
// Uses TVirtualX to register the native window ID, configuring mouse tracking,
// minimal padding, and an initial "NuTrackN" splash label.
//==============================================================================
QRootCanvas::QRootCanvas(QWidget *parent)
    : QWidget(parent),
      fCanvas(nullptr),
      xMousePosition(0),
      yMousePosition(0),
      controlKeyIsPressed(false),
      cKeyWasPressed(false),
      zKeyWasPressed(false),
      mKeyWasPressed(false),
      fKeyWasPressed(false),
      m_zoomHUD(new QZoomHUD(nullptr)),
      m_mainCanvas(nullptr)
{
    // Configure widget attributes for embedded ROOT TCanvas
    setAttribute(Qt::WA_PaintOnScreen, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NativeWindow, true);
    setUpdatesEnabled(kFALSE);
    setMouseTracking(kTRUE);
    setMinimumSize(300, 200);

    // Position and initialize zoom HUD
    m_zoomHUD->hide();

    // Install application event filter so Ctrl press is caught regardless of keyboard focus
    if (qApp) {
        qApp->installEventFilter(this);
    }

    // Register widget with TVirtualX using native window id
    int wid = gVirtualX->AddWindow((ULong_t)winId(), width(), height());
    fCanvas = new TCanvas("Root Canvas", width(), height(), wid);
    TQObject::Connect("TGPopupMenu", "PoppedDown()", "TCanvas", fCanvas, "Update()");

    // Set canvas borders and margins as small as possible for maximal spectrum viewing area
    Double_t canvasHeight = fCanvas->GetWh();
    Double_t proportion = (canvasHeight > 0.0) ? (0.1 / canvasHeight) : 0.01;
    gPad->SetMargin(proportion, proportion, proportion, proportion);

    // Initial splash text drawn in the center of the canvas
    setFocusPolicy(Qt::StrongFocus);
    TLatex l;
    l.SetTextSize(0.15);
    l.SetTextAlign(22);
    l.SetTextColor(kBlack);
    l.DrawLatex(0.5, 0.5, "NuTrackN");
}

QRootCanvas::~QRootCanvas()
{
    if (m_zoomHUD) {
        delete m_zoomHUD;
        m_zoomHUD = nullptr;
    }
}

bool QRootCanvas::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Control) {
            controlKeyIsPressed = true;
            QPoint localPos = mapFromGlobal(QCursor::pos());
            if (rect().contains(localPos)) {
                updateZoomHUD(localPos.x(), localPos.y());
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Control && !ke->isAutoRepeat()) {
            controlKeyIsPressed = false;
            hideZoomHUD();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void QRootCanvas::updateZoomHUD(int mouseX, int mouseY)
{
    if (!m_zoomHUD) return;
    if (!m_mainCanvas) {
        QWidget *p = parentWidget();
        while (p) {
            m_mainCanvas = qobject_cast<QMainCanvas*>(p);
            if (m_mainCanvas) break;
            p = p->parentWidget();
        }
    }
    if (!m_mainCanvas) return;

    TH1F *hist = m_mainCanvas->HijF[m_mainCanvas->SelectedElement_i][m_mainCanvas->SelectedElement_j];
    if (!hist) {
        m_zoomHUD->hide();
        return;
    }

    // Determine target channel at cursor position
    int binX = 0;
    if (fCanvas) {
        fCanvas->cd((m_mainCanvas->SelectedElement_i - 1) * m_mainCanvas->maxElement_j + m_mainCanvas->SelectedElement_j);
    }
    const std::string objectInfo = hist->GetObjectInfo(mouseX, mouseY);
    const size_t bPos = objectInfo.find("binx=");
    if (bPos != std::string::npos) {
        binX = std::atoi(objectInfo.c_str() + bPos + 5);
    }
    if (binX < 1 || binX > hist->GetNbinsX()) {
        binX = m_mainCanvas->getBinFromClick(mouseX, mouseY);
    }
    if (binX < 1 || binX > hist->GetNbinsX()) {
        if (width() > 0) {
            const double frac = static_cast<double>(mouseX) / width();
            binX = std::max(1, std::min(hist->GetNbinsX(), static_cast<int>(frac * hist->GetNbinsX())));
        }
    }
    if (binX < 1) binX = 1;

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(hist);
    bool isCalibrated = (trackHist && trackHist->IsCalibrated());
    double energy = isCalibrated ? trackHist->ChannelToEnergy(binX) : -1.0;

    // Window bin range
    const int halfRange = 25;
    const int startB = std::max(1, binX - halfRange);
    const int endB = std::min(hist->GetNbinsX(), binX + halfRange);

    // Collect all active fit and background functions for the current pad
    std::vector<TF1*> fitFuncs;
    std::vector<TF1*> bkgFuncs;

    int el_i = m_mainCanvas->SelectedElement_i;
    int el_j = m_mainCanvas->SelectedElement_j;
    for (TObject *obj : m_mainCanvas->autoFitMarkers[el_i][el_j]) {
        if (TF1 *f = dynamic_cast<TF1*>(obj)) {
            TString fname = f->GetName();
            if (fname.Contains("background") && !fname.Contains("gaussianWithBackground")) {
                bkgFuncs.push_back(f);
            } else {
                fitFuncs.push_back(f);
            }
        }
    }
    if (m_mainCanvas->gaussianWithBackgroundFunction) {
        if (std::find(fitFuncs.begin(), fitFuncs.end(), m_mainCanvas->gaussianWithBackgroundFunction) == fitFuncs.end()) {
            fitFuncs.push_back(m_mainCanvas->gaussianWithBackgroundFunction);
        }
    }
    if (m_mainCanvas->backgroundFunction) {
        if (std::find(bkgFuncs.begin(), bkgFuncs.end(), m_mainCanvas->backgroundFunction) == bkgFuncs.end()) {
            bkgFuncs.push_back(m_mainCanvas->backgroundFunction);
        }
    }
    if (hist->GetListOfFunctions()) {
        TIter next(hist->GetListOfFunctions());
        while (TObject *obj = next()) {
            if (TF1 *f = dynamic_cast<TF1*>(obj)) {
                TString fname = f->GetName();
                if (fname.Contains("background") && !fname.Contains("gaussianWithBackground")) {
                    if (std::find(bkgFuncs.begin(), bkgFuncs.end(), f) == bkgFuncs.end()) {
                        bkgFuncs.push_back(f);
                    }
                } else {
                    if (std::find(fitFuncs.begin(), fitFuncs.end(), f) == fitFuncs.end()) {
                        fitFuncs.push_back(f);
                    }
                }
            }
        }
    }

    const double xLow = hist->GetXaxis()->GetBinLowEdge(startB);
    const double xUp  = hist->GetXaxis()->GetBinUpEdge(endB);

    const int nSamples = 250;
    std::vector<double> fitCurve(nSamples, -1.0);
    std::vector<double> bkgCurve(nSamples, -1.0);
    bool hasActualFit = false;
    QString fitInfo;

    // Check if any actual fit function overlaps with the zoom window [xLow, xUp]
    for (TF1 *f : fitFuncs) {
        if (!f) continue;
        Double_t fMin = f->GetXmin();
        Double_t fMax = f->GetXmax();
        if (fMax >= xLow && fMin <= xUp) {
            hasActualFit = true;
            for (int k = 0; k < nSamples; ++k) {
                double frac = static_cast<double>(k) / static_cast<double>(nSamples - 1);
                double xVal = xLow + frac * (xUp - xLow);
                if (xVal >= fMin && xVal <= fMax) {
                    double val = f->Eval(xVal);
                    if (val > fitCurve[k]) {
                        fitCurve[k] = val;
                    }
                }
            }
            double curX = hist->GetBinCenter(binX);
            if (curX >= fMin && curX <= fMax) {
                if (f->GetNpar() >= 3) {
                    double cent = f->GetParameter(1);
                    double sig = std::abs(f->GetParameter(2));
                    fitInfo = QString("Fit: μ=%1 σ=%2").arg(cent, 0, 'f', 1).arg(sig, 0, 'f', 1);
                } else {
                    fitInfo = "Fitted Peak";
                }
            }
        }
    }

    for (TF1 *bkg : bkgFuncs) {
        if (!bkg) continue;
        Double_t bMin = bkg->GetXmin();
        Double_t bMax = bkg->GetXmax();
        if (bMax >= xLow && bMin <= xUp) {
            for (int k = 0; k < nSamples; ++k) {
                double frac = static_cast<double>(k) / static_cast<double>(nSamples - 1);
                double xVal = xLow + frac * (xUp - xLow);
                if (xVal >= bMin && xVal <= bMax) {
                    double val = bkg->Eval(xVal);
                    if (val > bkgCurve[k]) {
                        bkgCurve[k] = val;
                    }
                }
            }
        }
    }

    m_zoomHUD->updateData(hist, binX, energy, isCalibrated, fitCurve, bkgCurve, hasActualFit, fitInfo);

    // Smart placement: keep HUD in top-right, unless cursor is near top-right, then flip to top-left
    int hudW = m_zoomHUD->width();
    int hudH = m_zoomHUD->height();
    int targetX = (mouseX > width() - hudW - 30 && mouseY < hudH + 40)
                  ? 15
                  : std::max(10, width() - hudW - 15);
    const QPoint globalPos = mapToGlobal(QPoint(targetX, 15));
    m_zoomHUD->move(globalPos);
    m_zoomHUD->show();
    m_zoomHUD->raise();
}

void QRootCanvas::hideZoomHUD()
{
    if (m_zoomHUD) {
        m_zoomHUD->hide();
    }
}

void QRootCanvas::enterEvent(QEvent *event)
{
    setFocus();
    QWidget::enterEvent(event);
}

void QRootCanvas::leaveEvent(QEvent *event)
{
    hideZoomHUD();
    QWidget::leaveEvent(event);
}

//==============================================================================
// QRootCanvas::mouseMoveEvent
//==============================================================================
// Emits real-time mouse coordinates for live channel/count readouts and forwards
// mouse motion events to the underlying ROOT TCanvas.
//==============================================================================
void QRootCanvas::mouseMoveEvent(QMouseEvent *e)
{
    // Always track cursor position
    xMousePosition = e->x();
    yMousePosition = e->y();

    // Check if CTRL is pressed via event modifiers, keyboard query, or state flag
    bool ctrlDown = (e->modifiers() & Qt::ControlModifier) || 
                    (QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier) ||
                    controlKeyIsPressed;
    if (ctrlDown) {
        controlKeyIsPressed = true;
        updateZoomHUD(e->x(), e->y());
    } else if (controlKeyIsPressed) {
        controlKeyIsPressed = false;
        hideZoomHUD();
    }

    // Notify main canvas of cursor position to update coordinates and hover tracking
    emit mousePilgrimCoordRequest(e->x(), e->y());
    if (fCanvas) {
        fCanvas->Modified();
        fCanvas->Update();
        if (e->buttons() & Qt::MiddleButton) {
            fCanvas->HandleInput(kButton2Motion, e->x(), e->y());
        } else if (e->buttons() & Qt::RightButton) {
            fCanvas->HandleInput(kButton3Motion, e->x(), e->y());
        } else {
            fCanvas->HandleInput(kMouseMotion, e->x(), e->y());
        }

        // Live coordinate update while dragging with mouse button held
        if (e->buttons() & Qt::LeftButton) {
            emit mouseLeftClickCoordRequest(e->x(), e->y());
            emit showXY(xMousePosition, yMousePosition);
        }
    }
}

//==============================================================================
// QRootCanvas::wheelEvent
//==============================================================================
// Translates vertical wheel rotations into vertical spectrum pan / scroll events.
//==============================================================================
void QRootCanvas::wheelEvent(QWheelEvent *e)
{
    const QPoint mousePos = e->position().toPoint();
    emit mousePilgrimCoordRequest(mousePos.x(), mousePos.y());
    const int delta = e->angleDelta().y();
    if (delta > 0) { // Wheel scrolled up: zoom out / expand Y range
        emit requesttranslatedownTheScreen();
    } else if (delta < 0) { // Wheel scrolled down: zoom in / compress Y range
        emit requesttranslateupTheScreen();
    }
}

//==============================================================================
// QRootCanvas::mousePressEvent
//==============================================================================
// Handles mouse clicks:
// - Left click: records click position and emits coordinate display request.
// - Middle click: forwards to ROOT.
// - Right click: opens spectrum management context menu.
//==============================================================================
void QRootCanvas::mousePressEvent(QMouseEvent *e)
{
    xMousePosition = e->x();
    yMousePosition = e->y();
    if (fCanvas) {
        switch (e->button()) {
            case Qt::LeftButton:
                emit mouseLeftClickCoordRequest(e->x(), e->y());
                emit showXY(xMousePosition, yMousePosition);
                break;
            case Qt::MiddleButton:
                fCanvas->HandleInput(kButton2Down, e->x(), e->y());
                break;
            case Qt::RightButton:
                showContextMenu(e);
                break;
            default:
                break;
        }
    }
}

//==============================================================================
// QRootCanvas::mouseReleaseEvent
//==============================================================================
// Handles mouse button release. If the Left button is released while Ctrl is
// held down, triggers an automated Gaussian peak fit at the clicked position.
//==============================================================================
void QRootCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (fCanvas) {
        switch (e->button()) {
            case Qt::LeftButton: {
                bool ctrlDown = controlKeyIsPressed || (e->modifiers() & Qt::ControlModifier) || (QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier);
                if (ctrlDown) {
                    emit autoFitRequested(e->x(), e->y());
                    updateZoomHUD(e->x(), e->y());
                }
                break;
            }
            case Qt::MiddleButton:
                fCanvas->HandleInput(kButton2Up, e->x(), e->y());
                break;
            case Qt::RightButton:
                break;
            default:
                break;
        }
    }
}

//==============================================================================
// QRootCanvas::showContextMenu
//==============================================================================
// Constructs and displays the right-click popup context menu for spectrum grid
// operations (adding/removing matrix rows and columns, refreshing canvas).
//==============================================================================
void QRootCanvas::showContextMenu(QMouseEvent *e)
{
    QMenu contextMenu(tr("Spectrum Actions"), this);

    // Row / Column matrix manipulation actions
    QAction *actionAddLine = new QAction(tr("Add Line"), this);
    connect(actionAddLine, &QAction::triggered, this, &QRootCanvas::AddLineRequest);
    contextMenu.addAction(actionAddLine);

    QAction *actionAddCol = new QAction(tr("Add Column"), this);
    connect(actionAddCol, &QAction::triggered, this, &QRootCanvas::AddCulomnRequest);
    contextMenu.addAction(actionAddCol);

    QAction *actionDelLine = new QAction(tr("Delete Line"), this);
    connect(actionDelLine, &QAction::triggered, this, &QRootCanvas::DeleteLineRequest);
    contextMenu.addAction(actionDelLine);

    QAction *actionDelCol = new QAction(tr("Delete Column"), this);
    connect(actionDelCol, &QAction::triggered, this, &QRootCanvas::DeleteCulomnRequest);
    contextMenu.addAction(actionDelCol);

    contextMenu.addSeparator();

    // Canvas redrawing
    QAction *actionRefresh = new QAction(tr("Refresh Display"), this);
    connect(actionRefresh, &QAction::triggered, this, &QRootCanvas::RefreshScreenRequest);
    contextMenu.addAction(actionRefresh);

    contextMenu.exec(e->globalPos());
}

//==============================================================================
// QRootCanvas::keyPressEvent
//==============================================================================
// Multi-key modal state machine replicating legacy Xtrackn / GASP keyboard shortcuts:
// - CTRL + C/Z/Y : Application exit confirmation.
// - C Prefix (Compute):
//     C + I : Net peak integration without background subtraction.
//     C + J : Peak integration with linear background subtraction.
//     C + V : Gaussian multi-peak fit.
// - Z Prefix (Zero / Delete):
//     Z + I : Delete integral range markers.
//     Z + B : Delete background markers.
//     Z + R : Delete fit range markers.
//     Z + G : Delete Gauss peak centroid markers.
//     Z + A : Delete all markers on the spectrum.
// - M Prefix (Markers visibility):
//     M + I, M + B, M + R, M + G, M + A : Redraw / toggle marker overlays.
// - F Prefix: Fullscreen / Unzoom.
// - Single Key Navigation:
//     Space : Place zoom window boundary marker.
//     E     : Execute zoom between spacebar markers.
//     I     : Place integral marker at current mouse position.
//     B     : Place background marker at current mouse position.
//     R     : Place fit range boundary marker.
//     G     : Place Gaussian peak center marker.
//     =     : Clear all drawn lines/markers from screen.
//     Arrows: Pan / translate spectrum view (left, right, up, down).
//     ? / H : Display interactive help command list.
//==============================================================================
void QRootCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        controlKeyIsPressed = true;
        QPoint localPos = mapFromGlobal(QCursor::pos());
        updateZoomHUD(localPos.x(), localPos.y());
        return;
    }

    // Handle key sequences following a CTRL press
    if (controlKeyIsPressed) {
        switch (event->key()) {
            case Qt::Key_C:
            case Qt::Key_Z:
            case Qt::Key_Y: {
                QMessageBox::StandardButton quiting = QMessageBox::question(
                    this, tr("Quit"), tr("Are you sure you want to quit?"),
                    QMessageBox::Yes | QMessageBox::No);
                if (quiting == QMessageBox::Yes) {
                    emit killSwitch();
                }
                break;
            }
            default:
                break;
        }
        controlKeyIsPressed = false;
        hideZoomHUD();
        return;
    }
    // Handle commands prefixed by 'C' (Computation routines)
    else if (cKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_I:
                // C + I: Integration without background subtraction
                emit requestIntegrationNoBackground();
                break;
            case Qt::Key_J:
                // C + J: Integration with linear background subtraction
                emit requestIntegrationWithBackground();
                break;
            case Qt::Key_V:
                // C + V: Gaussian multi-peak fit over marked region
                emit requestFitGauss();
                break;
            case Qt::Key_C:
                // Redundant C press: cancel prefix
                break;
            default:
                std::cout << "Waited for execute command after C was pressed but no valid command arrived after it" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for execute command after C was pressed but no valid command arrived after it\n");
                break;
        }
        cKeyWasPressed = false;
    }
    // Handle commands prefixed by 'Z' (Zero / Delete markers)
    else if (zKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_I:
                // Z + I: Delete integral markers
                emit requestDeleteIntegralMarkers();
                break;
            case Qt::Key_B:
                // Z + B: Delete background markers
                emit requestDeleteBackgroundMarkers();
                break;
            case Qt::Key_R:
                // Z + R: Delete range markers
                emit requestDeleteRangeMarkers();
                break;
            case Qt::Key_G:
                // Z + G: Delete Gauss centroid markers
                emit requestDeleteGaussMarkers();
                break;
            case Qt::Key_A:
                // Z + A: Delete all active markers
                emit requestDeleteAllMarkers();
                break;
            case Qt::Key_Z:
                // Redundant Z press: cancel prefix
                break;
            default:
                std::cout << "Waited for delete command after Z was pressed but no valid command arrived after it" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for delete command after Z was pressed but no valid command arrived after it\n");
                break;
        }
        zKeyWasPressed = false;
    }
    // Handle commands prefixed by 'M' (Show / Redraw markers)
    else if (mKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_I:
                // M + I: Redraw integral markers
                emit requestShowIntegralMarkers();
                break;
            case Qt::Key_B:
                // M + B: Redraw background markers
                emit requestShowBackgroundMarkers();
                break;
            case Qt::Key_R:
                // M + R: Redraw range markers
                emit requestShowRangeMarkers();
                break;
            case Qt::Key_G:
                // M + G: Redraw Gauss peak centroid markers
                emit requestShowGaussMarkers();
                break;
            case Qt::Key_A:
                // M + A: Redraw all markers
                emit requestShowAllMarkers();
                break;
            case Qt::Key_M:
                // Redundant M press: cancel prefix
                break;
            default:
                std::cout << "Waited for show command after M was pressed but no valid command arrived after it" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for show command after M was pressed but no valid command arrived after it\n");
                break;
        }
        mKeyWasPressed = false;
    }
    // Handle commands prefixed by 'F' (Fullscreen / zoom reset)
    else if (fKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_F:
            case Qt::Key_S:
                emit fullscreen();
                break;
            default:
                break;
        }
        fKeyWasPressed = false;
    }
    // Single-key analysis and navigation triggers
    else {
        switch (event->key()) {
            case Qt::Key_H:
            case Qt::Key_Question:
                // '?' or 'H': Display interactive help menu
                emit requestHelp();
                break;
            case Qt::Key_C:
                cKeyWasPressed = true;
                break;
            case Qt::Key_Z:
                zKeyWasPressed = true;
                break;
            case Qt::Key_M:
                mKeyWasPressed = true;
                break;
            case Qt::Key_I:
                // 'I': Place integral boundary marker at cursor
                emit addIntegralMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Space:
                // Spacebar: Place zoom boundary marker at cursor
                emit addSpaceBarMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_F:
                fKeyWasPressed = true;
                break;
            case Qt::Key_Right:
                // Right Arrow: Pan spectrum to higher channels
                emit requesttranslateplusTheScreen();
                break;
            case Qt::Key_Left:
                // Left Arrow: Pan spectrum to lower channels
                emit requesttranslateminusTheScreen();
                break;
            case Qt::Key_Down:
                // Down Arrow: Pan spectrum vertical scale downward
                emit requesttranslatedownTheScreen();
                break;
            case Qt::Key_Up:
                // Up Arrow: Pan spectrum vertical scale upward
                emit requesttranslateupTheScreen();
                break;
            case Qt::Key_B:
                // 'B': Place background sample marker at cursor
                emit addBackgroundMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_R:
                // 'R': Place fit range boundary marker at cursor
                emit requestAddRangeMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_G:
                // 'G': Place Gaussian peak estimate marker at cursor
                emit requestAddGaussMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Equal:
                // '=': Clear drawn overlay lines and reset display
                emit requestClearTheScreen();
                break;
            case Qt::Key_E:
                // 'E': Execute zoom between spacebar markers
                emit requestZoomTheScreen();
                break;
            default:
                QWidget::keyPressEvent(event);
                break;
        }
    }
}

//==============================================================================
// QRootCanvas::keyReleaseEvent
//==============================================================================
// Tracks release of modifier keys (such as Control) to reset modal flags.
//==============================================================================
void QRootCanvas::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Control:
            controlKeyIsPressed = false;
            hideZoomHUD();
            break;
        default:
            QWidget::keyReleaseEvent(event);
            break;
    }
}

//==============================================================================
// QRootCanvas::focusOutEvent
//==============================================================================
// Hides the zoom HUD if the canvas loses window focus.
//==============================================================================
void QRootCanvas::focusOutEvent(QFocusEvent *event)
{
    controlKeyIsPressed = false;
    hideZoomHUD();
    QWidget::focusOutEvent(event);
}

//==============================================================================
// QRootCanvas::resizeEvent
//==============================================================================
// Resizes the embedded CERN ROOT TCanvas whenever the parent Qt widget is resized.
//==============================================================================
void QRootCanvas::resizeEvent(QResizeEvent *event)
{
    if (m_zoomHUD && m_zoomHUD->isVisible()) {
        int hudW = m_zoomHUD->width();
        int hudH = m_zoomHUD->height();
        int targetX = (xMousePosition > width() - hudW - 30 && yMousePosition < hudH + 40)
                      ? 15
                      : std::max(10, width() - hudW - 15);
        const QPoint globalPos = mapToGlobal(QPoint(targetX, 15));
        m_zoomHUD->move(globalPos);
    }
    if (fCanvas) {
        fCanvas->SetCanvasSize(event->size().width(), event->size().height());
        fCanvas->Resize();
        fCanvas->Update();
    }
}

//==============================================================================
// QRootCanvas::paintEvent
//==============================================================================
// Synchronizes the embedded ROOT X11 pad painting system with Qt's paint engine.
//==============================================================================
void QRootCanvas::paintEvent(QPaintEvent *)
{
    if (fCanvas) {
        fCanvas->Resize();
        fCanvas->Update();
    }
}

//==============================================================================
// QMainCanvas::closeEvent
//==============================================================================
// Confirms whether the user truly intends to terminate the application.
//==============================================================================
void QMainCanvas::closeEvent(QCloseEvent *e)
{
    QMessageBox::StandardButton quiting = QMessageBox::question(
        this, tr("Quit"), tr("Are you sure you want to quit?"),
        QMessageBox::Yes | QMessageBox::No);

    if (quiting == QMessageBox::Yes) {
        e->accept();
    } else {
        e->ignore();
    }
}

//==============================================================================
// QMainCanvas Constructor
//==============================================================================
// Builds the main user interface:
// 1. Instantiates a vertical QSplitter allowing interactive mouse resizing of the prompt.
// 2. Embeds QRootCanvas, coordinate readout bar (white background), and analysis buttons
//    into a top container panel.
// 3. Connects all QRootCanvas user-interaction signals to QMainCanvas slots.
// 4. Sets up the ROOT graphics event processing timer (fRootTimer).
// 5. Allocates the default 10240-channel TracknHistogram instance.
//==============================================================================
QMainCanvas::QMainCanvas(QWidget *parent)
    : QWidget(parent),
      mainSplitter(nullptr),
      backgroundCovarianceMatrix(nullptr)
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Vertical splitter allowing interactive mouse dragging between canvas/controls and prompt
    mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->setObjectName("mainSplitter");
    mainSplitter->setHandleWidth(6);
    mainSplitter->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background-color: #d0d0d0;"
        "  border-top: 1px solid #b0b0b0;"
        "  border-bottom: 1px solid #b0b0b0;"
        "}"
        "QSplitter::handle:vertical:hover {"
        "  background-color: #3399ff;"
        "}"
    );

    // Top container widget holding spectrum canvas, coordinate readout, and action buttons
    QWidget *topContainer = new QWidget(mainSplitter);
    QVBoxLayout *topLayout = new QVBoxLayout(topContainer);
    topLayout->setContentsMargins(4, 4, 4, 4);
    topLayout->setSpacing(2);

    // Helpers for consistent 3D beveled buttons and clean status readout panels
    auto makeButton = [](const QString &text, QWidget *parent, bool enabled = true) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, parent);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setEnabled(enabled);
        btn->setStyleSheet(
            "QPushButton {"
            "  border-top: 2px solid #ffffff;"
            "  border-left: 2px solid #ffffff;"
            "  border-right: 2px solid #606060;"
            "  border-bottom: 2px solid #606060;"
            "  background-color: #e0e0e0;"
            "  color: #000000;"
            "  font-weight: bold;"
            "  font-size: 16px;"
            "  padding: 3px 6px;"
            "}"
            "QPushButton:pressed {"
            "  border-top: 2px solid #606060;"
            "  border-left: 2px solid #606060;"
            "  border-right: 2px solid #ffffff;"
            "  border-bottom: 2px solid #ffffff;"
            "  background-color: #d0d0d0;"
            "}"
            "QPushButton:disabled {"
            "  color: #888888;"
            "  background-color: #e8e8e8;"
            "  border-top: 2px solid #f0f0f0;"
            "  border-left: 2px solid #f0f0f0;"
            "  border-right: 2px solid #a0a0a0;"
            "  border-bottom: 2px solid #a0a0a0;"
            "}"
        );
        return btn;
    };

    auto makeLabel = [](const QString &text, QWidget *parent, Qt::Alignment align = Qt::AlignLeft) -> QLabel* {
        QLabel *lbl = new QLabel(text, parent);
        lbl->setAlignment(align | Qt::AlignVCenter);
        lbl->setFixedHeight(33);
        lbl->setStyleSheet(
            "background-color: #ffffff;"
            "color: #000000;"
            "border: 1px solid #999999;"
            "padding: 2px 6px;"
            "font-size: 16px;"
            "font-weight: normal;"
        );
        return lbl;
    };

    // 1. Top Status Grid (2 rows x 4 columns)
    QGridLayout *topGrid = new QGridLayout();
    topGrid->setSpacing(2);
    topGrid->setContentsMargins(0, 0, 0, 2);

    labelXMin = makeLabel("X Min:  0.0", topContainer);
    labelXMax = makeLabel("X Max:  0.0", topContainer);
    labelYMin = makeLabel("Y Min:  0.00", topContainer);
    labelYMax = makeLabel("Y Max:  0.00", topContainer);

    labelChannel = makeLabel("Channel", topContainer);
    labelEnergy = makeLabel("Energy", topContainer);
    labelCounts = makeLabel("Counts", topContainer);
    labelCursorY = makeLabel("Y", topContainer);

    labelX = labelChannel;
    labelY = labelCounts;

    topGrid->addWidget(labelXMin, 0, 0);
    topGrid->addWidget(labelXMax, 0, 1);
    topGrid->addWidget(labelYMin, 0, 2);
    topGrid->addWidget(labelYMax, 0, 3);

    topGrid->addWidget(labelChannel, 1, 0);
    topGrid->addWidget(labelEnergy, 1, 1);
    topGrid->addWidget(labelCounts, 1, 2);
    topGrid->addWidget(labelCursorY, 1, 3);

    topLayout->addLayout(topGrid);

    // 2. Middle Section (Left Bar + Canvas + Right Bar)
    QHBoxLayout *middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(4);
    middleLayout->setContentsMargins(0, 0, 0, 0);

    // Left button column (bottom-aligned)
    QVBoxLayout *leftBar = new QVBoxLayout();
    leftBar->setSpacing(2);
    leftBar->addStretch(1);

    QPushButton *btnEnCal = makeButton("EnCal", topContainer, false);
    btnEnCal->setFixedWidth(93);
    btnEnCal->setFixedHeight(36);
    leftBar->addWidget(btnEnCal);

    QPushButton *btnDT = makeButton("DT", topContainer, false);
    btnDT->setFixedWidth(93);
    btnDT->setFixedHeight(36);
    leftBar->addWidget(btnDT);

    QPushButton *btnCal2P = makeButton("Cal2P", topContainer, true);
    btnCal2P->setFixedWidth(93);
    btnCal2P->setFixedHeight(39);
    btnCal2P->setToolTip("2-point first-order calibration based on the last fitted peaks or zoom markers");
    leftBar->addWidget(btnCal2P);
    connect(btnCal2P, &QPushButton::clicked, this, &QMainCanvas::Cal2pMain);

    middleLayout->addLayout(leftBar);

    // Interactive ROOT canvas
    canvas = new QRootCanvas(topContainer);
    canvas->setMainCanvas(this);
    canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvas->setStyleSheet("border: 1px solid #707070;");
    middleLayout->addWidget(canvas, 1);

    // Right button column (11 rows)
    QVBoxLayout *rightBar = new QVBoxLayout();
    rightBar->setSpacing(2);

    QPushButton *btnInt = makeButton("Int", topContainer, true);
    btnInt->setFixedWidth(93);
    btnInt->setFixedHeight(36);
    btnInt->setToolTip(tr("Integrate peak area (gross or net with background)."));
    rightBar->addWidget(btnInt);
    connect(btnInt, &QPushButton::clicked, this, &QMainCanvas::areaFunctionWithBackground);

    QPushButton *btnFit = makeButton("Fit", topContainer, true);
    btnFit->setFixedWidth(93);
    btnFit->setFixedHeight(36);
    rightBar->addWidget(btnFit);
    connect(btnFit, &QPushButton::clicked, this, &QMainCanvas::fitGauss);

    QPushButton *btnPkS = makeButton("PkS", topContainer, false);
    btnPkS->setFixedWidth(93);
    btnPkS->setFixedHeight(36);
    rightBar->addWidget(btnPkS);

    QPushButton *btnRefresh = makeButton("=", topContainer, true);
    btnRefresh->setFixedWidth(93);
    btnRefresh->setFixedHeight(36);
    rightBar->addWidget(btnRefresh);
    connect(btnRefresh, &QPushButton::clicked, this, &QMainCanvas::RefreshScreen);

    QPushButton *btnLinLog = makeButton("L", topContainer, true);
    btnLinLog->setFixedWidth(93);
    btnLinLog->setFixedHeight(36);
    rightBar->addWidget(btnLinLog);
    connect(btnLinLog, &QPushButton::clicked, this, &QMainCanvas::toggleLogY);

    QPushButton *btnFF = makeButton("FF", topContainer, true);
    btnFF->setFixedWidth(93);
    btnFF->setFixedHeight(36);
    rightBar->addWidget(btnFF);
    connect(btnFF, &QPushButton::clicked, this, &QMainCanvas::zoomOut);

    QPushButton *btnFX = makeButton("FX", topContainer, false);
    btnFX->setFixedWidth(93);
    btnFX->setFixedHeight(36);
    rightBar->addWidget(btnFX);

    QPushButton *btnFY = makeButton("FY", topContainer, false);
    btnFY->setFixedWidth(93);
    btnFY->setFixedHeight(36);
    rightBar->addWidget(btnFY);

    QPushButton *btnSX = makeButton("SX", topContainer, false);
    btnSX->setFixedWidth(93);
    btnSX->setFixedHeight(36);
    rightBar->addWidget(btnSX);

    QPushButton *btnSY = makeButton("SY", topContainer, false);
    btnSY->setFixedWidth(93);
    btnSY->setFixedHeight(36);
    rightBar->addWidget(btnSY);

    // Split row for < and >
    QHBoxLayout *navRow = new QHBoxLayout();
    navRow->setSpacing(2);
    QPushButton *btnLeft = makeButton("<", topContainer, true);
    btnLeft->setFixedWidth(45);
    btnLeft->setFixedHeight(36);
    connect(btnLeft, &QPushButton::clicked, this, &QMainCanvas::translateminusTheScreen);
    navRow->addWidget(btnLeft);

    QPushButton *btnRight = makeButton(">", topContainer, true);
    btnRight->setFixedWidth(45);
    btnRight->setFixedHeight(36);
    connect(btnRight, &QPushButton::clicked, this, &QMainCanvas::translateplusTheScreen);
    navRow->addWidget(btnRight);

    rightBar->addLayout(navRow);

    middleLayout->addLayout(rightBar);
    topLayout->addLayout(middleLayout, 1);

    // 3. Bottom Toolbar (Control Buttons + Status Displays)
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(4);
    bottomLayout->setContentsMargins(0, 2, 0, 0);

    // Left Button Grid (2 rows x 4 columns)
    QGridLayout *bottomBtnGrid = new QGridLayout();
    bottomBtnGrid->setSpacing(2);

    QPushButton *btnR = makeButton("R", topContainer, true);
    btnR->setFixedWidth(54);
    btnR->setFixedHeight(36);
    btnR->setToolTip(tr("Opens histogram spectra in multiple formats or sizes."));
    connect(btnR, &QPushButton::clicked, this, &QMainCanvas::clicked1);
    bottomBtnGrid->addWidget(btnR, 0, 0);

    QPushButton *btnW = makeButton("W", topContainer, true);
    btnW->setFixedWidth(54);
    btnW->setFixedHeight(36);
    btnW->setToolTip(tr("Write / export current spectrum to a file."));
    connect(btnW, &QPushButton::clicked, this, &QMainCanvas::clickedW);
    bottomBtnGrid->addWidget(btnW, 0, 1);

    QPushButton *btnDec = makeButton("# -", topContainer, true);
    btnDec->setFixedWidth(54);
    btnDec->setFixedHeight(36);
    btnDec->setToolTip(tr("Load previous spectrum from multi-spectrum file."));
    connect(btnDec, &QPushButton::clicked, this, &QMainCanvas::onSpectrumDecrement);
    bottomBtnGrid->addWidget(btnDec, 0, 2);

    QPushButton *btnInc = makeButton("# +", topContainer, true);
    btnInc->setFixedWidth(54);
    btnInc->setFixedHeight(36);
    btnInc->setToolTip(tr("Load next spectrum from multi-spectrum file."));
    connect(btnInc, &QPushButton::clicked, this, &QMainCanvas::onSpectrumIncrement);
    bottomBtnGrid->addWidget(btnInc, 0, 3);

    QPushButton *btnOpenCM = makeButton("Open CM", topContainer, false);
    btnOpenCM->setFixedHeight(36);
    bottomBtnGrid->addWidget(btnOpenCM, 1, 0, 1, 2);

    QPushButton *btnGateCM = makeButton("Gate CM", topContainer, false);
    btnGateCM->setFixedHeight(36);
    bottomBtnGrid->addWidget(btnGateCM, 1, 2, 1, 2);

    bottomLayout->addLayout(bottomBtnGrid);

    // Right Status Grid (2 rows)
    QGridLayout *bottomStatusGrid = new QGridLayout();
    bottomStatusGrid->setSpacing(2);

    labelSpectrumFile = makeLabel("<none>", topContainer, Qt::AlignCenter);
    bottomStatusGrid->addWidget(labelSpectrumFile, 0, 0, 1, 3);

    labelWorkingPath = makeLabel(QDir::currentPath(), topContainer, Qt::AlignLeft);
    bottomStatusGrid->addWidget(labelWorkingPath, 1, 0, 1, 2);

    QHBoxLayout *outBox = new QHBoxLayout();
    outBox->setSpacing(2);
    outBox->setContentsMargins(0, 0, 0, 0);

    labelOutputFile = makeLabel("-> <none>", topContainer, Qt::AlignLeft);
    outBox->addWidget(labelOutputFile, 1);

    QPushButton *iconButton = new QPushButton(topContainer);
    iconButton->setFocusPolicy(Qt::NoFocus);
    iconButton->setIcon(QIcon("icon.png"));
    iconButton->setIconSize(QSize(22, 22));
    iconButton->setToolTip(tr("Color & Theme Settings"));
    iconButton->setFixedSize(33, 33);
    iconButton->setStyleSheet(
        "QPushButton { border: 1px solid #999999; background: #ffffff; }"
        "QPushButton:pressed { background: #e0e0e0; }"
    );
    connect(iconButton, &QPushButton::clicked, this, &QMainCanvas::OpenColorSelectionDialog);
    outBox->addWidget(iconButton);

    bottomStatusGrid->addLayout(outBox, 1, 2);

    bottomLayout->addLayout(bottomStatusGrid, 1);

    topLayout->addLayout(bottomLayout);

    mainSplitter->addWidget(topContainer);
    rootLayout->addWidget(mainSplitter);

    // 5. Connect user interaction signals from canvas to analysis slots
    connect(canvas, &QRootCanvas::requestIntegrationNoBackground, this, &QMainCanvas::areaFunction);
    connect(canvas, &QRootCanvas::requestIntegrationWithBackground, this, &QMainCanvas::areaFunctionWithBackground);
    connect(canvas, &QRootCanvas::autoFitRequested, this, &QMainCanvas::autoFit);
    connect(canvas, &QRootCanvas::requestClearTheScreen, this, &QMainCanvas::clearTheScreen);
    connect(canvas, &QRootCanvas::addBackgroundMarkerRequested, this, &QMainCanvas::addBackgroundMarker);
    connect(canvas, &QRootCanvas::addIntegralMarkerRequested, this, &QMainCanvas::addIntegralMarker);
    connect(canvas, &QRootCanvas::showXY, this, &QMainCanvas::showXYcoord);
    connect(canvas, &QRootCanvas::requestZoomTheScreen, this, &QMainCanvas::zoomTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateplusTheScreen, this, &QMainCanvas::translateplusTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateminusTheScreen, this, &QMainCanvas::translateminusTheScreen);
    connect(canvas, &QRootCanvas::requesttranslatedownTheScreen, this, &QMainCanvas::translatedownTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateupTheScreen, this, &QMainCanvas::translateupTheScreen);
    connect(canvas, &QRootCanvas::fullscreen, this, &QMainCanvas::zoomOut);
    connect(canvas, &QRootCanvas::requestDeleteBackgroundMarkers, this, &QMainCanvas::deleteBackgroundMarkers);
    connect(canvas, &QRootCanvas::requestDeleteIntegralMarkers, this, &QMainCanvas::deleteIntegralMarkers);
    connect(canvas, &QRootCanvas::requestDeleteAllMarkers, this, &QMainCanvas::deleteAllMarkers);
    connect(canvas, &QRootCanvas::requestShowBackgroundMarkers, this, &QMainCanvas::showBackgroundMarkers);
    connect(canvas, &QRootCanvas::requestShowIntegralMarkers, this, &QMainCanvas::showIntegralMarkers);
    connect(canvas, &QRootCanvas::requestShowAllMarkers, this, &QMainCanvas::showAllMarkers);
    connect(canvas, &QRootCanvas::addSpaceBarMarkerRequested, this, &QMainCanvas::addSpaceBarMarker);
    connect(canvas, &QRootCanvas::requestAddRangeMarker, this, &QMainCanvas::addRangeMarker);
    connect(canvas, &QRootCanvas::requestDeleteRangeMarkers, this, &QMainCanvas::deleteRangeMarkers);
    connect(canvas, &QRootCanvas::requestShowRangeMarkers, this, &QMainCanvas::showRangeMarkers);
    connect(canvas, &QRootCanvas::requestAddGaussMarker, this, &QMainCanvas::addGaussMarker);
    connect(canvas, &QRootCanvas::requestDeleteGaussMarkers, this, &QMainCanvas::deleteGaussMarkers);
    connect(canvas, &QRootCanvas::requestShowGaussMarkers, this, &QMainCanvas::showGaussMarkers);
    connect(canvas, &QRootCanvas::requestFitGauss, this, &QMainCanvas::fitGauss);
    connect(canvas, &QRootCanvas::requestHelp, this, &QMainCanvas::offerHelp);
    connect(canvas, &QRootCanvas::killSwitch, qApp, &QCoreApplication::quit);

    connect(canvas, &QRootCanvas::mousePilgrimCoordRequest, this, &QMainCanvas::IdentifyLastPilgrimHistogram);
    connect(canvas, &QRootCanvas::mouseLeftClickCoordRequest, this, &QMainCanvas::IdentifyLastClickedHistogram);

    connect(canvas, &QRootCanvas::AddLineRequest, this, &QMainCanvas::AddLine);
    connect(canvas, &QRootCanvas::AddCulomnRequest, this, &QMainCanvas::AddCulomn);
    connect(canvas, &QRootCanvas::DeleteLineRequest, this, &QMainCanvas::DeleteLine);
    connect(canvas, &QRootCanvas::DeleteCulomnRequest, this, &QMainCanvas::DeleteCulomn);
    connect(canvas, &QRootCanvas::RefreshScreenRequest, this, &QMainCanvas::RefreshScreen);

    // 6. Root event loop timer: calls handle_root_events() every 20ms
    fRootTimer = new QTimer(this);
    connect(fRootTimer, &QTimer::timeout, this, &QMainCanvas::handle_root_events);
    fRootTimer->start(20);

    // 7. Initialize default 10240-channel TracknHistogram for the primary spectrum cell
    HijF[1][1] = new TracknHistogram("HijF[1][1]", "", 10240, 0, 10240);
    HijF[1][1]->GetXaxis()->SetNdivisions(0, kTRUE);
    HijF[1][1]->GetXaxis()->SetLabelSize(0);
    HijF[1][1]->GetYaxis()->SetNdivisions(0, kTRUE);
    HijF[1][1]->GetYaxis()->SetLabelSize(0);
    HijF[1][1]->SetStats(0);
}

//==============================================================================
// QMainCanvas::clearDrawnObjects
//==============================================================================
// Safely removes and deletes all dynamically allocated temporary visual markers
// (lines, boxes, shaded regions) from the canvas and list, preventing dangling
// pointers and use-after-free crashes.
//==============================================================================
void QMainCanvas::clearDrawnObjects()
{
    while (TObject *obj = listOfObjectsDrawnOnScreen.First()) {
        listOfObjectsDrawnOnScreen.Remove(obj);
        delete obj;
    }
}

//==============================================================================
// QMainCanvas Destructor
//==============================================================================
// Cleans up dynamically allocated resources including the background covariance
// matrix produced by fitBackground() and any remaining drawn marker objects.
//==============================================================================
QMainCanvas::~QMainCanvas()
{
    clearDrawnObjects();
    delete backgroundCovarianceMatrix;
    backgroundCovarianceMatrix = nullptr;
    delete gaussianCenterMarkerText;
    gaussianCenterMarkerText = nullptr;
    delete lineR; lineR = nullptr;
    delete lineL; lineL = nullptr;
    delete lineU; lineU = nullptr;
    delete lineD; lineD = nullptr;
}

//==============================================================================
// QMainCanvas::getBinFromClick
//==============================================================================
// Parses the ROOT TObject information string returned by GetObjectInfo(x, y)
// on the active histogram pad to extract the integer channel bin number.
//
// Format produced by ROOT GetObjectInfo:
//   "x=..., y=..., binx=..., biny=..., binc=..."
//
// Skips the first two coordinate tokens (x and y) and extracts the binx integer,
// returning 1 as a fallback if the click falls outside valid bounds.
//==============================================================================
int QMainCanvas::getBinFromClick(int x, int y)
{
    // Ensure the clicked histogram pad is selected and activated
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return 1;

    std::string objectInfo = hist->GetObjectInfo(x, y);

    // Skip section 1: x coordinate position
    size_t to = objectInfo.find(" ");
    if (to == std::string::npos) return 1;
    objectInfo = objectInfo.substr(to + 1);

    // Skip section 2: y coordinate position
    to = objectInfo.find(" ");
    if (to == std::string::npos) return 1;
    objectInfo = objectInfo.substr(to + 1);

    // Extract section 3: binx (channel index)
    size_t from = objectInfo.find("=");
    to = objectInfo.find(" ");
    if (from == std::string::npos || to == std::string::npos || to <= from + 1) return 1;

    try {
        std::string temp = objectInfo.substr(from + 1, to - from - 2);
        return std::stoi(temp);
    } catch (...) {
        return 1;
    }
}

//==============================================================================
// QMainCanvas::clicked1
//==============================================================================
// Opens a file dialog allowing the user to select a gamma spectrum file,
// loads the data via TracknHistogram::LoadFromFile (supporting both binary
// uint32 and ASCII formats), updates the display, and preserves existing zoom.
// If the user cancels the dialog or loading fails, the previous spectrum
// is completely preserved.
//==============================================================================
void QMainCanvas::clicked1()
{
    // Display file chooser dialog defaulting to all files so extensionless spectra are visible
    QString selectedFilter = tr("All Files (*)");
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open Spectrum File"), QString(),
        tr("All Files (*);;Spectra (*.spe *.dat *.txt *.asc);;All Files (*.*)"),
        &selectedFilter);

    // If the dialog was cancelled or no file was chosen, keep the existing spectrum
    // untouched and restore keyboard focus to the spectrum canvas
    if (fileName.isEmpty()) {
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    // Delegate format detection, length configuration, and parsing to SpectrumImportDialog
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist) {
        std::cerr << "Active histogram is null or invalid." << std::endl;
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    SpectrumImportDialog importDlg(fileName, this);
    if (importDlg.exec() != QDialog::Accepted) {
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    const std::vector<double> &spectrumData = importDlg.getLoadedData();
    if (spectrumData.empty() || !trackHist->LoadFromData(spectrumData, fileName.toStdString())) {
        std::cerr << "Failed to load spectrum from: " << fileName.toStdString() << std::endl;
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    // Configure canvas background only after a file has been successfully loaded
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);

    // Update axes and leave 10% empty space on top of the spectrum
    HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->UnZoom();
    adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());
    HijC[SelectedElement_i][SelectedElement_j].push_back(
        (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());

    // Set color based on overlay index and draw to canvas pad
    const int activeColorIdx = (HijC[SelectedElement_i][SelectedElement_j].size() - 1) % colors_hist.size();
    HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[activeColorIdx]);
    HijC[SelectedElement_i][SelectedElement_j].back()->SetLineColor(colors_hist[activeColorIdx]);
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    // Redraw all previously loaded spectra on the same pad with "SAME" option
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size() - 1; ++k) {
        if (HijC[SelectedElement_i][SelectedElement_j][k]) {
            HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(
                colors_hist[k % colors_hist.size()]);
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
        }
    }

    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // If zoom markers are present, preserve the zoomed region and adjust Ymax
    int i = zoom_markers.size();
    if (i >= 2) {
        if (zoom_markers[i - 2] < zoom_markers[i - 1]) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(
                zoom_markers[i - 2], zoom_markers[i - 1]);
        } else if (zoom_markers[i - 1] < zoom_markers[i - 2]) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(
                zoom_markers[i - 1], zoom_markers[i - 2]);
        }
        adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
    }

    ColorTheFrameOfTheHistogram();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    m_currentSpectrumFile = fileName;
    m_currentSpectrumIndex = importDlg.getSelectedSpectrumIndex();
    m_currentSpectrumCount = importDlg.getTotalSpectraCount();
    m_currentSpectrumLength = importDlg.getSelectedLength();
    m_currentSpectrumFormat = importDlg.getSelectedFormat();

    if (labelSpectrumFile) {
        QString disp = QFileInfo(fileName).fileName();
        if (m_currentSpectrumCount > 1) {
            disp += QString("#%1").arg(m_currentSpectrumIndex);
        }
        labelSpectrumFile->setText(disp);
    }
    updateAxisStatusLabels();

    if (m_currentSpectrumCount > 1) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Loaded spectrum %1#%2 (%3 of %4, %5 channels)\n")
                .arg(QFileInfo(fileName).fileName())
                .arg(m_currentSpectrumIndex)
                .arg(m_currentSpectrumIndex + 1)
                .arg(m_currentSpectrumCount)
                .arg(spectrumData.size()));
    } else {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Loaded spectrum %1 (%2 channels)\n")
                .arg(QFileInfo(fileName).fileName())
                .arg(spectrumData.size()));
    }

    // Ensure keyboard focus returns to the canvas so spacebar immediately places markers
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::clickedW
//==============================================================================
// Opens the SpectrumExportDialog allowing the user to export the active spectrum
// to disk in Long (32-bit int), Float (32-bit), Double (64-bit), or Fortran
// ASCII (10I9) format, with custom or standard channel count, and supporting
// overwrite, append, or indexed direct-access write for multi-spectrum files.
// Upon successful export, updates labelOutputFile ("-> filename").
//==============================================================================
void QMainCanvas::clickedW()
{
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist || trackHist->GetNbinsX() <= 0) {
        QMessageBox::information(this, tr("No Spectrum Loaded"),
                                 tr("There is no active spectrum to export.\nPlease load a spectrum first with 'R'."));
        if (canvas) canvas->setFocus();
        return;
    }

    std::vector<double> data = trackHist->GetBinData();
    if (data.empty()) {
        QMessageBox::information(this, tr("Empty Spectrum"),
                                 tr("The active spectrum contains no channel data to export."));
        if (canvas) canvas->setFocus();
        return;
    }

    QString suggestedPath = m_currentOutputFile;
    if (suggestedPath.isEmpty() && !m_currentSpectrumFile.isEmpty()) {
        QFileInfo fi(m_currentSpectrumFile);
        suggestedPath = fi.dir().filePath(fi.baseName() + "_out.spe");
    }

    SpectrumExportDialog exportDlg(data, suggestedPath, m_currentSpectrumFormat, trackHist->GetNbinsX(), this);
    if (exportDlg.exec() != QDialog::Accepted) {
        if (canvas) canvas->setFocus();
        return;
    }

    m_currentOutputFile = exportDlg.getSelectedFilePath();

    if (labelOutputFile) {
        QString outName = QFileInfo(m_currentOutputFile).fileName();
        if (exportDlg.getExportMode() == SpectrumExportMode::WriteAtIndex) {
            outName += QString("#%1").arg(exportDlg.getTargetIndex());
        }
        labelOutputFile->setText(QString("-> %1").arg(outName));
    }

    QString modeDesc;
    if (exportDlg.getExportMode() == SpectrumExportMode::Append) {
        modeDesc = "appended";
    } else if (exportDlg.getExportMode() == SpectrumExportMode::WriteAtIndex) {
        modeDesc = QString("written at index #%1").arg(exportDlg.getTargetIndex());
    } else {
        modeDesc = "saved";
    }

    CommandPrompt::getInstance()->appendPlainText(
        QString("Exported spectrum (%1 channels) %2 to %3\n")
            .arg(exportDlg.getSelectedLength())
            .arg(modeDesc)
            .arg(QFileInfo(m_currentOutputFile).fileName()));

    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// Multi-Spectrum Navigation Slots (# - and # +)
//==============================================================================
void QMainCanvas::onSpectrumIncrement()
{
    stepSpectrumIndex(+1);
}

void QMainCanvas::onSpectrumDecrement()
{
    stepSpectrumIndex(-1);
}

void QMainCanvas::stepSpectrumIndex(int delta)
{
    if (m_currentSpectrumFile.isEmpty()) {
        CommandPrompt::getInstance()->appendPlainText("No spectrum file currently loaded.\n");
        return;
    }

    if (m_currentSpectrumCount <= 1) {
        CommandPrompt::getInstance()->appendPlainText("Current file is a single spectrum (no other spectra in file).\n");
        return;
    }

    const int targetIndex = m_currentSpectrumIndex + delta;
    if (targetIndex < 0) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Already at first spectrum (1 of %1).\n").arg(m_currentSpectrumCount));
        return;
    }
    if (targetIndex >= m_currentSpectrumCount) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Already at last spectrum (%1 of %1).\n").arg(m_currentSpectrumCount));
        return;
    }

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist) {
        return;
    }

    std::vector<double> spectrumData;
    QString err;
    if (!ReadSpectrumData(m_currentSpectrumFile.toStdString(), m_currentSpectrumFormat,
                          m_currentSpectrumLength, targetIndex, spectrumData, &err) || spectrumData.empty()) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Failed to read spectrum #%1: %2\n").arg(targetIndex).arg(err));
        return;
    }

    // Preserve active zoom window before loading new data (LoadFromData unzooms by default)
    TAxis *xAxis = HijF[SelectedElement_i][SelectedElement_j]->GetXaxis();
    bool wasZoomed = false;
    double prevXmin = 0.0, prevXmax = 0.0;
    if (xAxis && (xAxis->GetFirst() > 1 || xAxis->GetLast() < xAxis->GetNbins())) {
        wasZoomed = true;
        prevXmin = xAxis->GetBinLowEdge(xAxis->GetFirst());
        prevXmax = xAxis->GetBinUpEdge(xAxis->GetLast());
    }

    // Ensure baseline spectrum is present in HijC if it was previously empty
    if (HijC[SelectedElement_i][SelectedElement_j].empty()) {
        TH1F *baseClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        baseClone->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(baseClone);
    }

    if (!trackHist->LoadFromData(spectrumData, m_currentSpectrumFile.toStdString())) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Failed to load spectrum data for #%1.\n").arg(targetIndex));
        return;
    }

    m_currentSpectrumIndex = targetIndex;

    if (delta > 0) {
        // Xtrackn behavior: preserve previous spectra and overlay the new one with a different color
        TH1F *newClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        const int colorIdx = HijC[SelectedElement_i][SelectedElement_j].size() % colors_hist.size();
        newClone->SetLineColor(colors_hist[colorIdx]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(newClone);
    } else {
        // Stepping backwards (delta < 0, # -):
        // If overlays exist, remove the most recent overlay so navigation steps back cleanly
        if (HijC[SelectedElement_i][SelectedElement_j].size() > 1) {
            delete HijC[SelectedElement_i][SelectedElement_j].back();
            HijC[SelectedElement_i][SelectedElement_j].pop_back();
        } else {
            // Single spectrum displayed: replace it with the new target spectrum
            if (!HijC[SelectedElement_i][SelectedElement_j].empty()) {
                delete HijC[SelectedElement_i][SelectedElement_j].back();
                HijC[SelectedElement_i][SelectedElement_j].pop_back();
            }
            TH1F *newClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
            newClone->SetLineColor(colors_hist[0]);
            HijC[SelectedElement_i][SelectedElement_j].push_back(newClone);
        }
    }

    // Ensure line colors match their respective positions in colors_hist
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size(); ++k) {
        if (HijC[SelectedElement_i][SelectedElement_j][k]) {
            HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(
                colors_hist[k % colors_hist.size()]);
        }
    }

    const int activeColorIdx = (HijC[SelectedElement_i][SelectedElement_j].size() - 1) % colors_hist.size();
    HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[activeColorIdx]);

    // Restore zoom range if active
    if (wasZoomed) {
        HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(prevXmin, prevXmax);
    } else if (zoom_markers.size() >= 2) {
        std::size_t zm = zoom_markers.size();
        double zLow = std::min(zoom_markers[zm - 2], zoom_markers[zm - 1]);
        double zHigh = std::max(zoom_markers[zm - 2], zoom_markers[zm - 1]);
        HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(zLow, zHigh);
    }

    // Update axes and canvas considering all visible overlaid spectra
    adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    // Redraw all previous spectra in HijC with "SAME" option
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size() - 1; ++k) {
        if (HijC[SelectedElement_i][SelectedElement_j][k]) {
            HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(
                colors_hist[k % colors_hist.size()]);
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
        }
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    if (labelSpectrumFile) {
        QString disp = QString("%1#%2")
                           .arg(QFileInfo(m_currentSpectrumFile).fileName())
                           .arg(m_currentSpectrumIndex);
        labelSpectrumFile->setText(disp);
    }
    updateAxisStatusLabels();

    CommandPrompt::getInstance()->appendPlainText(
        QString("Loaded spectrum %1#%2 (%3 of %4, %5 channels)\n")
            .arg(QFileInfo(m_currentSpectrumFile).fileName())
            .arg(m_currentSpectrumIndex)
            .arg(m_currentSpectrumIndex + 1)
            .arg(m_currentSpectrumCount)
            .arg(spectrumData.size()));

    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::OpenColorSelectionDialog
//==============================================================================
// Delegates color theme configuration to Design module.
//==============================================================================
void QMainCanvas::OpenColorSelectionDialog() {
    openColorSelectionDialog(this, this);
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::Cal2pMain
//==============================================================================
// Delegates two-point energy calibration dialog to calib module.
//==============================================================================
void QMainCanvas::Cal2pMain() {
    // If puncte_calib2p has fewer than 2 markers, fall back to spacebar_markers if available
    if (puncte_calib2p.size() < 2 && spacebar_markers.size() >= 2) {
        puncte_calib2p.clear();
        for (Double_t sm : spacebar_markers) {
            puncte_calib2p.push_back(static_cast<Float_t>(sm));
        }
    }

    runTwoPointCalibrationDialog(
        this, puncte_calib2p,
        dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]));

    // Refresh peak centroid text labels on the active histogram if calibrated
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (trackHist && trackHist->IsCalibrated() && !gaussCenters[SelectedElement_i][SelectedElement_j].empty()) {
        renderPeakLabels(SelectedElement_i, SelectedElement_j);
    }

    if (canvas) {
        canvas->setFocus();
    }
}


//==============================================================================
// QMainCanvas::addSpaceBarMarker
//==============================================================================
// Places a cyan vertical marker line on the spectrum at the channel bin clicked
// by the user. Pairs of spacebar markers define the boundaries of the region of
// interest to be zoomed when the user subsequently presses 'E', and also serve
// as reference markers for two-point energy calibration (Cal2P).
//==============================================================================
void QMainCanvas::addSpaceBarMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);

    // Record the channel in spacebar, zoom, and calibration marker collections
    spacebar_markers.push_back(static_cast<Double_t>(binX));
    zoom_markers.push_back(binX);
    puncte_calib2p.push_back(static_cast<Float_t>(binX));

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    const Double_t yMax = hist ? (hist->GetMaximum() * 1.05) : 100.0;

    // Create a cyan vertical marker line and draw it over the spectrum
    TLine *spacebarLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    spacebarLine->SetLineColor(kCyan);
    spacebarLine->SetLineWidth(2);
    spacebarLine->Draw("same");

    // Register graphical object so it can be cleared on reset or zoom
    listOfObjectsDrawnOnScreen.Add(spacebarLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::areaFunction
//==============================================================================
// Computes gross peak area without background subtraction (triggered by 'C + I'
// shortcut). Delegates calculation to integral_function() in Integral.h with
// an empty background marker set, and overlays peak index labels on the canvas.
//==============================================================================
void QMainCanvas::areaFunction()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    if (integral_markers.empty()) {
        const QString msg = "Place markers with 'I' for the integral to be calculated.\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
        return;
    }

    std::vector<Int_t> placeholder_background_markers;
    std::vector<IntegratedPeak> peaks;
    integral_function(hist,
                      integral_markers,
                      placeholder_background_markers,
                      slope,
                      addition,
                      &peaks);

    // Render peak index labels on the canvas above peak centroids
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    for (const auto &peak : peaks) {
        Int_t bin = hist->FindBin(peak.centroid);
        Double_t peakY = hist->GetBinContent(bin);
        if (peakY <= 0.0) peakY = hist->GetMaximum() * 0.5;
        Double_t labelY = peakY * 1.05;

        char labelBuf[32];
        if (peak.isCalibrated) {
            snprintf(labelBuf, sizeof(labelBuf), "[%d] %.1f", peak.index, peak.energy);
        } else {
            snprintf(labelBuf, sizeof(labelBuf), "[%d]", peak.index);
        }

        TLatex *lbl = new TLatex(peak.centroid, labelY, labelBuf);
        lbl->SetName(Form("IntPeakLabel_%d", peak.index));
        lbl->SetTextFont(43);
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::areaFunctionWithBackground
//==============================================================================
// Computes net peak area (triggered by 'Int' UI button or 'C + J' shortcut).
// If background markers exist, fits linear background and overlays baseline;
// if no background markers exist, gracefully falls back to gross peak integration.
// Labels peak index above peak centroid on the canvas.
//==============================================================================
void QMainCanvas::areaFunctionWithBackground()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    if (integral_markers.empty()) {
        const QString msg = "Place markers with 'I' for the integral to be calculated.\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
        return;
    }

    std::vector<IntegratedPeak> peaks;
    integral_function(hist,
                      integral_markers,
                      background_markers,
                      slope,
                      addition,
                      &peaks);

    // If background markers exist, draw blue baseline
    if (!background_markers.empty() && background_markers.size() >= 2) {
        const Double_t xStart = background_markers.front() - 0.5;
        const Double_t xEnd   = background_markers.back() - 0.5;
        TLine *backgroundLine = new TLine(xStart, slope * xStart + addition,
                                          xEnd,   slope * xEnd   + addition);
        backgroundLine->SetLineColor(kBlue);
        backgroundLine->SetLineWidth(2);
        backgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundLine);
    }

    // Render peak index labels on the canvas above peak centroids
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    for (const auto &peak : peaks) {
        Int_t bin = hist->FindBin(peak.centroid);
        Double_t peakY = hist->GetBinContent(bin);
        if (peakY <= 0.0) peakY = hist->GetMaximum() * 0.5;
        Double_t labelY = peakY * 1.05;

        char labelBuf[32];
        if (peak.isCalibrated) {
            snprintf(labelBuf, sizeof(labelBuf), "[%d] %.1f", peak.index, peak.energy);
        } else {
            snprintf(labelBuf, sizeof(labelBuf), "[%d]", peak.index);
        }

        TLatex *lbl = new TLatex(peak.centroid, labelY, labelBuf);
        lbl->SetName(Form("IntPeakLabel_%d", peak.index));
        lbl->SetTextFont(43);
        lbl->SetTextSize(18);
        lbl->SetTextColor(kCyan);
        lbl->SetTextAlign(21);
        lbl->Draw("same");
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::autoFit
//==============================================================================
// Delegates automated single-peak Gaussian fitting to the PeakFit module.
//==============================================================================
void QMainCanvas::autoFit(int x, int y)
{
    runAutoFit(this, x, y);
}

//==============================================================================
// QMainCanvas::findMinValueInInterval
//==============================================================================
// Delegates interval minimum search to PeakFit module.
//==============================================================================
Double_t QMainCanvas::findMinValueInInterval(int intervalStart, int intervalFinish)
{
    return ::findMinValueInInterval(HijF[SelectedElement_i][SelectedElement_j],
                                    intervalStart, intervalFinish);
}

//==============================================================================
// QMainCanvas::findMaxValueInInterval
//==============================================================================
// Delegates interval maximum search to PeakFit module.
//==============================================================================
Double_t QMainCanvas::findMaxValueInInterval(int intervalStart, int intervalFinish)
{
    return ::findMaxValueInInterval(HijF[SelectedElement_i][SelectedElement_j],
                                    intervalStart, intervalFinish);
}

//==============================================================================
// QMainCanvas::addBackgroundMarker
//==============================================================================
// Drops a blue vertical background marker at the clicked channel coordinate.
// Background markers are placed in pairs [left, right]. When an even marker
// completes a pair, a blue baseline and a hatched shaded box (fill style 3545)
// are drawn across the background estimation interval.
//==============================================================================
void QMainCanvas::addBackgroundMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    background_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    // When placing the second marker of a pair, ensure previous boundary is redrawn
    if (background_markers.size() % 2 == 0 && !background_markers.empty()) {
        const std::size_t i = background_markers.size();
        TLine *backgroundLineSecond = new TLine(background_markers[i - 2] - 0.5, 0.0,
                                                background_markers[i - 2] - 0.5, yMax);
        backgroundLineSecond->SetLineColor(kBlue);
        backgroundLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        backgroundLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(backgroundLineSecond);
    }

    // Draw vertical boundary line at current clicked position
    TLine *backgroundLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);
    backgroundLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(backgroundLine);

    // When completing a pair, draw baseline and hatched region
    if (background_markers.size() % 2 == 0) {
        const Int_t leftBin = background_markers[background_markers.size() - 2];
        TLine *bottomBackgroundLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
        bottomBackgroundLine->SetLineColor(kBlue);
        bottomBackgroundLine->SetLineWidth(2);
        bottomBackgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

        TBox *backgroundArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
        backgroundArea->SetFillColor(kBlue);
        backgroundArea->SetFillStyle(3545);
        backgroundArea->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundArea);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::addIntegralMarker
//==============================================================================
// Drops a yellow vertical marker line at the clicked channel coordinate marking
// peak integration boundaries.
//==============================================================================
void QMainCanvas::addIntegralMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    integral_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    // If second marker of pair, ensure the first is properly drawn
    if (integral_markers.size() % 2 == 0 && !integral_markers.empty()) {
        const std::size_t i = integral_markers.size();
        TLine *integralLineSecond = new TLine(integral_markers[i - 2] - 0.5, 0.0,
                                              integral_markers[i - 2] - 0.5, yMax);
        integralLineSecond->SetLineColor(kYellow);
        integralLineSecond->SetLineWidth(2);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        integralLineSecond->Draw();
        listOfObjectsDrawnOnScreen.Add(integralLineSecond);
    }

    TLine *integralLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    integralLine->SetLineColor(kYellow);
    integralLine->SetLineWidth(2);
    integralLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(integralLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::clearTheScreen
//==============================================================================
// Clears all visual marker lines, shaded boxes, and fit curves from the screen,
// freeing the graphical objects from memory without modifying the histogram data.
// Triggered by the '=' key shortcut.
//==============================================================================
void QMainCanvas::clearTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Free all dynamically allocated graphical primitives drawn on the canvas
    clearDrawnObjects();

    // Clear fit markers and redraw clean base histogram
    autoFitMarkers[SelectedElement_i][SelectedElement_j].clear();
    gaussCenters[SelectedElement_i][SelectedElement_j].clear();
    gaussCentersHeight[SelectedElement_i][SelectedElement_j].clear();
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (HijF[SelectedElement_i][SelectedElement_j]) {
        HijF[SelectedElement_i][SelectedElement_j]->Draw();

        for (auto *h : HijC[SelectedElement_i][SelectedElement_j]) {
            delete h;
        }
        HijC[SelectedElement_i][SelectedElement_j].clear();
        HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[0]);
        TH1F *baseClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        baseClone->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(baseClone);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::zoomTheScreen
//==============================================================================
// Executes zoom between the two most recently placed spacebar markers.
// Triggered by the 'E' key shortcut.
//==============================================================================
void QMainCanvas::zoomTheScreen()
{
    // Clear temporary overlay markers before applying zoom
    clearDrawnObjects();

    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    const std::size_t n = zoom_markers.size();

    if (n >= 2) {
        const double low  = std::min(zoom_markers[n - 2], zoom_markers[n - 1]);
        const double high = std::max(zoom_markers[n - 2], zoom_markers[n - 1]);
        if (HijF[SelectedElement_i][SelectedElement_j]) {
            TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
            hist->GetXaxis()->SetRangeUser(low, high);
            adjustYAxisToVisibleMax(hist);
        }
    } else {
        const QString msg = "At least two spacebar markers are required to define a zoom window.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::zoomOut
//==============================================================================
// Resets spectrum axes to their full unzoomed range with 10% empty space at top.
// Triggered by 'F + F' or 'F + S' keyboard shortcuts or 'FF' button.
//==============================================================================
void QMainCanvas::zoomOut()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        // Reset both X and Y axis ranges to unzoomed full spectrum scale with 10% headroom
        hist->GetXaxis()->UnZoom();
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::toggleLogY
//==============================================================================
// Toggles logarithmic vs linear Y-axis scale on the active histogram pad
// (corresponding to the 'L' button in Xtrackn).
//==============================================================================
void QMainCanvas::toggleLogY()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) return;
    if (!canvas || !canvas->getCanvas()) return;

    TVirtualPad *pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (!pad) pad = canvas->getCanvas();
    if (pad) {
        pad->SetLogy(pad->GetLogy() ? 0 : 1);
        pad->Modified();
        pad->Update();
    }
    updateAxisStatusLabels();
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::adjustYAxisToVisibleMax
//==============================================================================
// Scans the visible X-axis bin range of the given histogram, calculates the
// highest count within that range, and sets the Y-axis maximum to leave 10%
// empty space at the top of the spectrum so the largest peak does not touch
// the top border.
//==============================================================================
void QMainCanvas::adjustYAxisToVisibleMax(TH1F *hist, int z, int g)
{
    if (!hist) return;
    TAxis *xAxis = hist->GetXaxis();
    if (!xAxis) return;

    Int_t firstBin = xAxis->GetFirst();
    Int_t lastBin  = xAxis->GetLast();
    if (firstBin < 1) firstBin = 1;
    if (lastBin > hist->GetNbinsX()) lastBin = hist->GetNbinsX();

    double localMax = 0.0;
    for (Int_t b = firstBin; b <= lastBin; ++b) {
        double content = hist->GetBinContent(b);
        if (content > localMax) {
            localMax = content;
        }
    }

    // Also consider overlaid spectra on the target pad to avoid clipping peaks
    const int targetZ = (z >= 1 && z < 12) ? z : SelectedElement_i;
    const int targetG = (g >= 1 && g < 12) ? g : SelectedElement_j;
    if (targetZ >= 1 && targetZ < 12 && targetG >= 1 && targetG < 12) {
        for (TH1F *overlay : HijC[targetZ][targetG]) {
            if (!overlay || overlay == hist) continue;
            Int_t ovFirst = firstBin;
            Int_t ovLast  = lastBin;
            if (ovFirst < 1) ovFirst = 1;
            if (ovLast > overlay->GetNbinsX()) ovLast = overlay->GetNbinsX();
            for (Int_t b = ovFirst; b <= ovLast; ++b) {
                double content = overlay->GetBinContent(b);
                if (content > localMax) {
                    localMax = content;
                }
            }
        }
    }

    if (localMax <= 0.0) {
        localMax = 10.0;
    }

    // Leave 10% empty space above the largest visible peak
    hist->GetYaxis()->SetRangeUser(0.0, localMax * 1.10);
}

//==============================================================================
// QMainCanvas::renderPeakLabels
//==============================================================================
// Removes any existing TLatex peak centroid annotations from the specified pad
// and renders the current peak list (gaussCenters) with a fixed pixel font size
// (does not resize with the window), displaying calibrated energy if available.
//==============================================================================
void QMainCanvas::renderPeakLabels(int z, int g)
{
    if (z < 1 || z >= 12 || g < 1 || g >= 12) return;
    if (!HijF[z][g] || !canvas || !canvas->getCanvas()) return;

    if (maxElement_i > 1 || maxElement_j > 1) {
        int padIndex = (z - 1) * maxElement_j + g;
        canvas->getCanvas()->cd(padIndex);
    } else {
        canvas->getCanvas()->cd();
    }
    TVirtualPad *pad = gPad;
    if (!pad) return;

    // Remove any previously drawn peak labels from the pad to avoid overlapping text
    TList *primitives = pad->GetListOfPrimitives();
    if (primitives) {
        std::vector<TObject*> toRemove;
        TIter next(primitives);
        while (TObject *obj = next()) {
            if (obj && obj->InheritsFrom(TLatex::Class())) {
                toRemove.push_back(obj);
            }
        }
        for (TObject *obj : toRemove) {
            primitives->Remove(obj);
            delete obj;
        }
    }

    // Render each fitted peak label with fixed pixel font size
    TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(HijF[z][g]);
    for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
        Double_t center = gaussCenters[z][g][k];
        Double_t height = (k < gaussCentersHeight[z][g].size()) ? gaussCentersHeight[z][g][k] : 0.0;
        char buffer[64];
        if (tHist && tHist->IsCalibrated()) {
            snprintf(buffer, sizeof(buffer), "%.2f", tHist->ChannelToEnergy(center));
        } else {
            snprintf(buffer, sizeof(buffer), "%.2f", center);
        }

        TLatex *lbl = new TLatex(center, height, buffer);
        lbl->SetName("PeakLabel");
        lbl->SetTextFont(43); // Font 4 (Helvetica), Precision 3 (exact screen pixels)
        lbl->SetTextSize(20); // Fixed 20-pixel font size (increased by ~50% from 13px)
        lbl->SetTextAlign(21); // Centered horizontally at peak center, bottom-aligned
        lbl->Draw();
    }

    pad->Modified();
    pad->Update();
}

//==============================================================================
// QMainCanvas::updateAxisStatusLabels
//==============================================================================
// Updates the X Min, X Max, Y Min, and Y Max status display labels in the
// top status header bar to reflect the current visible axis ranges.
//==============================================================================
void QMainCanvas::updateAxisStatusLabels()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    TAxis *xAxis = hist->GetXaxis();
    double xMin = xAxis ? xAxis->GetBinLowEdge(xAxis->GetFirst()) : 0.0;
    double xMax = xAxis ? xAxis->GetBinUpEdge(xAxis->GetLast()) : 0.0;
    double yMin = hist->GetMinimum();
    double yMax = hist->GetMaximum();

    auto formatCoord = [](double val) -> QString {
        if (std::abs(val) <= 100000.0) {
            if (std::abs(val - std::round(val)) < 1e-5) {
                return QString::number(static_cast<long long>(std::round(val)));
            } else {
                return QString::number(val, 'f', 2);
            }
        } else {
            return QString::number(val, 'e', 2);
        }
    };

    if (labelXMin) labelXMin->setText(QString("X Min: %1").arg(formatCoord(xMin)));
    if (labelXMax) labelXMax->setText(QString("X Max: %1").arg(formatCoord(xMax)));
    if (labelYMin) labelYMin->setText(QString("Y Min: %1").arg(formatCoord(yMin)));
    if (labelYMax) labelYMax->setText(QString("Y Max: %1").arg(formatCoord(yMax)));
}

//==============================================================================
// QMainCanvas::translateplusTheScreen
//==============================================================================
// Pans the spectrum display horizontally to the right (toward higher channels)
// by ~3% of the visible window width. Triggered by the Right Arrow key.
//==============================================================================
void QMainCanvas::translateplusTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Remove drawn lines/markers when panning
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first  = xAxis->GetFirst();
        const Int_t last   = xAxis->GetLast();
        const Int_t step   = std::max(1, (last - first) / 33);
        const Int_t maxBin = hist->GetNbinsX();
        const Int_t newFirst = std::min(maxBin, first + step);
        const Int_t newLast  = std::min(maxBin, last + step);
        xAxis->SetRange(newFirst, newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::translateminusTheScreen
//==============================================================================
// Pans the spectrum display horizontally to the left (toward lower channels)
// by ~3% of the visible window width. Triggered by the Left Arrow key.
//==============================================================================
void QMainCanvas::translateminusTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // Remove drawn lines/markers when panning
    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        TAxis *xAxis = hist->GetXaxis();
        const Int_t first    = xAxis->GetFirst();
        const Int_t last     = xAxis->GetLast();
        const Int_t step     = std::max(1, (last - first) / 33);
        const Int_t newFirst = std::max(1, first - step);
        const Int_t newLast  = std::max(1, last - step);
        xAxis->SetRange(newFirst, newLast);
        adjustYAxisToVisibleMax(hist);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::translatedownTheScreen
//==============================================================================
// Expands the vertical count scale (zooms out vertically by 1.10x).
// Triggered by the Down Arrow key or mouse wheel scroll up.
//==============================================================================
void QMainCanvas::translatedownTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        double curMax = hist->GetMaximum();
        if (curMax <= 0.0) curMax = 10.0;
        hist->GetYaxis()->SetRangeUser(0, curMax * 1.10);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::translateupTheScreen
//==============================================================================
// Compresses the vertical count scale (zooms in vertically by 1.10x).
// Triggered by the Up Arrow key or mouse wheel scroll down.
//==============================================================================
void QMainCanvas::translateupTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        double curMax = hist->GetMaximum();
        if (curMax > 2.0) {
            hist->GetYaxis()->SetRangeUser(0, curMax / 1.10);
        }
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    updateAxisStatusLabels();
}

//==============================================================================
// QMainCanvas::deleteBackgroundMarkers
//==============================================================================
// Clears all stored background marker positions (shortcut: 'Z + B').
//==============================================================================
void QMainCanvas::deleteBackgroundMarkers()
{
    background_markers.clear();
}

//==============================================================================
// QMainCanvas::deleteIntegralMarkers
//==============================================================================
// Clears all stored peak integration marker positions (shortcut: 'Z + I').
//==============================================================================
void QMainCanvas::deleteIntegralMarkers()
{
    integral_markers.clear();
}

//==============================================================================
// QMainCanvas::deleteAllMarkers
//==============================================================================
// Clears all active markers of all types from memory (shortcut: 'Z + A').
//==============================================================================
void QMainCanvas::deleteAllMarkers()
{
    deleteBackgroundMarkers();
    deleteIntegralMarkers();
    deleteRangeMarkers();
    deleteGaussMarkers();
}

//==============================================================================
// QMainCanvas::showBackgroundMarkers
//==============================================================================
// Re-renders all stored background markers, baselines, and shaded regions on
// the canvas (shortcut: 'M + B').
//==============================================================================
void QMainCanvas::showBackgroundMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    for (std::size_t i = 0; i < background_markers.size(); ++i) {
        TLine *backgroundLine = new TLine(background_markers[i] - 0.5, 0.0,
                                          background_markers[i] - 0.5, yMax);
        backgroundLine->SetLineColor(kBlue);
        backgroundLine->SetLineWidth(2);
        backgroundLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(backgroundLine);

        // When completing a pair, redraw baseline and hatched box
        if (i % 2 == 1) {
            TLine *bottomBackgroundLine = new TLine(background_markers[i - 1] - 0.5, 0.0,
                                                    background_markers[i] - 0.5, 0.0);
            bottomBackgroundLine->SetLineColor(kBlue);
            bottomBackgroundLine->SetLineWidth(2);
            bottomBackgroundLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

            TBox *backgroundArea = new TBox(background_markers[i - 1] - 0.5, 0.0,
                                            background_markers[i] - 0.5, yMax);
            backgroundArea->SetFillColor(kBlue);
            backgroundArea->SetFillStyle(3545);
            backgroundArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(backgroundArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showIntegralMarkers
//==============================================================================
// Re-renders all stored yellow peak integral markers on the canvas (shortcut: 'M + I').
//==============================================================================
void QMainCanvas::showIntegralMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    for (std::size_t i = 0; i < integral_markers.size(); ++i) {
        TLine *integralLine = new TLine(integral_markers[i] - 0.5, 0.0,
                                        integral_markers[i] - 0.5, yMax);
        integralLine->SetLineColor(kYellow);
        integralLine->SetLineWidth(2);
        integralLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(integralLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showAllMarkers
//==============================================================================
// Redraws all stored markers (background, integral, range, and Gauss) on the
// canvas (shortcut: 'M + A').
//==============================================================================
void QMainCanvas::showAllMarkers()
{
    showBackgroundMarkers();
    showIntegralMarkers();
    showRangeMarkers();
    showGaussMarkers();
}

//==============================================================================
// QMainCanvas::addRangeMarker
//==============================================================================
// Drops a red vertical range boundary marker at the clicked channel. Exactly two
// range markers define the region for multi-peak Gaussian fitting.
//==============================================================================
void QMainCanvas::addRangeMarker(Int_t x, Int_t y)
{
    if (range_markers.size() < 2) {
        int binX = getBinFromClick(x, y);
        range_markers.push_back(binX);

        TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
        if (!hist) return;

        const Double_t yMax = hist->GetMaximum() * 1.05;

        // When placing the second marker of a pair, ensure the first is drawn
        if (range_markers.size() % 2 == 0 && !range_markers.empty()) {
            const std::size_t i = range_markers.size();
            TLine *rangeLineSecond = new TLine(range_markers[i - 2] - 0.5, 0.0,
                                               range_markers[i - 2] - 0.5, yMax);
            rangeLineSecond->SetLineColor(kRed);
            rangeLineSecond->SetLineWidth(2);
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            rangeLineSecond->Draw();
            listOfObjectsDrawnOnScreen.Add(rangeLineSecond);
        }

        TLine *rangeLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
        rangeLine->SetLineColor(kRed);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        // If pair is complete, draw baseline and shaded red region
        if (range_markers.size() % 2 == 0) {
            const Int_t leftBin = range_markers[range_markers.size() - 2];
            TLine *bottomRangeLine = new TLine(leftBin - 0.5, 0.0, binX - 0.5, 0.0);
            bottomRangeLine->SetLineColor(kRed);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(leftBin - 0.5, 0.0, binX - 0.5, maxValueInHistogram * 1.05);
            rangeArea->SetFillColor(kRed);
            rangeArea->SetFillStyle(3545);
            rangeArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(rangeArea);
        }

        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    } else {
        // Warn user if attempting to place more than 2 range markers
        printf("\a");
        CommandPrompt::getInstance()->appendPlainText("Fitting range is already defined by two markers (use Z+R to clear).\n");
    }
}

//==============================================================================
// QMainCanvas::deleteRangeMarkers
//==============================================================================
// Clears stored fit range boundary markers (shortcut: 'Z + R').
//==============================================================================
void QMainCanvas::deleteRangeMarkers()
{
    range_markers.clear();
}

//==============================================================================
// QMainCanvas::showRangeMarkers
//==============================================================================
// Re-renders all stored fit range markers and shaded intervals on the canvas
// (shortcut: 'M + R').
//==============================================================================
void QMainCanvas::showRangeMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    for (std::size_t i = 0; i < range_markers.size(); ++i) {
        TLine *rangeLine = new TLine(range_markers[i] - 0.5, 0.0,
                                     range_markers[i] - 0.5, yMax);
        rangeLine->SetLineColor(kRed);
        rangeLine->SetLineWidth(2);
        rangeLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(rangeLine);

        if (i % 2 == 1) {
            TLine *bottomRangeLine = new TLine(range_markers[i - 1] - 0.5, 0.0,
                                               range_markers[i] - 0.5, 0.0);
            bottomRangeLine->SetLineColor(kRed);
            bottomRangeLine->SetLineWidth(2);
            bottomRangeLine->Draw("same");
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *rangeArea = new TBox(range_markers[i - 1] - 0.5, 0.0,
                                       range_markers[i] - 0.5, yMax);
            rangeArea->SetFillColor(kRed);
            rangeArea->SetFillStyle(3545);
            rangeArea->Draw("same");
            listOfObjectsDrawnOnScreen.Add(rangeArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::addGaussMarker
//==============================================================================
// Drops a pink vertical marker line at the clicked channel coordinate marking
// an initial peak centroid estimate for multi-peak Gaussian fitting.
//==============================================================================
void QMainCanvas::addGaussMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);
    gauss_markers.push_back(binX);

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    TLine *gaussLine = new TLine(binX - 0.5, 0.0, binX - 0.5, yMax);
    gaussLine->SetLineColor(kPink);
    gaussLine->SetLineWidth(2);
    gaussLine->Draw("same");
    listOfObjectsDrawnOnScreen.Add(gaussLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::deleteGaussMarkers
//==============================================================================
// Clears all stored Gaussian peak centroid estimate markers (shortcut: 'Z + G').
//==============================================================================
void QMainCanvas::deleteGaussMarkers()
{
    gauss_markers.clear();
}

//==============================================================================
// QMainCanvas::showGaussMarkers
//==============================================================================
// Re-renders all stored pink Gaussian centroid estimate markers on the canvas
// (shortcut: 'M + G').
//==============================================================================
void QMainCanvas::showGaussMarkers()
{
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    const Double_t yMax = hist->GetMaximum() * 1.05;

    for (std::size_t i = 0; i < gauss_markers.size(); ++i) {
        TLine *gaussLine = new TLine(gauss_markers[i] - 0.5, 0.0,
                                     gauss_markers[i] - 0.5, yMax);
        gaussLine->SetLineColor(kPink);
        gaussLine->SetLineWidth(2);
        gaussLine->Draw("same");
        listOfObjectsDrawnOnScreen.Add(gaussLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}


//==============================================================================
// QMainCanvas::fitGauss
//==============================================================================
// Delegates multi-peak Gaussian fitting with background to the PeakFit module.
//==============================================================================
void QMainCanvas::fitGauss()
{
    runMultiPeakFit(this);
}

//==============================================================================
// QMainCanvas::handle_root_events
//==============================================================================
// Processes pending ROOT events by invoking gSystem->ProcessEvents().
// Called periodically via fRootTimer (every 20 ms) to keep the ROOT graphics
// pipeline responsive within the Qt event loop.
//==============================================================================
void QMainCanvas::handle_root_events()
{
    if (gSystem) {
        gSystem->ProcessEvents();
    }
}

//==============================================================================
// QMainCanvas::changeEvent
//==============================================================================
// Handles Qt window state change events (minimize, maximize, restore).
// Resizes and updates the ROOT TCanvas when the window is maximized or restored
// so that the sub-pads and spectra scale correctly with the window geometry.
//==============================================================================
void QMainCanvas::changeEvent(QEvent *e)
{
    if (e && e->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *event = static_cast<QWindowStateChangeEvent*>(e);
        if ((event->oldState() & Qt::WindowMaximized) ||
            (event->oldState() & Qt::WindowMinimized) ||
            (event->oldState() == Qt::WindowNoState && this->windowState() == Qt::WindowMaximized)) {
            if (canvas && canvas->getCanvas()) {
                canvas->getCanvas()->Resize();
                canvas->getCanvas()->Update();
            }
        }
    }
    QWidget::changeEvent(e);
}

//==============================================================================
// QMainCanvas::AddCulomn
//==============================================================================
// Adds a new column of sub-pads to the canvas grid (shortcut: Ctrl + Right).
// Re-divides the ROOT TCanvas into (maxElement_j + 1) columns by maxElement_i
// rows. Existing histograms and overlaid spectra are cloned and re-rendered into
// their corresponding pads, and the selected histogram is duplicated into the
// newly added column.
//==============================================================================
void QMainCanvas::AddCulomn()
{
    // HijF is dimensioned [12][12]; indices range from 1 to 10 safely
    if (maxElement_j >= 10) return;
    if (!canvas || !canvas->getCanvas()) return;

    if (maxElement_i == 1 && maxElement_j == 1 && HijF[1][1]) {
        selectedHisto = static_cast<TracknHistogram*>(
            HijF[1][1]->Clone("h11f"));
    } else if (!selectedHisto && HijF[SelectedElement_i][SelectedElement_j]) {
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    maxElement_j++;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    rootCanvas->SetFillColor(0);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";

            if (g == maxElement_j) {
                if (selectedHisto) {
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(histTitle.c_str()));
                    if (HijF[z][g]) {
                        while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                            HijF[z][g]->GetListOfFunctions()->RemoveLast();
                        }
                        HijF[z][g]->SetLineColor(4);
                        HijF[z][g]->SetTitle(histTitle.c_str());
                    }
                    HijC[z][g].push_back(static_cast<TH1F*>(selectedHisto->Clone()));
                }
            } else if (HijF[z][g]) {
                HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(histTitle.c_str()));
                HijF[z][g]->SetTitle(histTitle.c_str());
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::AddLine
//==============================================================================
// Adds a new row of sub-pads to the canvas grid (shortcut: Ctrl + Up).
// Re-divides the ROOT TCanvas into maxElement_j columns by (maxElement_i + 1)
// rows. Existing histograms and overlaid spectra are cloned and re-rendered into
// their corresponding pads, and the selected histogram is duplicated into the
// newly added row.
//==============================================================================
void QMainCanvas::AddLine()
{
    // HijF is dimensioned [12][12]; indices range from 1 to 10 safely
    if (maxElement_i >= 10) return;
    if (!canvas || !canvas->getCanvas()) return;

    if (maxElement_i == 1 && maxElement_j == 1 && HijF[1][1]) {
        selectedHisto = static_cast<TracknHistogram*>(
            HijF[1][1]->Clone("h11f"));
    } else if (!selectedHisto && HijF[SelectedElement_i][SelectedElement_j]) {
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    maxElement_i++;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    rootCanvas->SetFillColor(0);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";

            if (z == maxElement_i) {
                if (selectedHisto) {
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(histTitle.c_str()));
                    if (HijF[z][g]) {
                        while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                            HijF[z][g]->GetListOfFunctions()->RemoveLast();
                        }
                        HijF[z][g]->SetLineColor(4);
                        HijF[z][g]->SetTitle(histTitle.c_str());
                    }
                    HijC[z][g].push_back(static_cast<TH1F*>(selectedHisto->Clone()));
                }
            } else if (HijF[z][g]) {
                HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(histTitle.c_str()));
                HijF[z][g]->SetTitle(histTitle.c_str());
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::IdentifyLastClickedHistogram
//==============================================================================
// Identifies which sub-pad cell in the (row, column) grid was selected by the
// user via a left mouse click at pixel coordinates (x, y).
// Updates SelectedElement_i, SelectedElement_j, and selectedHisto, then highlights
// the active histogram with an azure border frame.
//==============================================================================
void QMainCanvas::IdentifyLastClickedHistogram(Double_t x, Double_t y)
{
    mouseLeftClickXcoord = x;
    mouseLeftClickYcoord = y;

    if (!canvas || !canvas->getCanvas()) return;
    if (maxElement_i <= 0 || maxElement_j <= 0) return;

    const UInt_t canvasWidth = canvas->getCanvas()->GetWw();
    const UInt_t canvasHeight = canvas->getCanvas()->GetWh();
    if (canvasWidth == 0 || canvasHeight == 0) return;

    for (int j = 0; j < maxElement_j; ++j) {
        if (j * canvasWidth / maxElement_j <= mouseLeftClickXcoord &&
            mouseLeftClickXcoord < (j + 1) * canvasWidth / maxElement_j) {
            SelectedElement_j = j + 1;
            break;
        }
    }

    for (int h = 0; h < maxElement_i; ++h) {
        if (h * canvasHeight / maxElement_i <= mouseLeftClickYcoord &&
            mouseLeftClickYcoord < (h + 1) * canvasHeight / maxElement_i) {
            SelectedElement_i = h + 1;
            break;
        }
    }

    SelectedElement_j = std::max(1, std::min(SelectedElement_j, maxElement_j));
    SelectedElement_i = std::max(1, std::min(SelectedElement_i, maxElement_i));

    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];

    ColorTheFrameOfTheHistogram();
}

//==============================================================================
// QMainCanvas::IdentifyLastPilgrimHistogram
//==============================================================================
// Tracks the current cursor position as the mouse moves across the canvas.
// Determines which histogram grid pad the cursor is currently hovering over
// and updates PilgrimElement_i and PilgrimElement_j accordingly.
//==============================================================================
void QMainCanvas::IdentifyLastPilgrimHistogram(Double_t x, Double_t y)
{
    mousePilgrimX = x;
    mousePilgrimY = y;

    if (!canvas || !canvas->getCanvas()) return;
    if (maxElement_i <= 0 || maxElement_j <= 0) return;

    const UInt_t canvasWidth = canvas->getCanvas()->GetWw();
    const UInt_t canvasHeight = canvas->getCanvas()->GetWh();
    if (canvasWidth == 0 || canvasHeight == 0) return;

    for (int j = 0; j < maxElement_j; ++j) {
        if (j * canvasWidth / maxElement_j <= mousePilgrimX &&
            mousePilgrimX < (j + 1) * canvasWidth / maxElement_j) {
            PilgrimElement_j = j + 1;
            break;
        }
    }

    for (int h = 0; h < maxElement_i; ++h) {
        if (h * canvasHeight / maxElement_i <= mousePilgrimY &&
            mousePilgrimY < (h + 1) * canvasHeight / maxElement_i) {
            PilgrimElement_i = h + 1;
            break;
        }
    }

    PilgrimElement_j = std::max(1, std::min(PilgrimElement_j, maxElement_j));
    PilgrimElement_i = std::max(1, std::min(PilgrimElement_i, maxElement_i));

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::ColorTheFrameOfTheHistogram
//==============================================================================
// Draws an azure border frame around the currently selected histogram sub-pad
// when multiple sub-pads are visible (maxElement_i > 1 or maxElement_j > 1),
// visually indicating which pad has active keyboard and analysis focus.
//==============================================================================
void QMainCanvas::ColorTheFrameOfTheHistogram()
{
    delete lineR; lineR = nullptr;
    delete lineL; lineL = nullptr;
    delete lineD; lineD = nullptr;
    delete lineU; lineU = nullptr;

    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    if (maxElement_i > 1 || maxElement_j > 1) {
        const Double_t xFirst = hist->GetXaxis()->GetFirst() - 1;
        const Double_t xLast  = hist->GetXaxis()->GetLast();
        const Double_t xMax   = hist->GetXaxis()->GetXmax();
        const Double_t yMax   = hist->GetMaximum();

        lineR = new TLine(xFirst, 0.0, xFirst, yMax);
        lineL = new TLine(xLast, 0.0, xLast, yMax);
        lineU = new TLine(hist->GetXaxis()->GetFirst(), yMax, xLast, yMax);
        lineD = new TLine(0.0, 0.0, xMax, 0.0);

        const Color_t frameColor = kAzure + 1;
        const Width_t frameWidth = 4;

        lineR->SetLineColor(frameColor);
        lineR->SetLineWidth(frameWidth);
        lineR->Draw();

        lineL->SetLineColor(frameColor);
        lineL->SetLineWidth(frameWidth);
        lineL->Draw();

        lineU->SetLineColor(frameColor);
        lineU->SetLineWidth(frameWidth);
        lineU->Draw();

        lineD->SetLineColor(frameColor);
        lineD->SetLineWidth(frameWidth);
        lineD->Draw();
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showXYcoord
//==============================================================================
// Extracts the spectrum channel (binX) and count value (binC) at the cursor
// position (x, y) via ROOT GetObjectInfo() and updates the UI status labels
// (labelX and labelY) in real time.
//==============================================================================
void QMainCanvas::showXYcoord(Double_t x, Double_t y)
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    const std::string objectInfo = hist->GetObjectInfo(static_cast<Int_t>(x), static_cast<Int_t>(y));

    int binX = 0;
    double binC = 0.0;

    // Parse "binx=..."
    const size_t binxPos = objectInfo.find("binx=");
    const size_t bincPos = objectInfo.find(" binc=");
    if (binxPos != std::string::npos && bincPos != std::string::npos && bincPos > binxPos + 5) {
        try {
            binX = std::stoi(objectInfo.substr(binxPos + 5, bincPos - binxPos - 5));
        } catch (...) {
            binX = 0;
        }
    }

    // Parse "binc=..."
    if (bincPos != std::string::npos) {
        const size_t sumPos = objectInfo.find(" Sum=", bincPos);
        const size_t startVal = bincPos + 6;
        try {
            if (sumPos != std::string::npos && sumPos > startVal) {
                binC = std::stod(objectInfo.substr(startVal, sumPos - startVal));
            } else if (startVal < objectInfo.length()) {
                binC = std::stod(objectInfo.substr(startVal));
            }
        } catch (...) {
            binC = 0.0;
        }
    }

    QString countsStr;
    if (std::abs(binC) <= 100000.0) {
        if (std::abs(binC - std::round(binC)) < 1e-5) {
            countsStr = QString::number(static_cast<long long>(std::round(binC)));
        } else {
            countsStr = QString::number(binC, 'g', 6);
        }
    } else {
        countsStr = QString::number(binC, 'e', 2);
    }

    if (labelChannel) {
        labelChannel->setText(QString("Channel: %1").arg(binX));
    }
    if (labelEnergy) {
        TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(hist);
        if (tHist && tHist->IsCalibrated()) {
            double energy = tHist->ChannelToEnergy(static_cast<Double_t>(binX));
            labelEnergy->setText(QString("Energy: %1").arg(QString::number(energy, 'f', 2)));
        } else {
            labelEnergy->setText("Energy: 0.0");
        }
    }
    if (labelCounts) {
        labelCounts->setText(QString("Counts: %1").arg(countsStr));
    }
    if (labelCursorY) {
        labelCursorY->setText(QString("Y: %1").arg(QString::number(y, 'f', 2)));
    }

    if (labelX && labelX != labelChannel) {
        labelX->setText(QString::number(binX));
    }
    if (labelY && labelY != labelCounts) {
        labelY->setText(countsStr);
    }
}


//==============================================================================
// QMainCanvas::DeleteCulomn
//==============================================================================
// Removes the rightmost column of sub-pads from the canvas grid (shortcut: Ctrl + Left).
// Re-divides the ROOT TCanvas into (maxElement_j - 1) columns by maxElement_i rows,
// frees resources belonging to the removed column, and re-renders remaining spectra.
//==============================================================================
void QMainCanvas::DeleteCulomn()
{
    if (maxElement_j <= 1) return;
    if (!canvas || !canvas->getCanvas()) return;

    const int deletedCol = maxElement_j;
    maxElement_j--;

    // Keep selection within valid bounds
    if (SelectedElement_j > maxElement_j) {
        SelectedElement_j = maxElement_j;
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    rootCanvas->SetFillColor(0);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";
            HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(histTitle.c_str()));
            HijF[z][g]->SetTitle(histTitle.c_str());

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }

        // Clean up resources for the deleted column
        for (auto *hist : HijC[z][deletedCol]) {
            delete hist;
        }
        HijC[z][deletedCol].clear();
        gaussCenters[z][deletedCol].clear();
        gaussCentersHeight[z][deletedCol].clear();
        for (auto *obj : autoFitMarkers[z][deletedCol]) {
            delete obj;
        }
        autoFitMarkers[z][deletedCol].clear();
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::DeleteLine
//==============================================================================
// Removes the bottom row of sub-pads from the canvas grid (shortcut: Ctrl + Down).
// Re-divides the ROOT TCanvas into maxElement_j columns by (maxElement_i - 1) rows,
// frees resources belonging to the removed row, and re-renders remaining spectra.
//==============================================================================
void QMainCanvas::DeleteLine()
{
    if (maxElement_i <= 1) return;
    if (!canvas || !canvas->getCanvas()) return;

    const int deletedRow = maxElement_i;
    maxElement_i--;

    // Keep selection within valid bounds
    if (SelectedElement_i > maxElement_i) {
        SelectedElement_i = maxElement_i;
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    rootCanvas->SetFillColor(0);

    if (maxElement_i == 1 && maxElement_j == 1) {
        if (HijF[1][1]) {
            HijF[1][1]->Draw();
        }
        // Re-render Gaussian center annotations
        renderPeakLabels(1, 1);
        for (std::size_t k = 0; k < HijC[1][1].size(); ++k) {
            if (HijC[1][1][k]) {
                if (k < colors_hist.size()) {
                    HijC[1][1][k]->SetLineColor(colors_hist[k]);
                }
                HijC[1][1][k]->Draw("SAME");
            }
        }
        for (auto *obj : autoFitMarkers[1][1]) {
            if (obj) obj->Draw("SAME");
        }
    } else {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

        for (int z = 1; z <= maxElement_i; ++z) {
            for (int g = 1; g <= maxElement_j; ++g) {
                if (!HijF[z][g]) continue;

                const int padIndex = (z - 1) * maxElement_j + g;
                rootCanvas->cd(padIndex);
                HijF[z][g]->Draw();

                // Re-render Gaussian center annotations
                renderPeakLabels(z, g);

                // Re-render overlaid comparison spectra
                for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                    if (HijC[z][g][k]) {
                        if (k < colors_hist.size()) {
                            HijC[z][g][k]->SetLineColor(colors_hist[k]);
                        }
                        HijC[z][g][k]->Draw("SAME");
                    }
                }

                // Re-render auto-fit markers (fit curves, background lines)
                for (auto *obj : autoFitMarkers[z][g]) {
                    if (obj) {
                        rootCanvas->cd(padIndex);
                        obj->Draw("SAME");
                    }
                }
            }
        }
    }

    // Clean up resources for the deleted row
    for (int g = 1; g < 12; ++g) {
        for (auto *hist : HijC[deletedRow][g]) {
            delete hist;
        }
        HijC[deletedRow][g].clear();
        gaussCenters[deletedRow][g].clear();
        gaussCentersHeight[deletedRow][g].clear();
        for (auto *obj : autoFitMarkers[deletedRow][g]) {
            delete obj;
        }
        autoFitMarkers[deletedRow][g].clear();
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}
//==============================================================================
// QMainCanvas::RefreshScreen
//==============================================================================
// Completely redraws and updates all spectrum pads and overlaid objects
// in the current grid configuration (shortcut: '=').
// Clears the canvas, re-divides into maxElement_j by maxElement_i pads,
// and re-plots all base histograms, overlays, Gaussian centroid labels,
// and fit curves.
//==============================================================================
void QMainCanvas::RefreshScreen()
{
    if (!canvas || !canvas->getCanvas()) return;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    rootCanvas->SetFillColor(0);

    if (maxElement_i > 1 || maxElement_j > 1) {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);
    }

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";
            HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(histTitle.c_str()));
            HijF[z][g]->SetTitle(histTitle.c_str());

            const int padIndex = (z - 1) * maxElement_j + g;
            if (maxElement_i > 1 || maxElement_j > 1) {
                rootCanvas->cd(padIndex);
            }

            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    if (maxElement_i > 1 || maxElement_j > 1) {
                        rootCanvas->cd(padIndex);
                    }
                    obj->Draw("SAME");
                }
            }
        }
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::offerHelp
//==============================================================================
// Displays the full command and keyboard shortcut reference guide in the
// CommandPrompt interactive log window (shortcut: 'H' or '?').
//==============================================================================
void QMainCanvas::offerHelp()
{
    CommandPrompt *prompt = CommandPrompt::getInstance();
    if (!prompt) return;

    prompt->appendPlainText(" **********************  COMMAND-LIST  *********************\n\n");
    prompt->appendPlainText(" spacebar               Place a Marker on the position of the cursor\n");
    prompt->appendPlainText(" AJ AG                  Automatic CJ, CG  at marker position\n");
    prompt->appendPlainText(" B G I R S W            Insert a marker of type Background, G, Integral, Range, S or W\n");
    prompt->appendPlainText(" CB CI CJ MI MJ         Background, Integration(CI without background, CJ with), CB+CI\n");
    prompt->appendPlainText(" CG CV MG MV            Gaussfit, CB+CG. Show markers\n");
    prompt->appendPlainText(" CP MP                  Automatic peak search. Show peaks\n");
    prompt->appendPlainText(" Dn Cn Mn Zn n          Define, Execute, Show, Erase command string n=1...9\n");
    prompt->appendPlainText(" DD                     Change the display parameters\n");
    prompt->appendPlainText(" DE                     Define how to do efficiency correction\n");
    prompt->appendPlainText(" DG                     Define peak width (individual/common) for fit\n");
    prompt->appendPlainText(" DK AK                  Energy and Width calibration\n");
    prompt->appendPlainText(" DF DL                  Define output file for Area calculations\n");
    prompt->appendPlainText(" DT CT AT               Recalibration using Trackfit\n");
    prompt->appendPlainText(" DW CW                  Define, Extract cuts from compressed matrix\n");
    prompt->appendPlainText(" DQ                     Define matrix and background subtraction mode\n");
    prompt->appendPlainText(" E                      Expand/Zoom between last two Markers\n");
    prompt->appendPlainText(" X                      Expand around current cursor position\n");
    prompt->appendPlainText(" FF FX FY               Full display Full_x Full_y\n");
    prompt->appendPlainText(" SX SY                  same X or Y scale for all windows\n");
    prompt->appendPlainText(" FO FU                  Set Y-maximum or Y-minimum by marker\n");
    prompt->appendPlainText(" H ?                    Help (this list)\n");
    prompt->appendPlainText(" K                      Energy calibration from previous 2 energies\n");
    prompt->appendPlainText(" L                      Change the histogram Linear/Logarithmic\n");
    prompt->appendPlainText(" N                      Input new spectrum\n");
    prompt->appendPlainText(" DN MN ZN               Define display behaviour at input of new spectrum\n");
    prompt->appendPlainText(" OS                     Write out current spectrum\n");
    prompt->appendPlainText(" O=                     Postscript plot of current display\n");
    prompt->appendPlainText(" P                      Insert a peak by energy\n");
    prompt->appendPlainText(" Q                      Display projection of compressed matrix\n");
    prompt->appendPlainText(" V                      Marker writing also counts in channel\n");
    prompt->appendPlainText(" MZ                     Draw a line at zero counts\n");
    prompt->appendPlainText(" ZA                     Delete all B/G/I markers\n");
    prompt->appendPlainText(" ZB ZI ZJ ZG ZV         Delete corresponding type of markers\n");
    prompt->appendPlainText(" ZF ZL                  Close output file for Area calculations\n");
    prompt->appendPlainText(" DP MP ZP               Define, Show, Delete peaks in buffer\n");
    prompt->appendPlainText(" + -                    Insert/delete a peak by marker\n");
    prompt->appendPlainText(" =                      Repeat the display\n");
    prompt->appendPlainText(" < >                    Shift display 3/4 to Left, Right\n");
    prompt->appendPlainText(" CTL_RIGHTARROW         Increase # of windows adding one column more\n");
    prompt->appendPlainText(" CTL_LEFTARROW          Decrease # of windows deleting last column\n");
    prompt->appendPlainText(" CTL_UPARROW            Increase # of windows adding one row more\n");
    prompt->appendPlainText(" CTL_DOWNARROW          Decrease # of windows deleting last row\n");
    prompt->appendPlainText(" CTL_C CTL_Y CTL_Z      Close the program\n");
    prompt->appendPlainText(" _________________________________________________________\n\n");
}

void QMainCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        if (canvas) {
            canvas->setFocus();
            canvas->keyPressEvent(event);
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void QMainCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        if (canvas) {
            canvas->keyReleaseEvent(event);
            return;
        }
    }
    QWidget::keyReleaseEvent(event);
}

