#include "canvas.h"
#include "Design.h"
#include "PeakFit.h"
#include "calib.h"
#include "tracknhistogram.h"
#include "SpectrumImportDialog.h"
#include "SpectrumExportDialog.h"
#include "TrackFitDialog.h"
#include "MatrixReader.h"
#include "MatrixDialog.h"
#include "DisplayParamsDialog.h"
#include "EfficiencyDialog.h"
#include "AutoCalibDialog.h"
#include "RunByRunManager.h"
#include "MacroDialog.h"
#include "IntegralDialog.h"
#include "LogSettingsDialog.h"
#include "HelpDialog.h"
#include "RemoteClient.h"
#include "RemoteConnectDialog.h"
#include "NuFileDialog.h"
#include <QInputDialog>
#include <QShortcut>

#include <TCanvas.h>
#include <TH1F.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TVirtualX.h>
#include <TVirtualPad.h>
#include <TLatex.h>
#include <TLine.h>
#include <TStyle.h>
#include <TColor.h>
#include <TFrame.h>

#include <QApplication>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QKeyEvent>
#include <QAction>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QDir>
#include <QCollator>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

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
        Design::saveSettings();
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
    topContainer->setObjectName("topContainer");
    topContainer->setAttribute(Qt::WA_StyledBackground, true);
    topContainer->setStyleSheet(QString("QWidget#topContainer { background-color: %1; }").arg(Design::getUIBackgroundColor().name()));
    this->setStyleSheet(QString("QMainCanvas { background-color: %1; }").arg(Design::getUIBackgroundColor().name()));
    QVBoxLayout *topLayout = new QVBoxLayout(topContainer);
    topLayout->setContentsMargins(4, 4, 4, 4);
    topLayout->setSpacing(2);

    // Helpers for consistent 3D beveled buttons and clean status readout panels
    auto makeButton = [](const QString &text, QWidget *parent, bool enabled = true) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, parent);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setEnabled(enabled);
        btn->setFont(Design::getButtonFont());
        btn->setStyleSheet(Design::getButtonStyleSheet());
        return btn;
    };

    auto makeLabel = [](const QString &text, QWidget *parent, Qt::Alignment align = Qt::AlignLeft) -> QLabel* {
        QLabel *lbl = new QLabel(text, parent);
        lbl->setAlignment(align | Qt::AlignVCenter);
        lbl->setFixedHeight(33);
        lbl->setFont(Design::getButtonFont());
        lbl->setStyleSheet(Design::getStatusLabelStyleSheet());
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

    for (QLabel *lbl : {labelXMin, labelXMax, labelYMin, labelYMax}) {
        lbl->setCursor(Qt::PointingHandCursor);
        lbl->installEventFilter(this);
    }
    labelXMin->setToolTip(tr("X Min range:\nLeft click: Shift right (+20%)\nRight click: Shift left (-20%)\nHold Ctrl for fine step (2.5%)"));
    labelXMax->setToolTip(tr("X Max range:\nLeft click: Shift right (+20%)\nRight click: Shift left (-20%)\nHold Ctrl for fine step (2.5%)"));
    labelYMin->setToolTip(tr("Y Min range:\nLeft click: Shift up (+20%)\nRight click: Shift down (-20%)\nHold Ctrl for fine step (2.5%)"));
    labelYMax->setToolTip(tr("Y Max range:\nLeft click: Shift up (+20%)\nRight click: Shift down (-20%)\nHold Ctrl for fine step (2.5%)"));

    labelChannel = makeLabel("Channel", topContainer);
    labelEnergy = makeLabel("Energy", topContainer);
    labelCounts = makeLabel("Counts", topContainer);
    labelCursorY = makeLabel("Y", topContainer);

    labelX = labelChannel;
    labelY = labelCounts;

    const QString axisTip = tr("Left-Click: +20% | Right-Click: -20% | Ctrl+Click: 2.5% fine step");
    labelXMin->setToolTip(axisTip);
    labelXMax->setToolTip(axisTip);
    labelYMin->setToolTip(axisTip);
    labelYMax->setToolTip(axisTip);

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

    QPushButton *btnRunByRun = makeButton("Run-by-Run", topContainer, true);
    btnRunByRun->setFixedWidth(93);
    btnRunByRun->setFixedHeight(36);
    btnRunByRun->setToolTip(tr("Open Run-by-Run Energy Calibration Manager (In-Beam Drift Alignment) [Shortcut: A+R]"));
    leftBar->addWidget(btnRunByRun);
    connect(btnRunByRun, &QPushButton::clicked, this, &QMainCanvas::openRunByRunManager);

    QPushButton *btnEnCal = makeButton("EnCal", topContainer, true);
    btnEnCal->setFixedWidth(93);
    btnEnCal->setFixedHeight(36);
    btnEnCal->setToolTip("Open Energy Calibration Manager (*E / DK) [Shortcut: D+K]");
    leftBar->addWidget(btnEnCal);
    connect(btnEnCal, &QPushButton::clicked, this, &QMainCanvas::openEnCalDialog);

    btnDT = makeButton("DT", topContainer, true);
    btnDT->setFixedWidth(93);
    btnDT->setFixedHeight(36);
    btnDT->setToolTip(tr("AutoTrace / TrackFit automated recalibration (*T / DT) [Shortcut: D+T]\nClick: Define/interactive dialog\nCtrl+Click: Direct AutoTrace fit (*T)"));
    btnDT->installEventFilter(this);
    leftBar->addWidget(btnDT);
    connect(btnDT, &QPushButton::clicked, this, &QMainCanvas::openTrackFitDialog);

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
    btnInt->setToolTip(tr("Integrate peak area with or without background subtraction [Shortcuts: C+I (gross), C+J (net)]"));
    rightBar->addWidget(btnInt);
    connect(btnInt, &QPushButton::clicked, this, [this]() { areaFunctionWithBackground(true, true); });

    QPushButton *btnFit = makeButton("Fit", topContainer, true);
    btnFit->setFixedWidth(93);
    btnFit->setFixedHeight(36);
    btnFit->setToolTip(tr("Gaussian multi-peak fit over marked region [Shortcut: C+V]"));
    rightBar->addWidget(btnFit);
    connect(btnFit, &QPushButton::clicked, this, &QMainCanvas::fitGauss);

    QPushButton *btnPkS = makeButton("PkS", topContainer, true);
    btnPkS->setFixedWidth(93);
    btnPkS->setFixedHeight(36);
    btnPkS->setToolTip(tr("Search for peaks in spectrum (TSpectrum) [Shortcut: C+P]"));
    rightBar->addWidget(btnPkS);
    connect(btnPkS, &QPushButton::clicked, this, &QMainCanvas::searchPeaks);

    QPushButton *btnRefresh = makeButton("=", topContainer, true);
    btnRefresh->setFixedWidth(93);
    btnRefresh->setFixedHeight(36);
    btnRefresh->setToolTip(tr("Repeat clean display, clear overlays & markers [Shortcut: =]"));
    rightBar->addWidget(btnRefresh);
    connect(btnRefresh, &QPushButton::clicked, this, &QMainCanvas::clearTheScreen);

    QPushButton *btnLinLog = makeButton("L", topContainer, true);
    btnLinLog->setFixedWidth(93);
    btnLinLog->setFixedHeight(36);
    btnLinLog->setToolTip(tr("Toggle Linear / Logarithmic scale [Shortcut: L]"));
    rightBar->addWidget(btnLinLog);
    connect(btnLinLog, &QPushButton::clicked, this, &QMainCanvas::toggleLogY);

    QPushButton *btnFF = makeButton("FF", topContainer, true);
    btnFF->setFixedWidth(93);
    btnFF->setFixedHeight(36);
    btnFF->setToolTip(tr("Full display (FX+FY): unzoom horizontal axis and autoscale vertical axis [Shortcut: F+F]"));
    rightBar->addWidget(btnFF);
    connect(btnFF, &QPushButton::clicked, this, &QMainCanvas::zoomOut);

    QPushButton *btnFX = makeButton("FX", topContainer, true);
    btnFX->setFixedWidth(93);
    btnFX->setFixedHeight(36);
    btnFX->setToolTip(tr("Full X: unzoom horizontal axis to full spectrum, keep vertical scale [Shortcut: F+X]"));
    rightBar->addWidget(btnFX);
    connect(btnFX, &QPushButton::clicked, this, [this]() { fullX(); });

    QPushButton *btnFY = makeButton("FY", topContainer, true);
    btnFY->setFixedWidth(93);
    btnFY->setFixedHeight(36);
    btnFY->setToolTip(tr("Full Y: autoscale vertical axis to visible peaks in current window [Shortcut: F+Y]"));
    rightBar->addWidget(btnFY);
    connect(btnFY, &QPushButton::clicked, this, [this]() { fullY(); });

    QPushButton *btnSX = makeButton("SX", topContainer, true);
    btnSX->setFixedWidth(93);
    btnSX->setFixedHeight(36);
    btnSX->setToolTip(tr("Same X: apply active window X zoom to all windows on screen [Shortcut: S+X]"));
    rightBar->addWidget(btnSX);
    connect(btnSX, &QPushButton::clicked, this, &QMainCanvas::sameX);

    QPushButton *btnSY = makeButton("SY", topContainer, true);
    btnSY->setFixedWidth(93);
    btnSY->setFixedHeight(36);
    btnSY->setToolTip(tr("Same Y: apply active window Y scale and mode to all windows on screen [Shortcut: S+Y]"));
    rightBar->addWidget(btnSY);
    connect(btnSY, &QPushButton::clicked, this, &QMainCanvas::sameY);

    // Split row for < and >
    QHBoxLayout *navRow = new QHBoxLayout();
    navRow->setSpacing(2);
    QPushButton *btnLeft = makeButton("<", topContainer, true);
    btnLeft->setFixedWidth(45);
    btnLeft->setFixedHeight(36);
    btnLeft->setToolTip(tr("Fine pan left ~3% of visible window [Shortcuts: Left Arrow / Shift+Wheel]"));
    connect(btnLeft, &QPushButton::clicked, this, &QMainCanvas::translateminusTheScreen);
    navRow->addWidget(btnLeft);

    QPushButton *btnRight = makeButton(">", topContainer, true);
    btnRight->setFixedWidth(45);
    btnRight->setFixedHeight(36);
    btnRight->setToolTip(tr("Fine pan right ~3% of visible window [Shortcuts: Right Arrow / Shift+Wheel]"));
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

    QPushButton *btnR = makeButton("Read", topContainer, true);
    btnR->setFixedWidth(64);
    btnR->setFixedHeight(36);
    btnR->setToolTip(tr("Open / read histogram spectra in multiple formats or sizes [Shortcut: N]"));
    connect(btnR, &QPushButton::clicked, this, &QMainCanvas::clicked1);
    bottomBtnGrid->addWidget(btnR, 0, 0);

    QPushButton *btnW = makeButton("Write", topContainer, true);
    btnW->setFixedWidth(66);
    btnW->setFixedHeight(36);
    btnW->setToolTip(tr("Write / export current spectrum to a file [Shortcut: O+S]"));
    connect(btnW, &QPushButton::clicked, this, &QMainCanvas::clickedW);
    bottomBtnGrid->addWidget(btnW, 0, 1);

    QPushButton *btnPrint = makeButton("Print", topContainer, true);
    btnPrint->setFixedWidth(66);
    btnPrint->setFixedHeight(36);
    btnPrint->setToolTip(tr("Export publication graphics (PDF / PS / PNG) [Shortcut: O+=]"));
    connect(btnPrint, &QPushButton::clicked, this, &QMainCanvas::printPlot);
    bottomBtnGrid->addWidget(btnPrint, 0, 2);

    btnRec = makeButton("Rec", topContainer, true);
    btnRec->setFixedWidth(66);
    btnRec->setFixedHeight(36);
    btnRec->setCheckable(true);
    btnRec->setStyleSheet(Design::getButtonStyleSheet() +
        "QPushButton:checked { background-color: #b71c1c; color: #ffffff; border: 1px solid #ef5350; }");
    btnRec->setToolTip(tr("Record peak integration & fit results to a calculation logfile\n"
                          "Click: Start (choose file) / Stop & close\n"
                          "Ctrl+Click: Switch to a new logfile\n"
                          "[Shortcuts: D+F / Z+F]"));
    connect(btnRec, &QPushButton::clicked, this, [this]() {
        const bool ctrlHeld = (QApplication::keyboardModifiers() & Qt::ControlModifier);
        if (ctrlHeld || !m_isAreaLoggingEnabled) {
            defineAreaLogFile();
        } else {
            closeAreaLogFile();
        }
        if (btnRec) {
            btnRec->setChecked(m_isAreaLoggingEnabled);
        }
    });
    bottomBtnGrid->addWidget(btnRec, 0, 3);

    btnDec = makeButton("# -", topContainer, true);
    btnDec->setFixedWidth(54);
    btnDec->setFixedHeight(36);
    btnDec->setToolTip(tr("Previous spectrum / run:\nLeft click: Previous detector (*2)\nRight click: Previous run (same detector) (*4)\nCtrl + Left click: Macro 2\nCtrl + Right click: Macro 4"));
    btnDec->installEventFilter(this);
    connect(btnDec, &QPushButton::clicked, this, &QMainCanvas::onSpectrumDecrement);
    bottomBtnGrid->addWidget(btnDec, 0, 4);

    btnInc = makeButton("# +", topContainer, true);
    btnInc->setFixedWidth(54);
    btnInc->setFixedHeight(36);
    btnInc->setToolTip(tr("Next spectrum / run:\nLeft click: Next detector (*1)\nRight click: Next run (same detector) (*3)\nCtrl + Left click: Macro 1\nCtrl + Right click: Macro 3"));
    btnInc->installEventFilter(this);
    connect(btnInc, &QPushButton::clicked, this, &QMainCanvas::onSpectrumIncrement);
    bottomBtnGrid->addWidget(btnInc, 0, 5);

    btnOpenCM = makeButton("Open CM", topContainer, true);
    btnOpenCM->setFixedHeight(36);
    btnOpenCM->setToolTip(tr("Open GASPware compressed coincidence matrix (.cmat)."));
    connect(btnOpenCM, &QPushButton::clicked, this, &QMainCanvas::onOpenCMClicked);
    bottomBtnGrid->addWidget(btnOpenCM, 1, 0);

    btnGateCM = makeButton("Gate CM", topContainer, true);
    btnGateCM->setFixedHeight(36);
    btnGateCM->setToolTip(tr("Project 1D coincidence gate from loaded matrix."));
    connect(btnGateCM, &QPushButton::clicked, this, &QMainCanvas::onGateCMClicked);
    bottomBtnGrid->addWidget(btnGateCM, 1, 1);

    btnRemote = makeButton("SSH Connect", topContainer, true);
    btnRemote->setFixedHeight(36);
    btnRemote->setToolTip(tr("Configure parameters and connect to Remote DAQ Computer over SSH/SFTP (Ctrl+Shift+R)."));
    connect(btnRemote, &QPushButton::clicked, this, &QMainCanvas::onOpenRemoteDataClicked);
    bottomBtnGrid->addWidget(btnRemote, 1, 2, 1, 4);

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
    QIcon themeBtnIcon(":/icon.png");
    if (themeBtnIcon.isNull()) themeBtnIcon = QIcon("icon.png");
    iconButton->setIcon(themeBtnIcon);
    iconButton->setIconSize(QSize(22, 22));
    iconButton->setToolTip(tr("Color & Theme Settings"));
    iconButton->setFixedSize(66, 33);
    iconButton->setStyleSheet(Design::getButtonStyleSheet());
    connect(iconButton, &QPushButton::clicked, this, &QMainCanvas::OpenColorSelectionDialog);
    outBox->addWidget(iconButton);

    QPushButton *btnMacro = makeButton(tr("MAC"), topContainer, true);
    btnMacro->setToolTip(tr("Macros & Command Strings (Block 4) [Shortcut: D+M]"));
    btnMacro->setFixedSize(76, 33);
    connect(btnMacro, &QPushButton::clicked, this, &QMainCanvas::openMacroDialog);
    outBox->addWidget(btnMacro);

    QPushButton *btnLog = makeButton(tr("LOG"), topContainer, true);
    btnLog->setToolTip(tr("Diagnostic Log Folder, Auto-Prune Rules & Manual Cleanup"));
    btnLog->setFixedSize(66, 33);
    connect(btnLog, &QPushButton::clicked, this, &QMainCanvas::openLogSettingsDialog);
    outBox->addWidget(btnLog);

    QPushButton *btnHelp = makeButton(tr("?"), topContainer, true);
    btnHelp->setToolTip(tr("Help & Command Reference Guide (Shortcut: H, ?, or F1)"));
    btnHelp->setFixedSize(66, 33);
    connect(btnHelp, &QPushButton::clicked, this, &QMainCanvas::offerHelp);
    outBox->addWidget(btnHelp);

    bottomStatusGrid->addLayout(outBox, 1, 2);

    bottomLayout->addLayout(bottomStatusGrid, 1);

    topLayout->addLayout(bottomLayout);

    mainSplitter->addWidget(topContainer);
    rootLayout->addWidget(mainSplitter);

    // 5. Connect user interaction signals from canvas to analysis slots
    connect(canvas, &QRootCanvas::requestIntegrationNoBackground, this, &QMainCanvas::areaFunction);
    connect(canvas, &QRootCanvas::requestIntegrationWithBackground, this, [this]() { areaFunctionWithBackground(true); });
    connect(canvas, &QRootCanvas::autoFitRequested, this, &QMainCanvas::autoFit);
    connect(canvas, &QRootCanvas::requestClearTheScreen, this, &QMainCanvas::clearTheScreen);
    connect(canvas, &QRootCanvas::addBackgroundMarkerRequested, this, &QMainCanvas::addBackgroundMarker);
    connect(canvas, &QRootCanvas::addIntegralMarkerRequested, this, &QMainCanvas::addIntegralMarker);
    connect(canvas, &QRootCanvas::showXY, this, &QMainCanvas::showXYcoord);
    connect(canvas, &QRootCanvas::requestGoToEnergy, this, &QMainCanvas::goToEnergy);
    connect(canvas, &QRootCanvas::requestZoomAroundCursor, this, &QMainCanvas::zoomAroundCursor);
    connect(canvas, &QRootCanvas::requestAutoIntegration, this, &QMainCanvas::autoIntegrationAtCursor);
    connect(canvas, &QRootCanvas::requestZoomTheScreen, this, &QMainCanvas::zoomTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateplusTheScreen, this, &QMainCanvas::translateplusTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateminusTheScreen, this, &QMainCanvas::translateminusTheScreen);
    connect(canvas, &QRootCanvas::requesttranslatedownTheScreen, this, &QMainCanvas::translatedownTheScreen);
    connect(canvas, &QRootCanvas::requesttranslateupTheScreen, this, &QMainCanvas::translateupTheScreen);
    connect(canvas, &QRootCanvas::fullscreen, this, &QMainCanvas::zoomOut);
    connect(canvas, &QRootCanvas::requestFullX, this, [this]() { fullX(); });
    connect(canvas, &QRootCanvas::requestFullY, this, [this]() { fullY(); });
    connect(canvas, &QRootCanvas::requestSameX, this, &QMainCanvas::sameX);
    connect(canvas, &QRootCanvas::requestSameY, this, &QMainCanvas::sameY);
    connect(canvas, &QRootCanvas::requestDeleteBackgroundMarkers, this, &QMainCanvas::deleteBackgroundMarkers);
    connect(canvas, &QRootCanvas::requestDeleteIntegralMarkers, this, &QMainCanvas::deleteIntegralMarkers);
    connect(canvas, &QRootCanvas::requestDeleteAllMarkers, this, &QMainCanvas::deleteAllMarkers);
    connect(canvas, &QRootCanvas::requestShowBackgroundMarkers, this, &QMainCanvas::showBackgroundMarkers);
    connect(canvas, &QRootCanvas::requestShowIntegralMarkers, this, &QMainCanvas::showIntegralMarkers);
    connect(canvas, &QRootCanvas::requestShowAllMarkers, this, &QMainCanvas::showAllMarkers);
    connect(canvas, &QRootCanvas::requestMJMarkers, this, &QMainCanvas::showMJMarkers);
    connect(canvas, &QRootCanvas::requestMVMarkers, this, &QMainCanvas::showMVMarkers);
    connect(canvas, &QRootCanvas::requestDeleteZJMarkers, this, &QMainCanvas::deleteZJMarkers);
    connect(canvas, &QRootCanvas::requestDeleteZVMarkers, this, &QMainCanvas::deleteZVMarkers);
    connect(canvas, &QRootCanvas::requestDrawZeroLine, this, &QMainCanvas::drawZeroLine);
    connect(canvas, &QRootCanvas::requestShiftDisplayLeft75, this, &QMainCanvas::shiftDisplayLeft75);
    connect(canvas, &QRootCanvas::requestShiftDisplayRight75, this, &QMainCanvas::shiftDisplayRight75);
    connect(canvas, &QRootCanvas::requestDeleteNearestGaussMarker, this, &QMainCanvas::deleteNearestGaussMarker);
    connect(canvas, &QRootCanvas::requestQuickCalibration, this, &QMainCanvas::quickEnergyCalibration);
    connect(canvas, &QRootCanvas::requestMatrixProjection, this, &QMainCanvas::showMatrixProjection);
    connect(canvas, &QRootCanvas::requestFitBackground, this, [this]() { fitBackgroundHelper(this); });
    connect(canvas, &QRootCanvas::addSpaceBarMarkerRequested, this, &QMainCanvas::addSpaceBarMarker);
    connect(canvas, &QRootCanvas::requestAddRangeMarker, this, &QMainCanvas::addRangeMarker);
    connect(canvas, &QRootCanvas::requestDeleteRangeMarkers, this, &QMainCanvas::deleteRangeMarkers);
    connect(canvas, &QRootCanvas::requestShowRangeMarkers, this, &QMainCanvas::showRangeMarkers);
    connect(canvas, &QRootCanvas::requestAddGaussMarker, this, &QMainCanvas::addGaussMarker);
    connect(canvas, &QRootCanvas::requestDeleteGaussMarkers, this, &QMainCanvas::deleteGaussMarkers);
    connect(canvas, &QRootCanvas::requestShowGaussMarkers, this, &QMainCanvas::showGaussMarkers);
    connect(canvas, &QRootCanvas::addGateMarkerRequested, this, &QMainCanvas::addGateMarker);
    connect(canvas, &QRootCanvas::requestDeleteGateMarkers, this, &QMainCanvas::deleteGateMarkers);
    connect(canvas, &QRootCanvas::requestShowGateMarkers, this, &QMainCanvas::showGateMarkers);
    connect(canvas, &QRootCanvas::requestGateCut, this, &QMainCanvas::onGateCMClicked);
    connect(canvas, &QRootCanvas::requestFitGauss, this, &QMainCanvas::fitGauss);
    connect(canvas, &QRootCanvas::requestPeakSearch, this, &QMainCanvas::searchPeaks);
    connect(canvas, &QRootCanvas::requestDeletePeakMarkers, this, &QMainCanvas::deletePeakMarkers);
    connect(canvas, &QRootCanvas::requestShowPeakMarkers, this, &QMainCanvas::showPeakMarkers);
    connect(canvas, &QRootCanvas::requestEnCalDialog, this, &QMainCanvas::openEnCalDialog);
    connect(canvas, &QRootCanvas::requestTrackFitDialog, this, &QMainCanvas::openTrackFitDialog);
    connect(canvas, &QRootCanvas::requestCTCalibration, this, &QMainCanvas::openTrackFitDialog);
    connect(canvas, &QRootCanvas::requestATCalibration, this, &QMainCanvas::executeATCalibration);
    
    // Block 3: Setup & Parameter Definition Dialogs
    connect(canvas, &QRootCanvas::requestDisplayParamsDialog, this, &QMainCanvas::openDisplayParamsDialog);
    connect(canvas, &QRootCanvas::requestEfficiencyDialog, this, &QMainCanvas::openEfficiencyDialog);
    connect(canvas, &QRootCanvas::requestPeakWidthMode, this, &QMainCanvas::openPeakWidthModeDialog);
    connect(canvas, &QRootCanvas::requestMatrixSetup, this, &QMainCanvas::onOpenCMClicked);
    connect(canvas, &QRootCanvas::requestAutoCalibDialog, this, &QMainCanvas::openAutoCalibDialog);
    connect(canvas, &QRootCanvas::requestRunByRunManager, this, &QMainCanvas::openRunByRunManager);

    // Block 4: Command Strings / Macros (Dn, Cn, Mn, Zn, n)
    connect(canvas, &QRootCanvas::requestDefineMacro, this, &QMainCanvas::defineMacro);
    connect(canvas, &QRootCanvas::requestExecuteMacro, this, &QMainCanvas::executeMacro);
    connect(canvas, &QRootCanvas::requestCycleMacro, this, [this](int id) { cycleMacro(id); });
    connect(canvas, &QRootCanvas::requestShowMacro, this, &QMainCanvas::showMacro);
    connect(canvas, &QRootCanvas::requestClearMacro, this, &QMainCanvas::clearMacro);
    connect(canvas, &QRootCanvas::requestMacroDialog, this, &QMainCanvas::openMacroDialog);
    
    // File I/O
    connect(canvas, &QRootCanvas::requestOpenSpectrumDialog, this, &QMainCanvas::clicked1);
    connect(canvas, &QRootCanvas::requestExportSpectrumDialog, this, &QMainCanvas::clickedW);
    connect(canvas, &QRootCanvas::requestPrintPlot, this, &QMainCanvas::printPlot);
    connect(canvas, &QRootCanvas::requestSetYMax, this, &QMainCanvas::setYMax);
    connect(canvas, &QRootCanvas::requestSetYMin, this, &QMainCanvas::setYMin);

    connect(canvas, &QRootCanvas::requestHelp, this, &QMainCanvas::offerHelp);
    connect(canvas, &QRootCanvas::requestToggleLogY, this, &QMainCanvas::toggleLogY);
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
    Design::applyGraphTypography();
    gStyle->SetOptTitle(0);
    gStyle->SetGridColor(kGray + 2);
    gStyle->SetGridStyle(2); // Dashed lines
    gStyle->SetGridWidth(1);
    const Color_t initialRootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    gStyle->SetCanvasColor(initialRootBg);
    gStyle->SetPadColor(initialRootBg);
    gStyle->SetFrameFillColor(initialRootBg);
    HijF[1][1] = new TracknHistogram("HijF[1][1]", "", 10240, 0.0, 10240.0);
    HijF[1][1]->GetXaxis()->SetNdivisions(510, kTRUE);
    HijF[1][1]->GetXaxis()->SetLabelSize(0);
    HijF[1][1]->GetXaxis()->SetTickLength(0);
    HijF[1][1]->GetYaxis()->SetNdivisions(510, kTRUE);
    HijF[1][1]->GetYaxis()->SetLabelSize(0);
    HijF[1][1]->GetYaxis()->SetTickLength(0);
    HijF[1][1]->SetStats(0);

    // Block 4: Initialize default macro presets
    m_macros[1] = {1, "*1", "", 1, "Next Spectrum / Detector (Current Run)"};
    m_macros[2] = {2, "*2", "", 1, "Previous Spectrum / Detector (Current Run)"};
    m_macros[3] = {3, "*3", "", 1, "Next Run (Same Detector)"};
    m_macros[4] = {4, "*4", "", 1, "Previous Run (Same Detector)"};
    for (int i = 0; i <= 9; ++i) {
        if (m_macros.find(i) == m_macros.end()) {
            m_macros[i] = {i, "", "", 1, ""};
        }
    }

    // Secret shortcut to open all dialogs simultaneously for visual inspection (Ctrl+Shift+D and F12)
    QShortcut *shortcutSecret = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), this, nullptr, nullptr, Qt::ApplicationShortcut);
    connect(shortcutSecret, &QShortcut::activated, this, &QMainCanvas::openAllDialogsForInspection);

    QShortcut *shortcutF12 = new QShortcut(QKeySequence(Qt::Key_F12), this, nullptr, nullptr, Qt::ApplicationShortcut);
    connect(shortcutF12, &QShortcut::activated, this, &QMainCanvas::openAllDialogsForInspection);

    QShortcut *shortcutRemote = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), this, nullptr, nullptr, Qt::ApplicationShortcut);
    connect(shortcutRemote, &QShortcut::activated, this, &QMainCanvas::onOpenRemoteDataClicked);

    m_remoteSession = RemoteSession::globalSession();
    updateSSHButtonState();
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
    for (int z = 0; z < 12; ++z) {
        for (int g = 0; g < 12; ++g) {
            peakSearchPrimitives[z][g].clear();
        }
    }
    multiPeakBkgLine = nullptr;
}

