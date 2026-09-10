#include "canvas.h"
#include "Design.h"
#include "PeakFit.h"


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
      fKeyWasPressed(false)
{
    // Configure widget attributes for embedded ROOT TCanvas
    setAttribute(Qt::WA_PaintOnScreen, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NativeWindow, true);
    setUpdatesEnabled(kFALSE);
    setMouseTracking(kTRUE);
    setMinimumSize(300, 200);

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

//==============================================================================
// QRootCanvas::mouseMoveEvent
//==============================================================================
// Emits real-time mouse coordinates for live channel/count readouts and forwards
// mouse motion events to the underlying ROOT TCanvas.
//==============================================================================
void QRootCanvas::mouseMoveEvent(QMouseEvent *e)
{
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
            xMousePosition = e->x();
            yMousePosition = e->y();
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
    if (fCanvas) {
        const QPoint mousePos = e->position().toPoint();
        if (e->angleDelta().y() > 0) { // Wheel scrolled up: pan downward
            fCanvas->HandleInput(kWheelUp, mousePos.x(), mousePos.y());
            emit requesttranslatedownTheScreen();
        } else if (e->angleDelta().y() < 0) { // Wheel scrolled down: pan upward
            fCanvas->HandleInput(kWheelDown, mousePos.x(), mousePos.y());
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
            case Qt::LeftButton:
                if (controlKeyIsPressed) {
                    emit autoFitRequested(e->x(), e->y());
                }
                break;
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
                std::cout << "Waited for execute command after CTRL was pressed but no valid command arrived after it" << std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for execute command after CTRL was pressed but no valid command arrived after it\n");
                break;
        }
        controlKeyIsPressed = false;
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
            case Qt::Key_Control:
                controlKeyIsPressed = true;
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
            break;
        default:
            QWidget::keyReleaseEvent(event);
            break;
    }
}

//==============================================================================
// QRootCanvas::resizeEvent
//==============================================================================
// Resizes the embedded CERN ROOT TCanvas whenever the parent Qt widget is resized.
//==============================================================================
void QRootCanvas::resizeEvent(QResizeEvent *event)
{
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
// 1. Instantiates and embeds QRootCanvas inside a vertical layout.
// 2. Builds a coordinate readout bar (X channel, Y counts).
// 3. Adds toolbar icon buttons (Open file, Color settings, 2-Point calibration).
// 4. Adds primary analysis push buttons (Select File, Integral without/with background).
// 5. Connects all QRootCanvas user-interaction signals to QMainCanvas slots.
// 6. Sets up the ROOT graphics event processing timer (fRootTimer).
// 7. Allocates the default 10240-channel TracknHistogram instance.
//==============================================================================
QMainCanvas::QMainCanvas(QWidget *parent)
    : QWidget(parent),
      backgroundCovarianceMatrix(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *coordBarLayout = new QHBoxLayout();

    // 1. Embed the interactive ROOT canvas
    canvas = new QRootCanvas(this);
    mainLayout->addWidget(canvas);

    // 2. Coordinate status bar for channel and count displays
    coordBarLayout->addStretch();

    QLabel *labelXTitle = new QLabel("X:", this);
    coordBarLayout->addWidget(labelXTitle);

    labelX = new QLabel("", this);
    labelX->setAlignment(Qt::AlignCenter);
    labelX->setFixedSize(110, 20);
    labelX->setStyleSheet("border: 1px solid #555555; background-color: #2b2b2b; color: #ffffff;");
    coordBarLayout->addWidget(labelX);

    QLabel *labelYTitle = new QLabel("Y:", this);
    coordBarLayout->addWidget(labelYTitle);

    labelY = new QLabel("", this);
    labelY->setAlignment(Qt::AlignCenter);
    labelY->setFixedSize(110, 20);
    labelY->setStyleSheet("border: 1px solid #555555; background-color: #2b2b2b; color: #ffffff;");
    coordBarLayout->addWidget(labelY);

    coordBarLayout->addStretch();

    // 3. Toolbar icon buttons
    QPushButton *readiconButton = new QPushButton(this);
    readiconButton->setIcon(QIcon("readicon.png"));
    readiconButton->setToolTip(tr("Open Spectrum File"));
    readiconButton->setFixedSize(24, 24);
    coordBarLayout->addWidget(readiconButton);
    connect(readiconButton, &QPushButton::clicked, this, &QMainCanvas::clicked1);

    QPushButton *iconButton = new QPushButton(this);
    iconButton->setIcon(QIcon("icon.png"));
    iconButton->setToolTip(tr("Color & Theme Settings"));
    iconButton->setFixedSize(24, 24);
    coordBarLayout->addWidget(iconButton);
    connect(iconButton, &QPushButton::clicked, this, &QMainCanvas::OpenColorSelectionDialog);

    QPushButton *c2piconButton = new QPushButton(this);
    c2piconButton->setIcon(QIcon("c2picon.png"));
    c2piconButton->setToolTip(tr("Two-Point Energy Calibration"));
    c2piconButton->setFixedSize(24, 24);
    coordBarLayout->addWidget(c2piconButton);
    connect(c2piconButton, &QPushButton::clicked, this, &QMainCanvas::Cal2pMain);

    mainLayout->addLayout(coordBarLayout);

    // 4. Primary analysis push buttons
    QPushButton *btnSelectFile = new QPushButton(tr("&Select your file"), this);
    mainLayout->addWidget(btnSelectFile);
    connect(btnSelectFile, &QPushButton::clicked, this, &QMainCanvas::clicked1);

    QPushButton *btnIntegralNoBkg = new QPushButton(tr("&Integral No Background"), this);
    mainLayout->addWidget(btnIntegralNoBkg);
    connect(btnIntegralNoBkg, &QPushButton::clicked, this, &QMainCanvas::areaFunction);

    QPushButton *btnIntegralWithBkg = new QPushButton(tr("Integral &With Background"), this);
    mainLayout->addWidget(btnIntegralWithBkg);
    connect(btnIntegralWithBkg, &QPushButton::clicked, this, &QMainCanvas::areaFunctionWithBackground);

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
//==============================================================================
void QMainCanvas::clicked1()
{
    // Reset active histogram content and configure canvas background
    HijF[SelectedElement_i][SelectedElement_j]->Reset();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);

    // Display file chooser dialog supporting multiple spectrum file extensions
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open Spectrum File"), QString(),
        tr("Spectra (*.spe *.dat *.txt *.asc *.*);;All Files (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    // Delegate binary and ASCII data parsing to TracknHistogram
    TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (trackHist) {
        if (!trackHist->LoadFromFile(fileName.toStdString())) {
            std::cerr << "Failed to load spectrum from: " << fileName.toStdString() << std::endl;
            return;
        }
    } else {
        std::cerr << "Active histogram is null or invalid." << std::endl;
        return;
    }

    // Update maximum counts and unzoom axes
    maxValueInHistogram = HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(
        HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());
    HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->UnZoom();
    HijF[SelectedElement_i][SelectedElement_j]->GetYaxis()->UnZoom();
    HijC[SelectedElement_i][SelectedElement_j].push_back(
        (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());

    // Set color based on overlay index and draw to canvas pad
    HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(
        colors_hist[HijC[SelectedElement_i][SelectedElement_j].size() - 1]);
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    HijF[SelectedElement_i][SelectedElement_j]->Draw();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    // Redraw all previously loaded spectra on the same pad with "SAME" option
    for (std::size_t k = 0; k < HijC[SelectedElement_i][SelectedElement_j].size() - 1; ++k) {
        HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(colors_hist[k]);
        canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
        HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
    }

    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    // If zoom markers are present, preserve the zoomed region
    int i = zoom_markers.size();
    if (i >= 2) {
        if (zoom_markers[i - 2] < zoom_markers[i - 1]) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(
                zoom_markers[i - 2], zoom_markers[i - 1]);
        } else if (zoom_markers[i - 1] < zoom_markers[i - 2]) {
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(
                zoom_markers[i - 1], zoom_markers[i - 2]);
        }
    }

    ColorTheFrameOfTheHistogram();

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::OpenColorSelectionDialog
//==============================================================================
// Delegates color theme configuration to Design module.
//==============================================================================
void QMainCanvas::OpenColorSelectionDialog() {
    openColorSelectionDialog(this, this);
}

//==============================================================================
// QMainCanvas::Cal2pMain
//==============================================================================
// Delegates two-point energy calibration dialog to calib module.
//==============================================================================
void QMainCanvas::Cal2pMain() {
    runTwoPointCalibrationDialog(
        this, puncte_calib2p,
        dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]));
}


//==============================================================================
// QMainCanvas::addSpaceBarMarker
//==============================================================================
// Places a cyan vertical marker line on the spectrum at the channel bin clicked
// by the user. Pairs of spacebar markers define the boundaries of the region of
// interest to be zoomed when the user subsequently presses 'E'.
//==============================================================================
void QMainCanvas::addSpaceBarMarker(Int_t x, Int_t y)
{
    int binX = getBinFromClick(x, y);

    // Record the channel in both spacebar and zoom marker collections
    spacebar_markers.push_back(static_cast<Double_t>(binX));
    zoom_markers.push_back(binX);

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
// shortcut or UI button). Delegates calculation to integral_function() in
// Integral.h with an empty background marker set.
//==============================================================================
void QMainCanvas::areaFunction()
{
    // A stand-in empty vector is used so integral_function performs a gross integral
    // with no background subtraction regardless of existing background markers
    std::vector<Int_t> placeholder_background_markers;
    integral_function(HijF[SelectedElement_i][SelectedElement_j],
                      integral_markers,
                      placeholder_background_markers,
                      slope,
                      addition);
}

//==============================================================================
// QMainCanvas::areaFunctionWithBackground
//==============================================================================
// Computes net peak area with linear background subtraction (triggered by
// 'C + J' shortcut or UI button). Computes background slope and intercept
// from background markers and overlays the subtracted background line.
//==============================================================================
void QMainCanvas::areaFunctionWithBackground()
{
    if (background_markers.empty()) {
        const QString msg = "There are no background markers, so an integral with background cannot be performed\n";
        CommandPrompt::getInstance()->appendPlainText(msg);
        std::cout << msg.toStdString();
        return;
    }

    integral_function(HijF[SelectedElement_i][SelectedElement_j],
                      integral_markers,
                      background_markers,
                      slope,
                      addition);

    // Draw a blue line showing the fitted background across the marked interval
    const Double_t xStart = background_markers[0] - 0.5;
    const Double_t xEnd   = background_markers.back() - 0.5;
    TLine *backgroundLine = new TLine(xStart, slope * xStart + addition,
                                      xEnd,   slope * xEnd   + addition);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);
    backgroundLine->Draw("same");

    listOfObjectsDrawnOnScreen.Add(backgroundLine);

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
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    if (HijF[SelectedElement_i][SelectedElement_j]) {
        HijF[SelectedElement_i][SelectedElement_j]->Draw();

        HijC[SelectedElement_i][SelectedElement_j].clear();
        HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(kBlue);
        HijC[SelectedElement_i][SelectedElement_j].push_back(
            (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());
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
            HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->SetRangeUser(low, high);
        }
    } else {
        const QString msg = "At least two spacebar markers are required to define a zoom window.\n";
        std::cout << msg.toStdString();
        CommandPrompt::getInstance()->appendPlainText(msg);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::zoomOut
//==============================================================================
// Resets spectrum axes to their full unzoomed range.
// Triggered by 'F + F' or 'F + S' keyboard shortcuts.
//==============================================================================
void QMainCanvas::zoomOut()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        // Reset both X and Y axis ranges to unzoomed full spectrum scale
        hist->GetXaxis()->UnZoom();
        hist->GetYaxis()->UnZoom();
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
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
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
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
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::translatedownTheScreen
//==============================================================================
// Expands the vertical count scale (zooms out vertically by 1.05x).
// Triggered by the Down Arrow key.
//==============================================================================
void QMainCanvas::translatedownTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        hist->GetYaxis()->SetRangeUser(0, hist->GetMaximum() * 1.05);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::translateupTheScreen
//==============================================================================
// Compresses the vertical count scale (zooms in vertically by 1.05x).
// Triggered by the Up Arrow key.
//==============================================================================
void QMainCanvas::translateupTheScreen()
{
    IdentifyLastClickedHistogram(mousePilgrimX, mousePilgrimY);

    clearDrawnObjects();

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (hist) {
        hist->GetYaxis()->SetRangeUser(0, hist->GetMaximum() / 1.05);
    }

    ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
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

//______________________________________________________________________________
void QMainCanvas::handle_root_events()
{
   //call the inner loop of ROOT
   gSystem->ProcessEvents();
}

//______________________________________________________________________________
void QMainCanvas::changeEvent(QEvent * e)
{
    //Handles regular stuff like minimizing and maximizing the window
   if (e->type() == QEvent ::WindowStateChange) {
      QWindowStateChangeEvent * event = static_cast< QWindowStateChangeEvent * >( e );
      if (( event->oldState() & Qt::WindowMaximized ) ||
          ( event->oldState() & Qt::WindowMinimized ) ||
          ( event->oldState() == Qt::WindowNoState && 
            this->windowState() == Qt::WindowMaximized )) {
         if (canvas->getCanvas()) {
            canvas->getCanvas()->Resize();
            canvas->getCanvas()->Update();
         }
      }
   }
}

void QMainCanvas::AddCulomn(){
    if(maxElement_i==1 && maxElement_j==1){
        selectedHisto=static_cast<TracknHistogram*>(HijF[1][1]->Clone(("h"+QString::number(1)+QString::number(1)+"f").toStdString().c_str()));
    }
    maxElement_j++;
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);

       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(g==maxElement_j){
                    //HijF[z][g] = selectedHisto;
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
 while(HijF[z][g]->GetListOfFunctions()->GetSize()>0)
    {
        HijF[z][g]->GetListOfFunctions()->RemoveLast();
    }
                    HijF[z][g]->SetLineColor(4);
                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    HijC[z][g].push_back((TH1F*)selectedHisto->Clone());

            }

               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
               canvas->getCanvas()->cd((z-1)*maxElement_j+g);
               HijF[z][g]->GetYaxis()->SetRangeUser(0, HijF[z][g]->GetMaximum() * 1);
               HijF[z][g]->Draw();
               for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
    Double_t center = gaussCenters[z][g][k];
    Double_t height = gaussCentersHeight[z][g][k];
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", center);
    gaussianCenterMarkerText->DrawLatex(center, height, buffer);
}
               for(std::size_t k=0;k<HijC[z][g].size();k++){
            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*maxElement_j+g);
        obj->Draw("SAME"); // sau 'Draw("some options")', în funcție de tipul obiectului
          //clearTheScreen();

canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
    }
}


            }

    }}

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::AddLine(){
        if(maxElement_i==1 && maxElement_j==1){
        selectedHisto=static_cast<TracknHistogram*>(HijF[1][1]->Clone(("h"+QString::number(1)+QString::number(1)+"f").toStdString().c_str()));
    }
    maxElement_i++;
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);

       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(z==maxElement_i){
                   HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
 while(HijF[z][g]->GetListOfFunctions()->GetSize()>0)
    {
        HijF[z][g]->GetListOfFunctions()->RemoveLast();
    }


                    HijF[z][g]->SetLineColor(4);

                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    HijC[z][g].push_back((TH1F*)selectedHisto->Clone());
            }
               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
               HijF[z][g]->GetYaxis()->SetRangeUser(0, HijF[z][g]->GetMaximum() * 1);

               HijF[z][g]->Draw();
