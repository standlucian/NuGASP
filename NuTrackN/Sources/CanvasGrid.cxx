#include "canvas.h"
#include "tracknhistogram.h"
#include "Design.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TLine.h>
#include <TVirtualPad.h>
#include <TAxis.h>
#include <TColor.h>
#include <TStyle.h>
#include <TFrame.h>
#include <QWindowStateChangeEvent>
#include <QString>
#include <QLabel>

#include <cmath>
#include <algorithm>
#include <string>

//==============================================================================
// QMainCanvas::changeEvent
//==============================================================================
// Handles Qt window state change events (minimize, maximize, restore).
// Resizes and updates the ROOT TCanvas when the window is maximized or restored
// so that the sub-pads and spectra scale correctly with the window geometry.
//==============================================================================
void QMainCanvas::changeEvent(QEvent *e)
{
    if (e && e->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *event = static_cast<QWindowStateChangeEvent*>(e);
        if ((event->oldState() & Qt::WindowMaximized) ||
            (event->oldState() & Qt::WindowMinimized) ||
            (event->oldState() == Qt::WindowNoState && this->windowState() == Qt::WindowMaximized)) {
            if (canvas && canvas->getCanvas()) {
                canvas->getCanvas()->Resize();
                canvas->getCanvas()->Update();
            }
        }
    }
    QWidget::changeEvent(e);
}

//==============================================================================
// QMainCanvas::AddCulomn
//==============================================================================
// Adds a new column of sub-pads to the canvas grid (shortcut: Ctrl + Right).
// Re-divides the ROOT TCanvas into (maxElement_j + 1) columns by maxElement_i
// rows. Existing histograms and overlaid spectra are safely re-rendered into
// their corresponding pads, and a new TracknHistogram is initialized for the
// newly added column.
//==============================================================================
void QMainCanvas::AddCulomn()
{
    // HijF is dimensioned [12][12]; indices range from 1 to 10 safely
    if (maxElement_j >= 10) return;
    if (!canvas || !canvas->getCanvas()) return;

    TracknHistogram *sourceHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!sourceHist) {
        sourceHist = dynamic_cast<TracknHistogram*>(HijF[1][1]);
    }

    maxElement_j++;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";

            if (g == maxElement_j) {
                // Initialize the new column cell as a clean TracknHistogram
                if (sourceHist) {
                    HijF[z][g] = new TracknHistogram(*sourceHist);
                    HijF[z][g]->SetName(histTitle.c_str());
                    HijF[z][g]->SetTitle("");
                    HijF[z][g]->SetLineColor(4);
                    while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                        HijF[z][g]->GetListOfFunctions()->RemoveLast();
                    }
                } else {
                    HijF[z][g] = new TracknHistogram(histTitle.c_str(), "", 10240, 0, 10240);
                    HijF[z][g]->SetLineColor(4);
                }
                for (auto *h : HijC[z][g]) {
                    delete h;
                }
                HijC[z][g].clear();
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render Peak Search tick marks and labels
            renderPeakSearchLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }
    }

    // Automatically make the newly created column active
    SelectedElement_j = maxElement_j;
    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    ColorTheFrameOfTheHistogram();
    updateAxisStatusLabels();

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::AddLine
//==============================================================================
// Adds a new row of sub-pads to the canvas grid (shortcut: Ctrl + Up).
// Re-divides the ROOT TCanvas into maxElement_j columns by (maxElement_i + 1)
// rows. Existing histograms and overlaid spectra are safely re-rendered into
// their corresponding pads, and a new TracknHistogram is initialized for the
// newly added row.
//==============================================================================
void QMainCanvas::AddLine()
{
    // HijF is dimensioned [12][12]; indices range from 1 to 10 safely
    if (maxElement_i >= 10) return;
    if (!canvas || !canvas->getCanvas()) return;

    TracknHistogram *sourceHist = dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
    if (!sourceHist) {
        sourceHist = dynamic_cast<TracknHistogram*>(HijF[1][1]);
    }

    maxElement_i++;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";

            if (z == maxElement_i) {
                // Initialize the new row cell as a clean TracknHistogram
                if (sourceHist) {
                    HijF[z][g] = new TracknHistogram(*sourceHist);
                    HijF[z][g]->SetName(histTitle.c_str());
                    HijF[z][g]->SetTitle("");
                    HijF[z][g]->SetLineColor(4);
                    while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                        HijF[z][g]->GetListOfFunctions()->RemoveLast();
                    }
                } else {
                    HijF[z][g] = new TracknHistogram(histTitle.c_str(), "", 10240, 0, 10240);
                    HijF[z][g]->SetLineColor(4);
                }
                for (auto *h : HijC[z][g]) {
                    delete h;
                }
                HijC[z][g].clear();
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render Peak Search tick marks and labels
            renderPeakSearchLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }
    }

    // Automatically make the newly created row active
    SelectedElement_i = maxElement_i;
    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    ColorTheFrameOfTheHistogram();
    updateAxisStatusLabels();

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::IdentifyLastClickedHistogram
//==============================================================================
// Identifies which sub-pad cell in the (row, column) grid was selected by the
// user via a left mouse click at pixel coordinates (x, y).
// Updates SelectedElement_i, SelectedElement_j, and selectedHisto, then highlights
// the active histogram with an azure border frame.
//==============================================================================
void QMainCanvas::IdentifyLastClickedHistogram(Double_t x, Double_t y)
{
    mouseLeftClickXcoord = x;
    mouseLeftClickYcoord = y;

    if (!canvas || !canvas->getCanvas()) return;
    if (maxElement_i <= 0 || maxElement_j <= 0) return;

    const UInt_t canvasWidth = canvas->getCanvas()->GetWw();
    const UInt_t canvasHeight = canvas->getCanvas()->GetWh();
    if (canvasWidth == 0 || canvasHeight == 0) return;

    for (int j = 0; j < maxElement_j; ++j) {
        if (j * canvasWidth / maxElement_j <= mouseLeftClickXcoord &&
            mouseLeftClickXcoord < (j + 1) * canvasWidth / maxElement_j) {
            SelectedElement_j = j + 1;
            break;
        }
    }

    for (int h = 0; h < maxElement_i; ++h) {
        if (h * canvasHeight / maxElement_i <= mouseLeftClickYcoord &&
            mouseLeftClickYcoord < (h + 1) * canvasHeight / maxElement_i) {
            SelectedElement_i = h + 1;
            break;
        }
    }

    SelectedElement_j = std::max(1, std::min(SelectedElement_j, maxElement_j));
    SelectedElement_i = std::max(1, std::min(SelectedElement_i, maxElement_i));

    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];

    ColorTheFrameOfTheHistogram();
}