//==============================================================================
// QMainCanvas Destructor
//==============================================================================
// Cleans up dynamically allocated resources including the background covariance
// matrix produced by fitBackground() and any remaining drawn marker objects.
//==============================================================================
QMainCanvas::~QMainCanvas()
{
    stopAreaLogging();
    clearDrawnObjects();
    delete backgroundCovarianceMatrix;
    backgroundCovarianceMatrix = nullptr;
    delete gaussianCenterMarkerText;
    gaussianCenterMarkerText = nullptr;
    delete lineR; lineR = nullptr;
    delete lineL; lineL = nullptr;
    delete lineU; lineU = nullptr;
    delete lineD; lineD = nullptr;
    delete peakSearchParamsDialog;
    peakSearchParamsDialog = nullptr;
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
// QMainCanvas::getChannelFromClick
//==============================================================================
// Converts the ROOT 1-based bin returned by getBinFromClick into a 0-based
// spectrum channel index: channel = max(0, binX - 1).
//==============================================================================
int QMainCanvas::getChannelFromClick(int x, int y)
{
    int bin = getBinFromClick(x, y);
    return std::max(0, bin - 1);
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
    QString remotePath;
    bool isRemote = false;
    QString initialDir;
    if (!m_currentSpectrumFile.isEmpty() && QFileInfo::exists(m_currentSpectrumFile)) {
        initialDir = QFileInfo(m_currentSpectrumFile).absolutePath();
    }
    QString fileName = NuFileDialog::getOpenSpectrum(
        this, tr("Open Spectrum File"), initialDir, &remotePath, &isRemote);

    // If the dialog was cancelled or no file was chosen, keep the existing spectrum
    // untouched and restore keyboard focus to the spectrum canvas
    if (fileName.isEmpty()) {
        CommandPrompt::logDebug("Spectrum", "Open spectrum cancelled.");
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    if (isRemote) {
        m_currentRemotePath = remotePath;
    } else {
        m_currentRemotePath.clear();
    }

    // Delegate format detection, length configuration, and parsing to SpectrumImportDialog
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist) {
        CommandPrompt::logError("Spectrum", "Active histogram is null or invalid.");
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }
    
    double prevXMin = 0, prevXMax = 0, prevYMin = 0, prevYMax = 0;
    bool hasPrevScale = false;
    bool wasZoomed = false;
    if (m_loadBehavior == LoadBehavior::PreserveScale && trackHist->GetXaxis()) {
        TAxis *oldXAxis = trackHist->GetXaxis();
        wasZoomed = (oldXAxis->GetFirst() > 1 || oldXAxis->GetLast() < oldXAxis->GetNbins());
        prevXMin = oldXAxis->GetBinLowEdge(oldXAxis->GetFirst());
        prevXMax = oldXAxis->GetBinUpEdge(oldXAxis->GetLast());
        prevYMin = trackHist->GetMinimum();
        prevYMax = trackHist->GetMaximum();
        hasPrevScale = true;
    }

    SpectrumImportDialog importDlg(fileName, this);
    if (importDlg.exec() != QDialog::Accepted) {
        CommandPrompt::logDebug(
            "Spectrum",
            QString("Import cancelled for '%1'.").arg(QFileInfo(fileName).fileName()));
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    const std::vector<double> &spectrumData = importDlg.getLoadedData();
    if (spectrumData.empty() || !trackHist->LoadFromData(spectrumData, fileName.toStdString())) {
        CommandPrompt::logError(
            "Spectrum",
            QString("Failed to load spectrum from '%1'.").arg(fileName));
        if (canvas) {
            canvas->setFocus();
        }
        return;
    }

    // Configure canvas background only after a file has been successfully loaded
    canvas->getCanvas()->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    canvas->getCanvas()->SetFillColor(rootBg);

    // Update axes and leave 10% empty space on top of the spectrum
    HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->UnZoom();
    adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());

    // Clean up old comparison overlays on this pad for the freshly loaded spectrum file
    for (auto *h : HijC[SelectedElement_i][SelectedElement_j]) {
        delete h;
    }
    HijC[SelectedElement_i][SelectedElement_j].clear();
    HijC[SelectedElement_i][SelectedElement_j].push_back(
        (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());

    // Clear stale markers, fit curves, peak labels, and Zoom-HUD fit pointers for this pad
    clearDrawnObjects();
    if (fitParamsDialog && fitParamsDialog->isVisible()) {
        fitParamsDialog->hide();
    }
    if (peakSearchParamsDialog && peakSearchParamsDialog->isVisible()) {
        peakSearchParamsDialog->hide();
    }
    if (bkgDebounceTimer && bkgDebounceTimer->isActive()) {
        bkgDebounceTimer->stop();
    }
    m_bkgFixed = false;
    m_peakFixedStates.clear();

    TVirtualPad *activePad = (maxElement_i > 1 || maxElement_j > 1)
        ? canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j)
        : canvas->getCanvas()->cd();

    std::vector<TObject*> deletedObjs;
    for (TObject *obj : autoFitMarkers[SelectedElement_i][SelectedElement_j]) {
        if (!obj) continue;
        if (activePad && activePad->GetListOfPrimitives()) {
            activePad->GetListOfPrimitives()->Remove(obj);
        }
        bool alreadyDeleted = false;
        for (TObject *d : deletedObjs) {
            if (d == obj) {
                alreadyDeleted = true;
                break;
            }
        }
        if (!alreadyDeleted) {
            deletedObjs.push_back(obj);
            delete obj;
        }
    }
    gaussianWithBackgroundFunction = nullptr;
    backgroundFunction = nullptr;
    autoFitMarkers[SelectedElement_i][SelectedElement_j].clear();
    gaussCenters[SelectedElement_i][SelectedElement_j].clear();
    gaussCentersHeight[SelectedElement_i][SelectedElement_j].clear();
    renderPeakLabels(SelectedElement_i, SelectedElement_j);
    peakSearchCenters[SelectedElement_i][SelectedElement_j].clear();
    peakSearchHeights[SelectedElement_i][SelectedElement_j].clear();
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);

    // Set line color and draw to active pad
    HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[0]);
    HijC[SelectedElement_i][SelectedElement_j].back()->SetLineColor(colors_hist[0]);
    if (maxElement_i > 1 || maxElement_j > 1) {
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    } else {
        canvas->getCanvas()->cd();
    }
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    if (m_loadBehavior == LoadBehavior::PreserveScale && hasPrevScale) {
        if (wasZoomed && HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(prevXMin, prevXMax);
            for (TH1F *ov : HijC[SelectedElement_i][SelectedElement_j]) {
                if (ov && ov != HijF[SelectedElement_i][SelectedElement_j] && ov->GetXaxis()) {
                    ov->GetXaxis()->SetRangeUser(prevXMin, prevXMax);
                }
            }
        }
        if (prevYMax > prevYMin) {
            HijF[SelectedElement_i][SelectedElement_j]->GetYaxis()->SetRangeUser(prevYMin, prevYMax);
            HijF[SelectedElement_i][SelectedElement_j]->SetMinimum(prevYMin);
            HijF[SelectedElement_i][SelectedElement_j]->SetMaximum(prevYMax);
            for (TH1F *ov : HijC[SelectedElement_i][SelectedElement_j]) {
                if (!ov || ov == HijF[SelectedElement_i][SelectedElement_j]) continue;
                ov->GetYaxis()->SetRangeUser(prevYMin, prevYMax);
                ov->SetMinimum(prevYMin);
                ov->SetMaximum(prevYMax);
            }
        }
    } else {
        // Autoscale behavior
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
    }

    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    ColorTheFrameOfTheHistogram();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    if (!isRemote) {
        m_currentRemotePath.clear();
    }
    m_currentSpectrumFile = fileName;
    m_currentSpectrumIndex = importDlg.getSelectedSpectrumIndex();
    m_currentSpectrumCount = importDlg.getTotalSpectraCount();
    const int selectedLen = importDlg.getSelectedLength();
    m_currentSpectrumLength = (selectedLen > 0) ? selectedLen : static_cast<int>(spectrumData.size());
    m_currentSpectrumFormat = importDlg.getSelectedFormat();

    if (labelSpectrumFile) {
        QString disp = isRemote ? QString("[Remote] %1").arg(QFileInfo(remotePath).fileName())
                                : QFileInfo(fileName).fileName();
        if (m_currentSpectrumCount > 1) {
            disp += QString("#%1").arg(m_currentSpectrumIndex);
        }
        labelSpectrumFile->setText(disp);
    }
    if (labelWorkingPath) {
        QString dirDisp = isRemote ? QFileInfo(remotePath).path()
                                   : QFileInfo(fileName).absolutePath();
        labelWorkingPath->setText(dirDisp);
    }
    updateAxisStatusLabels();

    const QString fmtLabel = spectrumFormatToString(m_currentSpectrumFormat);
    const QString sourceLabel = isRemote
        ? QString("%1 (remote: %2)").arg(QFileInfo(remotePath).fileName(), remotePath)
        : QFileInfo(fileName).fileName();
    if (m_currentSpectrumCount > 1) {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Loaded %1#%2 (%3 of %4, %5 ch, %6)")
                .arg(sourceLabel)
                .arg(m_currentSpectrumIndex)
                .arg(m_currentSpectrumIndex + 1)
                .arg(m_currentSpectrumCount)
                .arg(spectrumData.size())
                .arg(fmtLabel));
    } else {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Loaded %1 (%2 ch, %3)")
                .arg(sourceLabel)
                .arg(spectrumData.size())
                .arg(fmtLabel));
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
        CommandPrompt::logWarning("Export", "No active spectrum loaded to export.");
        QMessageBox::information(this, tr("No Spectrum Loaded"),
                                 tr("There is no active spectrum to export.\nPlease load a spectrum first with 'R'."));
        if (canvas) canvas->setFocus();
        return;
    }

    std::vector<double> data = trackHist->GetBinData();
    if (data.empty()) {
        CommandPrompt::logWarning("Export", "Active spectrum contains no channel data to export.");
        QMessageBox::information(this, tr("Empty Spectrum"),
                                 tr("The active spectrum contains no channel data to export."));
        if (canvas) canvas->setFocus();
        return;
    }

    QString suggestedPath = m_currentOutputFile;
    bool suggestedIsRemote = false;
    if (suggestedPath.isEmpty()) {
        if (m_remoteSession && m_remoteSession->isConnected() && !m_currentRemotePath.isEmpty()) {
            QFileInfo rInfo(m_currentRemotePath);
            QString rDir = rInfo.dir().path();
            if (rDir == ".") rDir = "";
            suggestedPath = rDir.isEmpty()
                ? ("/" + rInfo.baseName() + "_out")
                : (rDir.endsWith('/') ? rDir + rInfo.baseName() + "_out" : rDir + "/" + rInfo.baseName() + "_out");
            suggestedIsRemote = true;
        } else if (!m_currentSpectrumFile.isEmpty() && QFileInfo::exists(m_currentSpectrumFile)) {
            QFileInfo fi(m_currentSpectrumFile);
            suggestedPath = fi.dir().filePath(fi.baseName() + "_out");
        } else if (m_currentMatrix && m_currentMatrix->isOpen() && !m_currentMatrix->getFilePath().isEmpty()) {
            QFileInfo fi(m_currentMatrix->getFilePath());
            suggestedPath = fi.dir().filePath(fi.baseName() + "_out");
        } else {
            suggestedPath = QDir::current().filePath(QStringLiteral("spectrum_out"));
        }
    }

    SpectrumExportDialog exportDlg(data, suggestedPath, m_currentSpectrumFormat, trackHist->GetNbinsX(), this, suggestedIsRemote);
    if (exportDlg.exec() != QDialog::Accepted) {
        CommandPrompt::logDebug("Export", "Export spectrum cancelled.");
        if (canvas) canvas->setFocus();
        return;
    }

    m_currentOutputFile = exportDlg.getSelectedFilePath();

    if (exportDlg.isRemoteSave() && !exportDlg.getSelectedRemotePath().isEmpty()) {
        if (labelOutputFile) {
            QString outName = QString("[Remote] %1").arg(QFileInfo(exportDlg.getSelectedRemotePath()).fileName());
            if (exportDlg.getExportMode() == SpectrumExportMode::WriteAtIndex) {
                outName += QString("#%1").arg(exportDlg.getTargetIndex());
            }
            labelOutputFile->setText(QString("-> %1").arg(outName));
        }
    } else {
        if (labelOutputFile) {
            QString outName = QFileInfo(m_currentOutputFile).fileName();
            if (exportDlg.getExportMode() == SpectrumExportMode::WriteAtIndex) {
                outName += QString("#%1").arg(exportDlg.getTargetIndex());
            }
            labelOutputFile->setText(QString("-> %1").arg(outName));
        }
    }

    QString modeDesc;
    if (exportDlg.getExportMode() == SpectrumExportMode::Append) {
        modeDesc = "Appended";
    } else if (exportDlg.getExportMode() == SpectrumExportMode::WriteAtIndex) {
        modeDesc = QString("Wrote index #%1 of").arg(exportDlg.getTargetIndex());
    } else {
        modeDesc = "Saved";
    }

    const QString fmtName = spectrumFormatToString(exportDlg.getSelectedFormat());
    const QString targetName = (exportDlg.isRemoteSave() && !exportDlg.getSelectedRemotePath().isEmpty())
        ? QString("%1 (remote: %2)").arg(QFileInfo(exportDlg.getSelectedRemotePath()).fileName(), exportDlg.getSelectedRemotePath())
        : QFileInfo(m_currentOutputFile).fileName();
    CommandPrompt::logInfo(
        "Export",
        QString("%1 %2 (%3 ch, %4)")
            .arg(modeDesc, targetName)
            .arg(exportDlg.getSelectedLength())
            .arg(fmtName));

    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// Multi-Spectrum Navigation Slots (# - and # +)