for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
    Double_t center = gaussCenters[z][g][k];
    Double_t height = gaussCentersHeight[z][g][k];
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", center);
    gaussianCenterMarkerText->DrawLatex(center, height, buffer);
}
for(std::size_t k=0;k<HijC[z][g].size();k++){

            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*maxElement_j+g);
        obj->Draw("SAME");

canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
    }
}



            }

    }

        }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::IdentifyLastClickedHistogram(Double_t x, Double_t y){
    mouseLeftClickXcoord=x;mouseLeftClickYcoord=y;
   // selectedHisto=nullptr;



    for(int j=0;j<maxElement_j;j++){
        if(j*canvas->getCanvas()->GetWw()/maxElement_j<mouseLeftClickXcoord && mouseLeftClickXcoord<(j+1)*canvas->getCanvas()->GetWw()/maxElement_j){
            SelectedElement_j=j+1;}}
    for(int h=0;h<maxElement_i;h++){
        if(h*canvas->getCanvas()->GetWh()/maxElement_i<mouseLeftClickYcoord && mouseLeftClickYcoord<(h+1)*canvas->getCanvas()->GetWh()/maxElement_i){
            SelectedElement_i=h+1;}}
    selectedHisto=HijF[SelectedElement_i][SelectedElement_j];
    std::cout<<"i="<<SelectedElement_i<<"  j="<<SelectedElement_j<<"\n";

ColorTheFrameOfTheHistogram();

}

