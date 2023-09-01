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
         xMousePosition=e->x();
         yMousePosition=e->y();
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
                std::cout<<"Waited for execute command after C was pressed but no valid command arrived after it"<<std::endl;
                break;
        }
        cKeyWasPressed=0;
    }
    else if(zKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_B:
                //If B key is pressed after Z, call delete background markers
                emit requestDeleteBackgroundMarkers();
                break;
            case Qt::Key_R:
                //If B key is pressed after Z, call delete background markers
                emit requestDeleteRangeMarkers();
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
                break;
        }
        zKeyWasPressed=0;
    }
    else if(mKeyWasPressed)
    {
        switch(event->key())
        {
            case Qt::Key_B:
                //If B key is pressed after M, call show background markers
                emit requestShowBackgroundMarkers();
                break;
            case Qt::Key_R:
                //If B key is pressed after Z, call delete background markers
                emit requestShowRangeMarkers();
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
                break;
        }
        mKeyWasPressed=0;
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
            case Qt::Key_Z:
                //if the C key is pressed, mark that down and prepare to execute a command
                zKeyWasPressed=1;
                break;
            case Qt::Key_M:
                //if the C key is pressed, mark that down and prepare to execute a command
                mKeyWasPressed=1;
                break;
            case Qt::Key_B:
                //if the B key is pressed, add a background marker on screen and remember the background position
                emit addBackgroundMarkerRequested(xMousePosition, yMousePosition);
                break;
            case Qt::Key_R:
                //if the R key is pressed, add a range marker on screen and remember the range position
                emit requestAddRangeMarker(xMousePosition, yMousePosition);
                break;
            case Qt::Key_Equal:
                //if the = key is pressed, clear the screen of everything except the histogram
                emit requestClearTheScreen();
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

   //connects the keyboard command = to clearing the screen;
   connect(canvas,SIGNAL(requestClearTheScreen()), this, SLOT(clearTheScreen()));

   //connects the keyboard command B to adding the background markers;
   connect(canvas,SIGNAL(addBackgroundMarkerRequested(Int_t, Int_t)), this, SLOT(addBackgroundMarker(Int_t, Int_t)));

   //connects the keyboard command Z+B to clearing the background markers;
   connect(canvas,SIGNAL(requestDeleteBackgroundMarkers()), this, SLOT(deleteBackgroundMarkers()));

   //connects the keyboard command Z+A to clearing all markers;
   connect(canvas,SIGNAL(requestDeleteAllMarkers()), this, SLOT(deleteAllMarkers()));

   //connects the keyboard command M+B to clearing the background markers;
   connect(canvas,SIGNAL(requestShowBackgroundMarkers()), this, SLOT(showBackgroundMarkers()));

   //connects the keyboard command M+A to clearing all markers;
   connect(canvas,SIGNAL(requestShowAllMarkers()), this, SLOT(showAllMarkers()));

   //connects the keyboard command R to adding a range marker;
   connect(canvas,SIGNAL(requestAddRangeMarker(Int_t, Int_t)), this, SLOT(addRangeMarker(Int_t, Int_t)));

   //connects the keyboard command Z+R to clearing the range markers;
   connect(canvas,SIGNAL(requestDeleteRangeMarkers()), this, SLOT(deleteRangeMarkers()));

   //connects the keyboard command M+R to clearing the range markers;
   connect(canvas,SIGNAL(requestShowRangeMarkers()), this, SLOT(showRangeMarkers()));

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
   for(unsigned long int i=1;i<=data.size();i++)
       h1f->AddBinContent(i,data[i-1]);

   maxValueInHistogram=h1f->GetBinContent(h1f->GetMaximumBin());

   //Draws the spectrum and tells the canvas to update itself
   h1f->Draw();
   canvas->getCanvas()->Modified();
   canvas->getCanvas()->Update();
}

