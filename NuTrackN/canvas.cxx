#include "canvas.h"
#include "Design.h"

//______________________________________________________________________________
QRootCanvas::QRootCanvas(QWidget *parent) : QWidget(parent, 0), fCanvas(0)
{
   // QRootCanvas constructor.

   // set options needed to properly update the canvas when resizing the widget
   // and to properly handle context menus and mouse move events
   setAttribute(Qt::WA_PaintOnScreen, false);
   setAttribute(Qt::WA_OpaquePaintEvent, true);
   setAttribute(Qt::WA_NativeWindow, true);
   setUpdatesEnabled(kFALSE);
   setMouseTracking(kTRUE);
   //Minimum size of the spectra
   setMinimumSize(300, 200);

   // register the QWidget in TVirtualX, giving its native window id
   int wid = gVirtualX->AddWindow((ULong_t)winId(), width(), height());
   // create the ROOT TCanvas, giving as argument the QWidget registered id
   fCanvas = new TCanvas("Root Canvas", width(), height(), wid);
   TQObject::Connect("TGPopupMenu", "PoppedDown()", "TCanvas", fCanvas, "Update()");

    Double_t proportion = 0.1/fCanvas->GetWh();
    gPad->SetMargin(proportion, proportion, proportion, proportion);

   setFocusPolicy(Qt::StrongFocus);
   TLatex l;
   l.SetTextSize(0.15);
   l.SetTextAlign(22);
   l.SetTextColor(kBlack);
   l.DrawLatex(0.5, 0.5, "NuTrackN");
}

//______________________________________________________________________________
void QRootCanvas::mouseMoveEvent(QMouseEvent *e)
{
   // Handle mouse move events.
emit mousePilgrimCoordRequest(e->x(), e->y());
    fCanvas->Modified();
    fCanvas->Update();
   //This tells the canvas to handle events when the mouse moves and any or none of the mouse buttons are pressed. These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
   if (fCanvas) {
      if (e->buttons() & Qt::LeftButton) {
         //fCanvas->HandleInput(kButton1Motion, e->x(), e->y());
      } else if (e->buttons() & Qt::MiddleButton) {
         fCanvas->HandleInput(kButton2Motion, e->x(), e->y());
      } else if (e->buttons() & Qt::RightButton) {
         fCanvas->HandleInput(kButton3Motion, e->x(), e->y());
      } else {
         fCanvas->HandleInput(kMouseMotion, e->x(), e->y());
         xMousePosition=e->x();
         yMousePosition=e->y();
      }
   }
}

void QRootCanvas::wheelEvent(QWheelEvent *e)
{
    // Handle mouse wheel events.

    // This tells the canvas to handle events when the mouse wheel is scrolled.
    // These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
    if (fCanvas) {
        if (e->delta() > 0) { // Wheel scrolled up
            fCanvas->HandleInput(kWheelUp, e->x(), e->y());
            emit requesttranslatedownTheScreen();
        } else if (e->delta() < 0) { // Wheel scrolled down
            fCanvas->HandleInput(kWheelDown, e->x(), e->y());
            emit requesttranslateupTheScreen();
        }

        // Save the x and y positions of the mouse when the wheel is scrolled.
        xMousePosition = e->x();
        yMousePosition = e->y();
    }
}
//______________________________________________________________________________
void QRootCanvas::mousePressEvent( QMouseEvent *e )
{
   // Handle mouse button press events.

    //This tells the canvas to handle events when any of the mouse buttons are pressed. These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
   if (fCanvas) {
      switch (e->button()) {
         case Qt::LeftButton :
            //fCanvas->HandleInput(kButton1Down, e->x(), e->y());
             emit mouseLeftClickCoordRequest(e->x(), e->y());
            break;
         case Qt::MiddleButton :
            fCanvas->HandleInput(kButton2Down, e->x(), e->y());
            break;
         case Qt::RightButton :
            // does not work properly on Linux...
            // ...adding setAttribute(Qt::WA_PaintOnScreen, true) 
            // seems to cure the problem
            //fCanvas->HandleInput(kButton3Down, e->x(), e->y());
showContextMenu(e);
            break;
         default:
            break;
      }
   }
}

//______________________________________________________________________________
void QRootCanvas::mouseReleaseEvent( QMouseEvent *e )
{
   // Handle mouse button release events.

    //This tells the canvas to handle events when any of the mouse buttons are released. These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
   if (fCanvas) {
      switch (e->button()) {
         case Qt::LeftButton :
            //If the left button is released AND the Ctrl key is pressed, call the autofit function (to be implemented)
            if(controlKeyIsPressed)
            {
                emit autoFitRequested(e->x(),e->y());
            }
            //fCanvas->HandleInput(kButton1Up, e->x(), e->y());
            break;
         case Qt::MiddleButton :
            fCanvas->HandleInput(kButton2Up, e->x(), e->y());
            break;
         case Qt::RightButton :
            // does not work properly on Linux...
            // ...adding setAttribute(Qt::WA_PaintOnScreen, true) 
            // seems to cure the problem
            //fCanvas->HandleInput(kButton3Up, e->x(), e->y());

            break;
         default:
            break;
      }
   }
}

//______________________________________________________________________________
void QRootCanvas::showContextMenu(QMouseEvent *e)
{
    QMenu contextMenu(tr("Context menu"), this);

    QAction *action1 = new QAction("Add Line", this);
    connect(action1, &QAction::triggered, this, &QRootCanvas::AddLineRequest);
    contextMenu.addAction(action1);

    QAction *action2 = new QAction("Add Column", this);
    connect(action2, &QAction::triggered, this, &QRootCanvas::AddCulomnRequest);
    contextMenu.addAction(action2);

        QAction *action3 = new QAction("Delete Line", this);
    connect(action3, &QAction::triggered, this, &QRootCanvas::DeleteLineRequest);
    contextMenu.addAction(action3);

    QAction *action4 = new QAction("Delete Column", this);
    connect(action4, &QAction::triggered, this, &QRootCanvas::DeleteCulomnRequest);
    contextMenu.addAction(action4);

        QAction *action5 = new QAction("Refresh Display", this);
    connect(action5, &QAction::triggered, this, &QRootCanvas::AddCulomnRequest);
    contextMenu.addAction(action5);

    // adding more actions as needed...
//draw_pixel_line(canvas, width()/2, width()/2, 0, height());
    contextMenu.exec(e->globalPos());
}