//==============================================================================
// QMainCanvas::IdentifyLastPilgrimHistogram
//==============================================================================
// Tracks the current cursor position as the mouse moves across the canvas.
// Determines which histogram grid pad the cursor is currently hovering over
// and updates PilgrimElement_i and PilgrimElement_j accordingly.
//==============================================================================
void QMainCanvas::IdentifyLastPilgrimHistogram(Double_t x, Double_t y)
{
    mousePilgrimX = x;
    mousePilgrimY = y;

    if (!canvas || !canvas->getCanvas()) return;
    if (maxElement_i <= 0 || maxElement_j <= 0) return;

    const UInt_t canvasWidth = canvas->getCanvas()->GetWw();
    const UInt_t canvasHeight = canvas->getCanvas()->GetWh();
    if (canvasWidth == 0 || canvasHeight == 0) return;

    for (int j = 0; j < maxElement_j; ++j) {
        if (j * canvasWidth / maxElement_j <= mousePilgrimX &&
            mousePilgrimX < (j + 1) * canvasWidth / maxElement_j) {
            PilgrimElement_j = j + 1;
            break;
        }
    }

    for (int h = 0; h < maxElement_i; ++h) {
        if (h * canvasHeight / maxElement_i <= mousePilgrimY &&
            mousePilgrimY < (h + 1) * canvasHeight / maxElement_i) {
            PilgrimElement_i = h + 1;
            break;
        }
    }

    PilgrimElement_j = std::max(1, std::min(PilgrimElement_j, maxElement_j));
    PilgrimElement_i = std::max(1, std::min(PilgrimElement_i, maxElement_i));

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::ColorTheFrameOfTheHistogram
//==============================================================================
// Draws an azure border frame around the currently selected histogram sub-pad
// when multiple sub-pads are visible (maxElement_i > 1 or maxElement_j > 1),
// visually indicating which pad has active keyboard and analysis focus.
//==============================================================================
void QMainCanvas::ColorTheFrameOfTheHistogram()
{
    delete lineR; lineR = nullptr;
    delete lineL; lineL = nullptr;
    delete lineD; lineD = nullptr;
    delete lineU; lineU = nullptr;

    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);

    if (maxElement_i > 1 || maxElement_j > 1) {
        const Double_t xFirst = hist->GetXaxis()->GetFirst() - 1;
        const Double_t xLast  = hist->GetXaxis()->GetLast();
        const Double_t xMax   = hist->GetXaxis()->GetXmax();
        const Double_t yMax   = hist->GetMaximum();

        TVirtualPad *pad = gPad;
        const bool isLog = (pad && pad->GetLogy() != 0);
        const Double_t yMin = isLog ? (hist->GetMinimum() > 0.0 ? hist->GetMinimum() : 0.5) : 0.0;

        lineR = new TLine(xFirst, yMin, xFirst, yMax);
        lineL = new TLine(xLast, yMin, xLast, yMax);
        lineU = new TLine(hist->GetXaxis()->GetFirst(), yMax, xLast, yMax);
        lineD = new TLine(0.0, yMin, xMax, yMin);

        const Color_t frameColor = kAzure + 1;
        const Width_t frameWidth = 4;

        lineR->SetLineColor(frameColor);
        lineR->SetLineWidth(frameWidth);
        lineR->Draw();

        lineL->SetLineColor(frameColor);
        lineL->SetLineWidth(frameWidth);
        lineL->Draw();

        lineU->SetLineColor(frameColor);
        lineU->SetLineWidth(frameWidth);
        lineU->Draw();

        lineD->SetLineColor(frameColor);
        lineD->SetLineWidth(frameWidth);
        lineD->Draw();
    }

    canvas->getCanvas()->Modified();
    canvas->getCanvas()->Update();
}