void QMainCanvas::IdentifyLastPilgrimHistogram(Double_t x, Double_t y){
    mousePilgrimX=x;mousePilgrimY=y;
    for(int j=0;j<maxElement_j;j++){
        if(j*canvas->getCanvas()->GetWw()/maxElement_j<mousePilgrimX && mousePilgrimX<(j+1)*canvas->getCanvas()->GetWw()/maxElement_j){
        PilgrimElement_j=j+1;}}
    for(int h=0;h<maxElement_i;h++){
        if(h*canvas->getCanvas()->GetWh()/maxElement_i<mousePilgrimY && mousePilgrimY<(h+1)*canvas->getCanvas()->GetWh()/maxElement_i){
        PilgrimElement_i=h+1;}}
    std::cout<<PilgrimElement_i<<PilgrimElement_j<<"\n";
    //canvas->getCanvas()->cd((PilgrimElement_i-1)*maxElement_j+PilgrimElement_j);
    //ColorTheFrameOfTheHistogram();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::ColorTheFrameOfTheHistogram()
{
    delete lineR;lineR=nullptr;
    delete lineL;lineL=nullptr;
    delete lineD;lineD=nullptr;
    delete lineU;lineU=nullptr;
      //canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j)->Draw();
      canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);
    lineR = new TLine(HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetFirst()-1, 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetFirst()-1, HijF[SelectedElement_i][SelectedElement_j]->GetMaximum());
    lineL= new TLine(HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetLast(), 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetLast(), HijF[SelectedElement_i][SelectedElement_j]->GetMaximum());
    lineU= new TLine(HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetFirst(),  HijF[SelectedElement_i][SelectedElement_j]->GetMaximum(), HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetLast(), HijF[SelectedElement_i][SelectedElement_j]->GetMaximum());
    lineD= new TLine(0, 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetXmax(), 0);
    if(maxElement_i>1 || maxElement_j>1){
    lineR->SetLineColor(kAzure+1);  // Setarea culorii liniei la roșu
    lineR->SetLineWidth(4);     // Setarea grosimii liniei
    lineR->Draw();
    lineL->SetLineColor(kAzure+1);  // Setarea culorii liniei la roșu
    lineL->SetLineWidth(4);     // Setarea grosimii liniei
    lineL->Draw();
    lineU->SetLineColor(kAzure+1);  // Setarea culorii liniei la roșu
    lineU->SetLineWidth(4);     // Setarea grosimii liniei
    lineU->Draw();
    lineD->SetLineColor(kAzure+1);  // Setarea culorii liniei la roșu
    lineD->SetLineWidth(4);     // Setarea grosimii liniei
    lineD->Draw();}
    HijF[SelectedElement_i][SelectedElement_j]->GetYaxis()->SetRangeUser(0, HijF[SelectedElement_i][SelectedElement_j]->GetMaximum()  );

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

  void QMainCanvas::showXYcoord(Double_t x, Double_t y){
std::string objectInfo, temp;
double_t from, to, binX, binC;
canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);
objectInfo = HijF[SelectedElement_i][SelectedElement_j]->GetObjectInfo(x, y);

// Extrage binX
from = objectInfo.find("binx=");
to = objectInfo.find(" binc=");
temp = objectInfo.substr(from + 5, to - from - 6);
binX = std::stoi(temp);

// Extrage binC
from = to;
to = objectInfo.find(" Sum=");
temp = objectInfo.substr(from + 6);
binC = std::stoi(temp);


std::cout<<objectInfo<<"\n";





      //labelY->setText(QString::number(PilgrimElement_i));
    //labelX = new QLabel("d", this);
    labelX->setText(QString::number(binX));
    labelY->setText(QString::number(binC, 'e', 2));

}