void QRootCanvas::keyPressEvent(QKeyEvent *event)
{
    //Checks if any of the top keys (CTRL,C, M, Z) was pressed just before
    if(controlKeyIsPressed)
    {
        switch(event->key())
        {
        case Qt::Key_C:
        case Qt::Key_Z:
        case Qt::Key_Y:
            //if either the C,Z or Y key was pressed after the CTRL key then the program will attempt to quit asking the user if they intended it or not
            QMessageBox::StandardButton quiting;
            quiting=QMessageBox::question(this,"Quit","Are you sure you want to quit?",QMessageBox::Yes|QMessageBox::No);
            if(quiting==QMessageBox::Yes)
            {
                emit killSwitch();
            }
            break;
        default:
            std::cout<<"Waited for execute command after C was pressed but no valid command arrived after it"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("Waited for execute command after C was pressed but no valid command arrived after it\n");
            break;
        }
        controlKeyIsPressed=0;
    }
    else if(cKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_I:
                //If I key is pressed after C, call integration without background
                emit requestIntegrationNoBackground();
                break;
            case Qt::Key_J:
                //If J key is pressed after C, call integration with background
                emit requestIntegrationWithBackground();
                break;
            case Qt::Key_V:
                //If V key is pressed after C, call fitting with Gaussian functions
                emit requestFitGauss();
                break;
            case Qt::Key_C:
                //if the C key is pressed after C, do nothing
                break;
            default:
                std::cout<<"Waited for execute command after C was pressed but no valid command arrived after it"<<std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for execute command after C was pressed but no valid command arrived after it\n");
                break;
        }
        cKeyWasPressed=0;
    }
    else if(zKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_I:
                //If I key is pressed after Z, call delete integral markers
                emit requestDeleteIntegralMarkers();
                break;
            case Qt::Key_B:
                //If B key is pressed after Z, call delete background markers
                emit requestDeleteBackgroundMarkers();
                break;
            case Qt::Key_R:
                //If B key is pressed after Z, call delete background markers
                emit requestDeleteRangeMarkers();
                break;
            case Qt::Key_G:
                //If B key is pressed after Z, call delete background markers
                emit requestDeleteGaussMarkers();
                break;
            case Qt::Key_A:
                //If A key is pressed after Z, call delete all markers
                emit requestDeleteAllMarkers();
                break;
            case Qt::Key_Z:
                //if the Z key is pressed after Z, do nothing
                break;
            default:
                std::cout<<"Waited for delete command after Z was pressed but no valid command arrived after it"<<std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for delete command after Z was pressed but no valid command arrived after it\n");
                break;
        }
        zKeyWasPressed=0;
    }
    else if(mKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_I:
                //If I key is pressed after M, call show integral markers
                emit requestShowIntegralMarkers();
                break;
            case Qt::Key_B:
                //If B key is pressed after M, call show background markers
                emit requestShowBackgroundMarkers();
                break;
            case Qt::Key_R:
                //If B key is pressed after Z, call delete background markers
                emit requestShowRangeMarkers();
                break;
            case Qt::Key_G:
                //If B key is pressed after Z, call delete background markers
                emit requestShowGaussMarkers();
                break;
            case Qt::Key_A:
                //If A key is pressed after M, call show all markers
                emit requestShowAllMarkers();
                break;
            case Qt::Key_M:
                //if the M key is pressed after M, do nothing
                break;
            default:
                std::cout<<"Waited for show command after M was pressed but no valid command arrived after it"<<std::endl;
                CommandPrompt::getInstance()->appendPlainText("Waited for show command after M was pressed but no valid command arrived after it\n");
                break;
        }
        mKeyWasPressed=0;
    }
        else if(fKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_F:
                //If I key is pressed after M, call show integral markers
                emit fullscreen();
                break;
        }
        fKeyWasPressed=0;
    }
    else
    {
        //Looks at what key was pressed and does different things depending on what was pressed
        switch(event->key())
        {
            case Qt::Key_H:
            case Qt::Key_Question:
                //If shift is pressed, mark that down to later check if the / is pressed at the same time
                emit requestHelp();
                break;
            case Qt::Key_Control:
                //If control is pressed, mark that down to later check if the mouse is clicked at the same time
                controlKeyIsPressed=1;
                break;
            case Qt::Key_C:
                //if the C key is pressed, mark that down and prepare to execute a command
                cKeyWasPressed=1;
                break;
            case Qt::Key_Z:
                //if the C key is pressed, mark that down and prepare to execute a command
                zKeyWasPressed=1;
                break;
            case Qt::Key_M:
                //if the C key is pressed, mark that down and prepare to execute a command
                mKeyWasPressed=1;
                break;
            case Qt::Key_I:
                //if the I key is pressed, add an integral marker on screen and remember the integral position
                emit addIntegralMarkerRequested(xMousePosition, yMousePosition);
                break;
                case Qt::Key_Space:
                //if the [SpaceBar] key is pressed, add an zoom marker on screen and remember the zoom position
                emit addSpaceBarMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_F:
                //if the F key is pressed, mark that down and prepare to execute a command
                fKeyWasPressed=1;
                break;
            case Qt::Key_Right:
                //if the > key is pressed, add an integral marker on screen and remember the integral position
                emit requesttranslateplusTheScreen();
                break;
            case Qt::Key_Left:
                //if the < key is pressed, add an integral marker on screen and remember the integral position
                emit requesttranslateminusTheScreen();
                break;
            case Qt::Key_Down:
                //if the v key is pressed, add an integral marker on screen and remember the integral position
                emit requesttranslatedownTheScreen();
                break;
            case Qt::Key_Up:
                //if the ^ key is pressed, add an integral marker on screen and remember the integral position
                emit requesttranslateupTheScreen();
                break;
            case Qt::Key_B:
                //if the B key is pressed, add a background marker on screen and remember the background position
                emit addBackgroundMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_R:
                //if the R key is pressed, add a range marker on screen and remember the range position
                emit requestAddRangeMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_G:
                //if the G key is pressed, add a Gauss marker on screen and remember the Gauss position
                emit requestAddGaussMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Equal:
                //if the = key is pressed, clear the screen of everything except the histogram
                emit requestClearTheScreen();
            break;
            case Qt::Key_Return:
                //Pentru Petre
                break;
            case Qt::Key_E:
                //if the E key is pressed, zoom the region between the spacebar markers
                emit requestZoomTheScreen();
            break;
            default:
                QWidget::keyPressEvent(event);
                break;

        }
    }
}

void QRootCanvas::keyReleaseEvent(QKeyEvent *event)
{
    //Looks at what key was released and does different things depending on what was released
    switch(event->key())
    {
        case Qt::Key_Control:
            //If control is released, mark that down so no check is done with the mouse
            controlKeyIsPressed=0;
            break;
        default:
            QWidget::keyReleaseEvent(event);
            break;
    }
}

//______________________________________________________________________________
void QRootCanvas::resizeEvent( QResizeEvent *event )
{
   // Handle resize events.

    //Instructs the spectra what to do when the main window gets resized
   if (fCanvas) {
      fCanvas->SetCanvasSize(event->size().width(), event->size().height()); 
      fCanvas->Resize();
      fCanvas->Update();
   }
}

//______________________________________________________________________________
void QRootCanvas::paintEvent( QPaintEvent * )
{
   // Handle paint events.

    //Not sure what this does, I have checked the documentation but it is unclear to me
   if (fCanvas) {
      fCanvas->Resize();
      fCanvas->Update();
   }
}

//______________________________________________________________________________
void QMainCanvas::closeEvent(QCloseEvent *e)
{
    //This function is called when then app is attempting to close
    //A window is created which asks the user if he is sure he wants to quit
    QMessageBox::StandardButton quiting;
    quiting=QMessageBox::question(this,"Quit","Are you sure you want to quit?",QMessageBox::Yes|QMessageBox::No);
    //If the answers if yes the app proceeds to close, otherwise it does not
    if(quiting==QMessageBox::Yes)
    {
        e->accept();
    }
    else
    {
        e->ignore();
    }
}

//------------------------------------------------------------------------------

