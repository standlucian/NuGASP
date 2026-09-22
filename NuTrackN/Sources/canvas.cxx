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

#include <TCanvas.h>
#include <TH1F.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TVirtualX.h>
#include <TVirtualPad.h>
#include <TLatex.h>
#include <TLine.h>
#include <TStyle.h>

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

    QPushButton *btnEnCal = makeButton("EnCal", topContainer, true);
    btnEnCal->setFixedWidth(93);
    btnEnCal->setFixedHeight(36);
    btnEnCal->setToolTip("Open Energy Calibration Manager (*E / DK) [Shortcut: D+K]");
    leftBar->addWidget(btnEnCal);
    connect(btnEnCal, &QPushButton::clicked, this, &QMainCanvas::openEnCalDialog);

    QPushButton *btnDT = makeButton("DT", topContainer, true);
    btnDT->setFixedWidth(93);
    btnDT->setFixedHeight(36);
    btnDT->setToolTip("AutoTrace / TrackFit automated recalibration (*T / DT) [Shortcut: D+T]");
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
    connect(btnInt, &QPushButton::clicked, this, &QMainCanvas::areaFunctionWithBackground);

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

    btnOpenCM = makeButton("Open CM", topContainer, true);
    btnOpenCM->setFixedHeight(36);
    btnOpenCM->setToolTip(tr("Open GASPware compressed coincidence matrix (.cmat)."));
    connect(btnOpenCM, &QPushButton::clicked, this, &QMainCanvas::onOpenCMClicked);
    bottomBtnGrid->addWidget(btnOpenCM, 1, 0, 1, 2);

    btnGateCM = makeButton("Gate CM", topContainer, true);
    btnGateCM->setFixedHeight(36);
    btnGateCM->setToolTip(tr("Project 1D coincidence gate from loaded matrix."));
    connect(btnGateCM, &QPushButton::clicked, this, &QMainCanvas::onGateCMClicked);
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
    gStyle->SetOptTitle(0);
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

    // Clean up old comparison overlays on this pad for the freshly loaded spectrum file
    for (auto *h : HijC[SelectedElement_i][SelectedElement_j]) {
        delete h;
    }
    HijC[SelectedElement_i][SelectedElement_j].clear();
    HijC[SelectedElement_i][SelectedElement_j].push_back(
        (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());

    // Set line color and draw to active pad
    HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(colors_hist[0]);
    HijC[SelectedElement_i][SelectedElement_j].back()->SetLineColor(colors_hist[0]);
    if (maxElement_i > 1 || maxElement_j > 1) {
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    } else {
        canvas->getCanvas()->cd();
    }
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

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

    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
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
}

void QMainCanvas::keyPressEvent(QKeyEvent *event)
{
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

void QMainCanvas::onOpenCMClicked()
{
    QString initialDir = m_currentSpectrumFile.isEmpty()
        ? QDir::currentPath()
        : QFileInfo(m_currentSpectrumFile).absolutePath();

    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open Compressed Matrix (CM)"), initialDir,
        tr("GASPware Matrix (*.cmat *.mat);;All Files (*)"));

    if (fileName.isEmpty()) {
        if (canvas) canvas->setFocus();
        return;
    }

    if (!m_currentMatrix) {
        m_currentMatrix = std::make_shared<MatrixReader>();
    }

    QString errMsg;
    if (!m_currentMatrix->open(fileName, &errMsg)) {
        QMessageBox::critical(this, tr("Open Matrix Error"), errMsg);
        if (canvas) canvas->setFocus();
        return;
    }

    if (btnGateCM) {
        btnGateCM->setEnabled(true);
    }

    TracknHistogram *trackHist = getActiveTracknHistogram();
    bool isCalib = trackHist ? trackHist->IsCalibrated() : false;
    double a0 = trackHist ? trackHist->GetCalibA0() : 0.0;
    double a1 = trackHist ? trackHist->GetCalibA1() : 1.0;
    double a2 = trackHist ? trackHist->GetCalibA2() : 0.0;

    MatrixDialog dlg(m_currentMatrix, isCalib, a0, a1, a2, this);
    connect(&dlg, &MatrixDialog::loadProjectionRequested, this,
            [this](const std::vector<double> &data, const QString &title) {
                loadSpectrumDataToPad(data, title, false);
            });

    dlg.exec();

    if (canvas) canvas->setFocus();
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

    int initGateMin = -1, initGateMax = -1;
    if (gate_markers.size() >= 2) {
        double g1 = gate_markers[gate_markers.size() - 2];
        double g2 = gate_markers[gate_markers.size() - 1];
        initGateMin = static_cast<int>(std::round(std::min(g1, g2)));
        initGateMax = static_cast<int>(std::round(std::max(g1, g2)));
    } else if (zoom_markers.size() >= 2) {
        double z1 = zoom_markers[zoom_markers.size() - 2];
        double z2 = zoom_markers[zoom_markers.size() - 1];
        initGateMin = static_cast<int>(std::round(std::min(z1, z2)));
        initGateMax = static_cast<int>(std::round(std::max(z1, z2)));
    } else if (range_markers.size() >= 2) {
        double r1 = range_markers[range_markers.size() - 2];
        double r2 = range_markers[range_markers.size() - 1];
        initGateMin = static_cast<int>(std::round(std::min(r1, r2)));
        initGateMax = static_cast<int>(std::round(std::max(r1, r2)));
    }

    MatrixGateDialog dlg(m_currentMatrix, isCalib, a0, a1, a2, initGateMin, initGateMax, this);
    connect(&dlg, &MatrixGateDialog::loadGateSliceRequested, this,
            [this](const std::vector<double> &data, const QString &title, bool asOverlay) {
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

    if (!asOverlay) {
        if (!trackHist->LoadFromData(data, title.toStdString())) {
            std::cerr << "Failed to load spectrum into active histogram: " << title.toStdString() << std::endl;
            return;
        }

        canvas->getCanvas()->SetBorderMode(0);
        canvas->getCanvas()->SetFillColor(0);

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

        m_currentSpectrumFile = title;
        m_currentSpectrumCount = 1;
        m_currentSpectrumIndex = 0;
        m_currentSpectrumLength = static_cast<int>(data.size());

        CommandPrompt *prompt = CommandPrompt::getInstance();
        if (prompt) {
            prompt->appendPlainText(QString("Loaded matrix spectrum: %1 (%2 channels)\n")
                                        .arg(title)
                                        .arg(data.size()));
        }
    } else {
        TracknHistogram *overlayHist = dynamic_cast<TracknHistogram*>(trackHist->Clone());
        if (!overlayHist) return;

        overlayHist->LoadFromData(data, title.toStdString());
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

        CommandPrompt *prompt = CommandPrompt::getInstance();
        if (prompt) {
            prompt->appendPlainText(QString("Overlaid matrix gate: %1 (color index %2)\n")
                                        .arg(title)
                                        .arg(colorIdx));
        }
    }

    updateAxisStatusLabels();
}
