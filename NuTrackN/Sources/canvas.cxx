#include "canvas.h"
#include "Design.h"


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
// QMainCanvas Destructor
//==============================================================================
// Cleans up dynamically allocated resources including the background covariance
// matrix produced by fitBackground().
//==============================================================================
QMainCanvas::~QMainCanvas()
{
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
// Performs an automated single-peak Gaussian fit around the clicked channel.
//
// Model function:
//   y(x) = [0] * exp( -(x - [1])^2 / (2 * [2]) ) + [3] * x + [4]
// where:
//   [0] = Gaussian amplitude (peak height)
//   [1] = Centroid (channel)
//   [2] = Variance sigma^2
//   [3] = Background slope
//   [4] = Background intercept
//
// Adaptive range adjustment:
//   Initially fits [binX - 20, binX + 20]. If the fitted FWHM exceeds (41 / 6)
//   channels, the fit range is expanded to [centroid - 3*FWHM, centroid + 3*FWHM]
//   and re-evaluated for improved convergence on broad peaks.
//
// Outputs:
//   Monospace summary table printed to the terminal console and stdout with:
//   Peak#, Channel, Energy, Area, and Width (FWHM).
//   Automatically registers the peak centroid in puncte_calib2p for 2-point calibration.
//==============================================================================
void QMainCanvas::autoFit(int x, int y)
{
    int binX = getBinFromClick(x, y);
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return;

    int binC = hist->GetBinContent(binX);
    Double_t gaussianHeight = 0.0, gaussianCenter = 0.0, gaussianSigma = 0.0;
    Double_t bkgSlope = 0.0, bkg0 = 0.0, gaussianFWHM = 0.0;
    Double_t gaussianCenterError = 0.0, gaussianIntegral = 0.0;
    Double_t gaussianIntegralError = 0.0, gaussianFWHMError = 0.0;

    // Define model: Gaussian peak superimposed on a linear background
    delete gaussianWithBackground;
    delete gaussianWithBackgroundFunction;
    gaussianWithBackground = new TFormula("gaussianWithBackground", "[0]*exp(-(x-[1])^2/(2*[2]))+[3]*x+[4]");
    gaussianWithBackgroundFunction = new TF1("gaussianWithBackgroundFunction", "gaussianWithBackground", binX - 20, binX + 20);

    delete background;
    delete backgroundFunction;
    background = new TFormula("background", "[0]*x+[1]");
    backgroundFunction = new TF1("backgroundFunction", "background", binX - 20, binX + 20);

    gaussianCenterMarkerText = new TLatex();

    // Initial parameter estimates
    gaussianWithBackgroundFunction->SetParameter(0, binC);                                      // Height
    gaussianWithBackgroundFunction->SetParameter(1, binX);                                      // Centroid
    gaussianWithBackgroundFunction->SetParameter(2, 4.0);                                       // Initial sigma^2 estimate
    gaussianWithBackgroundFunction->SetParameter(3, 0.0);                                       // Initial slope
    gaussianWithBackgroundFunction->SetParameter(4, findMinValueInInterval(binX - 20, binX + 20)); // Baseline offset

    // Fit with ROOT Minuit: Q (Quiet), M (Improve fit), R (Use function range), S (Return fit result)
    TFitResultPtr fitResult = hist->Fit(gaussianWithBackgroundFunction, "QMRS", "same");

    // Extract fitted linear background parameters
    bkgSlope = gaussianWithBackgroundFunction->GetParameter(3);
    bkg0     = gaussianWithBackgroundFunction->GetParameter(4);
    backgroundFunction->FixParameter(0, bkgSlope);
    backgroundFunction->FixParameter(1, bkg0);

    // Extract FWHM (2.35482 * sigma) and calculate net integral above background
    gaussianSigma = gaussianWithBackgroundFunction->GetParameter(2);
    gaussianFWHM  = gaussianSigma * 2.35482;
    gaussianIntegral = gaussianWithBackgroundFunction->Integral(binX - 20, binX + 20)
                     - backgroundFunction->Integral(binX - 20, binX + 20);
    if (fitResult.Get()) {
        gaussianIntegralError = gaussianWithBackgroundFunction->IntegralError(
            binX - 20, binX + 20, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
    }

    // Adaptive refit: if peak is wide relative to 41-bin window, widen range to +/- 3*FWHM
    if (gaussianFWHM > (41.0 / 6.0)) {
        const Double_t fitMin = binX - gaussianFWHM * 3.0;
        const Double_t fitMax = binX + gaussianFWHM * 3.0;
        gaussianWithBackgroundFunction->SetRange(fitMin, fitMax);
        backgroundFunction->SetRange(fitMin, fitMax);

        fitResult = hist->Fit(gaussianWithBackgroundFunction, "QMRS", "");

        gaussianSigma = gaussianWithBackgroundFunction->GetParameter(2);
        gaussianFWHM  = gaussianSigma * 2.35482;
        gaussianIntegral = gaussianWithBackgroundFunction->Integral(fitMin, fitMax)
                         - backgroundFunction->Integral(fitMin, fitMax);
        if (fitResult.Get()) {
            gaussianIntegralError = gaussianWithBackgroundFunction->IntegralError(
                fitMin, fitMax, fitResult->GetParams(), fitResult->GetCovarianceMatrix().GetMatrixArray());
        }
    }

    // Extract final optimized peak parameters and uncertainties
    gaussianHeight      = gaussianWithBackgroundFunction->GetParameter(0);
    gaussianCenter      = gaussianWithBackgroundFunction->GetParameter(1);
    gaussianCenterError = gaussianWithBackgroundFunction->GetParError(1);
    gaussianFWHMError   = gaussianWithBackgroundFunction->GetParError(2) * 2.35482;

    // Display peak centroid label on canvas
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f", gaussianCenter);
    gaussianCenterMarkerText->DrawLatex(gaussianCenter, gaussianHeight, buffer);
    gaussCenters[SelectedElement_i][SelectedElement_j].push_back(gaussianCenter);
    gaussCentersHeight[SelectedElement_i][SelectedElement_j].push_back(gaussianHeight);

    // Draw the fitted total function and the background function
    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    gaussianWithBackgroundFunction->Draw("same");

    backgroundFunction->SetLineColor(kBlue);
    backgroundFunction->Draw("same");

    autoFitMarkers[SelectedElement_i][SelectedElement_j].push_back(backgroundFunction);
    autoFitMarkers[SelectedElement_i][SelectedElement_j].push_back(gaussianWithBackgroundFunction);

    // Save peak centroid into calibration list
    puncte_calib2p.push_back(gaussianCenter);

    // Format and print report table to CommandPrompt terminal and stdout
    QString headerRow = QString("%1%2%3%4%5")
        .arg("Peak#",    -10, QChar(' '))
        .arg("Channel",  -10, QChar(' '))
        .arg("Energy",   -15, QChar(' '))
        .arg("Area",     -25, QChar(' '))
        .arg("Width",    -10, QChar(' '));
    CommandPrompt::getInstance()->appendPlainText(headerRow);

    std::cout << std::left
              << std::setw(10) << "Peak#"
              << std::setw(10) << "Channel"
              << std::setw(15) << "Energy"
              << std::setw(25) << "Area"
              << std::setw(10) << "Width" << std::endl;

    QString numberStr           = QString("%1").arg("1", -10, QChar(' '));
    QString gaussianCenterStr   = QString("%1").arg(gaussianCenter, -10, 'f', 2, QChar(' '));
    QString energyStr           = QString("%1(%2)").arg(gaussianCenter, 0, 'f', 2).arg(qCeil(gaussianCenterError * 100));
    QString gaussianIntegralStr = QString("%1(%2)").arg(gaussianIntegral, 0, 'f', 0).arg(qRound(gaussianIntegralError));
    QString gaussianFWHMStr     = QString("%1(%2)").arg(gaussianFWHM, 0, 'f', 2).arg(qCeil(gaussianFWHMError * 100));

    QString dataRow = QString("%1%2%3%4%5")
        .arg(numberStr)
        .arg(gaussianCenterStr)
        .arg(energyStr,           -15, QChar(' '))
        .arg(gaussianIntegralStr, -25, QChar(' '))
        .arg(gaussianFWHMStr,     -10, QChar(' '));

    CommandPrompt::getInstance()->appendPlainText(dataRow + "\n");

    std::cout << std::setw(10) << "1"
              << std::setw(10) << std::fixed << std::setprecision(2) << gaussianCenter
              << std::setw(15) << energyStr.toStdString()
              << std::setw(25) << gaussianIntegralStr.toStdString()
              << std::setw(10) << gaussianFWHMStr.toStdString() << std::endl;

    // Prevent ROOT from duplicating function in histogram list
    TList *funcList = hist->GetListOfFunctions();
    if (funcList) {
        TObject *fitFunc = funcList->FindObject(gaussianWithBackgroundFunction->GetName());
        if (fitFunc) {
            funcList->Remove(fitFunc);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::findMinValueInInterval
//==============================================================================
// Finds and returns the minimum bin content within the specified channel range.
// Includes bounds validation against the histogram bin limits.
//==============================================================================
Double_t QMainCanvas::findMinValueInInterval(int intervalStart, int intervalFinish)
{
    if (intervalStart > intervalFinish) {
        std::swap(intervalStart, intervalFinish);
    }
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return 0.0;

    const int nBins = hist->GetNbinsX();
    intervalStart  = std::max(1, std::min(nBins, intervalStart));
    intervalFinish = std::max(1, std::min(nBins, intervalFinish));

    Double_t minValue = hist->GetBinContent(intervalStart);
    for (int i = intervalStart + 1; i <= intervalFinish; ++i) {
        const Double_t val = hist->GetBinContent(i);
        if (val < minValue) {
            minValue = val;
        }
    }
    return minValue;
}

//==============================================================================
// QMainCanvas::findMaxValueInInterval
//==============================================================================
// Finds and returns the maximum bin content within the specified channel range.
// Includes bounds validation against the histogram bin limits.
//==============================================================================
Double_t QMainCanvas::findMaxValueInInterval(int intervalStart, int intervalFinish)
{
    if (intervalStart > intervalFinish) {
        std::swap(intervalStart, intervalFinish);
    }
    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) return 0.0;

    const int nBins = hist->GetNbinsX();
    intervalStart  = std::max(1, std::min(nBins, intervalStart));
    intervalFinish = std::max(1, std::min(nBins, intervalFinish));

    Double_t maxValue = hist->GetBinContent(intervalStart);
    for (int i = intervalStart + 1; i <= intervalFinish; ++i) {
        const Double_t val = hist->GetBinContent(i);
        if (val > maxValue) {
            maxValue = val;
        }
    }
    return maxValue;
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
    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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
    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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
    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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
    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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

    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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

    TIter next(&listOfObjectsDrawnOnScreen);
    while (TObject *obj = next()) {
        delete obj;
    }
    listOfObjectsDrawnOnScreen.Clear();

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


//______________________________________________________________________________
void QMainCanvas::fitGauss()
{
    //ColorTheFrameOfTheHistogram();
    IdentifyLastClickedHistogram(mousePilgrimX,mousePilgrimY);
    bool goodRanges, goodGauss=0;
    std::string temp;
    std::ostringstream tempStringStream;
    double maxValue, fitIntegralError;

    checkBackgrounds();

    //Checks if the range is fine, that there are only two markers, orders them
    goodRanges=checkRanges();

    //Checks if the gauss markers are fine and inside the range
    if(goodRanges)
        goodGauss=checkGauss();

    //Clears the screen of all previous markers
    //clearTheScreen();

    //Adds the background, range and gauss markers back
    //showBackgroundMarkers();
    //showRangeMarkers();
    //showGaussMarkers();

    if(goodRanges&&goodGauss)
    {
        //Find the maximum value in the interval to use as a limit for fitting
        maxValue=findMaxValueInInterval(range_markers[0],range_markers[1]);

        //Fit the background from the background markers
        fitBackground();

        //Declaring a new formula which is a Gaussian and a simple background, and making it a Root function. Define a range on which it is applied
        //TFormula *background = new TFormula("background","[0]*x+[1]");
        //TFormula *gaussian = new TFormula("gaussian","[0]*exp(-(x-[1])^2/(2*[2]))");

        //Declaring the background function
        //TF1 *backgroundFunction = new TF1("backgroundFunction","background",range_markers[0],range_markers[1]);

        //Declaring the full function that will be used for fitting, initially with just the background
        TF1* fullFunction = new TF1("fullFunction","backgroundFunction", range_markers[0],range_markers[1]);

        for(uint i=0;i<gauss_markers.size();i++)
        {
            //Adding a new Gaussian function to the full function for every Gauss marker
            fullFunction = new TF1("fullFunction","fullFunction+gaussian", range_markers[0],range_markers[1]);
        }

        //Fixing the background parameters that have been obtained from the background fit. These should NOT vary!
        fullFunction->FixParameter(0, backgroundA1);
        fullFunction->FixParameter(1, backgroundA0);

        //For every gauss marker, setting the values of the 3 parameters (height, position, width) and their limits
        for(uint i=0;i<gauss_markers.size();i++)
        {
            fullFunction->SetParameter(2+i*3,HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(gauss_markers[i]-1));
            fullFunction->SetParLimits(2+i*3,0,maxValue*1.1);
            fullFunction->SetParameter(3+i*3,gauss_markers[i]-1);
            fullFunction->SetParLimits(3+i*3,range_markers[0],range_markers[1]);
            fullFunction->SetParameter(4+i*3,3.);
            fullFunction->SetParLimits(4+i*3,0.4,abs(range_markers[1]-range_markers[0])*4);
        }

        //std::cout<<fullFunction->GetFormula()->GetExpFormula()<<std::endl;

        //Fitting the histogram with the Gaussian function with background and putting the results in a special format
        //Fit options are Q - quiet; M - Minuit; R-respect range from function
        TFitResultPtr fitResult = HijF[SelectedElement_i][SelectedElement_j]->Fit(fullFunction,"Q M R S", "same");


        //If the fit fails (fitResult=4), then change some minimizer options and try again
        if((int) fitResult==4)
        {
            ROOT::Math::MinimizerOptions::SetDefaultStrategy(2);
            ROOT::Math::MinimizerOptions::SetDefaultTolerance(0.1);
            ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(10000000);

            fitResult = HijF[SelectedElement_i][SelectedElement_j]->Fit(fullFunction,"Q M R S", "same");
            //ROOT::Math::MinimizerOptions::SetDefaultStrategy(1);


            //If it still fails, set different tolerance and try again
            if((int) fitResult==4)
            {
                ROOT::Math::MinimizerOptions::SetDefaultTolerance(1);

                fitResult = HijF[SelectedElement_i][SelectedElement_j]->Fit(fullFunction,"Q M R S", "same");

                //If it still fails, set different tolerance and try again
                if((int) fitResult==4)
                {
                    ROOT::Math::MinimizerOptions::SetDefaultTolerance(10);

                    fitResult = HijF[SelectedElement_i][SelectedElement_j]->Fit(fullFunction,"Q M R S", "same");

                    //If it still fails, tell the user it has failed
                    if((int) fitResult==4)
                    {
                        std::cout<<"The fit has failed to converge despite our best attempts. Some errors will not be calculated."<<std::endl;
                        CommandPrompt::getInstance()->appendPlainText("The fit has failed to converge despite our best attempts. Some errors will not be calculated.\n");
                    }
                }
            }

            //Reset the minimizer options
            ROOT::Math::MinimizerOptions::SetDefaultStrategy(1);
            ROOT::Math::MinimizerOptions::SetDefaultTolerance(0.01);
            ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(1630);
        }


        //Writing the obtained data on screen, in a fixed format, so everything aligns nicely
        //First (fixed) row
        std::cout<<std::left;
        std::cout<<std::setw(10);
        std::cout<<"Peak#";
        std::cout<<std::setw(10);
        std::cout<<"Channel";
        std::cout<<std::setw(15);
        std::cout<<"Energy";
        std::cout<<std::setw(25);
        std::cout<<"Area";
        std::cout<<std::setw(10);
        std::cout<<"Width"<<std::endl;

        QString peakLabel = QString("%1").arg("Peak#",-10,QChar(' '));
        QString channelLabel = QString("%1").arg("Channel",-10, QChar(' '));
        QString energyLabel = QString("%1").arg("Energy",-15, QChar(' '));
        QString areaLabel = QString("%1").arg("Area",-25, QChar(' '));
        QString widthLabel = QString("%1").arg("Width",-10, QChar(' '));

        QString headerRow = QString("%1%2%3%4%5")
            .arg(peakLabel)
            .arg(channelLabel)
            .arg(energyLabel)
            .arg(areaLabel)
            .arg(widthLabel);
        CommandPrompt::getInstance()->appendPlainText(headerRow);

        for(uint i=0;i<gauss_markers.size();i++)
        {
            //Second row that contains variable numbers
            std::cout<<std::setw(10);
            std::cout<<i+1;
            std::cout<<std::setw(10);
            std::cout << std::fixed;
            std::cout<<std::setprecision(2)<<fullFunction->GetParameter(3+i*3);
            std::cout<<std::setw(15);
            tempStringStream.str(std::string());
            tempStringStream<< std::fixed<<std::setprecision(2)<<fullFunction->GetParameter(3+i*3)<<"("<<std::setprecision(0)<<ceil(fullFunction->GetParError(3+i*3)*100)<<")";
            temp=tempStringStream.str();
            std::cout<<temp;
            tempStringStream.str(std::string());

            //Creating a fake Gauss function to obtain the integral and integral error
            TF1* tempGaussFunction = new TF1("tempGaussFunction","gaussian", range_markers[0],range_markers[1]);
            tempGaussFunction->SetParameter(0,fullFunction->GetParameter(2+i*3));
            tempGaussFunction->SetParError(0,fullFunction->GetParError(2+i*3));
            tempGaussFunction->SetParameter(1,fullFunction->GetParameter(3+i*3));
            tempGaussFunction->SetParError(1,fullFunction->GetParError(3+i*3));
            tempGaussFunction->SetParameter(2,fullFunction->GetParameter(4+i*3));
            tempGaussFunction->SetParError(2,fullFunction->GetParError(4+i*3));

            //Getting the full covariance matrix and then cutting it for just our Gauss fit parameters
            TMatrixDSym covMatrix=fitResult->GetCovarianceMatrix();
            TMatrixDSym tempMatrix=fitResult->GetCovarianceMatrix().GetSub(2+i*3,4+i*3,2+i*3,4+i*3);

            //Obtaining the integral error. This will fail if the fit did not converge!
            fitIntegralError=tempGaussFunction->IntegralError(range_markers[0],range_markers[1],tempGaussFunction->GetParameters(),tempMatrix.GetMatrixArray());

            //The peak error is the quadratic sum of the Gauss integral error and the background integral error
            tempStringStream<< std::fixed<<std::setprecision(0)<<tempGaussFunction->Integral(range_markers[0],range_markers[1])<<"("<<round(sqrt(pow(fitIntegralError,2)+pow(backgroundIntegralError,2)))<<")";
            temp=tempStringStream.str();
            std::cout<<std::setw(25);
            std::cout<<temp;
            tempStringStream.str(std::string());
            tempStringStream<< std::fixed<<std::setprecision(2)<<fullFunction->GetParameter(4+i*3)*2.3548<<"("<<std::setprecision(0)<<ceil(fullFunction->GetParError(4+i*3)*2.3548*100)<<")";
            temp=tempStringStream.str();
            std::cout<<std::setw(10);
            std::cout<<temp<<std::endl;

            QString numberStr = QString("%1").arg(i+1);
            QString gaussianCenterStr = QString("%1").arg(fullFunction->GetParameter(3+i*3),0, ' ', 2);
            QString energyStr = QString("%1(%2)").arg(fullFunction->GetParameter(3+i*3), 0, ' ', 2).arg(ceil(fullFunction->GetParError(3+i*3)*100));
            QString gaussianIntegralStr = QString("%1(%2)").arg(tempGaussFunction->Integral(range_markers[0],range_markers[1]), 0, ' ', 0).arg(round(sqrt(pow(fitIntegralError,2)+pow(backgroundIntegralError,2))));
            QString gaussianFWHMStr = QString("%1(%2)").arg(fullFunction->GetParameter(4+i*3)*2.3548, 0, ' ', 2).arg(ceil(fullFunction->GetParError(4+i*3)*2.3548*100));


            QString dataRow = QString("%1%2%3%4%5")
                .arg(numberStr,-10,QChar(' '))
                .arg(gaussianCenterStr, -10, QChar(' '))
                .arg(energyStr, -15, QChar(' '))
                .arg(gaussianIntegralStr, -25, QChar(' '))
                .arg(gaussianFWHMStr, -10, QChar(' '));

            // Insert data row into QPlainTextEdit
            CommandPrompt::getInstance()->appendPlainText(dataRow);
        }
        CommandPrompt::getInstance()->appendPlainText("");
    }

    //Tell the canvas that stuff got modified
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::checkBackgrounds()
{
    //Check that background markers exists
    if(background_markers.size())
    {
        //If there is an odd number of background markers, delete the last one added
        if(background_markers.size()%2)
        {
            std::cout<<"There is an odd number of background markers, "<<background_markers.size()<<", so the last one, at "<<background_markers[background_markers.size()-1]<<", was removed"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("There is an odd number of background markers, "+ QString::number(background_markers.size())+", so the last one, at " + QString::number(background_markers[background_markers.size()-1]) + ", was removed \n");
            background_markers.pop_back();
        }

        //If the background markers overlap (meaning they define areas that overlap), then sort them so they don't overlap anymore
        if(overlapping_markers(background_markers))
        {
            std::cout<<"The background markers shown below produced regions which overlapped"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("The background markers shown below produced regions wwhich overlapped\n");
            for(uint i=0;i<background_markers.size()/2;i++)
            {
                std::cout<<background_markers[2*i]<<"-"<<background_markers[2*i+1]<<std::endl;
                CommandPrompt::getInstance()->appendPlainText(QString::number(background_markers[2*i])+"-"+ QString::number(background_markers[2*i+1])+"\n");
            }

            std::cout<<"Thus, we have reordered them in order to produce non-overlapping regions, as seen below:"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("Thus, we have reordered them in order to produce non-overlapping regions, as seen below:\n");
            sort(background_markers.begin(),background_markers.end());

            for(uint i=0;i<background_markers.size()/2;i++)
            {
                std::cout<<background_markers[2*i]<<"-"<<background_markers[2*i+1]<<std::endl;
                CommandPrompt::getInstance()->appendPlainText(QString::number(background_markers[2*i])+"-"+ QString::number(background_markers[2*i+1])+"\n");
            }
        }
    }
}

//______________________________________________________________________________
bool QMainCanvas::checkRanges()
{
    //Check that there are no fewer than 2 range markers. If there are, tell the user the ranges are not good and exit the fit
    if(range_markers.size()<2)
    {
        std::cout<<"There are fewer than 2 range markers added, namely "<<range_markers.size()<<", and the fitting procedure cannot run"<<std::endl;
        CommandPrompt::getInstance()->appendPlainText("There are fewer than 2 range markers added, namely "+QString::number(range_markers.size()) +", and the fitting procedure cannot run\n");
        return 0;
    }
    //Check that there are no more than 2 range markers. If there are, delete all but the first 2
    else if(range_markers.size()>2)
    {
        std::cout<<"There are more than 2 range markers added, namely "<<range_markers.size()<<". Only the first 2 markers will be used, namely "<<range_markers[0]<<"-"<<range_markers[1]<<std::endl;
        CommandPrompt::getInstance()->appendPlainText("There are more than 2 range markers added, namely "+QString::number(range_markers.size()) +". Only the first 2 markers will be used, namely "+ QString::number(range_markers[0])+"-"+QString ::number(range_markers[1])+"\n");

        for(uint i=2;i<=range_markers.size();i++)
            range_markers.pop_back();
        return 1;
    }

    //Sort the range markers just to be sure
    sort(range_markers.begin(),range_markers.end());

    return 1;
}

//______________________________________________________________________________
bool QMainCanvas::checkGauss()
{
    //Check that all the gauss markers are inside the range area, otherwise delete them
    for(uint i=0;i<gauss_markers.size();i++)
        if(gauss_markers[i]<range_markers[0]||gauss_markers[i]>range_markers[1])
        {
            std::cout<<"The peak center marker at "<<gauss_markers[i]<<" is not within the designated fit region "<<range_markers[0]<<"-"<<range_markers[1]<<" and has been removed"<<std::endl;

            CommandPrompt::getInstance()->appendPlainText("The peak center marker at "+QString::number(gauss_markers[i])+" is not within the designated fit region " + QString::number(range_markers[0])+"-"+QString ::number(range_markers[1])+"and has been removed\n");

            gauss_markers.erase(gauss_markers.begin()+i);
            i--;
        }

    //If there are no valid gauss markers left, tell the user and exit the fit
    if(gauss_markers.size()==0)
    {
        std::cout<<"There are no valid markers for any peak centers to fit! The program will not run!"<<std::endl;
        CommandPrompt::getInstance()->appendPlainText("There are no valid markers for any peak centers to fit! The program will not run!\n");
        return 0;
    }

    return 1;
}

//______________________________________________________________________________
void QMainCanvas::fitBackground()
{
    Double_t minimum=maxValueInHistogram, localMinimum;

    //Create another, temporary histogram
    TracknHistogram *tempHist = new TracknHistogram("tempHist","", 10240, 0, 10240);

    //Add only the background ranges to the temp histogram
    for (std::size_t i = 0; i < background_markers.size() / 2; ++i)
    {
        for (Int_t j = background_markers[2 * i]; j <= background_markers[2 * i + 1]; ++j)
            tempHist->AddBinContent(j, HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(j));

        localMinimum=findMinValueInInterval(background_markers[2*i],background_markers[2*i+1]);

        if(localMinimum<minimum)
            minimum=localMinimum;
    }

    //Declaring a new formula which is a simple background, and making it a Root function. Define a range on which it is applied
    //TFormula *background = new TFormula("background","[0]*x+[1]");
    TF1 *backgroundFunction = new TF1("backgroundFunction","background",0, 10240);

    //Setting the two parameters before the fit
    backgroundFunction->SetParameter(0,0.);
    backgroundFunction->SetParameter(1,minimum);

    //Fitting the background
    TFitResultPtr fitResult = tempHist->Fit(backgroundFunction,"QMSW", "same");

    //Obtaining the fit parameters, the background integral over the fit range, the integral error, and the covariance matrix
    backgroundA0=backgroundFunction->GetParameter(1);
    backgroundA1=backgroundFunction->GetParameter(0);
    backgroundIntegral=backgroundFunction->Integral(range_markers[0],range_markers[1]);
    backgroundIntegralError=backgroundFunction->IntegralError(range_markers[0],range_markers[1],fitResult->GetParams(),fitResult->GetCovarianceMatrix().GetMatrixArray());

    delete backgroundCovarianceMatrix;
    backgroundCovarianceMatrix = new TMatrixD(fitResult->GetCovarianceMatrix());

    tempHist->Delete();

    //Draw a line to show the background
    TLine *backgroundLine = new TLine(background_markers[0]-0.5, backgroundA1*(background_markers[0]-0.5)+backgroundA0, background_markers[background_markers.size()-1]-0.5, backgroundA1*(background_markers[background_markers.size()-1]-0.5)+backgroundA0);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);

    backgroundLine->Draw("same");
    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(backgroundLine);

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
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