//______________________________________________________________________________
QMainCanvas::QMainCanvas(QWidget *parent) : QWidget(parent)
{
   // QMainCanvas constructor.

   QVBoxLayout *l = new QVBoxLayout(this);

   //Adds the canvas to the window
   l->addWidget(canvas = new QRootCanvas(this));
   //Adds the button to the window
   l->addWidget(b = new QPushButton("&Select your file", this));
   //When the button is pressed, execute function clicked1
   connect(b, SIGNAL(clicked()), this, SLOT(clicked1()));
   //Same as the previous line of code, it adds a button to the window
   l->addWidget(b = new QPushButton("&Integral No Background", this));
   //Same as the previous line of code, it executes the function areaFunction when the button is clicked
   connect(b, SIGNAL(clicked()), this, SLOT(areaFunction()));
   //Same as the previous line of code, it adds a button to the window
   l->addWidget(b = new QPushButton("&Integral With Background", this));
   //Same as the previous line of code, it executes the function areaFunctionWithBackground when the button is clicked
   connect(b, SIGNAL(clicked()), this, SLOT(areaFunctionWithBackground()));
   l->addWidget(b = new QPushButton("&Cal2P", this));
   //Same as the previous line of code, it executes the function Cal2pMain when the button is clicked
   connect(b, SIGNAL(clicked()), this, SLOT(Cal2pMain()));




connect(canvas,SIGNAL(mouseLeftClickCoordRequest( Double_t,  Double_t)), this, SLOT(mouseLeftClickCoord(Double_t, Double_t)));
connect(canvas,SIGNAL(mouseLeftClickCoordRequest( Double_t,  Double_t)), this, SLOT(IdentifyLastClickedHistogram(Double_t , Double_t )));
connect(canvas,SIGNAL(mousePilgrimCoordRequest( Double_t,  Double_t)), this, SLOT(mousePilgrimCoord(Double_t, Double_t)));
connect(canvas,SIGNAL(mousePilgrimCoordRequest( Double_t,  Double_t)), this, SLOT(IdentifyLastPilgrimHistogram(Double_t , Double_t )));

//connect(canvas,SIGNAL(refreshLines()), this, SLOT(mouseMoveEvent));

connect(canvas,SIGNAL(AddLineRequest()), this, SLOT(AddLine()));

connect(canvas,SIGNAL(AddCulomnRequest()), this, SLOT(AddCulomn()));

connect(canvas,SIGNAL(DeleteLineRequest()), this, SLOT(DeleteLine()));

connect(canvas,SIGNAL(DeleteCulomnRequest()), this, SLOT(DeleteCulomn()));


connect(canvas,SIGNAL(requestIntegrationNoBackground()), this, SLOT(areaFunction()));


   //connects the keyboard command C+I to the areaFunction;
   connect(canvas,SIGNAL(requestIntegrationNoBackground()), this, SLOT(areaFunction()));

   //connects the keyboard command C+J to the areaFunction;
   connect(canvas,SIGNAL(requestIntegrationWithBackground()), this, SLOT(areaFunctionWithBackground()));

   //connects the keyboard/mouse combination command Ctrl+Left Click to the autoFit function;
   connect(canvas,SIGNAL(autoFitRequested(int, int)), this, SLOT(autoFit(int, int)));

   //connects the keyboard command = to clearing the screen;
   connect(canvas,SIGNAL(requestClearTheScreen()), this, SLOT(clearTheScreen()));

   //connects the keyboard command B to adding the background markers;
   connect(canvas,SIGNAL(addBackgroundMarkerRequested(Int_t, Int_t)), this, SLOT(addBackgroundMarker(Int_t, Int_t)));

   //connects the keyboard command I to adding the background markers;
   connect(canvas,SIGNAL(addIntegralMarkerRequested(Int_t, Int_t)), this, SLOT(addIntegralMarker(Int_t, Int_t)));

   //connects the keyboard command E ;
   connect(canvas,SIGNAL(requestZoomTheScreen()), this, SLOT(zoomTheScreen()));

   //connects the keyboard command > to adding the background markers;
   connect(canvas,SIGNAL(requesttranslateplusTheScreen()), this, SLOT(translateplusTheScreen()));

   //connects the keyboard command < to adding the background markers;
   connect(canvas,SIGNAL(requesttranslateminusTheScreen()), this, SLOT(translateminusTheScreen()));

   //connects the keyboard command V to adding the background markers;
   connect(canvas,SIGNAL(requesttranslatedownTheScreen()), this, SLOT(translatedownTheScreen()));

   //connects the keyboard command ^ to adding the background markers;
   connect(canvas,SIGNAL(requesttranslateupTheScreen()), this, SLOT(translateupTheScreen()));

   //connects the keyboard command F+S to clearing all markers;
   connect(canvas,SIGNAL(fullscreen()), this, SLOT(zoomOut()));

   //connects the keyboard command Z+B to clearing the background markers;
   connect(canvas,SIGNAL(requestDeleteBackgroundMarkers()), this, SLOT(deleteBackgroundMarkers()));

   //connects the keyboard command Z+I to clearing the integral markers;
   connect(canvas,SIGNAL(requestDeleteIntegralMarkers()), this, SLOT(deleteIntegralMarkers()));

   //connects the keyboard command Z+A to clearing all markers;
   connect(canvas,SIGNAL(requestDeleteAllMarkers()), this, SLOT(deleteAllMarkers()));

   //connects the keyboard command M+B to clearing the background markers;
   connect(canvas,SIGNAL(requestShowBackgroundMarkers()), this, SLOT(showBackgroundMarkers()));

   //connects the keyboard command M+I to clearing the integral markers;
   connect(canvas,SIGNAL(requestShowIntegralMarkers()), this, SLOT(showIntegralMarkers()));

   //connects the keyboard command M+A to clearing all markers;
   connect(canvas,SIGNAL(requestShowAllMarkers()), this, SLOT(showAllMarkers()));

    //connects the keyboard command space bar to adding the background markers;
   connect(canvas,SIGNAL(addSpaceBarMarkerRequested(Int_t, Int_t)), this, SLOT(addSpaceBarMarker(Int_t, Int_t)));

   //connects the keyboard command R to adding a range marker;
   connect(canvas,SIGNAL(requestAddRangeMarker(Int_t, Int_t)), this, SLOT(addRangeMarker(Int_t, Int_t)));

   //connects the keyboard command Z+R to clearing the range markers;
   connect(canvas,SIGNAL(requestDeleteRangeMarkers()), this, SLOT(deleteRangeMarkers()));

   //connects the keyboard command M+R to clearing the range markers;
   connect(canvas,SIGNAL(requestShowRangeMarkers()), this, SLOT(showRangeMarkers()));

   //connects the keyboard command G to adding a Gauss marker;
   connect(canvas,SIGNAL(requestAddGaussMarker(Int_t, Int_t)), this, SLOT(addGaussMarker(Int_t, Int_t)));

   //connects the keyboard command Z+G to clearing the Gauss markers;
   connect(canvas,SIGNAL(requestDeleteGaussMarkers()), this, SLOT(deleteGaussMarkers()));

   //connects the keyboard command M+G to clearing the Gauss markers;
   connect(canvas,SIGNAL(requestShowGaussMarkers()), this, SLOT(showGaussMarkers()));

   //connects the keyboard command C+V to fitting the Gauss Functions;
   connect(canvas,SIGNAL(requestFitGauss()), this, SLOT(fitGauss()));

   //connects the keyboard key ? to listing the commands
   connect(canvas,SIGNAL(requestHelp()),this,SLOT(offerHelp()));

   //connects the keyboard commands CTRL+C/Z/Y to quitting the app
   connect(canvas,SIGNAL(killSwitch()), qApp, SLOT(quit()));



   fRootTimer = new QTimer( this );
   //Every 20 ms, call function handle_root_events()
   QObject::connect( fRootTimer, SIGNAL(timeout()), this, SLOT(handle_root_events()) );
   fRootTimer->start( 20 );

   //Creates the new TH1F histogram with 10240 bins. Why 10240? Because that's how many our test file has.
    HijF[1][1] = new TracknHistogram("HijF[1][1]","Test random numbers", 10240, 0, 10240);
    HijF[1][1]->GetXaxis()->SetNdivisions(0, kTRUE);
    HijF[1][1]->GetXaxis()->SetLabelSize(0);
    HijF[1][1]->GetYaxis()->SetNdivisions(0, kTRUE);
    HijF[1][1]->GetYaxis()->SetLabelSize(0);
    HijF[1][1]->SetStats(0);
    //HijF[1][1]->GetYaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() * 1.05);
    //HijF[1][1]->GetYaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() / 1.05);
    IdentifyLastPilgrimHistogram(mousePilgrimX,mousePilgrimY);
    IdentifyLastClickedHistogram(mouseLeftClickXcoord,mouseLeftClickYcoord);

    //TH1F* h1f = new TracknHistogram("h2f","h2f", 10240, 0, 10240);
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();






}
bool isAsciiFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening the file." << std::endl;
        return false;
    }

    char c;
    while (file.get(c)) {
        if (static_cast<unsigned char>(c) > 127) {
            // If any character is outside the ASCII range, it's not an ASCII file.
            return false;
        }
    }

    return true;
}
//______________________________________________________________________________
void QMainCanvas::clicked1()
{
if(numberoftimes==0){

}
    //canvas->getCanvas()->Clear();
HijF[SelectedElement_i][SelectedElement_j]->Reset();
//canvas->getCanvas()->Clear();
canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
   // This opens a dialog to select a file for reading
   QString fileName = QFileDialog::getOpenFileName(this, "Open a file","C://");
   // Casts a QT string to C++ string
   std::string SpectrumName = fileName.toStdString();
   std::ifstream file(SpectrumName, std::ios::in);
   if (!file) {
      std::cerr << "Error: could not open file" << std::endl;
   }

   // read the data into a vector
   std::vector<uint32_t> data;
   uint32_t value;
    //std::ofstream testAfisare("testAfisare.txt");
   std::string line;

    if (isAsciiFile(SpectrumName)) { // Checks if the file is Ascii format and reads it
        std::cout << SpectrumName << " is an ASCII file." << std::endl;
        while (std::getline(file, line)) { // Read each line from the file
            std::istringstream iss(line);
            while (iss>>value) {
                data.push_back(value);
            }
        }
    } else {
        std::cout << SpectrumName << " is not an ASCII file." << std::endl;
        while (file.read(reinterpret_cast<char*>(&value), sizeof(value)))
                data.push_back(value);
    }


   //write the data into the histogram
   for(unsigned long int i=1;i<=data.size();i++)
       HijF[SelectedElement_i][SelectedElement_j]->AddBinContent(i,data[i-1]);

   maxValueInHistogram=HijF[SelectedElement_i][SelectedElement_j]->GetBinContent(HijF[SelectedElement_i][SelectedElement_j]->GetMaximumBin());

   //Draws the spectrum and tells the canvas to update itself
   //if(HijC[SelectedElement_i][SelectedElement_j].size()==0){
canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);

// Setăm culoarea liniei primei histograme la albastru
HijF[SelectedElement_i][SelectedElement_j]->SetLineColor(HijC[PilgrimElement_i][PilgrimElement_j].size()+4);

// Desenăm prima histogramă
HijF[SelectedElement_i][SelectedElement_j]->Draw();