//==============================================================================
// QMainCanvas::showXYcoord
//==============================================================================
// Extracts the spectrum channel (binX) and count value (binC) at the cursor
// position (x, y) via ROOT GetObjectInfo() and updates the UI status labels
// (labelX and labelY) in real time.
//==============================================================================
void QMainCanvas::showXYcoord(Double_t x, Double_t y)
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    canvas->getCanvas()->cd((SelectedElement_i - 1) * maxElement_j + SelectedElement_j);
    const std::string objectInfo = hist->GetObjectInfo(static_cast<Int_t>(x), static_cast<Int_t>(y));

    int binX = 0;
    double binC = 0.0;

    // Parse "binx=..."
    const size_t binxPos = objectInfo.find("binx=");
    const size_t bincPos = objectInfo.find(" binc=");
    if (binxPos != std::string::npos && bincPos != std::string::npos && bincPos > binxPos + 5) {
        try {
            binX = std::stoi(objectInfo.substr(binxPos + 5, bincPos - binxPos - 5));
        } catch (...) {
            binX = 0;
        }
    }

    // Parse "binc=..."
    if (bincPos != std::string::npos) {
        const size_t sumPos = objectInfo.find(" Sum=", bincPos);
        const size_t startVal = bincPos + 6;
        try {
            if (sumPos != std::string::npos && sumPos > startVal) {
                binC = std::stod(objectInfo.substr(startVal, sumPos - startVal));
            } else if (startVal < objectInfo.length()) {
                binC = std::stod(objectInfo.substr(startVal));
            }
        } catch (...) {
            binC = 0.0;
        }
    }

    QString countsStr;
    if (std::abs(binC) <= 100000.0) {
        if (std::abs(binC - std::round(binC)) < 1e-5) {
            countsStr = QString::number(static_cast<long long>(std::round(binC)));
        } else {
            countsStr = QString::number(binC, 'g', 6);
        }
    } else {
        countsStr = QString::number(binC, 'e', 2);
    }

    if (labelChannel) {
        labelChannel->setText(QString("Channel: %1").arg(binX));
    }
    if (labelEnergy) {
        TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(hist);
        if (tHist && tHist->IsCalibrated()) {
            double energy = tHist->ChannelToEnergy(static_cast<Double_t>(binX));
            labelEnergy->setText(QString("Energy: %1").arg(QString::number(energy, 'f', 2)));
        } else {
            labelEnergy->setText("Energy: 0.0");
        }
    }
    if (labelCounts) {
        labelCounts->setText(QString("Counts: %1").arg(countsStr));
    }
    if (labelCursorY) {
        labelCursorY->setText(QString("Y: %1").arg(QString::number(y, 'f', 2)));
    }

    if (labelX && labelX != labelChannel) {
        labelX->setText(QString::number(binX));
    }
    if (labelY && labelY != labelCounts) {
        labelY->setText(countsStr);
    }
}

