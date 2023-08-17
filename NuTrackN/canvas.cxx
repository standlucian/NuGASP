#include <QPushButton>
#include <QLayout>
#include <QTimer>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>

#include <stdlib.h>

#include <TCanvas.h>
#include <TVirtualX.h>
#include <TSystem.h>
#include <TFormula.h>
#include <TF1.h>
#include <TH1.h>
#include <TFrame.h>
#include <TTimer.h>
#include "canvas.h"

//------------------------------------------------------------------------------
static TH1F *h1f = 0;

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
   fRootTimer = new QTimer( this );
   //Every 20 ms, call function handle_root_events()
   QObject::connect( fRootTimer, SIGNAL(timeout()), this, SLOT(handle_root_events()) );
   fRootTimer->start( 20 );

   l->addWidget(FindPeak = new QPushButton("&Find Peak", this));
   //When the button is pressed, execute function clicked
   connect(FindPeak, SIGNAL(clicked()), this, SLOT(ClickedFindPeak()));
   fRootTimer = new QTimer( this );
   //Every 20 ms, call function handle_root_events()
   QObject::connect( fRootTimer, SIGNAL(timeout()), this, SLOT(handle_root_events()) );
   fRootTimer->start( 20 );
}

//______________________________________________________________________________
void QMainCanvas::clicked1()
{
   // Handle the "Draw Histogram" button clicked() event.

   //static TH1F *h1f = 0;

   // Create a one dimensional histogram (one float per bin)
   canvas->getCanvas()->Clear();
   canvas->getCanvas()->cd();
   canvas->getCanvas()->SetBorderMode(0);
   canvas->getCanvas()->SetFillColor(0);
   canvas->getCanvas()->SetGrid();

   //Creates the new TH1F histogram with 10240 bins. Why 10240? Because that's how many our test file has.
   if (h1f == 0) {
      h1f = new TH1F("h1f","Test random numbers", 10240, 0, 10);
   }
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

void QMainCanvas::ClickedFindPeak()
{
    double inputEnergy;
    int numBinsX = h1f->GetXaxis()->GetNbins();
        int nearestBin = -1;
        double minDistance = DBL_MAX;

        // Find the bin with the nearest energy to the input energy
        for (int bin = 1; bin <= numBinsX; ++bin)
        {
            double binCenter = h1f->GetXaxis()->GetBinCenter(bin);
            double distance = fabs(binCenter - inputEnergy);

            if (distance < minDistance)
            {
                minDistance = distance;
                nearestBin = bin;
            }
        }

        double nearestBinCenter = h1f->GetXaxis()->GetBinCenter(nearestBin);
        double nearestBinContent = h1f->GetBinContent(nearestBin);

        // Print the results
        std::cout << "Input Energy: " << inputEnergy << std::endl;
        std::cout << "Nearest Bin Center: " << nearestBinCenter << " (Bin " << nearestBin << ")" << std::endl;
        std::cout << "Nearest Bin Content: " << nearestBinContent << std::endl;

}