//==============================================================================
void QMainCanvas::onSpectrumIncrement()
{
    stepSpectrumIndex(+1, false);
}

//==============================================================================
// QMainCanvas::executeATCalibration
//==============================================================================
void QMainCanvas::executeATCalibration()
{
    // Auto trackfit calibration
    if (range_markers.size() < 2) {
        CommandPrompt::getInstance()->appendPlainText("Error: AT calibration requires at least 2 range markers.\n");
        return;
    }
    
    // Simulate auto trackfit by opening dialog and auto-triggering it (or directly triggering it if we had the backend logic)
    // For now we will open the dialog, but we could bypass it.
    openTrackFitDialog();
}

void QMainCanvas::onSpectrumDecrement()
{
    stepSpectrumIndex(-1, false);
}

void QMainCanvas::onSpectrumIncrementSameScale()
{
    stepSpectrumIndex(+1, true);
}

void QMainCanvas::onSpectrumDecrementSameScale()
{
    stepSpectrumIndex(-1, true);
}

void QMainCanvas::onRunIncrement()
{
    stepRun(+1);
}

void QMainCanvas::onRunDecrement()
{
    stepRun(-1);
}

//==============================================================================
// Block 4: Command Strings / Macros Implementation (Dn, Cn, Mn, Zn, n)
//==============================================================================
void QMainCanvas::executeMacro(int macroId)
{
    if (macroId < 0 || macroId > 9) return;
    auto it = m_macros.find(macroId);
    if (it == m_macros.end() || it->second.commandString.trimmed().isEmpty()) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Macro #%1 is empty. Use D%1 to define it.\n").arg(macroId));
        return;
    }

    CommandPrompt::logDebug(
        "Macro",
        QString("Executing Macro #%1: \"%2\"").arg(macroId).arg(it->second.commandString));

    executeCommandString(it->second.commandString);
}

void QMainCanvas::cycleMacro(int macroId, int cycles)
{
    if (macroId < 0 || macroId > 9) return;
    auto it = m_macros.find(macroId);
    if (it == m_macros.end() || it->second.commandString.trimmed().isEmpty()) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Cannot cycle empty Macro #%1. Use D%1 to define it first.\n").arg(macroId));
        return;
    }

    if (cycles <= 0) {
        bool ok = false;
        int defaultC = it->second.cycles > 0 ? it->second.cycles : 5;
        cycles = QInputDialog::getInt(this, tr("Cycle Macro %1 (C%1)").arg(macroId),
                                      tr("Number of cycles (#Cicli):"),
                                      defaultC, 1, 10000, 1, &ok);
        if (!ok || cycles <= 0) return;
    }

    m_macros[macroId].cycles = cycles;

    CommandPrompt::getInstance()->appendPlainText(
        QString("Starting Cycle on Macro %1: \"%2\" for %3 iterations...\n")
            .arg(macroId).arg(it->second.commandString).arg(cycles));

    for (int c = 1; c <= cycles; ++c) {
        CommandPrompt::getInstance()->appendPlainText(
            QString("[Macro %1 | Cycle %2/%3]\n").arg(macroId).arg(c).arg(cycles));
        executeCommandString(it->second.commandString);
        qApp->processEvents();
    }

    CommandPrompt::getInstance()->appendPlainText(
        QString("Completed %1 cycles of Macro %2.\n").arg(cycles).arg(macroId));
}