//==============================================================================
// QMainCanvas::DeleteCulomn
//==============================================================================
// Removes the rightmost column of sub-pads from the canvas grid (shortcut: Ctrl + Left).
// Re-divides the ROOT TCanvas into (maxElement_j - 1) columns by maxElement_i rows,
// frees resources belonging to the removed column, and re-renders remaining spectra.
//==============================================================================
void QMainCanvas::DeleteCulomn()
{
    if (maxElement_j <= 1) return;
    if (!canvas || !canvas->getCanvas()) return;

    const int deletedCol = maxElement_j;
    maxElement_j--;

    // Keep selection within valid bounds
    if (SelectedElement_j > maxElement_j) {
        SelectedElement_j = maxElement_j;
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            rootCanvas->cd(padIndex);
            adjustYAxisToVisibleMax(HijF[z][g], z, g);
            HijF[z][g]->Draw();

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    rootCanvas->cd(padIndex);
                    obj->Draw("SAME");
                }
            }
        }

        // Clean up resources for the deleted column
        for (auto *hist : HijC[z][deletedCol]) {
            delete hist;
        }
        HijC[z][deletedCol].clear();
        gaussCenters[z][deletedCol].clear();
        gaussCentersHeight[z][deletedCol].clear();
        peakSearchCenters[z][deletedCol].clear();
        peakSearchHeights[z][deletedCol].clear();
        for (auto *obj : peakSearchPrimitives[z][deletedCol]) {
            delete obj;
        }
        peakSearchPrimitives[z][deletedCol].clear();
        for (auto *obj : autoFitMarkers[z][deletedCol]) {
            delete obj;
        }
        autoFitMarkers[z][deletedCol].clear();
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::DeleteLine
//==============================================================================
// Removes the bottom row of sub-pads from the canvas grid (shortcut: Ctrl + Down).
// Re-divides the ROOT TCanvas into maxElement_j columns by (maxElement_i - 1) rows,
// frees resources belonging to the removed row, and re-renders remaining spectra.
//==============================================================================
void QMainCanvas::DeleteLine()
{
    if (maxElement_i <= 1) return;
    if (!canvas || !canvas->getCanvas()) return;

    const int deletedRow = maxElement_i;
    maxElement_i--;

    // Keep selection within valid bounds
    if (SelectedElement_i > maxElement_i) {
        SelectedElement_i = maxElement_i;
        selectedHisto = HijF[SelectedElement_i][SelectedElement_j];
    }

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);

    if (maxElement_i == 1 && maxElement_j == 1) {
        if (HijF[1][1]) {
            HijF[1][1]->Draw();
        }
        // Re-render Gaussian center annotations
        renderPeakLabels(1, 1);
        for (std::size_t k = 0; k < HijC[1][1].size(); ++k) {
            if (HijC[1][1][k]) {
                if (k < colors_hist.size()) {
                    HijC[1][1][k]->SetLineColor(colors_hist[k]);
                }
                HijC[1][1][k]->Draw("SAME");
            }
        }
        for (auto *obj : autoFitMarkers[1][1]) {
            if (obj) obj->Draw("SAME");
        }
    } else {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

        for (int z = 1; z <= maxElement_i; ++z) {
            for (int g = 1; g <= maxElement_j; ++g) {
                if (!HijF[z][g]) continue;

                const int padIndex = (z - 1) * maxElement_j + g;
                rootCanvas->cd(padIndex);
                HijF[z][g]->Draw();

                // Re-render Gaussian center annotations
                renderPeakLabels(z, g);

                // Re-render overlaid comparison spectra
                for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                    if (HijC[z][g][k]) {
                        if (k < colors_hist.size()) {
                            HijC[z][g][k]->SetLineColor(colors_hist[k]);
                        }
                        HijC[z][g][k]->Draw("SAME");
                    }
                }

                // Re-render auto-fit markers (fit curves, background lines)
                for (auto *obj : autoFitMarkers[z][g]) {
                    if (obj) {
                        rootCanvas->cd(padIndex);
                        obj->Draw("SAME");
                    }
                }
            }
        }
    }

    // Clean up resources for the deleted row
    for (int g = 1; g < 12; ++g) {
        for (auto *hist : HijC[deletedRow][g]) {
            delete hist;
        }
        HijC[deletedRow][g].clear();
        gaussCenters[deletedRow][g].clear();
        gaussCentersHeight[deletedRow][g].clear();
        peakSearchCenters[deletedRow][g].clear();
        peakSearchHeights[deletedRow][g].clear();
        for (auto *obj : peakSearchPrimitives[deletedRow][g]) {
            delete obj;
        }
        peakSearchPrimitives[deletedRow][g].clear();
        for (auto *obj : autoFitMarkers[deletedRow][g]) {
            delete obj;
        }
        autoFitMarkers[deletedRow][g].clear();
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
}