HijC[PilgrimElement_i][PilgrimElement_j].push_back((TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone());
if(HijC[PilgrimElement_i][PilgrimElement_j].size()>1){
    for(int k=0;k<HijC[PilgrimElement_i][PilgrimElement_j].size();k++){
        HijC[PilgrimElement_i][PilgrimElement_j][k]->SetLineColor(4+k);
        HijC[PilgrimElement_i][PilgrimElement_j][k]->Draw("SAME");
    }
}

// Clonăm histograma pentru a avea o copie pe care să o translatăm
/*TH1F *hClone = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();

// Setăm culoarea liniei clonei la o altă culoare pentru a o distinge
hClone->SetLineColor(kRed);

// Translatăm clona pe axa X (de exemplu, cu 1 unitate la dreapta)
hClone->GetXaxis()->SetLimits(hClone->GetXaxis()->GetXmin() + 3, hClone->GetXaxis()->GetXmax() + 3);

// Desenăm histograma clonată suprapusă peste originală, folosind opțiunea "SAME"
hClone->Draw("SAME");

TH1F *hClone1 = (TH1F*)HijF[SelectedElement_i][SelectedElement_j]->Clone();

// Setăm culoarea liniei clonei la o altă culoare pentru a o distinge
hClone1->SetLineColor(kGreen);

// Translatăm clona pe axa X (de exemplu, cu 1 unitate la dreapta)
hClone1->GetXaxis()->SetLimits(hClone1->GetXaxis()->GetXmin() + 6, hClone1->GetXaxis()->GetXmax() + 6);

// Desenăm histograma clonată suprapusă peste originală, folosind opțiunea "SAME"
hClone1->Draw("SAME"); */


      // HijC[SelectedElement_i][SelectedElement_j].push_back(HijF[SelectedElement_i][SelectedElement_j]);
//}
//if(HijC[SelectedElement_i][SelectedElement_j].size()>=1){
   // for(int k=0;k<HijC[SelectedElement_i][SelectedElement_j].size()-1;k++){
           // HijC[SelectedElement_i][SelectedElement_j][k]->SetLineColor(k+1);
            //canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);
            //canvas->getCanvas()->cd();
            //HijC[SelectedElement_i][SelectedElement_j][k]->Draw("SAME");
            //canvas->getCanvas()->Update();
            //std::cout<<HijC[SelectedElement_i][SelectedElement_j].size()<<"dadada123\n";
           // HijC[SelectedElement_i][SelectedElement_j].push_back(HijF[SelectedElement_i][SelectedElement_j]);



//std::cout << HijC[SelectedElement_i][SelectedElement_j].size() << " histograme desenate\n";


/*if(HijC[SelectedElement_i][SelectedElement_j].size()==0){
HijF[SelectedElement_i][SelectedElement_j]->Draw();}
if(HijC[SelectedElement_i][SelectedElement_j].size()>=1){
    canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);
    HijC[SelectedElement_i][SelectedElement_j][HijC[SelectedElement_i][SelectedElement_j].size()-1]->SetLineColor(kRed);
    HijC[SelectedElement_i][SelectedElement_j][HijC[SelectedElement_i][SelectedElement_j].size()-1]->Draw("same");
}
HijC[SelectedElement_i][SelectedElement_j].push_back(HijF[SelectedElement_i][SelectedElement_j]); */







canvas->getCanvas()->Modified();
canvas->getCanvas()->Update();




 numberoftimes++;
}

void QMainCanvas::Cal2pMain() {
    if(puncte_calib2p.size() > 1) {
        QDialog dialog(this);
        dialog.setWindowTitle("Two point calibration");
        dialog.setStyleSheet("background-color: #708090;");
        QFormLayout form(&dialog);

        // Add the first energy input
        QLineEdit *energy1LineEdit = new QLineEdit(&dialog);
        energy1LineEdit->setStyleSheet("background-color: white;");
        form.addRow(QString::number(puncte_calib2p[puncte_calib2p.size() - 2]) + " no. channel (First Energy):", energy1LineEdit);


        // Add the second energy input
        QLineEdit *energy2LineEdit = new QLineEdit(&dialog);
        energy2LineEdit->setStyleSheet("background-color: white;");
        form.addRow(QString::number(puncte_calib2p[puncte_calib2p.size() - 1]) + " no. channel (Second Energy):", energy2LineEdit);

        // Add Ok and Cancel buttons
        QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
        form.addRow(&buttonBox);

        QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
        QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

        // Muta dialogul in pozitia dorita pe ecran
        dialog.move(100, 500); // Coordonatele x și y pot fi ajustate conform necesităților

        if (dialog.exec() == QDialog::Accepted) {
            bool ok1, ok2;
            double energie1 = energy1LineEdit->text().toDouble(&ok1);
            double energie2 = energy2LineEdit->text().toDouble(&ok2);

            if(ok1 && ok2 && energie1 > 0 && energie2 > 0) {
                CalibrareIn2P(puncte_calib2p, energie1, energie2);
            } else {
                if(ok1 && ok2 && energie1 < 0 || energie2 < 0) {
                    std::cout << "The energy values ​​must be positive\n";
                    CommandPrompt::getInstance()->appendPlainText("The energy values ​​must be positive\n");
                } else {
                    std::cout << "The fields must be filled\n";
                    CommandPrompt::getInstance()->appendPlainText("The fields must be filled\n");
                }
            }
        }
    } else {
        std::cout << "Two markers are needed to calibrate in two points\n";
        CommandPrompt::getInstance()->appendPlainText("Two markers are needed to calibrate in two points\n");
    }
}


void QMainCanvas::addSpaceBarMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;
    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[PilgrimElement_i][PilgrimElement_j]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Add the position to the integral marker vector
    spacebar_markers.push_back((Double_t)binX);
    //std::cout<<(Double_t)binX<<std::endl;

    //Create a yellow integral line and add it to the screen
    TLine *spacebarLine = new TLine(binX, 0., binX, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
    spacebarLine->SetLineColor(kCyan);
    spacebarLine->SetLineWidth(2);

    spacebarLine->Draw("same");

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(spacebarLine);


   //HijF[1][1]->GetXaxis()->SetRangeUser(20, 80); // Zoom între 20 și 80
    addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j].push_back(binX);
   zoom_markers.push_back(binX);

   //canvas->getCanvas()->Modified();
   //canvas->getCanvas()->Update();

}

void QMainCanvas::areaFunction()
{
   //A stand in vector for the markers is used to call the integral function so it perform an integral with no background
   //Regardles of there are backgrounds or not
   std::vector<Double_t> placeholder_background_markers_matrix[PilgrimElement_i][PilgrimElement_j];
   integral_function(HijF[PilgrimElement_i][PilgrimElement_j],integral_markers,placeholder_background_markers_matrix[PilgrimElement_i][PilgrimElement_j],slope,addition);
}

void QMainCanvas::areaFunctionWithBackground()
{
   //The integral function is used with the background markers and the integral markers to perform an integral with backgorund
   //The function also returns the slope of the background
    if(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()==0)
    {
        CommandPrompt::getInstance()->appendPlainText("There are no background markers, so an integral with background can not be performed\n");
        std::cout<<"There are no background markers, so an integral with background can not be performed\n";
    }
    else
    {

       integral_function(HijF[PilgrimElement_i][PilgrimElement_j],integral_markers,background_markers_matrix[PilgrimElement_i][PilgrimElement_j],slope,addition);
       //Draw a line to show the background
       TLine *backgroundLine = new TLine(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][0]-0.5, slope*(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][0]-0.5)+addition, background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]-0.5, slope*(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]-0.5)+addition);
       backgroundLine->SetLineColor(kBlue);
       backgroundLine->SetLineWidth(2);

       backgroundLine->Draw("same");
       //Add the line to the list of things put on the screen, so it can be deleted
       listOfObjectsDrawnOnScreen.Add(backgroundLine);

       canvas->getCanvas()->Modified();
       canvas->getCanvas()->Update();
    }
}

//______________________________________________________________________________