void QMainCanvas::areaFunction()
{
   //For the function that calculates the integral we must give it some vectors of markers for the integral itself and the background
   //If the background markers vector is empty we will just do a common integral with no background

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
    Double_t xPos, yPos;
    Double_t gaussianHeight, gaussianCenter, gaussianSigma, bkgSlope, bkg0, gaussianFWHM, gaussianCenterError, gaussianIntegral, gaussianIntegralError, gaussianFWHMError;
    std::ostringstream tempStringStream;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=h1f->GetObjectInfo(x,y);

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
    TFitResultPtr fitResult = h1f->Fit(gaussianWithBackgroundFunction,"QMRS", "same");

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

        fitResult = h1f->Fit(gaussianWithBackgroundFunction,"QMRS", "");

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

    //Make list of objects that have been drawn to delete them later
    listOfObjectsDrawnOnScreen.Add(gaussianWithBackgroundFunction);
    listOfObjectsDrawnOnScreen.Add(backgroundFunction);
}

//______________________________________________________________________________
Double_t QMainCanvas::findMinValueInInterval(int intervalStart, int intervalFinish)
{
    int minValueFound=h1f->GetBinContent(intervalStart);

    for(int i=intervalStart+1; i<=intervalFinish; i++)
    {
        if(h1f->GetBinContent(i)<minValueFound)
            minValueFound=h1f->GetBinContent(i);
    }
    return minValueFound;
}

//______________________________________________________________________________
void QMainCanvas::addBackgroundMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=h1f->GetObjectInfo(x,y);

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
    background_markers.push_back(binX);

    //Create a blue background line and add it to the screen
    TLine *backgroundLine = new TLine(binX-0.5, 0., binX-0.5, maxValueInHistogram*1.05);
    backgroundLine->SetLineColor(kBlue);
    backgroundLine->SetLineWidth(2);

    backgroundLine->Draw("same");

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(backgroundLine);
}

//______________________________________________________________________________
void QMainCanvas::clearTheScreen()
{
    //Get a list of all the functions on the histogram and delete all except the first (which is the histogram itself)

    while(h1f->GetListOfFunctions()->GetSize()>1)
    {
        h1f->GetListOfFunctions()->RemoveLast();
    }

    //Get a list of all the functions drawn on screen and delete all of them!
    while(listOfObjectsDrawnOnScreen.GetSize())
    {
         listOfObjectsDrawnOnScreen.Last()->Delete();
         listOfObjectsDrawnOnScreen.RemoveLast();
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::deleteBackgroundMarkers()
{
    background_markers.clear();
}

//______________________________________________________________________________
void QMainCanvas::deleteAllMarkers()
{
    deleteBackgroundMarkers();
    deleteRangeMarkers();
}

//______________________________________________________________________________
void QMainCanvas::showBackgroundMarkers()
{
    for(uint i=0;i<background_markers.size();i++)
    {
        TLine *backgroundLine = new TLine(background_markers[i]-0.5, 0., background_markers[i]-0.5, maxValueInHistogram*1.05);
        backgroundLine->SetLineColor(kBlue);
        backgroundLine->SetLineWidth(2);

        backgroundLine->Draw("same");

        listOfObjectsDrawnOnScreen.Add(backgroundLine);
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//______________________________________________________________________________
void QMainCanvas::showAllMarkers()
{
    showBackgroundMarkers();
    showRangeMarkers();
}

void QMainCanvas::addRangeMarker(Int_t x, Int_t y)
{
    std::string objectInfo, temp;
    int from, to, binX;

    //Finding to what Histogram info the click location corresponds to, returned to us as a string with 5 numerical values
    objectInfo=h1f->GetObjectInfo(x,y);

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
    TLine *rangeLine = new TLine(binX-0.5, 0., binX-0.5, maxValueInHistogram*1.05);
    rangeLine->SetLineColor(kYellow);
    rangeLine->SetLineWidth(2);

    rangeLine->Draw("same");

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();

    //Add the line to the list of things put on the screen, so it can be deleted
    listOfObjectsDrawnOnScreen.Add(rangeLine);
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
        TLine *rangeLine = new TLine(range_markers[i]-0.5, 0., range_markers[i]-0.5, maxValueInHistogram*1.05);
        rangeLine->SetLineColor(kYellow);
        rangeLine->SetLineWidth(2);

        rangeLine->Draw("same");

        listOfObjectsDrawnOnScreen.Add(rangeLine);
    }

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
