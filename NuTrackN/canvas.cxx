#include "canvas.h"

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

   setFocusPolicy(Qt::StrongFocus);
}

//______________________________________________________________________________
void QRootCanvas::mouseMoveEvent(QMouseEvent *e)
{
   // Handle mouse move events.

   //This tells the canvas to handle events when the mouse moves and any or none of the mouse buttons are pressed. These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
   if (fCanvas) {
      if (e->buttons() & Qt::LeftButton) {
         fCanvas->HandleInput(kButton1Motion, e->x(), e->y());
      } else if (e->buttons() & Qt::MiddleButton) {
         fCanvas->HandleInput(kButton2Motion, e->x(), e->y());
      } else if (e->buttons() & Qt::RightButton) {
         fCanvas->HandleInput(kButton3Motion, e->x(), e->y());
      } else {
         fCanvas->HandleInput(kMouseMotion, e->x(), e->y());
      }
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
            fCanvas->HandleInput(kButton1Down, e->x(), e->y());
            break;
         case Qt::MiddleButton :
            fCanvas->HandleInput(kButton2Down, e->x(), e->y());
            break;
         case Qt::RightButton :
            // does not work properly on Linux...
            // ...adding setAttribute(Qt::WA_PaintOnScreen, true) 
            // seems to cure the problem
            fCanvas->HandleInput(kButton3Down, e->x(), e->y());
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
            fCanvas->HandleInput(kButton1Up, e->x(), e->y());
            break;
         case Qt::MiddleButton :
            fCanvas->HandleInput(kButton2Up, e->x(), e->y());
            break;
         case Qt::RightButton :
            // does not work properly on Linux...
            // ...adding setAttribute(Qt::WA_PaintOnScreen, true) 
            // seems to cure the problem
            fCanvas->HandleInput(kButton3Up, e->x(), e->y());
            break;
         default:
            break;
      }
   }
}

//______________________________________________________________________________

void QRootCanvas::keyPressEvent(QKeyEvent *event)
{
    //Checks if any of the top keys (C, M, Z) was pressed just before
    if(cKeyWasPressed)
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
            case Qt::Key_C:
                //if the C key is pressed after C, do nothing
                break;
            default:
                cKeyWasPressed=0;
                std::cout<<"Pressing the C key has called but no valid command arrived after it"<<std::endl;
                break;
        }
    }
    else
    {
        //Looks at what key was pressed and does different things depending on what was pressed
        switch(event->key())
        {
            case Qt::Key_Control:
                //If control is pressed, mark that down to later check if the mouse is clicked at the same time
                controlKeyIsPressed=1;
                break;
            case Qt::Key_C:
                //if the C key is pressed, mark that down and prepare to execute a command
                cKeyWasPressed=1;
                break;
            case Qt::Key_Return:
                //Pentru Petre
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

//------------------------------------------------------------------------------

//______________________________________________________________________________
QMainCanvas::QMainCanvas(QWidget *parent) : QWidget(parent)
{
   // QMainCanvas constructor.

   QVBoxLayout *l = new QVBoxLayout(this);

   //Adds the canvas to the window
   l->addWidget(canvas = new QRootCanvas(this));
   //Adds the button to the window
   l->addWidget(b = new QPushButton("&Draw Histogram", this));
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

   //connects the keyboard command C+I to the areaFunction;
   connect(canvas,SIGNAL(requestIntegrationNoBackground()), this, SLOT(areaFunction()));

   //connects the keyboard command C+J to the areaFunction;
   connect(canvas,SIGNAL(requestIntegrationWithBackground()), this, SLOT(areaFunctionWithBackground()));

   //connects the keyboard/mouse combination command Ctrl+Left Click to the autoFit function;
   connect(canvas,SIGNAL(autoFitRequested(int, int)), this, SLOT(autoFit(int, int)));

   fRootTimer = new QTimer( this );
   //Every 20 ms, call function handle_root_events()
   QObject::connect( fRootTimer, SIGNAL(timeout()), this, SLOT(handle_root_events()) );
   fRootTimer->start( 20 );

   //Creates the new TH1F histogram with 10240 bins. Why 10240? Because that's how many our test file has.
   h1f = new TracknHistogram("h1f","Test random numbers", 10240, 0, 10240);
}

//______________________________________________________________________________
void QMainCanvas::clicked1()
{
   // Handle the "Draw Histogram" button clicked() event.

   // Create a one dimensional histogram (one float per bin)
   canvas->getCanvas()->Clear();
   canvas->getCanvas()->cd();
   canvas->getCanvas()->SetBorderMode(0);
   canvas->getCanvas()->SetFillColor(0);
   canvas->getCanvas()->SetGrid();

   h1f->Reset();
   //This sets the color of the spectrum
   h1f->SetFillColor(kViolet + 2);
   h1f->SetFillStyle(3001);

   //opens a fixed file from the folder
   std::ifstream file("testSpectra", std::ios::in);
   if (!file) {
      std::cerr << "Error: could not open file" << std::endl;
   }

   // read the data into a vector
   std::vector<uint32_t> data;
   uint32_t value;
   while (file.read(reinterpret_cast<char*>(&value), sizeof(value))) {
       data.push_back(value);
   }

   //write the data into the histogram
   for(unsigned long int i=0;i<data.size();i++)
       h1f->AddBinContent(i,data[i]);

   //Draws the spectrum and tells the canvas to update itself
   h1f->Draw();
   canvas->getCanvas()->Modified();
   canvas->getCanvas()->Update();
}

void QMainCanvas::areaFunction()
{
   //For the function that calculates the integral we must give it some vectors of markers for the integral itself and the background
   //If the background markers vector is empty we will just do a common integral with no background
   std::vector<Double_t> integral_markers;
   std::vector<Double_t> background_markers;
   //The vector is populated with 4 markers
   integral_markers.push_back(300);
   integral_markers.push_back(400);
   integral_markers.push_back(500);
   integral_markers.push_back(600);
   //The integral function is called which will perform two integrals with no background since the background_markers vector is empty, in this case the best fitted line is y=0
   integral_function(h1f,integral_markers,background_markers,slope,addition);
}

void QMainCanvas::areaFunctionWithBackground()
{
   //For the function that calculates the integral we must give it some vectors of markers for the integral itself and the background
   std::vector<Double_t> integral_markers;
   std::vector<Double_t> background_markers;
   //The vectors are populated acordingly
   integral_markers.push_back(300);
   integral_markers.push_back(400);
   //The background markers are overlapping(used to test the function that checks for overlaps)
   background_markers.push_back(7000);
   background_markers.push_back(7020);
   background_markers.push_back(7010);
   background_markers.push_back(7030);
   //The integral function is called which will perform a single integral with a background and will also reutrn the equation of the best fotted line y=slope*x+addition
   integral_function(h1f,integral_markers,background_markers,slope,addition);
}

//______________________________________________________________________________

void QMainCanvas::autoFit(int x, int y)
{
    std::string objectInfo, temp;
    int from, to, binX, binC, sum;
    float xPos, yPos;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=h1f->GetObjectInfo(x,y);
    std::cout<<objectInfo<<std::endl;

    //Cut the first section, which represents the position on the x Axis of the click, in double precision float
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    xPos=std::stof(temp);

    //Cut the next section, which represents the position on the y Axis of the click
    objectInfo=objectInfo.substr(to+1);
    from=objectInfo.find("=");
    to=objectInfo.find(" ");
    temp=objectInfo.substr(from+1,to-from-2);
    yPos=std::stof(temp);

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
    sum=std::stoi(temp);
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