void QMainCanvas::autoFit(int x, int y)
{
    std::string objectInfo, temp;
    int from, to, binX, binC;//, sum;
    //Double_t xPos, yPos;
    Double_t gaussianHeight, gaussianCenter, gaussianSigma, bkgSlope, bkg0, gaussianFWHM, gaussianCenterError, gaussianIntegral, gaussianIntegralError, gaussianFWHMError;
    std::ostringstream tempStringStream;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[PilgrimElement_i][PilgrimElement_j]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    //xPos=std::stof(temp);

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    //yPos=std::stof(temp);

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Cut the next section, which is the value in the bin that was clicked
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binC=std::stoi(temp);

    //Cut the next section, which represents the sum of all bins shown on the screen until the bin on which it was clicked
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(")");
    temp=objectInfo.substr(from+1,to-from-1);
    //sum=std::stoi(temp);

    //Declaring a new formula which is a Gaussian and a simple background, and making it a Root function. Define a range on which it is applied
    TFormula *gaussianWithBackground = new TFormula("gaussianWithBackground","[0]*exp(-(x-[1])^2/(2*[2]))+[3]*x+[4]");
    TF1 *gaussianWithBackgroundFunction = new TF1("gaussianWithBackgroundFunction","gaussianWithBackground",binX-20,binX+20);

    //Declaring a new formula which is a Gaussian and a simple background, and making it a Root function. Define a range on which it is applied
    TFormula *background = new TFormula("background","[0]*x+[1]");
    TF1 *backgroundFunction = new TF1("backgroundFunction","background",binX-20,binX+20);

    TLatex *gaussianCenterMarkerText = new TLatex();

    //Declaring the initial values for the 5 parameters to be fit
    gaussianWithBackgroundFunction->SetParameter(0,binC);
    gaussianWithBackgroundFunction->SetParameter(1,binX);
    gaussianWithBackgroundFunction->SetParameter(2,4.);
    gaussianWithBackgroundFunction->SetParameter(3,0.);
    gaussianWithBackgroundFunction->SetParameter(4,findMinValueInInterval(binX-20,binX+20));

    //Fitting the histogram with the Gaussian function with background and putting the results in a special format
    //Fit options are Q - quiet; M - improved fitting; R-respect range from function
    TFitResultPtr fitResult = HijF[PilgrimElement_i][PilgrimElement_j]->Fit(gaussianWithBackgroundFunction,"QMRS", "same");

    //Read the background parameters and feed them into the background function for use in finding the integral!
    bkgSlope=gaussianWithBackgroundFunction->GetParameter(3);
    bkg0=gaussianWithBackgroundFunction->GetParameter(4);
    backgroundFunction->FixParameter(0,bkgSlope);
    backgroundFunction->FixParameter(1,bkg0);

    //Extract the Full Width Half Maximum (FWHM, related to the width of the Gaussian) and calculate the integral and integral error
    gaussianSigma=gaussianWithBackgroundFunction->GetParameter(2);
    gaussianFWHM=gaussianSigma*2.3548;
    gaussianIntegral=gaussianWithBackgroundFunction->Integral(binX-20,binX+20)-backgroundFunction->Integral(binX-20,binX+20);
    gaussianIntegralError=gaussianWithBackgroundFunction->IntegralError(binX-20,binX+20,fitResult->GetParams(),fitResult->GetCovarianceMatrix().GetMatrixArray());

    //If the FWHM is more than a sixth of the region we use for the autofit (41 bins), then redo the fitting in a region that is six times bigger than the FWHM
    //Reobtain the FWHM, integral and integral error, because the region changed
    if(gaussianFWHM>41./6)
    {
        gaussianWithBackgroundFunction->SetRange(binX-gaussianFWHM*3,binX+gaussianFWHM*3);
        backgroundFunction->SetRange(binX-gaussianFWHM*3,binX+gaussianFWHM*3);

        fitResult = HijF[PilgrimElement_i][PilgrimElement_j]->Fit(gaussianWithBackgroundFunction,"QMRS", "");

        gaussianSigma=gaussianWithBackgroundFunction->GetParameter(2);
        gaussianFWHM=gaussianSigma*2.3548;
        gaussianIntegral=gaussianWithBackgroundFunction->Integral(binX-gaussianFWHM*3,binX+gaussianFWHM*3)-backgroundFunction->Integral(binX-gaussianFWHM*3,binX+gaussianFWHM*3);
        gaussianIntegralError=gaussianWithBackgroundFunction->IntegralError(binX-gaussianFWHM*3,binX+gaussianFWHM*3,fitResult->GetParams(),fitResult->GetCovarianceMatrix().GetMatrixArray());
    }

    //Obtain the Gaussian's maximum, center (with error), FWHM error
    gaussianHeight=gaussianWithBackgroundFunction->GetParameter(0);
    gaussianCenter=gaussianWithBackgroundFunction->GetParameter(1);
    gaussianCenterError=gaussianWithBackgroundFunction->GetParError(1);
    gaussianFWHMError=gaussianWithBackgroundFunction->GetParError(2)*2.3548;

    //Create the text to be shown in screen showing the Gaussian center and add it to the list of objects to be later deleted
    char buffer[8];
    snprintf(buffer, sizeof buffer, "%f", gaussianCenter);
    listOfObjectsDrawnOnScreen.Add(gaussianCenterMarkerText->DrawLatex(gaussianCenter,gaussianHeight,buffer));

    //Draw the fitted function, so it remains on screen regardless of how many other fits are made
    gaussianWithBackgroundFunction->Draw("same");

    //A background function is made to show the subtracted background
    backgroundFunction->SetLineColor(kBlue);
    backgroundFunction->Draw("same");

    //Updating the canvas, so all the changes appear
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Writing the obtained data on screen, in a fixed format, so everything aligns nicely
    //First (fixed) row

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
    puncte_calib2p.push_back(gaussianCenter);


    //Second row that contains variable numbers
    std::cout<<std::setw(10);
    std::cout<<"1";
    std::cout<<std::setw(10);
    std::cout << std::fixed;
    std::cout<<std::setprecision(2)<<gaussianCenter;
    std::cout<<std::setw(15);
    tempStringStream<< std::fixed<<std::setprecision(2)<<gaussianCenter<<"("<<std::setprecision(0)<<ceil(gaussianCenterError*100)<<")";
    temp=tempStringStream.str();
    std::cout<<temp;
    tempStringStream.str(std::string());
    tempStringStream<< std::fixed<<std::setprecision(0)<<gaussianIntegral<<"("<<round(gaussianIntegralError)<<")";
    temp=tempStringStream.str();
    std::cout<<std::setw(25);
    std::cout<<temp;
    tempStringStream.str(std::string());
    tempStringStream<< std::fixed<<std::setprecision(2)<<gaussianFWHM<<"("<<std::setprecision(0)<<ceil(gaussianFWHMError*100)<<")";
    temp=tempStringStream.str();
    std::cout<<std::setw(10);
    std::cout<<temp<<std::endl;

    QString numberStr = QString("%1").arg("1", 0, ' ');
    QString gaussianCenterStr = QString("%1").arg(gaussianCenter,0, ' ', 2);
    QString energyStr = QString("%1(%2)").arg(gaussianCenter, 0, ' ', 2).arg(qCeil(gaussianCenterError * 100), 0, ' ',0);
    QString gaussianIntegralStr = QString("%1(%2)").arg(gaussianIntegral, 0, ' ', 0).arg(qRound(gaussianIntegralError), 0, ' ',0);
    QString gaussianFWHMStr = QString("%1(%2)").arg(gaussianFWHM, 0, ' ', 2).arg(qCeil(gaussianFWHMError * 100), 0, ' ',0);


    QString dataRow = QString("%1%2%3%4%5")
        .arg(numberStr,-10,QChar(' '))
        .arg(gaussianCenterStr, -10, QChar(' '))
        .arg(energyStr, -15, QChar(' '))
        .arg(gaussianIntegralStr, -25, QChar(' '))
        .arg(gaussianFWHMStr, -10, QChar(' '));

    // Insert data row into QPlainTextEdit
    CommandPrompt::getInstance()->appendPlainText(dataRow + '\n');

    //Make list of objects that have been drawn to delete them later
    listOfObjectsDrawnOnScreen.Add(gaussianWithBackgroundFunction);
    listOfObjectsDrawnOnScreen.Add(backgroundFunction);
}

//______________________________________________________________________________
Double_t QMainCanvas::findMinValueInInterval(int intervalStart, int intervalFinish)
{
    int minValueFound=HijF[1][1]->GetBinContent(intervalStart);

    for(int i=intervalStart+1; i<=intervalFinish; i++)
    {
        if(HijF[1][1]->GetBinContent(i)<minValueFound)
            minValueFound=HijF[1][1]->GetBinContent(i);
    }
    return minValueFound;
}

//______________________________________________________________________________
Double_t QMainCanvas::findMaxValueInInterval(int intervalStart, int intervalFinish)
{
    int maxValueFound=HijF[1][1]->GetBinContent(intervalStart);

    for(int i=intervalStart+1; i<=intervalFinish; i++)
    {
        if(HijF[1][1]->GetBinContent(i)>maxValueFound)
            maxValueFound=HijF[1][1]->GetBinContent(i);
    }
    return maxValueFound;
}