void QMainCanvas::defineMacro(int macroId)
{
    if (macroId < 0 || macroId > 9) return;
    bool ok = false;
    QString current = m_macros[macroId].commandString;
    QString input = QInputDialog::getText(
        this, tr("Define Macro %1 (D%1)").arg(macroId),
        tr("Enter Automatic command string #%1# (e.g. NFF, NCP, AG, CJ):").arg(macroId),
        QLineEdit::Normal, current, &ok);

    if (ok) {
        m_macros[macroId].id = macroId;
        m_macros[macroId].commandString = input.trimmed().toUpper();
        CommandPrompt::getInstance()->appendPlainText(
            QString("Automatic command string #%1# defined: \"%2\"\n")
                .arg(macroId).arg(m_macros[macroId].commandString));
    }
    if (canvas) canvas->setFocus();
}

void QMainCanvas::showMacro(int macroId)
{
    if (macroId < 0 || macroId > 9) return;
    auto it = m_macros.find(macroId);
    QString str = (it != m_macros.end() && !it->second.commandString.isEmpty())
        ? it->second.commandString
        : "<empty>";
    int cycles = (it != m_macros.end()) ? it->second.cycles : 1;
    CommandPrompt::getInstance()->appendPlainText(
        QString("Command string #%1#: \"%2\" (Default cycles: %3)\n")
            .arg(macroId).arg(str).arg(cycles));
}

void QMainCanvas::clearMacro(int macroId)
{
    if (macroId < 0 || macroId > 9) return;
    m_macros[macroId].commandString.clear();
    CommandPrompt::getInstance()->appendPlainText(
        QString("Command string #%1# erased (Z%1)\n").arg(macroId));
}

