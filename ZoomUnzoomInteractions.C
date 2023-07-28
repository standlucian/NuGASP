#include <TApplication.h>
#include <TCanvas.h>
#include <TBox.h>
#include <TMarker.h>
#include <TEventList.h>
#include <TGClient.h>
#include <TVirtualX.h>
#include <TROOT.h>

// Function declarations
void Zoom(TCanvas* canvas, Double_t x1, Double_t y1, Double_t x2, Double_t y2);
void Unzoom(TCanvas* canvas);
void Marker(TCanvas* canvas, Double_t x, Double_t y);
void ZoomBetweenMarkers(TCanvas* canvas);
void ZoomToCursorPosition(TCanvas* canvas);
void NavigateRight(TCanvas* canvas);
void NavigateLeft(TCanvas* canvas);

// Global variables to store marker positions
Double_t markerX1 = 0.0;
Double_t markerY1 = 0.0;
Double_t markerX2 = 0.0;
Double_t markerY2 = 0.0;
Bool_t markerPlaced = kFALSE;

// Function to handle key events
Bool_t HandleKeys(Event_t* event)
{
    // Convert the event to a TGClient's key symbol
    Int_t keysym = gVirtualX->KeysymToKeycode(event);
    // Check if SHIFT key is pressed
    Bool_t shiftPressed = event->fState & kKeyShiftMask;

    TCanvas* canvas = (TCanvas*)gPad;

    switch (keysym)
    {
    case kKey_Space:
        Marker(canvas, canvas->GetEventX(), canvas->GetEventY());
        break;
    case kKey_e:
        ZoomBetweenMarkers(canvas);
        break;
    case kKey_x:
        ZoomToCursorPosition(canvas);
        break;
    case kKey_greater:
        if (shiftPressed)
            NavigateRight(canvas);
        break;
    case kKey_less:
        if (shiftPressed)
            NavigateLeft(canvas);
        break;
    default:
        break;
    }

    return kTRUE;
}

// Function to handle mouse clicks
Bool_t HandleMouseClick(Event_t* event, Int_t px, Int_t py)
{
    // Check if CTRL key is pressed
    Bool_t ctrlPressed = event->fState & kKeyControlMask;

    TCanvas* canvas = (TCanvas*)gPad;

    if (ctrlPressed)
    {
        if (!markerPlaced)
        {
            Marker(canvas, px, py);
        }
        else
        {
            Zoom(canvas, markerX1, markerY1, px, py);
            markerPlaced = kFALSE;
        }
    }

    return kTRUE;
}

int main(int argc, char** argv)
{
    TApplication theApp("App", &argc, argv);
    TCanvas* canvas = new TCanvas("canvas", "ROOT Canvas", 800, 600);
    
    // Enable handling of key events
    canvas->Connect("ProcessedEvent(Event_t*, Int_t, Int_t, TObject*)", "HandleKeys(Event_t*)", 0, true);
    // Enable handling of mouse clicks
    canvas->Connect("ProcessedEvent(Event_t*, Int_t, Int_t, TObject*)", "HandleMouseClick(Event_t*, Int_t, Int_t)", 0, true);

    // Your data or histograms go here

    theApp.Run();
    return 0;
}

void Zoom(TCanvas* canvas, Double_t x1, Double_t y1, Double_t x2, Double_t y2)
{
    canvas->Range(x1, y1, x2, y2);
    canvas->Update();
}

void Unzoom(TCanvas* canvas)
{
    canvas->Range();
    canvas->Update();
}

void Marker(TCanvas* canvas, Double_t x, Double_t y)
{
    if (!markerPlaced)
    {
        markerX1 = x;
        markerY1 = y;
        markerPlaced = kTRUE;
    }
    else
    {
        markerX2 = x;
        markerY2 = y;
    }

    TMarker* marker = new TMarker(x, y, 20);
    marker->SetMarkerColor(kRed);
    marker->SetMarkerSize(1.5);
    marker->Draw();
    canvas->Update();
}

void ZoomBetweenMarkers(TCanvas* canvas)
{
    if (markerPlaced)
    {
        Zoom(canvas, markerX1, markerY1, markerX2, markerY2);
    }
}

void ZoomToCursorPosition(TCanvas* canvas)
{
    Zoom(canvas, canvas->GetEventX() - 0.1, canvas->GetEventY() - 0.1,
         canvas->GetEventX() + 0.1, canvas->GetEventY() + 0.1);
}

void NavigateRight(TCanvas* canvas)
{
    canvas->Range(canvas->GetUxmin() + 0.1, canvas->GetUymin(), canvas->GetUxmax() + 0.1, canvas->GetUymax());
    canvas->Update();
}

void NavigateLeft(TCanvas* canvas)
{
    canvas->Range(canvas->GetUxmin() - 0.1, canvas->GetUymin(), canvas->GetUxmax() - 0.1, canvas->GetUymax());
    canvas->Update();
}