//______________________________________________________________________________
void QMainCanvas::addBackgroundMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[PilgrimElement_i][PilgrimElement_j]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Add the position to the background marker vector
    background_markers_matrix[PilgrimElement_i][PilgrimElement_j].push_back(binX);

    //Create a blue background line and add it to the screen
    TLine *backgroundLine = new TLine(binX-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);

    backgroundLine->Draw("same");

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(backgroundLine);

    if(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()%2==0)
    {
        TLine *bottomBackgroundLine = new TLine(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-2]-0.5, 0., binX-0.5, 0);
        bottomBackgroundLine->SetLineColor(kBlue);
        bottomBackgroundLine->SetLineWidth(2);

        bottomBackgroundLine->Draw("same");

        //Add the line to the list of things put on the screen, so it can be deleted
        listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

        TBox *backgroundArea = new TBox(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-2]-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
        backgroundArea->SetFillColor(kBlue);
        backgroundArea->SetFillStyle(3545);
        backgroundArea->Draw("same");

        //Add the line to the list of things put on the screen, so it can be deleted
        listOfObjectsDrawnOnScreen.Add(backgroundArea);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::addIntegralMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[PilgrimElement_i][PilgrimElement_j]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Add the position to the integral marker vector
    integral_markers.push_back((Double_t)binX);
    //std::cout<<(Double_t)binX<<std::endl;

    //Create a yellow integral line and add it to the screen
    TLine *integralLine = new TLine(binX-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
    integralLine->SetLineColor(kYellow);
    integralLine->SetLineWidth(2);

    integralLine->Draw("same");

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(integralLine);
}


//______________________________________________________________________________
void QMainCanvas::clearTheScreen()
{
    //Get a list of all the functions on the histogram and delete all except the first (which is the histogram itself)

    while(HijF[PilgrimElement_i][PilgrimElement_j]->GetListOfFunctions()->GetSize()>1)
    {
        HijF[PilgrimElement_i][PilgrimElement_j]->GetListOfFunctions()->RemoveLast();
    }

    //Get a list of all the functions drawn on screen and delete all of them!
    while(listOfObjectsDrawnOnScreen.GetSize())
    {
         listOfObjectsDrawnOnScreen.Last()->Delete();
         listOfObjectsDrawnOnScreen.RemoveLast();
    }
 HijC[PilgrimElement_i][PilgrimElement_j].clear();
    //HijF[PilgrimElement_i][PilgrimElement_j]->Reset();
    HijF[PilgrimElement_i][PilgrimElement_j]->SetLineColor(kBlue);
    HijF[PilgrimElement_i][PilgrimElement_j]->Draw();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}
void QMainCanvas::zoomTheScreen()
{
    //Zoom the histogram in the region delimited by spacebar markers
    int i= addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j].size();
    int x1,x2;
    if(i>=2){
    if(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-2]<addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-1]){
    HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->SetRangeUser(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-2], addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-1]);}
    if(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-1]<addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-2]){
    HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->SetRangeUser(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-1], addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][i-2]);}}
    if(i<=1){
        std::cout<<"AI nev de doi space\n";}
  //clearTheScreen();
    IdentifyLastPilgrimHistogram(mousePilgrimX,mousePilgrimY);
  IdentifyLastClickedHistogram(mouseLeftClickXcoord,mouseLeftClickYcoord);
  //canvas->getCanvas()->Clear();
  canvas->getCanvas()->Modified();
  canvas->getCanvas()->Update();

}

void QMainCanvas::zoomOut()
{
  //Zoom out the histogram in its initial scale
  HijF[PilgrimElement_i][PilgrimElement_j]->GetYaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() );
  HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() );
  HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->UnZoom();
  HijF[PilgrimElement_i][PilgrimElement_j]->GetYaxis()->UnZoom();
  IdentifyLastPilgrimHistogram(mousePilgrimX,mousePilgrimY);
  IdentifyLastClickedHistogram(mouseLeftClickXcoord,mouseLeftClickYcoord);
  canvas->getCanvas()->Modified();
  canvas->getCanvas()->Update();
}

void QMainCanvas::translateplusTheScreen()
{
    //Translates the zoom in the positive(right) direction of the abscissa
    int k= addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j].size();
    if(k>1){
    addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]=addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]+fabs(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]-addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2])/50;
    addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2]=addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2]+fabs(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]-addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2])/50;
    zoomTheScreen();
    showAllMarkers();
    }
}

void QMainCanvas::translateminusTheScreen()
{
    //Translates the zoom in the negative(left) direction of the abscissa
    int k= addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j].size();
    if(k>1){
    addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]=addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]-fabs(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]-addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2])/50;
    addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2]=addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2]-fabs(addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-1]-addSpaceBarMatrixRequested[PilgrimElement_i][PilgrimElement_j][k-2])/50;
    zoomTheScreen();
    showAllMarkers();
    }
}

void QMainCanvas::translatedownTheScreen()
{
    //Increases the scale of the ordinate
    HijF[PilgrimElement_i][PilgrimElement_j]->GetYaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() * 1.05);
    IdentifyLastPilgrimHistogram(mousePilgrimX,mousePilgrimY);
    IdentifyLastClickedHistogram(mouseLeftClickXcoord,mouseLeftClickYcoord);
    //canvas->getCanvas()->Clear();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

}

void QMainCanvas::translateupTheScreen()
{
    //Decreases the scale of the ordinate
    HijF[PilgrimElement_i][PilgrimElement_j]->GetYaxis()->SetRangeUser(0, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum() / 1.05);
    IdentifyLastPilgrimHistogram(mousePilgrimX,mousePilgrimY);
    IdentifyLastClickedHistogram(mouseLeftClickXcoord,mouseLeftClickYcoord);
    //canvas->getCanvas()->Clear();
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

}

//______________________________________________________________________________
void QMainCanvas::deleteBackgroundMarkers()
{
    background_markers_matrix[PilgrimElement_i][PilgrimElement_j].clear();
}

void QMainCanvas::deleteIntegralMarkers()
{
    integral_markers.clear();
}

//______________________________________________________________________________
void QMainCanvas::deleteAllMarkers()
{
    deleteBackgroundMarkers();
    deleteIntegralMarkers();
    deleteRangeMarkers();
    deleteGaussMarkers();
}