void QMainCanvas::openMacroDialog()
{
    MacroDialog dlg(this, this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openLogSettingsDialog()
{
    LogSettingsDialog dlg(this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

bool QMainCanvas::executeMacroCommand(const QString &token)
{
    QString t = token.trimmed().toUpper();
    if (t.isEmpty()) return true;

    if (t == "N" || t == "N+" || t == "*1") {
        onSpectrumIncrement();
    } else if (t == "N-" || t == "*2") {
        onSpectrumDecrement();
    } else if (t == "*3" || t == "R+" || t == "NR") {
        onRunIncrement();
    } else if (t == "*4" || t == "R-" || t == "PR") {
        onRunDecrement();
    } else if (t == "OR" || t == "DR" || t == "SSH") {
        onOpenRemoteDataClicked();
    } else if (t == "FF") {
        zoomOut();
    } else if (t == "FX") {
        fullX();
    } else if (t == "FY") {
        fullY();
    } else if (t == "SX") {
        sameX();
    } else if (t == "SY") {
        sameY();
    } else if (t == "L") {
        toggleLogY();
    } else if (t == "CP") {
        searchPeaks();
    } else if (t == "MP") {
        showPeakMarkers();
    } else if (t == "ZP") {
        deletePeakMarkers();
    } else if (t == "CB") {
        fitBackgroundHelper(this);
    } else if (t == "CI") {
        areaFunction();
    } else if (t == "CJ") {
        areaFunctionWithBackground(true);
    } else if (t == "CG" || t == "CV") {
        fitGauss();
    } else if (t == "AG") {
        autoFit(0, 0);
    } else if (t == "AJ") {
        autoIntegrationAtCursor(0, 0);
    } else if (t == "Q") {
        showMatrixProjection();
    } else if (t == "=") {
        RefreshScreen();
    } else if (t == "<") {
        shiftDisplayLeft75();
    } else if (t == ">") {
        shiftDisplayRight75();
    } else if (t == "ZA") {
        deleteAllMarkers();
    } else if (t == "ZB") {
        deleteBackgroundMarkers();
    } else if (t == "ZI") {
        deleteIntegralMarkers();
    } else if (t == "ZR") {
        deleteRangeMarkers();
    } else if (t == "ZG") {
        deleteGaussMarkers();
    } else if (t == "ZW") {
        deleteGateMarkers();
    } else if (t == "ZJ") {
        deleteZJMarkers();
    } else if (t == "ZV") {
        deleteZVMarkers();
    } else if (t == "CW") {
        onGateCMClicked();
    } else if (t == "OS") {
        clickedW();
    } else if (t == "O=") {
        printPlot();
    } else if (t == "DF" || t == "DL") {
        defineAreaLogFile();
    } else if (t == "ZF" || t == "ZL") {
        closeAreaLogFile();
    } else if (t == "DD") {
        openDisplayParamsDialog();
    } else if (t == "DN") {
        openLoadBehaviorDialog();
    } else if (t == "MN") {
        setLoadBehavior(LoadBehavior::PreserveScale);
        CommandPrompt::logDebug("Display", "Display behavior set to Preserve Scale (MN).");
    } else if (t == "ZN") {
        setLoadBehavior(LoadBehavior::Autoscale);
        CommandPrompt::logDebug("Display", "Display behavior set to Auto Scale (ZN).");
    } else if (t == "MZ") {
        drawZeroLine();
    } else {
        CommandPrompt::getInstance()->appendPlainText(
            QString("Warning: Unrecognized macro command token '%1'\n").arg(t));
        return false;
    }
    return true;
}

void QMainCanvas::executeCommandString(const QString &cmdStr)
{
    QString s = cmdStr.trimmed();
    if (s.isEmpty()) return;

    // Tokenizer
    std::vector<QString> tokens;
    int i = 0;
    while (i < s.length()) {
        if (s[i].isSpace() || s[i] == ',' || s[i] == ';') {
            i++;
            continue;
        }
        if (i + 1 < s.length()) {
            QString two = s.mid(i, 2).toUpper();
            if (two == "FF" || two == "FX" || two == "FY" ||
                two == "SX" || two == "SY" ||
                two == "CP" || two == "MP" || two == "ZP" ||
                two == "CB" || two == "CI" || two == "CJ" ||
                two == "CG" || two == "CV" || two == "AG" ||
                two == "AJ" || two == "ZA" || two == "ZB" ||
                two == "ZI" || two == "ZR" || two == "ZG" ||
                two == "ZW" || two == "ZJ" || two == "ZV" ||
                two == "CW" || two == "DW" || two == "OS" ||
                two == "O=" || two == "MZ" ||
                two == "N+" || two == "N-" ||
                (s[i] == '*' && s[i+1].isDigit())) {
                tokens.push_back(two);
                i += 2;
                continue;
            }
        }
        QString one = s.mid(i, 1).toUpper();
        tokens.push_back(one);
        i += 1;
    }

    static int recursionDepth = 0;
    if (recursionDepth > 10) {
        CommandPrompt::getInstance()->appendPlainText("Error: Maximum macro recursion depth exceeded.\n");
        return;
    }
    recursionDepth++;

    for (const QString &tok : tokens) {
        if (tok.length() == 1 && tok[0].isDigit()) {
            int subId = tok.toInt();
            auto it = m_macros.find(subId);
            if (it != m_macros.end() && !it->second.commandString.isEmpty()) {
                executeCommandString(it->second.commandString);
            }
        } else {
            executeMacroCommand(tok);
        }
        qApp->processEvents();
    }

    recursionDepth--;
}

void QMainCanvas::stepSpectrumIndex(int delta, bool preserveScale)
{
    if (m_currentSpectrumFile.isEmpty()) {
        if (m_currentMatrix && m_currentMatrix->isOpen()) {
            CommandPrompt::logInfo(
                "Spectrum",
                "Detector stepping is disabled while viewing a 2D matrix.",
                "step_spectrum");
            return;
        }
        CommandPrompt::logWarning("Spectrum", "No spectrum file currently loaded.");
        return;
    }

    if (m_currentSpectrumCount <= 1) {
        CommandPrompt::logInfo(
            "Spectrum",
            "Current file is a single spectrum (no other spectra in file).",
            "step_spectrum");
        return;
    }

    const int targetIndex = m_currentSpectrumIndex + delta;
    if (targetIndex < 0) {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Already at first spectrum (1/%1).").arg(m_currentSpectrumCount),
            "step_spectrum");
        return;
    }
    if (targetIndex >= m_currentSpectrumCount) {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Already at last spectrum (%1/%1).").arg(m_currentSpectrumCount),
            "step_spectrum");
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
        CommandPrompt::logError(
            "Spectrum",
            QString("Failed to read spectrum #%1: %2").arg(targetIndex).arg(err));
        return;
    }

    // Preserve active zoom window and vertical scale before loading new data
    TAxis *xAxis = HijF[SelectedElement_i][SelectedElement_j]->GetXaxis();
    bool wasZoomed = false;
    double prevXmin = 0.0, prevXmax = 0.0;
    if (xAxis && (xAxis->GetFirst() > 1 || xAxis->GetLast() < xAxis->GetNbins())) {
        wasZoomed = true;
        prevXmin = xAxis->GetBinLowEdge(xAxis->GetFirst());
        prevXmax = xAxis->GetBinUpEdge(xAxis->GetLast());
    }
    const double prevYmin = HijF[SelectedElement_i][SelectedElement_j]->GetMinimum();
    const double prevYmax = HijF[SelectedElement_i][SelectedElement_j]->GetMaximum();

    // Ensure baseline spectrum is present in HijC if it was previously empty
    if (HijC[SelectedElement_i][SelectedElement_j].empty()) {
        TH1F *baseClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        baseClone->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(baseClone);
    }

    // Preserve active pad energy calibration across spectrum stepping (*1/*2);
    // TracknHistogram::LoadFromData() resets calibration unconditionally.
    const bool wasCalibrated = trackHist->IsCalibrated();
    const auto prevCalibSegments = trackHist->GetCalibrationSegments();
    const Double_t prevA0 = trackHist->GetCalibA0();
    const Double_t prevA1 = trackHist->GetCalibA1();
    const Double_t prevA2 = trackHist->GetCalibA2();

    if (!trackHist->LoadFromData(spectrumData, m_currentSpectrumFile.toStdString())) {
        CommandPrompt::logError(
            "Spectrum",
            QString("Failed to load spectrum data for #%1.").arg(targetIndex));
        return;
    }

    if (wasCalibrated) {
        if (!prevCalibSegments.empty()) {
            trackHist->SetSegmentedCalibration(prevCalibSegments);
        } else {
            trackHist->SetCalibration(prevA0, prevA1, prevA2);
        }
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

    // Restore zoom range if active or requested
    if (preserveScale || wasZoomed) {
        if (wasZoomed) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(prevXmin, prevXmax);
        }
    } else if (zoom_markers.size() >= 2) {
        std::size_t zm = zoom_markers.size();
        double zLow = std::min(zoom_markers[zm - 2], zoom_markers[zm - 1]);
        double zHigh = std::max(zoom_markers[zm - 2], zoom_markers[zm - 1]);
        HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(zLow, zHigh);
    }

    if (preserveScale && prevYmax > prevYmin) {
        HijF[SelectedElement_i][SelectedElement_j]->GetYaxis()->SetRangeUser(prevYmin, prevYmax);
        HijF[SelectedElement_i][SelectedElement_j]->SetMinimum(prevYmin);
        HijF[SelectedElement_i][SelectedElement_j]->SetMaximum(prevYmax);
    } else {
        // Update axes and canvas considering all visible overlaid spectra
        adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
    }
    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());

    if (maxElement_i > 1 || maxElement_j > 1) {
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    } else {
        canvas->getCanvas()->cd();
    }
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    // Redraw all previous spectra in HijC with "SAME" option
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size() - 1; ++k) {
        if (HijC[SelectedElement_i][SelectedElement_j][k]) {
            HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(
                colors_hist[k % colors_hist.size()]);
            if (maxElement_i > 1 || maxElement_j > 1) {
                canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            } else {
                canvas->getCanvas()->cd();
            }
            HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
        }
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    if (labelSpectrumFile) {
        QString baseDisp = !m_currentRemotePath.isEmpty()
                               ? QString("[Remote] %1").arg(QFileInfo(m_currentRemotePath).fileName())
                               : QFileInfo(m_currentSpectrumFile).fileName();
        QString disp = QString("%1#%2")
                           .arg(baseDisp)
                           .arg(m_currentSpectrumIndex);
        labelSpectrumFile->setText(disp);
    }
    updateAxisStatusLabels();

    const QString stepName = !m_currentRemotePath.isEmpty()
        ? QFileInfo(m_currentRemotePath).fileName()
        : QFileInfo(m_currentSpectrumFile).fileName();
    CommandPrompt::logInfo(
        "Spectrum",
        QString("Loaded %1#%2 (%3/%4, %5 ch)")
            .arg(stepName)
            .arg(m_currentSpectrumIndex)
            .arg(m_currentSpectrumIndex + 1)
            .arg(m_currentSpectrumCount)
            .arg(spectrumData.size()),
        "step_spectrum");

    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::stepRun
//==============================================================================
// Cycles to the next or previous run file in the current directory while
// keeping the exact same detector / spectrum index active and preserving zoom.
//==============================================================================
void QMainCanvas::stepRun(int delta)
{
    QString targetFilePath;
    int targetFileIndex = -1;
    int totalRunFiles = 0;
    QString displayFileName;
    bool isRemoteStep = false;

    if (m_currentSpectrumFile.isEmpty() && m_currentMatrix && m_currentMatrix->isOpen()) {
        CommandPrompt::logInfo(
            "Spectrum",
            "Run stepping is disabled while viewing a 2D matrix.",
            "step_run");
        return;
    }

    // 1. Check if we are operating in a remote SSH/SFTP session
    if (m_remoteSession && m_remoteSession->isConnected() && !m_currentRemotePath.isEmpty()) {
        isRemoteStep = true;
        QFileInfo remoteInfo(m_currentRemotePath);
        QString remoteDir = remoteInfo.dir().path();

        std::vector<RemoteFileInfo> remoteRunFiles;
        QString err;
        if (!m_remoteSession->listRunFiles(remoteDir, remoteInfo.fileName(), remoteRunFiles, &err) || remoteRunFiles.empty()) {
            CommandPrompt::logError(
                "Spectrum",
                QString("Failed to query remote run files in %1: %2")
                    .arg(remoteDir, err.isEmpty() ? "No matching run files found" : err));
            return;
        }

        int currentIndex = -1;
        for (int i = 0; i < static_cast<int>(remoteRunFiles.size()); ++i) {
            if (remoteRunFiles[i].fullPath == m_currentRemotePath ||
                remoteRunFiles[i].name.compare(remoteInfo.fileName(), Qt::CaseInsensitive) == 0) {
                currentIndex = i;
                break;
            }
        }

        if (currentIndex == -1) {
            CommandPrompt::logError(
                "Spectrum",
                QString("Could not locate active file %1 in remote directory list.").arg(remoteInfo.fileName()));
            return;
        }

        int targetIndex = currentIndex + delta;
        if (targetIndex < 0) {
            CommandPrompt::logInfo(
                "Spectrum",
                QString("Already at first run file (%1).").arg(remoteInfo.fileName()),
                "step_run");
            return;
        }
        if (targetIndex >= static_cast<int>(remoteRunFiles.size())) {
            CommandPrompt::logInfo(
                "Spectrum",
                QString("Already at last run file (%1).").arg(remoteInfo.fileName()),
                "step_run");
            return;
        }

        QString targetRemotePath = remoteRunFiles[targetIndex].fullPath;
        QString targetLocalPath;
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Fetching remote run %1 (%2/%3)...")
                .arg(remoteRunFiles[targetIndex].name)
                .arg(targetIndex + 1)
                .arg(remoteRunFiles.size()),
            "step_run");
        qApp->processEvents();

        if (!m_remoteSession->syncRemoteFileToCache(
                targetRemotePath, targetLocalPath, nullptr, &err, nullptr, false, true)) {
            return;
        }

        m_currentRemotePath = targetRemotePath;
        m_currentSpectrumFile = targetLocalPath;
        targetFilePath = targetLocalPath;
        targetFileIndex = targetIndex;
        totalRunFiles = static_cast<int>(remoteRunFiles.size());
        displayFileName = remoteRunFiles[targetIndex].name;
    } else {
        // 2. Discover local sibling run files matching the same parameter prefix & extension
        QString currentFilePath = m_currentSpectrumFile;

        if (currentFilePath.isEmpty()) {
            CommandPrompt::logWarning("Spectrum", "No spectrum file currently loaded.");
            return;
        }

        QFileInfo currentInfo(currentFilePath);
        QDir dir = currentInfo.dir();
        if (!dir.exists()) {
            CommandPrompt::logError("Spectrum", "Current data directory does not exist.");
            return;
        }

        std::vector<QString> candidateFilePaths;
        QString currentFileName = currentInfo.fileName();

        // Case A: GASP style "PREFIX.DIGITS" (e.g. G0.0007, G1.0042, F.0001)
        int lastDot = currentFileName.lastIndexOf('.');
        bool isGaspFormat = false;
        if (lastDot > 0) {
            QString afterDot = currentFileName.mid(lastDot + 1);
            bool isAllDigits = !afterDot.isEmpty();
            for (QChar c : afterDot) {
                if (!c.isDigit()) { isAllDigits = false; break; }
            }
            if (isAllDigits) {
                isGaspFormat = true;
                QString prefix = currentFileName.left(lastDot + 1); // e.g. "G0."
                QStringList entries = dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort);
                struct RunItem { QString path; int runNum; };
                std::vector<RunItem> gaspRuns;
                for (const QString &entry : entries) {
                    if (entry.startsWith(prefix, Qt::CaseInsensitive)) {
                        QString rest = entry.mid(prefix.length());
                        bool ok = false;
                        int r = rest.toInt(&ok);
                        if (ok) {
                            gaspRuns.push_back({dir.filePath(entry), r});
                        }
                    }
                }
                std::sort(gaspRuns.begin(), gaspRuns.end(), [](const RunItem &a, const RunItem &b) {
                    return a.runNum < b.runNum;
                });
                for (const auto &item : gaspRuns) {
                    candidateFilePaths.push_back(item.path);
                }
            }
        }

        if (!isGaspFormat) {
            // Case B: Standard filenames with embedded run numbers (e.g. run_007.spk, r05.mat)
            QString base = currentInfo.baseName();
            QString suffix = currentInfo.completeSuffix().toLower();
            int digitStart = base.length();
            while (digitStart > 0 && base[digitStart - 1].isDigit()) {
                digitStart--;
            }

            if (digitStart < base.length()) {
                QString prefix = base.left(digitStart);
                QStringList entries = dir.entryList(QDir::Files | QDir::Readable, QDir::NoSort);
                struct RunItem { QString path; int runNum; };
                std::vector<RunItem> standardRuns;
                for (const QString &entry : entries) {
                    QFileInfo eInfo(entry);
                    if (eInfo.completeSuffix().toLower() == suffix && eInfo.baseName().startsWith(prefix, Qt::CaseInsensitive)) {
                        QString rest = eInfo.baseName().mid(prefix.length());
                        bool ok = false;
                        int r = rest.toInt(&ok);
                        if (ok) {
                            standardRuns.push_back({dir.filePath(entry), r});
                        }
                    }
                }
                std::sort(standardRuns.begin(), standardRuns.end(), [](const RunItem &a, const RunItem &b) {
                    return a.runNum < b.runNum;
                });
                for (const auto &item : standardRuns) {
                    candidateFilePaths.push_back(item.path);
                }
            } else {
                // Fallback: Natural sort of all files with same extension in directory
                QStringList filters;
                if (!suffix.isEmpty()) {
                    filters << QString("*.%1").arg(suffix);
                } else {
                    filters << "*";
                }
                QStringList entries = dir.entryList(filters, QDir::Files | QDir::Readable, QDir::NoSort);
                QCollator collator;
                collator.setNumericMode(true);
                collator.setCaseSensitivity(Qt::CaseInsensitive);
                std::sort(entries.begin(), entries.end(), [&collator](const QString &a, const QString &b) {
                    return collator.compare(a, b) < 0;
                });
                for (const auto &e : entries) {
                    candidateFilePaths.push_back(dir.filePath(e));
                }
            }
        }

        if (candidateFilePaths.empty()) {
            CommandPrompt::getInstance()->appendPlainText("No sibling run files found in current directory.\n");
            return;
        }

        int currentIndex = -1;
        QString currentCanonical = currentInfo.canonicalFilePath();
        for (int i = 0; i < static_cast<int>(candidateFilePaths.size()); ++i) {
            QFileInfo cInfo(candidateFilePaths[i]);
            if (cInfo.canonicalFilePath() == currentCanonical ||
                cInfo.absoluteFilePath() == currentInfo.absoluteFilePath() ||
                cInfo.fileName().compare(currentInfo.fileName(), Qt::CaseInsensitive) == 0) {
                currentIndex = i;
                break;
            }
        }

        if (currentIndex == -1) {
            CommandPrompt::logError(
                "Spectrum",
                QString("Could not locate current file %1 in run list.").arg(currentInfo.fileName()));
            return;
        }

        int targetIdx = currentIndex + delta;
        if (targetIdx < 0) {
            CommandPrompt::logInfo(
                "Spectrum",
                QString("Already at first run file (%1).").arg(currentInfo.fileName()),
                "step_run");
            return;
        }
        if (targetIdx >= static_cast<int>(candidateFilePaths.size())) {
            CommandPrompt::logInfo(
                "Spectrum",
                QString("Already at last run file (%1).").arg(currentInfo.fileName()),
                "step_run");
            return;
        }

        targetFilePath = candidateFilePaths[targetIdx];
        targetFileIndex = targetIdx;
        totalRunFiles = static_cast<int>(candidateFilePaths.size());
        displayFileName = QFileInfo(targetFilePath).fileName();
    }

    // Multi-spectrum / 1D Spectrum run file
    // Preserve the user-confirmed format and channel length from the active run sequence
    const int targetDetIndex = m_currentSpectrumIndex;
    SpectrumDetectionResult det = AutoDetectSpectrumFile(targetFilePath.toStdString());
    SpectrumFormat fmt = m_currentSpectrumFormat;
    int length = (m_currentSpectrumLength > 0) ? m_currentSpectrumLength : det.guessedLength;
    int totalCount = m_currentSpectrumCount;
    if (length > 0 && det.fileSize > 0) {
        switch (fmt) {
            case SpectrumFormat::ShortInt16:
                totalCount = std::max(1, static_cast<int>(det.fileSize / sizeof(uint16_t)) / length);
                break;
            case SpectrumFormat::LongInt32:
            case SpectrumFormat::Float32:
                totalCount = std::max(1, static_cast<int>(det.fileSize / sizeof(uint32_t)) / length);
                break;
            case SpectrumFormat::Double64:
                totalCount = std::max(1, static_cast<int>(det.fileSize / sizeof(double)) / length);
                break;
            case SpectrumFormat::Ascii:
            case SpectrumFormat::Auto:
                if (det.totalChannelsFound > 0) {
                    totalCount = std::max(1, det.totalChannelsFound / length);
                }
                break;
        }
    }

    int detIndexToLoad = targetDetIndex;
    if (detIndexToLoad >= totalCount) {
        detIndexToLoad = std::max(0, totalCount - 1);
    }

    std::vector<double> spectrumData;
    QString err;
    if (!ReadSpectrumData(targetFilePath.toStdString(), fmt, length, detIndexToLoad, spectrumData, &err) || spectrumData.empty()) {
        CommandPrompt::logError(
            "Spectrum",
            QString("Failed to read run file %1 (detector #%2): %3")
                .arg(displayFileName)
                .arg(detIndexToLoad)
                .arg(err));
        return;
    }

    // Preserve active zoom window and vertical scale
    TAxis *xAxis = HijF[SelectedElement_i][SelectedElement_j]->GetXaxis();
    bool wasZoomed = false;
    double prevXmin = 0.0, prevXmax = 0.0;
    if (xAxis && (xAxis->GetFirst() > 1 || xAxis->GetLast() < xAxis->GetNbins())) {
        wasZoomed = true;
        prevXmin = xAxis->GetBinLowEdge(xAxis->GetFirst());
        prevXmax = xAxis->GetBinUpEdge(xAxis->GetLast());
    }
    const double prevYmin = HijF[SelectedElement_i][SelectedElement_j]->GetMinimum();
    const double prevYmax = HijF[SelectedElement_i][SelectedElement_j]->GetMaximum();

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist) return;

    // Preserve active pad energy calibration across run stepping (*3/*4);
    // TracknHistogram::LoadFromData() resets calibration unconditionally.
    const bool wasCalibrated = trackHist->IsCalibrated();
    const auto prevCalibSegments = trackHist->GetCalibrationSegments();
    const Double_t prevA0 = trackHist->GetCalibA0();
    const Double_t prevA1 = trackHist->GetCalibA1();
    const Double_t prevA2 = trackHist->GetCalibA2();

    if (!trackHist->LoadFromData(spectrumData, targetFilePath.toStdString())) {
        CommandPrompt::logError(
            "Spectrum",
            QString("Failed to load spectrum data for run %1.").arg(displayFileName));
        return;
    }

    if (wasCalibrated) {
        if (!prevCalibSegments.empty()) {
            trackHist->SetSegmentedCalibration(prevCalibSegments);
        } else {
            trackHist->SetCalibration(prevA0, prevA1, prevA2);
        }
    }

    // Ensure baseline spectrum is present in HijC if it was previously empty
    if (HijC[SelectedElement_i][SelectedElement_j].empty()) {
        TH1F *baseClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        baseClone->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(baseClone);
    }

    if (delta > 0) {
        // Overlay new spectrum with the next color in palette
        TH1F *newClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();
        const int colorIdx = HijC[SelectedElement_i][SelectedElement_j].size() % colors_hist.size();
        newClone->SetLineColor(colors_hist[colorIdx]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(newClone);
    } else {
        // Stepping backwards (delta < 0, Macro 4):
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

    m_currentSpectrumFile = targetFilePath;
    m_currentSpectrumIndex = detIndexToLoad;
    m_currentSpectrumCount = totalCount;
    m_currentSpectrumLength = length;
    m_currentSpectrumFormat = fmt;

    if (labelSpectrumFile) {
        QString disp = displayFileName;
        if (m_currentSpectrumCount > 1) {
            disp += QString("#%1").arg(m_currentSpectrumIndex);
        }
        labelSpectrumFile->setText(disp);
    }
    if (m_loadBehavior == LoadBehavior::PreserveScale) {
        if (wasZoomed && xAxis) {
            xAxis->SetRangeUser(prevXmin, prevXmax);
        }
        if (prevYmax > prevYmin) {
            HijF[SelectedElement_i][SelectedElement_j]->GetYaxis()->SetRangeUser(prevYmin, prevYmax);
            HijF[SelectedElement_i][SelectedElement_j]->SetMinimum(prevYmin);
            HijF[SelectedElement_i][SelectedElement_j]->SetMaximum(prevYmax);
            for (TH1F *ov : HijC[SelectedElement_i][SelectedElement_j]) {
                if (!ov || ov == HijF[SelectedElement_i][SelectedElement_j]) continue;
                ov->GetYaxis()->SetRangeUser(prevYmin, prevYmax);
                ov->SetMinimum(prevYmin);
                ov->SetMaximum(prevYmax);
            }
        }
    } else {
        if (wasZoomed && xAxis) {
            xAxis->SetRangeUser(prevXmin, prevXmax);
            adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
        } else {
            adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
        }
    }
    updateAxisStatusLabels();

    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());
    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];

    if (maxElement_i > 1 || maxElement_j > 1) {
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    } else {
        canvas->getCanvas()->cd();
    }
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    // Redraw all previous spectra in HijC with "SAME" option
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size() - 1; ++k) {
        if (HijC[SelectedElement_i][SelectedElement_j][k]) {
            HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(
                colors_hist[k % colors_hist.size()]);
            if (maxElement_i > 1 || maxElement_j > 1) {
                canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
            } else {
                canvas->getCanvas()->cd();
            }
            HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
        }
    }

    ColorTheFrameOfTheHistogram();
    if (canvas && canvas->getCanvas()) {
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
    }

    const QString remoteSuffix = isRemoteStep ? ", remote" : "";
    if (m_currentSpectrumCount > 1) {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Loaded run %1#%2 (run %3/%4, %5 ch%6)")
                .arg(displayFileName)
                .arg(m_currentSpectrumIndex)
                .arg(targetFileIndex + 1)
                .arg(totalRunFiles)
                .arg(spectrumData.size())
                .arg(remoteSuffix),
            "step_run");
    } else {
        CommandPrompt::logInfo(
            "Spectrum",
            QString("Loaded run %1 (run %2/%3, %4 ch%5)")
                .arg(displayFileName)
                .arg(targetFileIndex + 1)
                .arg(totalRunFiles)
                .arg(spectrumData.size())
                .arg(remoteSuffix),
            "step_run");
    }

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
// QMainCanvas::openAllDialogsForInspection
//==============================================================================
// Secret shortcut action (Ctrl+Shift+D or F12) that instantiates and cascades
// all dialogs non-modally for comprehensive visual theme and typography testing.
//==============================================================================
void QMainCanvas::openAllDialogsForInspection() {
    QList<QDialog*> dialogList;

    // 1. Appearance & Typography Settings Dialog
    dialogList.append(Design::createAppearanceDialog(this, this));

    // 2. Display Parameters & Grid Dialog
    dialogList.append(new DisplayParamsDialog(this, this));

    // 3. Spectrum Import / Read Dialog
    dialogList.append(new SpectrumImportDialog(m_currentSpectrumFile.isEmpty() ? "sample.spe" : m_currentSpectrumFile, this));

    // 4. Spectrum Export / Write Dialog
    std::vector<double> exportSample(1024, 0.0);
    if (selectedHisto) {
        int n = selectedHisto->GetNbinsX();
        exportSample.resize(n);
        for (int i = 0; i < n; ++i) exportSample[i] = selectedHisto->GetBinContent(i + 1);
    }
    dialogList.append(new SpectrumExportDialog(exportSample, "sample_export.spe", SpectrumFormat::LongInt32, exportSample.size(), this));

    // 5. Track Fit / Visual Inspection Dialog
    dialogList.append(new TrackFitDialog(this, getActiveTracknHistogram(), m_currentSpectrumIndex, this));

    // 6. Matrix 2D Inspection Dialog
    dialogList.append(new MatrixDialog(m_currentMatrix, false, 0.0, 1.0, 0.0, this));

    // 7. Area / Peak Integration Dialog
    dialogList.append(new IntegralDialog(this, this));

    // 8. Efficiency Calibration Dialog
    dialogList.append(new EfficiencyDialog(this, this));

    // 9. Automated Energy Calibration Dialog
    dialogList.append(new AutoCalibDialog(this, this));

    // 10. Macros & Command Strings Dialog
    dialogList.append(new MacroDialog(this));

    // Cascade all dialogs cleanly across the screen
    const int startX = 30;
    const int startY = 30;
    const int stepX = 40;
    const int stepY = 40;

    for (int i = 0; i < dialogList.size(); ++i) {
        QDialog *dlg = dialogList[i];
        if (!dlg) continue;
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
        dlg->raise();
        dlg->move(startX + i * stepX, startY + i * stepY);
    }

    if (CommandPrompt::getInstance()) {
        CommandPrompt::getInstance()->appendPlainText(">>> [SECRET KEY Ctrl+Shift+D / F12]: Opened all 10 dialogs for visual inspection.\n");
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
// QMainCanvas::openEnCalDialog
//==============================================================================
// Opens the full Energy Calibration Manager (EnCal) dialog.
// Supported via button 'EnCal' or shortcut 'D + K'.
//==============================================================================
void QMainCanvas::openEnCalDialog() {
    runEnergyCalibrationDialog(this, m_currentSpectrumIndex);
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::openTrackFitDialog
//==============================================================================
// Opens the AutoTrace / TrackFit automated recalibration dialog (DT).
// Supported via button 'DT' or shortcut 'D + T'.
//==============================================================================
void QMainCanvas::openTrackFitDialog() {
    TracknHistogram *hist = getActiveTracknHistogram();
    if (!hist) {
        QMessageBox::warning(this, "AutoTrace / TrackFit", "No active spectrum loaded in the selected pad.");
        return;
    }
    TrackFitDialog dlg(this, hist, m_currentSpectrumIndex, this);
    dlg.exec();
    if (canvas) {
        canvas->setFocus();
    }
}

//==============================================================================
// QMainCanvas::onDirectAutoTrace
//==============================================================================
// Direct AutoTrace fit (*T): Triggered via Ctrl + Click on the DT button.
// Directly runs the automated track fit routine.
//==============================================================================
void QMainCanvas::onDirectAutoTrace() {
    TracknHistogram *hist = getActiveTracknHistogram();
    if (!hist) {
        QMessageBox::warning(this, "AutoTrace / TrackFit", "No active spectrum loaded in the selected pad.");
        return;
    }
    CommandPrompt::getInstance()->appendPlainText("Executing Direct AutoTrace (*T)...\n");
    TrackFitDialog dlg(this, hist, m_currentSpectrumIndex, this);
    dlg.onAutoTraceClicked();
    if (canvas) {
        canvas->setFocus();
    }
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
    prompt->appendPlainText(" Dn Cn Mn Zn n          Define, Execute, Show, Erase command string n=0...9\n");
    prompt->appendPlainText(" DM                     Open Macro & Command String Manager Dialog\n");
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
    prompt->appendPlainText(" ZA                     Delete all active markers\n");
    prompt->appendPlainText(" ZB ZI ZJ ZG ZV ZW     Delete corresponding type of markers (ZW for gate markers)\n");
    prompt->appendPlainText(" MW                    Redraw coincidence gate markers\n");
    prompt->appendPlainText(" CW                    Extract coincidence cut from compressed matrix\n");
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

    // Launch interactive in-app Help & Reference Guide Dialog
    if (!m_helpDialog) {
        m_helpDialog = new HelpDialog(this);
    }
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

void QMainCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F1) {
        offerHelp();
        return;
    }

    if (((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_D) ||
        event->key() == Qt::Key_F12) {
        openAllDialogsForInspection();
        return;
    }

    if (canvas && focusWidget() != canvas) {
        canvas->setFocus();
        QApplication::sendEvent(canvas, event);
        return;
    }
    QWidget::keyPressEvent(event);
}

void QMainCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (canvas && focusWidget() != canvas) {
        QApplication::sendEvent(canvas, event);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

//==============================================================================
// QMainCanvas::eventFilter
//==============================================================================
// Intercepts mouse and keyboard modifier combinations for:
// - Table 2: Clickable Axis Range Labels (X Min, X Max, Y Min, Y Max)
// - Table 3: Buttons with modifiers (btnDT, btnInc, btnDec)
//==============================================================================
bool QMainCanvas::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        const bool isLeft = (me->button() == Qt::LeftButton);
        const bool isRight = (me->button() == Qt::RightButton);
        const bool hasCtrl = (me->modifiers() & Qt::ControlModifier) || (QApplication::keyboardModifiers() & Qt::ControlModifier);

        // Table 2: Clickable Axis Range Labels
        if (watched == labelXMin && (isLeft || isRight)) {
            adjustAxisRange("XMin", isLeft, hasCtrl);
            return true;
        } else if (watched == labelXMax && (isLeft || isRight)) {
            adjustAxisRange("XMax", isLeft, hasCtrl);
            return true;
        } else if (watched == labelYMin && (isLeft || isRight)) {
            adjustAxisRange("YMin", isLeft, hasCtrl);
            return true;
        } else if (watched == labelYMax && (isLeft || isRight)) {
            adjustAxisRange("YMax", isLeft, hasCtrl);
            return true;
        }

        // Table 3: Buttons with Ctrl or Right Click
        if (watched == btnDT) {
            if (isLeft && hasCtrl) {
                onDirectAutoTrace();
                return true;
            }
        } else if (watched == btnInc) {
            if (isLeft && hasCtrl) {
                executeMacro(1);
                return true;
            } else if (isRight) {
                if (hasCtrl) {
                    executeMacro(3);
                } else {
                    onRunIncrement();
                }
                return true;
            }
        } else if (watched == btnDec) {
            if (isLeft && hasCtrl) {
                executeMacro(2);
                return true;
            } else if (isRight) {
                if (hasCtrl) {
                    executeMacro(4);
                } else {
                    onRunDecrement();
                }
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        const bool isLeft = (me->button() == Qt::LeftButton);
        const bool isRight = (me->button() == Qt::RightButton);
        const bool hasCtrl = (me->modifiers() & Qt::ControlModifier) || (QApplication::keyboardModifiers() & Qt::ControlModifier);

        if (watched == btnDT && isLeft && hasCtrl) {
            return true;
        }
        if ((watched == btnInc || watched == btnDec) && (isRight || hasCtrl)) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void QMainCanvas::onOpenCMClicked()
{
    QString initialDir;
    if (m_currentMatrix && m_currentMatrix->isOpen() && !m_currentMatrix->getFilePath().isEmpty()) {
        initialDir = QFileInfo(m_currentMatrix->getFilePath()).absolutePath();
    } else if (!m_currentSpectrumFile.isEmpty() && QFileInfo::exists(m_currentSpectrumFile)) {
        initialDir = QFileInfo(m_currentSpectrumFile).absolutePath();
    } else {
        initialDir = QDir::currentPath();
    }

    QString remotePath;
    bool isRemote = false;
    QString fileName = NuFileDialog::getOpenMatrix(
        this, tr("Open Compressed Matrix (CM)"), initialDir, &remotePath, &isRemote);

    if (fileName.isEmpty()) {
        CommandPrompt::logDebug("Matrix", "Open matrix cancelled.");
        if (canvas) canvas->setFocus();
        return;
    }

    openMatrixFile(fileName, isRemote, remotePath);
}

bool QMainCanvas::openMatrixFile(const QString &fileName, bool isRemote, const QString &remotePath)
{
    if (fileName.isEmpty()) {
        return false;
    }

    auto candidateMatrix = std::make_shared<MatrixReader>();
    QString errMsg;
    if (!candidateMatrix->open(fileName, &errMsg)) {
        CommandPrompt::logError(
            "Matrix",
            QString("Failed to open '%1': %2").arg(QFileInfo(fileName).fileName(), errMsg));
        QMessageBox::critical(this, tr("Open Matrix Error"), errMsg);
        if (canvas) canvas->setFocus();
        return false;
    }

    m_currentMatrix = std::move(candidateMatrix);

    if (isRemote) {
        m_currentRemotePath = remotePath;
    } else {
        m_currentRemotePath.clear();
    }

    if (btnGateCM) {
        btnGateCM->setEnabled(true);
    }

    const QString sourceLabel = isRemote
        ? QString("%1 (remote: %2)").arg(m_currentMatrix->getFileName(), remotePath)
        : m_currentMatrix->getFileName();
    CommandPrompt::logDebug(
        "Matrix",
        QString("Opened %1 (%2x%3 ch, %4)")
            .arg(sourceLabel)
            .arg(m_currentMatrix->getResolutionX())
            .arg(m_currentMatrix->getResolutionY())
            .arg(m_currentMatrix->isSymmetric() ? "symmetric" : "asymmetric"));
    CommandPrompt::logDebug(
        "Matrix",
        QString("Matrix header: version=%1, step=%2x%3, grid=%4x%5 tiles, segsize=%6, size=%7 MB, ratio=%8x, totalCounts=%9")
            .arg(m_currentMatrix->getVersion())
            .arg(m_currentMatrix->getStepX())
            .arg(m_currentMatrix->getStepY())
            .arg(m_currentMatrix->getNumDivX())
            .arg(m_currentMatrix->getNumDivY())
            .arg(m_currentMatrix->getTileSize())
            .arg(m_currentMatrix->getFileSize() / 1048576.0, 0, 'f', 2)
            .arg(m_currentMatrix->getCompressionRatio(), 0, 'f', 1)
            .arg(m_currentMatrix->getTotalCounts()));

    TracknHistogram *trackHist = getActiveTracknHistogram();
    bool isCalib = trackHist ? trackHist->IsCalibrated() : false;
    double a0 = trackHist ? trackHist->GetCalibA0() : 0.0;
    double a1 = trackHist ? trackHist->GetCalibA1() : 1.0;
    double a2 = trackHist ? trackHist->GetCalibA2() : 0.0;

    MatrixDialog dlg(m_currentMatrix, isCalib, a0, a1, a2, this);
    connect(&dlg, &MatrixDialog::loadProjectionRequested, this,
            [this, isRemote, remotePath](const std::vector<double> &data, const QString &title,
                   const std::vector<double> &bgData, const QString &bgTitle) {
                m_currentSpectrumFile.clear();
                m_currentSpectrumCount = 0;
                m_currentSpectrumIndex = 0;
                loadSpectrumDataToPad(data, title, false);
                if (!bgData.empty()) {
                    loadSpectrumDataToPad(bgData, bgTitle, true);
                }
                if (labelSpectrumFile) {
                    if (isRemote) {
                        labelSpectrumFile->setText(QString("[Remote CM] %1").arg(QFileInfo(remotePath).fileName()));
                    } else {
                        labelSpectrumFile->setText(title);
                    }
                }
            });

    if (dlg.exec() == QDialog::Rejected) {
        CommandPrompt::logDebug(
            "Matrix",
            QString("Closed matrix dialog without loading projection for '%1'.")
                .arg(m_currentMatrix->getFileName()));
    }

    if (canvas) canvas->setFocus();
    return true;
}

void QMainCanvas::onGateCMClicked()
{
    if (!m_currentMatrix || !m_currentMatrix->isOpen()) {
        QMessageBox::information(this, tr("No Matrix Loaded"),
            tr("Please open a compressed coincidence matrix (.cmat) first using 'Open CM'."));
        onOpenCMClicked();
        return;
    }

    TracknHistogram *trackHist = getActiveTracknHistogram();
    bool isCalib = trackHist ? trackHist->IsCalibrated() : false;
    double a0 = trackHist ? trackHist->GetCalibA0() : 0.0;
    double a1 = trackHist ? trackHist->GetCalibA1() : 1.0;
    double a2 = trackHist ? trackHist->GetCalibA2() : 0.0;

    std::vector<MatrixGateRegion> initialGates;
    for (size_t i = 0; i + 1 < gate_markers.size(); i += 2) {
        int g1 = static_cast<int>(std::round(gate_markers[i]));
        int g2 = static_cast<int>(std::round(gate_markers[i + 1]));
        if (g1 > g2) std::swap(g1, g2);
        initialGates.push_back({g1, g2});
    }

    if (initialGates.empty()) {
        if (zoom_markers.size() >= 2) {
            int z1 = static_cast<int>(std::round(zoom_markers[zoom_markers.size() - 2]));
            int z2 = static_cast<int>(std::round(zoom_markers[zoom_markers.size() - 1]));
            if (z1 > z2) std::swap(z1, z2);
            initialGates.push_back({z1, z2});
        } else if (range_markers.size() >= 2) {
            int r1 = static_cast<int>(std::round(range_markers[range_markers.size() - 2]));
            int r2 = static_cast<int>(std::round(range_markers[range_markers.size() - 1]));
            if (r1 > r2) std::swap(r1, r2);
            initialGates.push_back({r1, r2});
        } else {
            initialGates.push_back({150, 160});
        }
    }

    MatrixGateDialog dlg(m_currentMatrix, isCalib, a0, a1, a2, initialGates, this);
    connect(&dlg, &MatrixGateDialog::loadGateSliceRequested, this,
            [this](const std::vector<double> &data, const QString &title, bool asOverlay) {
                if (asOverlay) {
                    // If gating as overlay, remove any previous auto-background overlay from the base spectrum
                    auto &clones = HijC[SelectedElement_i][SelectedElement_j];
                    for (auto it = clones.begin(); it != clones.end(); ) {
                        if (*it && QString((*it)->GetTitle()).contains("[Auto BG]")) {
                            delete *it;
                            it = clones.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                loadSpectrumDataToPad(data, title, asOverlay);
            });

    dlg.exec();

    if (canvas) canvas->setFocus();
}

void QMainCanvas::loadSpectrumDataToPad(const std::vector<double> &data, const QString &title, bool asOverlay)
{
    if (data.empty()) return;

    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!trackHist) return;

    // Preserve active pad energy calibration across LoadFromData()
    const bool wasCalibrated = trackHist->IsCalibrated();
    const auto prevCalibSegments = trackHist->GetCalibrationSegments();
    const Double_t prevA0 = trackHist->GetCalibA0();
    const Double_t prevA1 = trackHist->GetCalibA1();
    const Double_t prevA2 = trackHist->GetCalibA2();

    if (!asOverlay) {
        if (!trackHist->LoadFromData(data, title.toStdString())) {
            CommandPrompt::logError(
                "Matrix",
                QString("Failed to load matrix spectrum '%1' into active histogram.").arg(title));
            return;
        }

        if (wasCalibrated) {
            if (!prevCalibSegments.empty()) {
                trackHist->SetSegmentedCalibration(prevCalibSegments);
            } else {
                trackHist->SetCalibration(prevA0, prevA1, prevA2);
            }
        }

        canvas->getCanvas()->SetBorderMode(0);
        const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
        canvas->getCanvas()->SetFillColor(rootBg);

        HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->UnZoom();
        adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
        maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
            HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());

        for (auto *h : HijC[SelectedElement_i][SelectedElement_j]) {
            delete h;
        }
        HijC[SelectedElement_i][SelectedElement_j].clear();
        HijC[SelectedElement_i][SelectedElement_j].push_back(
            (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());

        HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[0]);
        HijC[SelectedElement_i][SelectedElement_j].back()->SetLineColor(colors_hist[0]);

        if (maxElement_i > 1 || maxElement_j > 1) {
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        } else {
            canvas->getCanvas()->cd();
        }
        HijF[SelectedElement_i][SelectedElement_j]->Draw();

        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();

        if (labelSpectrumFile) {
            labelSpectrumFile->setText(title);
        }

        m_currentSpectrumLength = static_cast<int>(data.size());
        updateAxisStatusLabels();

        CommandPrompt::logInfo(
            "Matrix",
            QString("Loaded %1 (%2 ch)")
                .arg(title)
                .arg(data.size()));
    } else {
        TracknHistogram *overlayHist = dynamic_cast<TracknHistogram*>(trackHist->Clone());
        if (!overlayHist) return;

        overlayHist->LoadFromData(data, title.toStdString());
        if (wasCalibrated) {
            if (!prevCalibSegments.empty()) {
                overlayHist->SetSegmentedCalibration(prevCalibSegments);
            } else {
                overlayHist->SetCalibration(prevA0, prevA1, prevA2);
            }
        }
        const int colorIdx = HijC[SelectedElement_i][SelectedElement_j].size() % colors_hist.size();
        overlayHist->SetLineColor(colors_hist[colorIdx]);
        HijC[SelectedElement_i][SelectedElement_j].push_back(overlayHist);

        adjustYAxisToVisibleMax(HijF[SelectedElement_i][SelectedElement_j]);
        if (maxElement_i > 1 || maxElement_j > 1) {
            canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        } else {
            canvas->getCanvas()->cd();
        }
        HijF[SelectedElement_i][SelectedElement_j]->Draw();
        for (size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size(); ++k) {
            if (HijC[SelectedElement_i][SelectedElement_j][k]) {
                HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
            }
        }
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();

        CommandPrompt::logInfo(
            "Matrix",
            QString("Overlaid matrix gate: %1 (color index %2)")
                .arg(title)
                .arg(colorIdx));
    }

    updateAxisStatusLabels();
}

#include "IntegralDialog.h"

//==============================================================================
// QMainCanvas::openIntegralDialog
//==============================================================================
void QMainCanvas::openIntegralDialog()
{
    if (!m_integralDialog) {
        m_integralDialog = new IntegralDialog(this, this);
    }
    m_integralDialog->updateFromCanvas();
    
    // Position at the top right of the main canvas
    m_integralDialog->move(this->mapToGlobal(QPoint(this->width() - m_integralDialog->sizeHint().width() - 40, 40)));
    
    m_integralDialog->show();
}

//==============================================================================
// QMainCanvas::closeIntegralDialog
//==============================================================================
void QMainCanvas::closeIntegralDialog()
{
    if (m_integralDialog) {
        m_integralDialog->hide();
    }
}

//==============================================================================
// QMainCanvas::printPlot
//==============================================================================
void QMainCanvas::printPlot()
{
    if (!canvas || !canvas->getCanvas()) {
        CommandPrompt::logError("Print", "Canvas is not initialized; cannot export plot.");
        return;
    }

    QString suggestedPath;
    const bool preferRemote = (m_remoteSession && m_remoteSession->isConnected() && !m_currentRemotePath.isEmpty());
    if (preferRemote) {
        QFileInfo rInfo(m_currentRemotePath);
        QString rDir = rInfo.dir().path();
        if (rDir == ".") rDir = "";
        const QString base = rInfo.baseName().isEmpty() ? QStringLiteral("spectrum") : rInfo.baseName();
        suggestedPath = rDir.isEmpty()
            ? ("/" + base + "_plot.pdf")
            : (rDir.endsWith('/') ? rDir + base + "_plot.pdf" : rDir + "/" + base + "_plot.pdf");
    } else if (!m_currentSpectrumFile.isEmpty() && QFileInfo::exists(m_currentSpectrumFile)) {
        QFileInfo fi(m_currentSpectrumFile);
        suggestedPath = fi.dir().filePath(fi.baseName() + "_plot.pdf");
    } else if (m_currentMatrix && m_currentMatrix->isOpen() && !m_currentMatrix->getFilePath().isEmpty()) {
        QFileInfo fi(m_currentMatrix->getFilePath());
        suggestedPath = fi.dir().filePath(fi.baseName() + "_plot.pdf");
    } else {
        suggestedPath = QDir::current().filePath(QStringLiteral("spectrum_plot.pdf"));
    }

    QString selectedFilter = tr("PDF Files (*.pdf)");
    QString remotePath;
    bool isRemote = false;
    QString fileName = NuFileDialog::getSaveFileName(
        this, tr("Save Plot as Postscript/PDF"), suggestedPath,
        tr("PDF Files (*.pdf);;Postscript (*.ps);;PNG Image (*.png)"),
        &selectedFilter, &remotePath, &isRemote, preferRemote ? 1 : 0);

    if (fileName.isEmpty()) {
        CommandPrompt::logDebug("Print", "Plot export cancelled.");
        return;
    }

    QString defaultExt = QStringLiteral(".pdf");
    if (selectedFilter.contains("*.ps", Qt::CaseInsensitive)) {
        defaultExt = QStringLiteral(".ps");
    } else if (selectedFilter.contains("*.png", Qt::CaseInsensitive)) {
        defaultExt = QStringLiteral(".png");
    }

    auto ensureKnownExt = [&defaultExt](const QString &path) -> QString {
        const QString lower = path.toLower();
        if (lower.endsWith(".pdf") || lower.endsWith(".ps") ||
            lower.endsWith(".eps") || lower.endsWith(".png") ||
            lower.endsWith(".svg")) {
            return path;
        }
        return path + defaultExt;
    };
    fileName = ensureKnownExt(fileName);
    if (isRemote && !remotePath.isEmpty()) {
        remotePath = ensureKnownExt(remotePath);
    }

    TCanvas *tc = canvas->getCanvas();
    const QColor bgColor = Design::getGraphBackgroundColor();
    const Color_t rootBg = TColor::GetColor(bgColor.name().toUtf8().constData());
    const Color_t axisCol = (bgColor.lightness() > 130) ? kBlack : kWhite;

    tc->SetBorderMode(0);
    tc->SetFillColor(rootBg);
    tc->SetFrameFillColor(rootBg);
    tc->SetFrameLineColor(axisCol);
    if (tc->GetFrame()) {
        tc->GetFrame()->SetFillColor(rootBg);
        tc->GetFrame()->SetLineColor(axisCol);
    }
    if (tc->GetListOfPrimitives()) {
        TIter next(tc->GetListOfPrimitives());
        TObject *obj = nullptr;
        while ((obj = next())) {
            if (obj && obj->InheritsFrom(TPad::Class())) {
                TPad *pad = static_cast<TPad*>(obj);
                pad->SetBorderMode(0);
                pad->SetFillColor(rootBg);
                pad->SetFrameFillColor(rootBg);
                pad->SetFrameLineColor(axisCol);
                if (pad->GetFrame()) {
                    pad->GetFrame()->SetFillColor(rootBg);
                    pad->GetFrame()->SetLineColor(axisCol);
                }
            }
        }
    }

    // Flush Qt/X11 expose events after modal dialog closes so raster (PNG) exports capture a clean canvas
    qApp->processEvents();
    tc->Modified();
    tc->Update();
    gSystem->ProcessEvents();

    // Remove any pre-existing local/staged file so a failed SaveAs cannot mask as stale success
    QFile::remove(fileName);

    // Guard LC_NUMERIC="C" so ROOT's TPDF/TPostScript never emits locale commas,
    // and scale PS/PDF line widths from ROOT's default 3.0x down to 0.75pt/px (1px @ 96dpi)
    const Float_t prevLineScale = gStyle->GetLineScalePS();
    gStyle->SetLineScalePS(0.75f);
    const char *prevLocale = std::setlocale(LC_NUMERIC, nullptr);
    std::string savedLocale = prevLocale ? prevLocale : "";
    std::setlocale(LC_NUMERIC, "C");
    tc->SaveAs(fileName.toStdString().c_str());
    if (!savedLocale.empty() && savedLocale != "C") {
        std::setlocale(LC_NUMERIC, savedLocale.c_str());
    }
    gStyle->SetLineScalePS(prevLineScale);

    QFileInfo outInfo(fileName);
    if (!outInfo.exists() || outInfo.size() <= 0) {
        CommandPrompt::logError("Print",
            QString("Failed to export plot to '%1' (output file missing or empty).").arg(fileName));
        return;
    }

    const qint64 bytes = outInfo.size();
    const QString sizeStr = (bytes >= 1024 * 1024)
        ? QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2)
        : QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);

    CommandPrompt::logDebug("Print",
        QString("Exported %1x%2 canvas (%3x%4 grid, bg=%5, lineScalePS=0.75) -> '%6' (%7).")
            .arg(tc->GetWw()).arg(tc->GetWh())
            .arg(maxElement_i).arg(maxElement_j)
            .arg(bgColor.name())
            .arg(fileName, sizeStr));

    if (isRemote && !remotePath.isEmpty()) {
        QString uploadErr;
        if (NuFileDialog::finishRemoteSave(fileName, remotePath, &uploadErr, this)) {
            CommandPrompt::logInfo("Print",
                QString("Plot exported & uploaded to remote: '%1' (%2).").arg(remotePath, sizeStr));
        } else {
            CommandPrompt::logError("Print",
                QString("Plot saved locally to '%1', but remote upload to '%2' failed: %3")
                    .arg(fileName, remotePath, uploadErr));
        }
    } else {
        CommandPrompt::logInfo("Print",
            QString("Plot saved to '%1' (%2).").arg(fileName, sizeStr));
    }
}

//==============================================================================
// QMainCanvas::setYMax
//==============================================================================
void QMainCanvas::setYMax(double yVal)
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) return;

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist && canvas && canvas->getCanvas()) {
        // Convert mouse coordinate Y to pad coordinate
        TVirtualPad *pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
        if (!pad) return;

        pad->cd();
        Double_t yCursorVal = pad->PadtoY(pad->AbsPixeltoY(static_cast<Int_t>(yVal)));
        const bool isLog = (pad->GetLogy() != 0);

        if (isLog && yCursorVal <= 0.0) {
            yCursorVal = 1.0;
        }

        double curMin = hist->GetMinimum();
        if (curMin == -1111.0 || (isLog && curMin <= 0.0)) {
            curMin = isLog ? 0.5 : 0.0;
        }
        if (yCursorVal <= curMin) {
            curMin = isLog ? std::max(0.1, yCursorVal * 0.1) : 0.0;
            if (yCursorVal <= curMin) {
                yCursorVal = curMin + 10.0;
            }
        }

        hist->GetYaxis()->SetRangeUser(curMin, yCursorVal);
        hist->SetMaximum(yCursorVal);
        hist->SetMinimum(curMin);

        for (TH1F *overlay : HijC[SelectedElement_i][SelectedElement_j]) {
            if (!overlay || overlay == hist) continue;
            overlay->GetYaxis()->SetRangeUser(curMin, yCursorVal);
            overlay->SetMaximum(yCursorVal);
            overlay->SetMinimum(curMin);
        }

        ColorTheFrameOfTheHistogram();
        renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
        renderPeakLabels(SelectedElement_i, SelectedElement_j);
        pad->Modified();
        pad->Update();
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
        updateAxisStatusLabels();
        if (canvas) {
            canvas->setFocus();
        }

        CommandPrompt::logDebug("Display", QString("Y-Max forced to %1 (FO).").arg(yCursorVal));
    }
}

//==============================================================================
// QMainCanvas::setYMin
//==============================================================================
void QMainCanvas::setYMin(double yVal)
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) return;

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist && canvas && canvas->getCanvas()) {
        TVirtualPad *pad = canvas->getCanvas()->GetPad((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        if (!pad) pad = canvas->getCanvas();
        if (!pad) return;

        pad->cd();
        Double_t yCursorVal = pad->PadtoY(pad->AbsPixeltoY(static_cast<Int_t>(yVal)));
        const bool isLog = (pad->GetLogy() != 0);

        if (isLog && yCursorVal <= 0.0) {
            yCursorVal = 0.5;
        }

        double curMax = hist->GetMaximum();
        if (curMax <= yCursorVal || curMax == -1111.0) {
            curMax = isLog ? yCursorVal * 10.0 : yCursorVal + 10.0;
        }

        hist->GetYaxis()->SetRangeUser(yCursorVal, curMax);
        hist->SetMinimum(yCursorVal);
        hist->SetMaximum(curMax);

        for (TH1F *overlay : HijC[SelectedElement_i][SelectedElement_j]) {
            if (!overlay || overlay == hist) continue;
            overlay->GetYaxis()->SetRangeUser(yCursorVal, curMax);
            overlay->SetMinimum(yCursorVal);
            overlay->SetMaximum(curMax);
        }

        ColorTheFrameOfTheHistogram();
        renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
        renderPeakLabels(SelectedElement_i, SelectedElement_j);
        pad->Modified();
        pad->Update();
        canvas->getCanvas()->Modified();
        canvas->getCanvas()->Update();
        updateAxisStatusLabels();
        if (canvas) {
            canvas->setFocus();
        }

        CommandPrompt::logDebug("Display", QString("Y-Min forced to %1 (FU).").arg(yCursorVal));
    }
}

void QMainCanvas::defineAreaLogFile() {
    QString suggestedPath;
    const bool preferRemote = (m_remoteSession && m_remoteSession->isConnected() && !m_currentRemotePath.isEmpty());
    if (preferRemote) {
        QFileInfo rInfo(m_currentRemotePath);
        QString rDir = rInfo.dir().path();
        if (rDir == ".") rDir = "";
        const QString base = rInfo.baseName().isEmpty() ? QStringLiteral("calculations") : (rInfo.baseName() + "_calc");
        suggestedPath = rDir.isEmpty()
            ? ("/" + base + ".area")
            : (rDir.endsWith('/') ? rDir + base + ".area" : rDir + "/" + base + ".area");
    } else if (!m_currentSpectrumFile.isEmpty() && QFileInfo::exists(m_currentSpectrumFile)) {
        QFileInfo fi(m_currentSpectrumFile);
        suggestedPath = fi.dir().filePath(fi.baseName() + "_calc.area");
    } else if (m_currentMatrix && m_currentMatrix->isOpen() && !m_currentMatrix->getFilePath().isEmpty()) {
        QFileInfo fi(m_currentMatrix->getFilePath());
        suggestedPath = fi.dir().filePath(fi.baseName() + "_calc.area");
    } else {
        suggestedPath = QDir::current().filePath(QStringLiteral("calculations.area"));
    }

    QString selectedFilter = tr("Area Log (*.area)");
    QString remotePath;
    bool isRemote = false;
    QString fileName = NuFileDialog::getSaveFileName(
        this, tr("Define Area Output File"), suggestedPath,
        tr("Area Log (*.area);;Text Files (*.txt)"),
        &selectedFilter, &remotePath, &isRemote, preferRemote ? 1 : 0, false);

    if (fileName.isEmpty()) {
        CommandPrompt::logDebug("CalcLog", "Calculation logfile selection cancelled.");
        if (canvas) canvas->setFocus();
        return;
    }

    const QString defaultExt = selectedFilter.contains("*.txt", Qt::CaseInsensitive)
        ? QStringLiteral(".txt")
        : QStringLiteral(".area");
    auto ensureLogExt = [&defaultExt](const QString &path) -> QString {
        const QString lower = path.toLower();
        if (lower.endsWith(".area") || lower.endsWith(".txt") || lower.endsWith(".log")) {
            return path;
        }
        return path + defaultExt;
    };
    fileName = ensureLogExt(fileName);
    if (isRemote && !remotePath.isEmpty()) {
        remotePath = ensureLogExt(remotePath);
    }

    bool appendMode = true;
    const bool existsAlready = (isRemote && !remotePath.isEmpty())
        ? (m_remoteSession && m_remoteSession->isConnected() && m_remoteSession->fileExists(remotePath))
        : QFileInfo::exists(fileName);

    if (existsAlready) {
        const QString displayName = (isRemote && !remotePath.isEmpty())
            ? QFileInfo(remotePath).fileName()
            : QFileInfo(fileName).fileName();
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Calculation Logfile Exists"));
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("The calculation logfile '%1' already exists.\nWould you like to append new results or overwrite it?")
                           .arg(displayName));
        QPushButton *btnAppend = msgBox.addButton(tr("Append"), QMessageBox::AcceptRole);
        QPushButton *btnOverwrite = msgBox.addButton(tr("Overwrite"), QMessageBox::DestructiveRole);
        QPushButton *btnCancel = msgBox.addButton(QMessageBox::Cancel);
        msgBox.setDefaultButton(btnAppend);
        msgBox.exec();

        if (msgBox.clickedButton() == btnCancel || !msgBox.clickedButton()) {
            CommandPrompt::logDebug("CalcLog", "Calculation logfile selection cancelled.");
            if (canvas) canvas->setFocus();
            return;
        }
        appendMode = (msgBox.clickedButton() != btnOverwrite);
    }

    if (startAreaLogging(fileName, remotePath, appendMode)) {
        const QString destLabel = isRemote && !remotePath.isEmpty() ? remotePath : fileName;
        CommandPrompt::logInfo("CalcLog",
            QString("Calculation logging enabled (%1) -> '%2'.")
                .arg(appendMode ? "append" : "overwrite", destLabel));
    } else {
        CommandPrompt::logError("CalcLog", QString("Failed to open calculation logfile '%1'.").arg(fileName));
    }
    if (canvas) canvas->setFocus();
}

