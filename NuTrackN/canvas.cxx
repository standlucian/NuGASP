

#include "canvas.h"

//------------------------------------------------------------------------------
Double_t m=0,n=0;
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
}

//______________________________________________________________________________
void QRootCanvas::mouseMoveEvent(QMouseEvent *e)
{
   // Handle mouse move events.

   //This tells the canvas to handle events when the mouse moves and any or none of the mouse buttons are pressed. These are functions of the parent TCanvas class, we should look in the documentation to see what they do.
   if (fCanvas) {
      if (e->buttons() & Qt::LeftButton) {
         fCanvas->HandleInput(kButton1Motion, e->x(), e->y());
      } else if (e->buttons() & Qt::MidButton) {
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
         case Qt::MidButton :
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
            fCanvas->HandleInput(kButton1Up, e->x(), e->y());
            break;
         case Qt::MidButton :
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
   //Same as the previous line of code, it executes the function areaFunction when the button is clicked
   connect(b, SIGNAL(clicked()), this, SLOT(areaFunctionWithBackground()));
   //Same as the previous line of code, it adds a button to the window
   fRootTimer = new QTimer( this );
   //Every 20 ms, call function handle_root_events()
   QObject::connect( fRootTimer, SIGNAL(timeout()), this, SLOT(handle_root_events()) );
   fRootTimer->start( 20 );
   h1f = new TH1F("h1f","Test random numbers", 10240, 0, 10);

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

   //Creates the new TH1F histogram with 10240 bins. Why 10240? Because that's how many our test file has.

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
   for(int i=0;i<data.size();i++)
       h1f->AddBinContent(i,data[i]);

   //Draws the spectrum and tells the canvas to update itself
   h1f->Draw();
   canvas->getCanvas()->Modified();
   canvas->getCanvas()->Update();
}

void QMainCanvas::areaFunction()
{
   //Here are declared some variables which will be used to call the function from integral.cxx
   //The area variable is the integral itsel and the error of intragral is the error which is calculated
   Double_t error_of_integral = 0, area = integral_no_background(h1f,error_of_integral,300,400);
   //The integral and error are written in the terminal
   std::cout<<"Integral:"<<area<<" Error:"<<error_of_integral<<std::endl;
   
}

void QMainCanvas::areaFunctionWithBackground()
{
   std::vector<Double_t> backgroundMarkers;
   backgroundMarkers.push_back(7000);
   backgroundMarkers.push_back(7020);
   backgroundMarkers.push_back(8080);
   backgroundMarkers.push_back(9000);
   if(backgroundMarkers.size()%2==1)
   {
       std::cout<<"Background markers must be in pairs\n";
   }
   else
   {
       //Here are declared some variables which will be used to call the function from integral.cxx
       //The area variable is the integral itsel and the error of intragral is the error which is calculated
       Double_t error_of_integral = 0, area = integral_background(h1f,error_of_integral,300,400,backgroundMarkers,m,n);
       //The integral and error are written in the terminal
       std::cout<<"Integral:"<<area<<" Reduced chi2:"<<error_of_integral<<std::endl;
       static TH1F* line;
       line = new TH1F("line","Line", 10240, 0, 10);
       //update();
       line->Reset();
       //This sets the color of the spectrum
       //line->setColorStyle(kYellow);
       line->SetFillStyle(0);
       for(int i=0;i<10240;i++)
           line->AddBinContent(i,m*i+n);
       line->Draw("same");
       canvas->getCanvas()->Modified();
       canvas->getCanvas()->Update();
   }
}
/*
void QMainCanvas::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setPen(QPen(Qt::red, 10, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(0, m*0+n, 10240, 10240*m+n);
    p.end(); // Don't forget this line!
}
*/
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