//______________________________________________________________________________
void QMainCanvas::showBackgroundMarkers()
{
    for(uint i=0;i<background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size();i++)
    {
        TLine *backgroundLine = new TLine(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i]-0.5, 0., background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
        backgroundLine->SetLineColor(kBlue);
        backgroundLine->SetLineWidth(2);

        backgroundLine->Draw("same");

        listOfObjectsDrawnOnScreen.Add(backgroundLine);

        if(i%2)
        {
            TLine *bottomBackgroundLine = new TLine(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i-1]-0.5, 0., background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i]-0.5, 0);
            bottomBackgroundLine->SetLineColor(kBlue);
            bottomBackgroundLine->SetLineWidth(2);

            bottomBackgroundLine->Draw("same");

            //Add the line to the list of things put on the screen, so it can be deleted
            listOfObjectsDrawnOnScreen.Add(bottomBackgroundLine);

            TBox *backgroundArea = new TBox(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i-1]-0.5, 0., background_markers_matrix[PilgrimElement_i][PilgrimElement_j][i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
            backgroundArea->SetFillColor(kBlue);
            backgroundArea->SetFillStyle(3545);
            backgroundArea->Draw("same");

            //Add the line to the list of things put on the screen, so it can be deleted
            listOfObjectsDrawnOnScreen.Add(backgroundArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::showIntegralMarkers()
{
    for(uint i=0;i<integral_markers.size();i++)
    {
        TLine *integralLine = new TLine(integral_markers[i]-0.5, 0., integral_markers[i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
        integralLine->SetLineColor(kRed);
        integralLine->SetLineWidth(2);

        integralLine->Draw("same");

        listOfObjectsDrawnOnScreen.Add(integralLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::showAllMarkers()
{
    showBackgroundMarkers();
    showIntegralMarkers();
    showRangeMarkers();
    showGaussMarkers();
}

//______________________________________________________________________________
void QMainCanvas::addRangeMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[PilgrimElement_i][PilgrimElement_j]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Add the position to the background marker vector
    range_markers.push_back(binX);

    //Create a blue background line and add it to the screen
    TLine *rangeLine = new TLine(binX-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
    rangeLine->SetLineColor(kYellow);
    rangeLine->SetLineWidth(2);

    rangeLine->Draw("same");

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(rangeLine);

    if(range_markers.size()%2==0)
    {
        TLine *bottomRangeLine = new TLine(range_markers[range_markers.size()-2]-0.5, 0., binX-0.5, 0);
        bottomRangeLine->SetLineColor(kYellow);
        bottomRangeLine->SetLineWidth(2);

        bottomRangeLine->Draw("same");

        //Add the line to the list of things put on the screen, so it can be deleted
        listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

        TBox *backgroundArea = new TBox(range_markers[range_markers.size()-2]-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
        backgroundArea->SetFillColor(kYellow);
        backgroundArea->SetFillStyle(3545);
        backgroundArea->Draw("same");

        //Add the line to the list of things put on the screen, so it can be deleted
        listOfObjectsDrawnOnScreen.Add(backgroundArea);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::deleteRangeMarkers()
{
    range_markers.clear();
}

//______________________________________________________________________________
void QMainCanvas::showRangeMarkers()
{
    for(uint i=0;i<range_markers.size();i++)
    {
        TLine *rangeLine = new TLine(range_markers[i]-0.5, 0., range_markers[i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
        rangeLine->SetLineColor(kYellow);
        rangeLine->SetLineWidth(2);

        rangeLine->Draw("same");

        listOfObjectsDrawnOnScreen.Add(rangeLine);

        if(i%2)
        {
            TLine *bottomRangeLine = new TLine(range_markers[i-1]-0.5, 0., range_markers[i]-0.5, 0);
            bottomRangeLine->SetLineColor(kYellow);
            bottomRangeLine->SetLineWidth(2);

            bottomRangeLine->Draw("same");
            //Add the line to the list of things put on the screen, so it can be deleted
            listOfObjectsDrawnOnScreen.Add(bottomRangeLine);

            TBox *backgroundArea = new TBox(range_markers[i-1]-0.5, 0., range_markers[i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
            backgroundArea->SetFillColor(kYellow);
            backgroundArea->SetFillStyle(3545);
            backgroundArea->Draw("same");

            //Add the line to the list of things put on the screen, so it can be deleted
            listOfObjectsDrawnOnScreen.Add(backgroundArea);
        }
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::addGaussMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=HijF[1][1]->GetObjectInfo(x,y);

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");

    //Cut the next section, which represents the bin which is actually shown at that position (due to zoom in procedures)
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    binX=std::stoi(temp);

    //Add the position to the background marker vector
    gauss_markers.push_back(binX);

    //Create a blue background line and add it to the screen
    TLine *gaussLine = new TLine(binX-0.5, 0., binX-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
    gaussLine->SetLineColor(kPink);
    gaussLine->SetLineWidth(2);

    gaussLine->Draw("same");

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(gaussLine);
}

//______________________________________________________________________________
void QMainCanvas::deleteGaussMarkers()
{
    gauss_markers.clear();
}

//______________________________________________________________________________
void QMainCanvas::showGaussMarkers()
{
    for(uint i=0;i<gauss_markers.size();i++)
    {
        TLine *gaussLine = new TLine(gauss_markers[i]-0.5, 0., gauss_markers[i]-0.5, HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*2);
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
    bool goodRanges, goodGauss=0;
    std::string temp;
    std::ostringstream tempStringStream;
    double maxValue, fitIntegralError;

    //Checking to see if the backgrounds are fine, not overlapping, not odd numbered, and fixes the background if needed
    checkBackgrounds();

    //Checks if the range is fine, that there are only two markers, orders them
    goodRanges=checkRanges();

    //Checks if the gauss markers are fine and inside the range
    if(goodRanges)
        goodGauss=checkGauss();

    //Clears the screen of all previous markers
    clearTheScreen();

    //Adds the background, range and gauss markers back
    showBackgroundMarkers();
    showRangeMarkers();
    showGaussMarkers();

    if(goodRanges&&goodGauss)
    {
        //Find the maximum value in the interval to use as a limit for fitting
        maxValue=findMaxValueInInterval(range_markers[0],range_markers[1]);

        //Fit the background from the background markers
        fitBackground();

        //Declaring a new formula which is a Gaussian and a simple background, and making it a Root function. Define a range on which it is applied
        TFormula *background = new TFormula("background","[0]*x+[1]");
        TFormula *gaussian = new TFormula("gaussian","[0]*exp(-(x-[1])^2/(2*[2]))");

        //Declaring the background function
        TF1 *backgroundFunction = new TF1("backgroundFunction","background",range_markers[0],range_markers[1]);

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
            fullFunction->SetParameter(2+i*3,HijF[1][1]->GetBinContent(gauss_markers[i]-1));
            fullFunction->SetParLimits(2+i*3,0,maxValue*1.1);
            fullFunction->SetParameter(3+i*3,gauss_markers[i]-1);
            fullFunction->SetParLimits(3+i*3,range_markers[0],range_markers[1]);
            fullFunction->SetParameter(4+i*3,3.);
            fullFunction->SetParLimits(4+i*3,0.4,abs(range_markers[1]-range_markers[0])*4);
        }

        //std::cout<<fullFunction->GetFormula()->GetExpFormula()<<std::endl;

        //Fitting the histogram with the Gaussian function with background and putting the results in a special format
        //Fit options are Q - quiet; M - Minuit; R-respect range from function
        TFitResultPtr fitResult = HijF[1][1]->Fit(fullFunction,"Q M R S", "same");


        //If the fit fails (fitResult=4), then change some minimizer options and try again
        if((int) fitResult==4)
        {
            ROOT::Math::MinimizerOptions::SetDefaultStrategy(2);
            ROOT::Math::MinimizerOptions::SetDefaultTolerance(0.1);
            ROOT::Math::MinimizerOptions::SetDefaultMaxFunctionCalls(10000000);

            fitResult = HijF[1][1]->Fit(fullFunction,"Q M R S", "same");
            //ROOT::Math::MinimizerOptions::SetDefaultStrategy(1);


            //If it still fails, set different tolerance and try again
            if((int) fitResult==4)
            {
                ROOT::Math::MinimizerOptions::SetDefaultTolerance(1);

                fitResult = HijF[1][1]->Fit(fullFunction,"Q M R S", "same");

                //If it still fails, set different tolerance and try again
                if((int) fitResult==4)
                {
                    ROOT::Math::MinimizerOptions::SetDefaultTolerance(10);

                    fitResult = HijF[1][1]->Fit(fullFunction,"Q M R S", "same");

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

            QString numberStr = QString("%1").arg(i+1, 0, ' ');
            QString gaussianCenterStr = QString("%1").arg(fullFunction->GetParameter(3+i*3),0, ' ', 2);
            QString energyStr = QString("%1(%2)").arg(fullFunction->GetParameter(3+i*3), 0, ' ', 2).arg(ceil(fullFunction->GetParError(3+i*3)*100), 0, ' ',0);
            QString gaussianIntegralStr = QString("%1(%2)").arg(tempGaussFunction->Integral(range_markers[0],range_markers[1]), 0, ' ', 0).arg(round(sqrt(pow(fitIntegralError,2)+pow(backgroundIntegralError,2))), 0, ' ',0);
            QString gaussianFWHMStr = QString("%1(%2)").arg(fullFunction->GetParameter(4+i*3)*2.3548, 0, ' ', 2).arg(ceil(fullFunction->GetParError(4+i*3)*2.3548*100), 0, ' ',0);


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
    if(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size())
    {
        //If there is an odd number of background markers, delete the last one added
        if(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()%2)
        {
            std::cout<<"There is an odd number of background markers, "<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()<<", so the last one, at "<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]<<", was removed"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("There is an odd number of background markers, "+ QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size())+", so the last one, at " + QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]) + ", was removed \n");
            background_markers_matrix[PilgrimElement_i][PilgrimElement_j].pop_back();
        }

        //If the background markers overlap (meaning they define areas that overlap), then sort them so they don't overlap anymore
        if(overlapping_markers(background_markers_matrix[PilgrimElement_i][PilgrimElement_j]))
        {
            std::cout<<"The background markers shown below produced regions which overlapped"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("The background markers shown below produced regions wwhich overlapped\n");
            for(uint i=0;i<background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()/2;i++)
            {
                std::cout<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i]<<"-"<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1]<<std::endl;
                CommandPrompt::getInstance()->appendPlainText(QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i])+"-"+ QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1])+"\n");
            }

            std::cout<<"Thus, we have reordered them in order to produce non-overlapping regions, as seen below:"<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("Thus, we have reordered them in order to produce non-overlapping regions, as seen below:\n");
            sort(background_markers_matrix[PilgrimElement_i][PilgrimElement_j].begin(),background_markers_matrix[PilgrimElement_i][PilgrimElement_j].end());

            for(uint i=0;i<background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()/2;i++)
            {
                std::cout<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i]<<"-"<<background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1]<<std::endl;
                CommandPrompt::getInstance()->appendPlainText(QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i])+"-"+ QString::number(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1])+"\n");
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
    for(uint i=0;i<background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()/2;i++)
    {
        for(uint j=background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i];j<=background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1];j++)
            tempHist->AddBinContent(j,HijF[1][1]->GetBinContent(j));

        localMinimum=findMinValueInInterval(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i],background_markers_matrix[PilgrimElement_i][PilgrimElement_j][2*i+1]);

        if(localMinimum<minimum)
            minimum=localMinimum;
    }

    //Declaring a new formula which is a simple background, and making it a Root function. Define a range on which it is applied
    TFormula *background = new TFormula("background","[0]*x+[1]");
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

    TMatrixD tempMatrix=fitResult->GetCovarianceMatrix();

    backgroundCovarianceMatrix=&tempMatrix;

    tempHist->Delete();

    //Draw a line to show the background
    TLine *backgroundLine = new TLine(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][0]-0.5, backgroundA1*(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][0]-0.5)+backgroundA0, background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]-0.5, backgroundA1*(background_markers_matrix[PilgrimElement_i][PilgrimElement_j][background_markers_matrix[PilgrimElement_i][PilgrimElement_j].size()-1]-0.5)+backgroundA0);
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
    int n=0;


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(g==maxElement_j){
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    addSpaceBarMatrixRequested[z][g]=addSpaceBarMatrixRequested[SelectedElement_i][PilgrimElement_j];
                    HijC[z][g].push_back((TH1F*)selectedHisto->Clone());}

               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
               n++;
               canvas->getCanvas()->cd(n);
               HijF[z][g]->Draw();
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
    int n=0;


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(z==maxElement_i){
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    addSpaceBarMatrixRequested[z][g]=addSpaceBarMatrixRequested[SelectedElement_i][PilgrimElement_j];
                    HijC[z][g].push_back((TH1F*)selectedHisto->Clone());
            }
               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
               n++;
               canvas->getCanvas()->cd(n);
               HijF[z][g]->Draw();}}

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}
void QMainCanvas::DeleteCulomn(){
    if(maxElement_i==1 && maxElement_j==1){
        selectedHisto=static_cast<TracknHistogram*>(HijF[1][1]->Clone(("h"+QString::number(1)+QString::number(1)+"f").toStdString().c_str()));
    }
    maxElement_j--;
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);
    int n=0;


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(g==maxElement_j){
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    addSpaceBarMatrixRequested[z][g]=addSpaceBarMatrixRequested[SelectedElement_i][PilgrimElement_j];
            }
               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
               n++;
               canvas->getCanvas()->cd(n);
               HijF[z][g]->Draw();
        }}

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