void QMainCanvas::closeAreaLogFile() {
    QString logName = getAreaLogFileName();
    if (stopAreaLogging()) {
        CommandPrompt::logInfo("CalcLog", QString("Calculation logging closed: '%1'.").arg(logName));
    } else {
        CommandPrompt::logWarning("CalcLog", "No calculation logfile is currently open.");
    }
    if (canvas) canvas->setFocus();
}

bool QMainCanvas::startAreaLogging(const QString& fileName, const QString& remotePath, bool appendMode) {
    if (m_areaLogFile.isOpen()) {
        m_areaLogStream.flush();
        m_areaLogFile.close();
        if (!m_areaLogRemotePath.isEmpty()) {
            QString prevErr;
            if (!NuFileDialog::finishRemoteSave(m_areaLogFile.fileName(), m_areaLogRemotePath, &prevErr, this)) {
                CommandPrompt::logError("CalcLog",
                    QString("Failed to upload previous calculation logfile '%1': %2").arg(m_areaLogRemotePath, prevErr));
            }
        }
    }

    // If appending to an existing remote logfile, synchronize it to local cache first
    // so new entries append rather than overwriting prior results.
    if (appendMode && !remotePath.isEmpty() && m_remoteSession && m_remoteSession->isConnected() &&
        m_remoteSession->fileExists(remotePath)) {
        QString syncedLocal;
        QString syncErr;
        if (m_remoteSession->syncRemoteFileToCache(remotePath, syncedLocal, nullptr, &syncErr)) {
            CommandPrompt::logDebug("CalcLog",
                QString("Synchronized existing remote logfile '%1' for append.").arg(remotePath));
        } else {
            CommandPrompt::logWarning("CalcLog",
                QString("Could not sync existing remote logfile '%1' (%2); appending locally.")
                    .arg(remotePath, syncErr));
        }
    }

    m_areaLogRemotePath = remotePath;
    m_areaLogFile.setFileName(fileName);
    const QIODevice::OpenMode openFlags = QIODevice::WriteOnly | QIODevice::Text |
                                          (appendMode ? QIODevice::Append : QIODevice::Truncate);
    if (m_areaLogFile.open(openFlags)) {
        m_areaLogStream.setDevice(&m_areaLogFile);
        m_isAreaLoggingEnabled = true;
        m_lastAreaLogContext = AreaLogContext::None;
        m_lastAreaLogTarget.clear();
        if (btnRec) {
            btnRec->setChecked(true);
            btnRec->setText("● Rec");
        }
        return true;
    }
    m_isAreaLoggingEnabled = false;
    m_lastAreaLogContext = AreaLogContext::None;
    m_lastAreaLogTarget.clear();
    if (btnRec) {
        btnRec->setChecked(false);
        btnRec->setText("Rec");
    }
    return false;
}