void QMainCanvas::DeleteCulomn(){
if(maxElement_j>1){

    maxElement_j--;
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
               canvas->getCanvas()->cd((z-1)*maxElement_j+g);
               HijF[z][g]->GetYaxis()->SetRangeUser(0, HijF[z][g]->GetMaximum() * 1);
               HijF[z][g]->Draw();
               for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
    Double_t center = gaussCenters[z][g][k];
    Double_t height = gaussCentersHeight[z][g][k];
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", center);
    gaussianCenterMarkerText->DrawLatex(center, height, buffer);
}
               for(std::size_t k=0;k<HijC[z][g].size();k++){
            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*maxElement_j+g);
        obj->Draw("SAME"); // sau 'Draw("some options")', în funcție de tipul obiectului
          //clearTheScreen();


    }
}


            }
canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
HijC[z][maxElement_j+1].clear();
gaussCenters[z][maxElement_j+1].clear();
gaussCentersHeight[z][maxElement_j+1].clear();
autoFitMarkers[z][maxElement_j+1].clear();
    }}
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}
}
void QMainCanvas::DeleteLine(){
    if(maxElement_i>1){

    maxElement_i--;
if(maxElement_i!=1 && maxElement_j!=1){
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);



       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){

canvas->getCanvas()->cd((z-1)*maxElement_j+g);


               HijF[z][g]->Draw();
for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
    Double_t center = gaussCenters[z][g][k];
    Double_t height = gaussCentersHeight[z][g][k];
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", center);
    gaussianCenterMarkerText->DrawLatex(center, height, buffer);
}
               for(std::size_t k=0;k<HijC[z][g].size();k++){

            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*maxElement_j+g);
        obj->Draw("SAME");

canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
    }
}


            }
canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
HijC[maxElement_i+1][g].clear();
gaussCenters[maxElement_i+1][g].clear();
gaussCentersHeight[maxElement_i+1][g].clear();
autoFitMarkers[maxElement_i+1][g].clear();
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    }


        }}
        if(maxElement_i==1 && maxElement_j!=1){
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    HijF[1][1]->Draw();
    ColorTheFrameOfTheHistogram();
           for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){

canvas->getCanvas()->cd((z-1)*maxElement_j+g);


               HijF[z][g]->Draw();
for (size_t k = 0; k < gaussCenters[z][g].size(); ++k) {
    Double_t center = gaussCenters[z][g][k];
    Double_t height = gaussCentersHeight[z][g][k];
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    char buffer[64];
    snprintf(buffer, sizeof buffer, "%f", center);
    gaussianCenterMarkerText->DrawLatex(center, height, buffer);
}
               for(std::size_t k=0;k<HijC[z][g].size();k++){

            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*maxElement_j+g);
        obj->Draw("SAME");

canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
    }
}


            }
canvas->getCanvas()->cd((z-1)*maxElement_j+g)->Draw();
HijC[maxElement_i+1][g].clear();
gaussCenters[maxElement_i+1][g].clear();
gaussCentersHeight[maxElement_i+1][g].clear();
autoFitMarkers[maxElement_i+1][g].clear();
canvas->getCanvas()->cd((z-1)*maxElement_j+g);
    }


        }
        }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
    }
}
void QMainCanvas::RefreshScreen(){
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);
    int n=0;


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){

               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
               n++;
               canvas->getCanvas()->cd(n);
               HijF[z][g]->GetYaxis()->SetRangeUser(0, HijF[z][g]->GetMaximum() * 1);
               HijF[z][g]->Draw();
               for(std::size_t k=0;k<HijC[z][g].size();k++){
            HijC[z][g][k]->SetLineColor(colors_hist[k]);
               HijC[z][g][k]->Draw("SAME");
               for (auto obj : autoFitMarkers[z][g]) {
    if (obj) {
        canvas->getCanvas()->cd((z-1)*g+g);
        obj->Draw("SAME"); // sau 'Draw("some options")', în funcție de tipul obiectului
          //clearTheScreen();

canvas->getCanvas()->cd((z-1)*g+g)->Draw();
    }
}



            }

    }
        }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}