void QMainCanvas::DeleteLine(){
        if(maxElement_i==1 && maxElement_j==1){
        selectedHisto=static_cast<TracknHistogram*>(HijF[1][1]->Clone(("h"+QString::number(1)+QString::number(1)+"f").toStdString().c_str()));
    }
    maxElement_i--;
    canvas->getCanvas()->Clear();
    canvas->getCanvas()->SetBorderMode(0);
    canvas->getCanvas()->SetFillColor(0);
    canvas->getCanvas()->Divide(maxElement_j, maxElement_i,0,0,0);
    int n=0;


       for(int z=1;z<=maxElement_i;z++){
           for(int g=1;g<=maxElement_j;g++){
               if(z==maxElement_i){
                    HijF[z][g] = static_cast<TracknHistogram*>(selectedHisto->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
                    HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());
                    addSpaceBarMatrixRequested[z][g]=addSpaceBarMatrixRequested[SelectedElement_i][PilgrimElement_j];
            }
               else{
               HijF[z][g] = static_cast<TracknHistogram*>(HijF[z][g]->Clone(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str()));
               HijF[z][g]-> SetTitle(("h"+QString::number(z)+QString::number(g)+"f").toStdString().c_str());}
               n++;
               canvas->getCanvas()->cd(n);
               HijF[z][g]->Draw();}}

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}
void QMainCanvas::IdentifyLastClickedHistogram(Double_t , Double_t ){
    //if(maxElement_i>1 || maxElement_j>1){
selectedHisto=nullptr;
    delete lineR;lineR=nullptr;
    delete lineL;lineL=nullptr;
    delete lineD;lineD=nullptr;
    delete lineU;lineU=nullptr;

    for(int j=0;j<maxElement_j;j++){
        if(j*canvas->getCanvas()->GetWw()/maxElement_j<mouseLeftClickXcoord && mouseLeftClickXcoord<(j+1)*canvas->getCanvas()->GetWw()/maxElement_j){
            SelectedElement_j=j+1;}}
    for(int h=0;h<maxElement_i;h++){
        if(h*canvas->getCanvas()->GetWh()/maxElement_i<mouseLeftClickYcoord && mouseLeftClickYcoord<(h+1)*canvas->getCanvas()->GetWh()/maxElement_i){
            SelectedElement_i=h+1;}}
    selectedHisto=HijF[SelectedElement_i][SelectedElement_j];
   // std::cout<<"i="<<SelectedElement_i<<"  j="<<SelectedElement_j<<"\n";

    canvas->getCanvas()->cd((SelectedElement_i-1)*maxElement_j+SelectedElement_j);
    lineR = new TLine(HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetFirst(), 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetFirst(), HijF[SelectedElement_i][SelectedElement_j]->GetMaximum()*1.05);
    lineL= new TLine(HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetLast(), 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetLast(), HijF[SelectedElement_i][SelectedElement_j]->GetMaximum()*1.05);
    lineU= new TLine(HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetFirst(),  HijF[SelectedElement_i][SelectedElement_j]->GetMaximum()*1.05, HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetLast(), HijF[SelectedElement_i][SelectedElement_j]->GetMaximum()*1.05);
    lineD= new TLine(0, 0, HijF[SelectedElement_i][SelectedElement_j]->GetXaxis()->GetXmax(), 0);
    if(maxElement_i>1 || maxElement_j>1){
    lineR->SetLineColor(kOrange+1);  // Setarea culorii liniei la roșu
    lineR->SetLineWidth(2);     // Setarea grosimii liniei
    lineR->Draw();
    lineL->SetLineColor(kOrange+1);  // Setarea culorii liniei la roșu
    lineL->SetLineWidth(2);     // Setarea grosimii liniei
    lineL->Draw();
    lineU->SetLineColor(kOrange+1);  // Setarea culorii liniei la roșu
    lineU->SetLineWidth(2);     // Setarea grosimii liniei
    lineU->Draw();
    lineD->SetLineColor(kOrange+1);  // Setarea culorii liniei la roșu
    lineD->SetLineWidth(2);     // Setarea grosimii liniei
    lineD->Draw();}
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
   // }
}

void QMainCanvas::IdentifyLastPilgrimHistogram(Double_t , Double_t ){
   // if(maxElement_i>1 || maxElement_j>1){
    pilgrimHisto=nullptr;
    delete lineR1;lineR1=nullptr;
    delete lineL1;lineL1=nullptr;
    delete lineD1;lineD1=nullptr;
    delete lineU1;lineU1=nullptr;

    for(int j=0;j<maxElement_j;j++){
        if(j*canvas->getCanvas()->GetWw()/maxElement_j<mousePilgrimX && mousePilgrimX<(j+1)*canvas->getCanvas()->GetWw()/maxElement_j){
            PilgrimElement_j=j+1;}}
    for(int h=0;h<maxElement_i;h++){
        if(h*canvas->getCanvas()->GetWh()/maxElement_i<mousePilgrimY && mousePilgrimY<(h+1)*canvas->getCanvas()->GetWh()/maxElement_i){
            PilgrimElement_i=h+1;}}
    pilgrimHisto=HijF[PilgrimElement_i][PilgrimElement_j];
    //std::cout<<"i="<<PilgrimElement_i<<"  j="<<PilgrimElement_j<<"\n";

    canvas->getCanvas()->cd((PilgrimElement_i-1)*maxElement_j+PilgrimElement_j);
    lineR1 = new TLine(HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetFirst(), 0, HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetFirst(), HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*1.05);
    lineL1= new TLine(HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetLast(), 0, HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetLast(), HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*1.05);
    lineU1= new TLine(HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetFirst(),  HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*1.05, HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetLast(), HijF[PilgrimElement_i][PilgrimElement_j]->GetMaximum()*1.05);
    lineD1= new TLine(0, 0, HijF[PilgrimElement_i][PilgrimElement_j]->GetXaxis()->GetXmax(), 0);
    if(maxElement_i>1 || maxElement_j>1){
    lineR1->SetLineColor(kGray+1);  // Setarea culorii liniei la roșu
    lineR1->SetLineWidth(0);     // Setarea grosimii liniei
    lineR1->Draw();
    lineL1->SetLineColor(kGray+1);  // Setarea culorii liniei la roșu
    lineL1->SetLineWidth(0);     // Setarea grosimii liniei
    lineL1->Draw();
    lineU1->SetLineColor(kGray+1);  // Setarea culorii liniei la roșu
    lineU1->SetLineWidth(0);     // Setarea grosimii liniei
    lineU1->Draw();
    lineD1->SetLineColor(kGray+1);  // Setarea culorii liniei la roșu
    lineD1->SetLineWidth(0);     // Setarea grosimii liniei
    lineD1->Draw();}
    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
  //  }
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