bool QMainCanvas::stopAreaLogging() {
    bool wasOpen = m_areaLogFile.isOpen();
    if (wasOpen) {
        m_areaLogStream.flush();
        m_areaLogFile.close();
        if (!m_areaLogRemotePath.isEmpty()) {
            QString uploadErr;
            if (!NuFileDialog::finishRemoteSave(m_areaLogFile.fileName(), m_areaLogRemotePath, &uploadErr, this)) {
                CommandPrompt::logError("CalcLog",
                    QString("Failed to upload calculation logfile to '%1': %2")
                        .arg(m_areaLogRemotePath, uploadErr));
            } else {
                CommandPrompt::logDebug("CalcLog",
                    QString("Uploaded calculation logfile to '%1'.").arg(m_areaLogRemotePath));
            }
            m_areaLogRemotePath.clear();
        }
    }
    m_isAreaLoggingEnabled = false;
    m_lastAreaLogContext = AreaLogContext::None;
    m_lastAreaLogTarget.clear();
    if (btnRec) {
        btnRec->setChecked(false);
        btnRec->setText("Rec");
    }
    return wasOpen;
}

void QMainCanvas::writeAreaLogHeader(bool isFitting) {
    if (!m_isAreaLoggingEnabled || !m_areaLogFile.isOpen()) return;

    AreaLogContext targetContext = isFitting ? AreaLogContext::Fitting : AreaLogContext::Integration;

    TracknHistogram *activeHist = getActiveTracknHistogram();
    const bool isCalib = (activeHist && activeHist->IsCalibrated());
    const QString unitTag = isCalib ? QStringLiteral("keV") : QStringLiteral("ch");

    QString specName;
    if (!m_currentRemotePath.isEmpty()) {
        specName = QFileInfo(m_currentRemotePath).fileName();
    } else if (!m_currentSpectrumFile.isEmpty()) {
        specName = QFileInfo(m_currentSpectrumFile).fileName();
    } else if (labelSpectrumFile && !labelSpectrumFile->text().isEmpty() &&
               labelSpectrumFile->text() != "<none>") {
        specName = labelSpectrumFile->text();
    } else if (activeHist && strlen(activeHist->GetTitle()) > 0) {
        specName = QString::fromUtf8(activeHist->GetTitle());
    } else {
        specName = QStringLiteral("Active Spectrum");
    }

    const QString targetTag = QString("%1 | %2").arg(specName, unitTag);
    if (m_lastAreaLogContext == targetContext && m_lastAreaLogTarget == targetTag) return;

    m_lastAreaLogContext = targetContext;
    m_lastAreaLogTarget = targetTag;

    const QString sepLine = QString(98, '-') + "\n";
    m_areaLogStream << sepLine;
    if (isFitting) {
        m_areaLogStream << QString("Gauss Fit Results [%1]:\n").arg(targetTag);
    } else {
        m_areaLogStream << QString("Integration Results [%1]:\n").arg(targetTag);
    }
    const QString centCol = QString("Centroid (%1)").arg(unitTag);
    const QString fwhmCol = QString("FWHM (%1)").arg(unitTag);
    const QString headerRow = QString("%1%2%3%4%5%6\n")
        .arg(centCol,    -16, QChar(' '))
        .arg(fwhmCol,    -14, QChar(' '))
        .arg("Gross",    -18, QChar(' '))
        .arg("Net",      -18, QChar(' '))
        .arg("Bkg",      -18, QChar(' '))
        .arg("Error",    -14, QChar(' '));
    m_areaLogStream << headerRow;
    m_areaLogStream << sepLine;
    m_areaLogStream.flush();
}

