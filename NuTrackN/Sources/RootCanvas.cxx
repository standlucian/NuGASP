#include "IntegralDialog.h"
#include "canvas.h"
#include "Design.h"
#include "PeakFit.h"
#include "tracknhistogram.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TVirtualX.h>
#include <TVirtualPad.h>
#include <TLatex.h>
#include <TF1.h>
#include <TQObject.h>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QGuiApplication>
#include <QCursor>

#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

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
      aKeyWasPressed(false),
      cKeyWasPressed(false),
      zKeyWasPressed(false),
      mKeyWasPressed(false),
      fKeyWasPressed(false),
      sKeyWasPressed(false),
      dKeyWasPressed(false),
      oKeyWasPressed(false),
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
    if (!m_mainCanvas) {
        QWidget *p = parentWidget();
        while (p) {
            m_mainCanvas = qobject_cast<QMainCanvas*>(p);
            if (m_mainCanvas) break;
            p = p->parentWidget();
        }
    }

    // Auto-dismiss fit parameters dialog on key press or mouse click outside the dialog
    if (m_mainCanvas && m_mainCanvas->fitParamsDialog && m_mainCanvas->fitParamsDialog->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->fitParamsDialog || m_mainCanvas->fitParamsDialog->isAncestorOf(w)))
                               || (me && m_mainCanvas->fitParamsDialog->frameGeometry().contains(me->globalPos()));
            if (isInside) {
                return false;
            }
            m_mainCanvas->fitParamsDialog->hide();
        } else if (event->type() == QEvent::KeyPress) {
            QWidget *fw = QApplication::focusWidget();
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->fitParamsDialog || m_mainCanvas->fitParamsDialog->isAncestorOf(w)))
                               || (fw && (fw == m_mainCanvas->fitParamsDialog || m_mainCanvas->fitParamsDialog->isAncestorOf(fw)));
            if (isInside) {
                return false;
            }
            m_mainCanvas->fitParamsDialog->hide();
        }
    }

    // Auto-dismiss peak search parameters dialog on key press or mouse click outside the dialog
    if (m_mainCanvas && m_mainCanvas->peakSearchParamsDialog && m_mainCanvas->peakSearchParamsDialog->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->peakSearchParamsDialog || m_mainCanvas->peakSearchParamsDialog->isAncestorOf(w)))
                               || (me && m_mainCanvas->peakSearchParamsDialog->frameGeometry().contains(me->globalPos()));
            if (isInside) {
                return false;
            }
            m_mainCanvas->peakSearchParamsDialog->hide();
        } else if (event->type() == QEvent::KeyPress) {
            QWidget *fw = QApplication::focusWidget();
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->peakSearchParamsDialog || m_mainCanvas->peakSearchParamsDialog->isAncestorOf(w)))
                               || (fw && (fw == m_mainCanvas->peakSearchParamsDialog || m_mainCanvas->peakSearchParamsDialog->isAncestorOf(fw)));
            if (isInside) {
                return false;
            }
            m_mainCanvas->peakSearchParamsDialog->hide();
        }
    }

    // Auto-dismiss integral dialog on key press or mouse click outside the dialog
    if (m_mainCanvas && m_mainCanvas->m_integralDialog && m_mainCanvas->m_integralDialog->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->m_integralDialog || m_mainCanvas->m_integralDialog->isAncestorOf(w)))
                               || (me && m_mainCanvas->m_integralDialog->frameGeometry().contains(me->globalPos()));
            if (isInside) {
                return false;
            }
            m_mainCanvas->m_integralDialog->hide();
        } else if (event->type() == QEvent::KeyPress) {
            QWidget *fw = QApplication::focusWidget();
            QWidget *w = qobject_cast<QWidget*>(watched);
            const bool isInside = (w && (w == m_mainCanvas->m_integralDialog || m_mainCanvas->m_integralDialog->isAncestorOf(w)))
                               || (fw && (fw == m_mainCanvas->m_integralDialog || m_mainCanvas->m_integralDialog->isAncestorOf(fw)));
            if (isInside) {
                return false;
            }
            m_mainCanvas->m_integralDialog->hide();
        }
    }

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
    const int deltaY = e->angleDelta().y();
    const int deltaX = e->angleDelta().x();

    if ((e->modifiers() & Qt::ShiftModifier) || std::abs(deltaX) > std::abs(deltaY)) {
        // Horizontal wheel or Shift + Wheel: Pan spectrum horizontally left/right (< / >)
        const int delta = (std::abs(deltaX) > std::abs(deltaY)) ? deltaX : deltaY;
        if (delta > 0) {
            emit requesttranslateminusTheScreen();
        } else if (delta < 0) {
            emit requesttranslateplusTheScreen();
        }
    } else {
        // Vertical wheel: Zoom out / in Y range
        if (deltaY > 0) { // Wheel scrolled up: zoom out / expand Y range
            emit requesttranslatedownTheScreen();
        } else if (deltaY < 0) { // Wheel scrolled down: zoom in / compress Y range
            emit requesttranslateupTheScreen();
        }
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
    if (m_mainCanvas && m_mainCanvas->fitParamsDialog && m_mainCanvas->fitParamsDialog->isVisible()) {
        m_mainCanvas->fitParamsDialog->hide();
    }
    if (m_mainCanvas && m_mainCanvas->peakSearchParamsDialog && m_mainCanvas->peakSearchParamsDialog->isVisible()) {
        m_mainCanvas->peakSearchParamsDialog->hide();
    }

    // Always keep mouse coordinates fresh on any key press
    QPoint curPos = mapFromGlobal(QCursor::pos());
    if (rect().contains(curPos)) {
        xMousePosition = curPos.x();
        yMousePosition = curPos.y();
        emit mousePilgrimCoordRequest(xMousePosition, yMousePosition);
    }

    if (event->key() == Qt::Key_Control) {
        controlKeyIsPressed = true;
        updateZoomHUD(xMousePosition, yMousePosition);
        return;
    }
    
    // Disable all prefix states if Esc is pressed
    if (event->key() == Qt::Key_Escape) {
        controlKeyIsPressed = aKeyWasPressed = cKeyWasPressed = zKeyWasPressed = false;
        mKeyWasPressed = fKeyWasPressed = sKeyWasPressed = dKeyWasPressed = oKeyWasPressed = false;
        return;
    }

    // Handle key sequences following a CTRL press
    if (controlKeyIsPressed || (event->modifiers() & Qt::ControlModifier)) {
        switch (event->key()) {
            case Qt::Key_Up:
                emit AddLineRequest();
                break;
            case Qt::Key_Down:
                emit DeleteLineRequest();
                break;
            case Qt::Key_Right:
                emit AddCulomnRequest();
                break;
            case Qt::Key_Left:
                emit DeleteCulomnRequest();
                break;
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
    // Handle commands prefixed by 'O' (Output / Save)
    else if (oKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_S:
                // O + S: Export Spectrum
                emit requestExportSpectrumDialog();
                break;
            case Qt::Key_Equal:
                // O + =: Output Postscript (save plot)
                emit requestPrintPlot();
                break;
            case Qt::Key_O:
                // Redundant O press: cancel prefix
                break;
            default:
                std::cout << "Waited for output command after O was pressed but no valid command arrived" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for output command after O was pressed but no valid command arrived\n");
                break;
        }
        oKeyWasPressed = false;
    }
    // Handle commands prefixed by 'A' (Automatic routines)
    else if (aKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_J:
                // A + J: Automatic integration with background
                emit requestAutoIntegration(xMousePosition, yMousePosition);
                break;
            case Qt::Key_G:
                // A + G: Automatic Gaussian multi-peak fit
                emit autoFitRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_T:
                // A + T: Automatic TrackFit Setup
                emit requestTrackFitDialog();
                break;
            case Qt::Key_K:
                // A + K: Automatic Energy Calibration (autoECALIBRATION)
                emit requestAutoCalibDialog();
                break;
            case Qt::Key_A:
                // Redundant A press: cancel prefix
                break;
            default:
                std::cout << "Waited for automatic command after A was pressed but no valid command arrived" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for automatic command after A was pressed but no valid command arrived\n");
                break;
        }
        aKeyWasPressed = false;
    }
    // Handle commands prefixed by 'C' (Computation routines)
    else if (cKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_B:
                // C + B: Background fit calculation
                emit requestFitBackground();
                break;
            case Qt::Key_I:
                // C + I: Integration without background subtraction
                emit requestIntegrationNoBackground();
                break;
            case Qt::Key_J:
                // C + J: Integration with linear background subtraction
                emit requestIntegrationWithBackground();
                break;
            case Qt::Key_G:
            case Qt::Key_V:
                // C + G or C + V: Gaussian multi-peak fit over marked region
                emit requestFitGauss();
                break;
            case Qt::Key_P:
                // C + P: Peak Search using TSpectrum
                emit requestPeakSearch();
                break;
            case Qt::Key_W:
                // C + W: Cut / slice gate from compressed matrix
                emit requestGateCut();
                break;
            case Qt::Key_T:
                // C + T: Calculate TrackFit
                emit requestTrackFitDialog();
                break;
            case Qt::Key_C:
                // Redundant C press: cancel prefix
                break;
            case Qt::Key_0:
            case Qt::Key_1:
            case Qt::Key_2:
            case Qt::Key_3:
            case Qt::Key_4:
            case Qt::Key_5:
            case Qt::Key_6:
            case Qt::Key_7:
            case Qt::Key_8:
            case Qt::Key_9: {
                // C + n: Cycle / loop command string n
                int id = event->key() - Qt::Key_0;
                emit requestCycleMacro(id);
                break;
            }
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
            case Qt::Key_P:
                // Z + P: Delete Peak Search markers
                emit requestDeletePeakMarkers();
                break;
            case Qt::Key_W:
                // Z + W: Delete gate markers
                emit requestDeleteGateMarkers();
                break;
            case Qt::Key_A:
                // Z + A: Delete all active markers
                emit requestDeleteAllMarkers();
                break;
            case Qt::Key_J:
                // Z + J: Delete Background and Integral markers
                emit requestDeleteZJMarkers();
                break;
            case Qt::Key_V:
                // Z + V: Delete Background, Range, and Gauss markers
                emit requestDeleteZVMarkers();
                break;
            case Qt::Key_Z:
                // Redundant Z press: cancel prefix
                break;
            case Qt::Key_F:
            case Qt::Key_L:
                // Z + F / Z + L: Close Area Output File
                if (m_mainCanvas) {
                    QString logName = m_mainCanvas->getAreaLogFileName();
                    if (m_mainCanvas->stopAreaLogging()) {
                        CommandPrompt::getInstance()->appendPlainText("Area output logging closed: " + logName + "\n");
                    } else {
                        CommandPrompt::getInstance()->appendPlainText("No area output file currently open.\n");
                    }
                }
                break;
            case Qt::Key_N:
                // Z + N: Set load behavior to Autoscale
                if (m_mainCanvas) m_mainCanvas->setLoadBehavior(QMainCanvas::LoadBehavior::Autoscale);
                CommandPrompt::getInstance()->appendPlainText("Display behavior: Auto Scale on new spectrum.\n");
                break;
            case Qt::Key_0:
            case Qt::Key_1:
            case Qt::Key_2:
            case Qt::Key_3:
            case Qt::Key_4:
            case Qt::Key_5:
            case Qt::Key_6:
            case Qt::Key_7:
            case Qt::Key_8:
            case Qt::Key_9: {
                // Z + n: Zero / clear command string n
                int id = event->key() - Qt::Key_0;
                emit requestClearMacro(id);
                break;
            }
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
            case Qt::Key_J:
                // M + J: Redraw Background and Integral markers
                emit requestMJMarkers();
                break;
            case Qt::Key_V:
                // M + V: Redraw Background, Range, and Gauss markers
                emit requestMVMarkers();
                break;
            case Qt::Key_P:
                // M + P: Redraw Peak Search markers
                emit requestShowPeakMarkers();
                break;
            case Qt::Key_W:
                // M + W: Redraw gate markers
                emit requestShowGateMarkers();
                break;
            case Qt::Key_A:
                // M + A: Redraw all markers
                emit requestShowAllMarkers();
                break;
            case Qt::Key_M:
                // Redundant M press: cancel prefix
                break;
            case Qt::Key_N:
                // M + N: Set load behavior to Preserve Scale
                if (m_mainCanvas) m_mainCanvas->setLoadBehavior(QMainCanvas::LoadBehavior::PreserveScale);
                CommandPrompt::getInstance()->appendPlainText("Display behavior: Preserve Scale on new spectrum.\n");
                break;
            case Qt::Key_Z:
                // M + Z: Draw a line at zero counts
                emit requestDrawZeroLine();
                break;
            case Qt::Key_0:
            case Qt::Key_1:
            case Qt::Key_2:
            case Qt::Key_3:
            case Qt::Key_4:
            case Qt::Key_5:
            case Qt::Key_6:
            case Qt::Key_7:
            case Qt::Key_8:
            case Qt::Key_9: {
                // M + n: Monitor / show command string n
                int id = event->key() - Qt::Key_0;
                emit requestShowMacro(id);
                break;
            }
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
            case Qt::Key_X:
                emit requestFullX();
                break;
            case Qt::Key_Y:
                emit requestFullY();
                break;
            case Qt::Key_O:
                // F + O: Force Y-Max to cursor Y
                emit requestSetYMax(yMousePosition);
                break;
            case Qt::Key_U:
                // F + U: Force Y-Min to cursor Y
                emit requestSetYMin(yMousePosition);
                break;
            default:
                std::cout << "Waited for command after F was pressed but no valid command arrived" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for command after F was pressed (valid: F, S, X, Y, O, U)\n");
                break;
        }
        fKeyWasPressed = false;
    }
    // Handle commands prefixed by 'S' (Same scale across windows)
    else if (sKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_X:
                emit requestSameX();
                break;
            case Qt::Key_Y:
                emit requestSameY();
                break;
            default:
                break;
        }
        sKeyWasPressed = false;
    }
    // Handle commands prefixed by 'D' (Diagnostics / Calibration)
    else if (dKeyWasPressed) {
        switch (event->key()) {
            case Qt::Key_K:
                emit requestEnCalDialog();
                break;
            case Qt::Key_T:
                emit requestTrackFitDialog();
                break;
            case Qt::Key_W:
                emit requestGateCut();
                break;
            case Qt::Key_D:
                // D + D: Display Parameters Dialog
                emit requestDisplayParamsDialog();
                break;
            case Qt::Key_E:
                // D + E: Efficiency Correction Dialog
                emit requestEfficiencyDialog();
                break;
            case Qt::Key_G:
                // D + G: Define Peak Width Mode (Coupled vs Independent)
                emit requestPeakWidthMode();
                break;
            case Qt::Key_Q:
                // D + Q: Matrix Setup / Coincidence Background
                emit requestMatrixSetup();
                break;
            case Qt::Key_F:
            case Qt::Key_L: {
                // D + F / D + L: Define Area Output File
                if (m_mainCanvas) {
                    QString fileName = QFileDialog::getSaveFileName(this, "Define Area Output File", "", "Area Log (*.area);;Text Files (*.txt);;All Files (*)");
                    if (!fileName.isEmpty()) {
                        if (m_mainCanvas->startAreaLogging(fileName)) {
                            CommandPrompt::getInstance()->appendPlainText("Area output logging enabled -> " + fileName + "\n");
                        } else {
                            CommandPrompt::getInstance()->appendPlainText("Error: Failed to open area output file: " + fileName + "\n");
                        }
                    }
                }
                break;
            }
            case Qt::Key_N: {
                // D + N: Open dialog to set display behavior
                if (m_mainCanvas) {
                    QStringList items;
                    items << "Auto Scale" << "Preserve Scale";
                    bool ok;
                    QString item = QInputDialog::getItem(this, "Set Display Behavior",
                                                         "Select behavior on new spectrum:", items, 0, false, &ok);
                    if (ok && !item.isEmpty()) {
                        if (item == "Auto Scale") m_mainCanvas->setLoadBehavior(QMainCanvas::LoadBehavior::Autoscale);
                        else m_mainCanvas->setLoadBehavior(QMainCanvas::LoadBehavior::PreserveScale);
                        CommandPrompt::getInstance()->appendPlainText("Display behavior set to: " + item + "\n");
                    }
                }
                break;
            }
            case Qt::Key_M:
                // D + M: Open Macro Dialog
                emit requestMacroDialog();
                break;
            case Qt::Key_0:
            case Qt::Key_1:
            case Qt::Key_2:
            case Qt::Key_3:
            case Qt::Key_4:
            case Qt::Key_5:
            case Qt::Key_6:
            case Qt::Key_7:
            case Qt::Key_8:
            case Qt::Key_9: {
                // D + n: Define command string n
                int id = event->key() - Qt::Key_0;
                emit requestDefineMacro(id);
                break;
            }
            default:
                break;
        }
        dKeyWasPressed = false;
    }
    // Single-key analysis and navigation triggers
    else {
        switch (event->key()) {
            case Qt::Key_H:
            case Qt::Key_Question:
                // '?' or 'H': Display interactive help menu
                emit requestHelp();
                break;
            case Qt::Key_D:
                dKeyWasPressed = true;
                break;
            case Qt::Key_A:
                aKeyWasPressed = true;
                break;
            case Qt::Key_C:
                cKeyWasPressed = true;
                break;
            case Qt::Key_Z:
                zKeyWasPressed = true;
                break;
            case Qt::Key_O:
                oKeyWasPressed = true;
                break;
            case Qt::Key_M:
                mKeyWasPressed = true;
                break;
            case Qt::Key_S:
                sKeyWasPressed = true;
                break;
            case Qt::Key_I:
                // 'I': Place integral boundary marker at cursor
                emit addIntegralMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_K:
                emit requestQuickCalibration();
                break;
            case Qt::Key_Q:
                emit requestMatrixProjection();
                break;
            case Qt::Key_Space:
                // Spacebar: Place zoom boundary marker at cursor
                emit addSpaceBarMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_N:
                emit requestOpenSpectrumDialog();
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
                // Up Arrow: Auto-scale Y axis for current plot (FY) matching legacy Xtrackn
                emit requestFullY();
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
            case Qt::Key_P:
                // 'P': Search/Go to a specific energy/channel
                emit requestGoToEnergy();
                break;
            case Qt::Key_X:
                // 'X': Zoom around current cursor position
                emit requestZoomAroundCursor(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Equal:
                // '=': Clear drawn overlay lines and reset display
                emit requestClearTheScreen();
                break;
            case Qt::Key_L:
                // 'L': Toggle linear / logarithmic Y-axis scale
                emit requestToggleLogY();
                break;
            case Qt::Key_E:
                // 'E': Execute zoom between spacebar markers
                emit requestZoomTheScreen();
                break;
            case Qt::Key_W:
                // 'W': Place coincidence gate boundary marker at cursor
                emit addGateMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Less:
            case Qt::Key_Comma:
                // '<' or ',': Shift spectrum view 3/4 (75%) to Left
                emit requestShiftDisplayLeft75();
                break;
            case Qt::Key_Greater:
            case Qt::Key_Period:
                // '>' or '.': Shift spectrum view 3/4 (75%) to Right
                emit requestShiftDisplayRight75();
                break;
            case Qt::Key_Plus:
                // '+': Add peak marker at cursor (same as 'G')
                emit requestAddGaussMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Minus:
                // '-': Delete nearest peak marker to cursor
                emit requestDeleteNearestGaussMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_0:
            case Qt::Key_1:
            case Qt::Key_2:
            case Qt::Key_3:
            case Qt::Key_4:
            case Qt::Key_5:
            case Qt::Key_6:
            case Qt::Key_7:
            case Qt::Key_8:
            case Qt::Key_9: {
                // n: Execute command string n once
                int id = event->key() - Qt::Key_0;
                emit requestExecuteMacro(id);
                break;
            }
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
    if (m_mainCanvas && m_mainCanvas->peakSearchParamsDialog && m_mainCanvas->peakSearchParamsDialog->isVisible()) {
        QPoint canvasTopRight = mapToGlobal(QPoint(width(), 0));
        int posX = canvasTopRight.x() - m_mainCanvas->peakSearchParamsDialog->width() - 25;
        int posY = canvasTopRight.y() + 45;
        m_mainCanvas->peakSearchParamsDialog->move(posX, posY);
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
