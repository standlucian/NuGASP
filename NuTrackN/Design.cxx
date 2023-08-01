#include "canvas.h"
#include "Design.h"
#include <QPalette>
#include <TCanvas.h>
#include <TStyle.h>
#include <TColor.h>
void changeBackgroundColor(TCanvas* canvas) {
    // Change the background color of the canvas
    if (canvas) {
            //canvas->SetFillColor(kRed);
            canvas->SetFillColor(TColor::GetColor("#3F83B0"));

            TStyle* style = gStyle; // Get the current style
            style->SetPadGridX(1); // Show grid lines along the X-axis
            style->SetPadGridY(1); // Show grid lines along the Y-axis
            style->SetGridColor(kGray); // Set grid color to gray
            style->SetGridStyle(3); // Set grid line style

            style->SetNdivisions(540, "X");
            style->SetNdivisions(540, "Y");


            canvas->Update();
}
}