void QMainCanvas::writeAreaLogData(double centroid, double fwhm, double gross, double net, double background, double error) {
    if (!m_isAreaLoggingEnabled || !m_areaLogFile.isOpen()) return;
    const QString dataRow = QString("%1%2%3%4%5%6\n")
        .arg(QString::number(centroid, 'f', 2),   -16, QChar(' '))
        .arg(QString::number(fwhm, 'f', 2),       -14, QChar(' '))
        .arg(QString::number(gross, 'f', 1),      -18, QChar(' '))
        .arg(QString::number(net, 'f', 1),        -18, QChar(' '))
        .arg(QString::number(background, 'f', 1), -18, QChar(' '))
        .arg(QString::number(error, 'f', 1),      -14, QChar(' '));
    m_areaLogStream << dataRow;
    m_areaLogStream.flush();
}

//==============================================================================
// Block 3: Setup & Parameter Definition Slots (DD, DE, DG, AK)
//==============================================================================
void QMainCanvas::openDisplayParamsDialog() {
    DisplayParamsDialog dlg(this, this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openLoadBehaviorDialog() {
    QStringList items;
    items << tr("Auto Scale") << tr("Preserve Scale");
    const int currentIdx = (m_loadBehavior == LoadBehavior::PreserveScale) ? 1 : 0;
    bool ok = false;
    QString item = QInputDialog::getItem(this, tr("Set Display Behavior"),
                                         tr("Select behavior on new spectrum:"), items, currentIdx, false, &ok);
    if (ok && !item.isEmpty()) {
        if (item == tr("Auto Scale")) {
            setLoadBehavior(LoadBehavior::Autoscale);
        } else {
            setLoadBehavior(LoadBehavior::PreserveScale);
        }
        CommandPrompt::logDebug("Display", QString("Display behavior set to %1 (DN).").arg(item));
    }
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openEfficiencyDialog() {
    EfficiencyDialog dlg(this, this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openPeakWidthModeDialog() {
    QStringList items;
    items << tr("Coupled / Common Width (GASP standard)")
          << tr("Independent / Decoupled Width (individual widths)");
    int currentIdx = m_uncoupleWidths ? 1 : 0;
    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, tr("Define Peak Width Mode (DG)"),
        tr("Select Gaussian peak width model for multi-peak fitting:"),
        items, currentIdx, false, &ok);
    if (ok && !selected.isEmpty()) {
        m_uncoupleWidths = (selected == items[1]);
        if (chkUncoupleWidths) {
            chkUncoupleWidths->setChecked(m_uncoupleWidths);
        }
        CommandPrompt::getInstance()->appendPlainText(
            QString("Peak width mode set to: %1 (DG)\n")
                .arg(m_uncoupleWidths ? "Independent / Decoupled" : "Coupled / Common"));
    }
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openAutoCalibDialog() {
    AutoCalibDialog dlg(this, this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

void QMainCanvas::openRunByRunManager() {
    RunByRunManager dlg(this, this);
    dlg.exec();
    if (canvas) canvas->setFocus();
}

double QMainCanvas::evaluateEfficiency(double energyKeV) const {
    if (m_efficiencyConfig.type == EfficiencyType::None) {
        return 1.0;
    }
    if (m_efficiencyConfig.type == EfficiencyType::Polynomial) {
        if (energyKeV <= 0.01) return 1.0;
        double lnE = std::log(energyKeV);
        double sum = 0.0;
        double term = 1.0;
        for (double c : m_efficiencyConfig.coeffs) {
            sum += c * term;
            term *= lnE;
        }
        double eff = std::exp(sum) * m_efficiencyConfig.scalingFactor;
        return (eff > 0.0) ? eff : 1.0;
    }
    if (m_efficiencyConfig.type == EfficiencyType::Spectrum) {
        const auto &vec = m_efficiencyConfig.spectrumData;
        int idx = static_cast<int>(std::round(energyKeV));
        if (idx >= 0 && idx < static_cast<int>(vec.size()) && vec[idx] > 0.0) {
            return vec[idx] * m_efficiencyConfig.scalingFactor;
        }
    }
    return 1.0;
}

//==============================================================================
// QMainCanvas::onOpenRemoteDataClicked
//==============================================================================
// Establishes/manages secure SSH/SFTP session to a remote computer, opens the
// remote file browser, caches selected data locally, and loads spectra/matrices.
//==============================================================================
void QMainCanvas::onOpenRemoteDataClicked()
{
    if (!m_remoteSession) {
        m_remoteSession = RemoteSession::globalSession();
    }

    RemoteConnectDialog connectDlg(m_remoteSession, this);
    connectDlg.exec();

    updateSSHButtonState();

    if (canvas) canvas->setFocus();
}

void QMainCanvas::updateSSHButtonState()
{
    if (!btnRemote) return;

    if (!m_remoteSession) {
        m_remoteSession = RemoteSession::globalSession();
    }

    if (m_remoteSession && m_remoteSession->isConnected()) {
        const RemoteProfile &prof = m_remoteSession->getProfile();
        QString hostName = prof.name.isEmpty() ? prof.host : prof.name;
        btnRemote->setText(QString("SSH: %1").arg(hostName));
        btnRemote->setToolTip(tr("Connected to %1@%2:%3\nRemote Working Dir: %4\nClick to manage parameters, verify connection, or disconnect.")
            .arg(prof.user, prof.host).arg(prof.port)
            .arg(m_remoteSession->getCurrentRemoteDir()));
        btnRemote->setStyleSheet(
            "QPushButton { background-color: #1b5e20; color: #e8f5e9; font-weight: bold; border: 1px solid #4caf50; border-radius: 4px; padding: 4px 8px; }"
            "QPushButton:hover { background-color: #2e7d32; }");

        QString remoteDir = m_remoteSession->getCurrentRemoteDir();
        if (labelWorkingPath) {
            labelWorkingPath->setText(QString("[SSH] %1:%2").arg(prof.host, remoteDir));
        }
    } else {
        btnRemote->setText(tr("SSH Connect"));
        btnRemote->setToolTip(tr("Configure parameters and connect to Remote DAQ Computer over SSH/SFTP (Ctrl+Shift+R)."));
        btnRemote->setStyleSheet(Design::getButtonStyleSheet());

        if (labelWorkingPath) {
            labelWorkingPath->setText(QDir::currentPath());
        }
    }
}