void QMainCanvas::offerHelp()
{
    CommandPrompt::getInstance()->appendPlainText(" **********************  COMMAND-LIST  *********************\n\n");
    CommandPrompt::getInstance()->appendPlainText(" spacebar               Place a Marker on the position of the cursor\n");
    CommandPrompt::getInstance()->appendPlainText(" AJ AG                  Automatic CJ, CG  at marker position\n");
    CommandPrompt::getInstance()->appendPlainText(" B G I R S W            Insert a marker of type Backgorund, G, Integral, Range, S  or W\n");
    CommandPrompt::getInstance()->appendPlainText(" CB CI CJ MI MJ         Background, Integration(CI without background, CJ with), CB+CI\n");
    CommandPrompt::getInstance()->appendPlainText(" CG CV MG MV            Gaussfit, CB+CG. Show markers\n");
    CommandPrompt::getInstance()->appendPlainText(" CP MP                  Automatic peak search. Show peaks\n");
    CommandPrompt::getInstance()->appendPlainText(" Dn Cn Mn Zn n          Define, Execute, Show, Erase command string n=1...9\n");
    CommandPrompt::getInstance()->appendPlainText(" DD                     Change the display parameters\n");
    CommandPrompt::getInstance()->appendPlainText(" DE                     Define how to do efficiency correction\n");
    CommandPrompt::getInstance()->appendPlainText(" DG                     Define peak width (individual/common) for fit\n");
    CommandPrompt::getInstance()->appendPlainText(" DK AK                  Energy and Width calibration\n");
    CommandPrompt::getInstance()->appendPlainText(" DF DL                  Define output file for Area calculations\n");
    CommandPrompt::getInstance()->appendPlainText(" DT CT AT               Recalibration using Trackfit\n");
    CommandPrompt::getInstance()->appendPlainText(" DW CW                  Define, Estract cuts from compressed matrix\n");
    CommandPrompt::getInstance()->appendPlainText(" DQ                     Define matrix and background subtraction mode\n");
    CommandPrompt::getInstance()->appendPlainText(" E                      Expand/Zoom between last two Markers\n");
    CommandPrompt::getInstance()->appendPlainText(" X                      Expand around current cursor position\n");
    CommandPrompt::getInstance()->appendPlainText(" FF FX FY               Full display Full_x Full_y\n");
    CommandPrompt::getInstance()->appendPlainText(" SX SY                  same X or Y scale for all windows\n");
    CommandPrompt::getInstance()->appendPlainText(" FO FU                  Set Y-maximum or Y-minimum by marker\n");
    CommandPrompt::getInstance()->appendPlainText(" H ?                    Help (this list)\n");
    CommandPrompt::getInstance()->appendPlainText(" K                      Energy calibration from previous 2 energies\n");
    CommandPrompt::getInstance()->appendPlainText(" L                      Change the histogram Linear/Logaritmic\n");
    CommandPrompt::getInstance()->appendPlainText(" N                      Input new spectrum\n");
    CommandPrompt::getInstance()->appendPlainText(" DN MN ZN               Define display behaviour at input of new spectrum\n");
    CommandPrompt::getInstance()->appendPlainText(" OS                     Write out current spectrum\n");
    CommandPrompt::getInstance()->appendPlainText(" O=                     Postscript plot of current display\n");
    CommandPrompt::getInstance()->appendPlainText(" P                      Insert a peak by energy\n");
    CommandPrompt::getInstance()->appendPlainText(" Q                      Display projection of compressed matrix\n");
    CommandPrompt::getInstance()->appendPlainText(" V                      Marker writing also counts in channel\n");
    CommandPrompt::getInstance()->appendPlainText(" MZ                     Draw a line at zero counts\n");
    CommandPrompt::getInstance()->appendPlainText(" ZA                     Delete all B/G/I markers\n");
    CommandPrompt::getInstance()->appendPlainText(" ZB ZI ZJ ZG ZV         Delete corresponding type of markers\n");
    CommandPrompt::getInstance()->appendPlainText(" ZF ZL                  Close output file for Area calculations\n");
    CommandPrompt::getInstance()->appendPlainText(" DP MP ZP               Define, Show, Delete peaks in buffer\n");
    CommandPrompt::getInstance()->appendPlainText(" + -                    Insert/delete a peak by marker\n");
    CommandPrompt::getInstance()->appendPlainText(" =                      Repeat the display\n");
    CommandPrompt::getInstance()->appendPlainText(" < >                    Shift display 3/4 to Left, Rigth\n");
    CommandPrompt::getInstance()->appendPlainText(" CTL_RIGHTARROW         Increase # of windows adding one column more\n");
    CommandPrompt::getInstance()->appendPlainText(" CTL_LEFTARROW          Decrease # of windows deleting last column\n");
    CommandPrompt::getInstance()->appendPlainText(" CTL_UPARROW            Increase # of windows adding one row more\n");
    CommandPrompt::getInstance()->appendPlainText(" CTL_DOWNARROW          Decrease # of windows deleting last row\n");
    CommandPrompt::getInstance()->appendPlainText(" CTL_C CTL_Y CTL_Z      Close the program\n");
    CommandPrompt::getInstance()->appendPlainText(" _________________________________________________________\n\n");
}
