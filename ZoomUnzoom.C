#include "TH1F.h"
#include "TCanvas.h"
#include "TROOT.h"

void zoomHistogram(TH1F* histogram, Double_t xmin, Double_t xmax, Double_t ymin, Double_t ymax) {
  TCanvas* canvas = histogram->GetCanvas();
  canvas->cd();

  // Set the new x and y ranges
  histogram->GetXaxis()->SetRangeUser(xmin, xmax);
  histogram->GetYaxis()->SetRangeUser(ymin, ymax);

  // Update the canvas to reflect the changes
  canvas->Update();
}

void unzoomHistogram(TH1F* histogram) {
  TCanvas* canvas = histogram->GetCanvas();
  canvas->cd();

  // Reset the x and y ranges to the full range
  histogram->GetXaxis()->UnZoom();
  histogram->GetYaxis()->UnZoom();

  // Update the canvas to reflect the changes
  canvas->Update();
}

void createHistogram() {
  // Create a ROOT histogram
  TH1F* histogram = new TH1F("histogram", "Histogram", 100, 0, 100);

  // Fill the histogram with some data (for demonstration purposes)
  for (int i = 0; i < 10000; ++i) {
    Double_t value = gRandom->Gaus(50, 10);
    histogram->Fill(value);
  }

  // Create a canvas to display the histogram
  TCanvas* canvas = new TCanvas("canvas", "Histogram Canvas", 800, 600);
  histogram->Draw();

  // Example usage: Zoom in on a specific region
  zoomHistogram(histogram, 30, 70, 0, 500);

  // Example usage: Unzoom to show the full range
  unzoomHistogram(histogram);
}

//implementation not working yet, I couldn't figure out why 