//==============================================================================
// QMainCanvas::RefreshScreen
//==============================================================================
// Completely redraws and updates all spectrum pads and overlaid objects
// in the current grid configuration (shortcut: '=').
// Clears the canvas, re-divides into maxElement_j by maxElement_i pads,
// and re-plots all base histograms, overlays, Gaussian centroid labels,
// and fit curves.
//==============================================================================
void QMainCanvas::RefreshScreen()
{
    if (!canvas || !canvas->getCanvas()) return;

    TCanvas *rootCanvas = canvas->getCanvas();
    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);

    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    const Color_t axisCol = (Design::getGraphBackgroundColor().lightness() > 130) ? kBlack : kWhite;

    gStyle->SetCanvasColor(rootBg);
    gStyle->SetPadColor(rootBg);
    gStyle->SetFrameFillColor(rootBg);
    gStyle->SetFrameLineColor(axisCol);

    rootCanvas->SetFillColor(rootBg);

    if (maxElement_i > 1 || maxElement_j > 1) {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);
    } else {
        rootCanvas->cd();
        if (gPad) {
            gPad->SetFillColor(rootBg);
            gPad->SetFrameFillColor(rootBg);
        }
    }

    bool anyHist = false;
    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (HijF[z][g]) { anyHist = true; break; }
        }
        if (anyHist) break;
    }

    if (!anyHist) {
        rootCanvas->SetFillColor(rootBg);
        if (gPad) {
            gPad->SetFillColor(rootBg);
            gPad->SetFrameFillColor(rootBg);
        }
        TLatex *l = new TLatex();
        l->SetTextSize(0.15);
        l->SetTextAlign(22);
        l->SetTextColor(axisCol);
        l->DrawLatex(0.5, 0.5, "NuTrackN");
        rootCanvas->Modified();
        rootCanvas->Update();
        canvas->update();
        return;
    }

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            if (maxElement_i > 1 || maxElement_j > 1) {
                rootCanvas->cd(padIndex);
            } else {
                rootCanvas->cd();
            }

            if (gPad) {
                gPad->SetFillColor(rootBg);
                gPad->SetFrameFillColor(rootBg);
            }

            adjustYAxisToVisibleMax(HijF[z][g], z, g);

            HijF[z][g]->GetXaxis()->SetAxisColor(axisCol);
            HijF[z][g]->GetXaxis()->SetLabelColor(axisCol);
            HijF[z][g]->GetYaxis()->SetAxisColor(axisCol);
            HijF[z][g]->GetYaxis()->SetLabelColor(axisCol);
            HijF[z][g]->Draw();

            if (gPad && gPad->GetFrame()) {
                gPad->GetFrame()->SetFillColor(rootBg);
                gPad->GetFrame()->SetLineColor(axisCol);
            }

            // Re-render Gaussian center annotations
            renderPeakLabels(z, g);

            // Re-render Peak Search tick marks and labels (matching GASP xtpSHOWPEAKS)
            renderPeakSearchLabels(z, g);

            // Re-render overlaid comparison spectra
            for (std::size_t k = 0; k < HijC[z][g].size(); ++k) {
                if (HijC[z][g][k]) {
                    if (k < colors_hist.size()) {
                        HijC[z][g][k]->SetLineColor(colors_hist[k]);
                    }
                    HijC[z][g][k]->Draw("SAME");
                }
            }

            // Re-render auto-fit markers (fit curves, background lines)
            for (auto *obj : autoFitMarkers[z][g]) {
                if (obj) {
                    if (maxElement_i > 1 || maxElement_j > 1) {
                        rootCanvas->cd(padIndex);
                    }
                    obj->Draw("SAME");
                }
            }
        }
    }

    if (maxElement_i > 1 || maxElement_j > 1) {
        ColorTheFrameOfTheHistogram();
    }

    rootCanvas->Modified();
    rootCanvas->Update();
    canvas->update();
}
