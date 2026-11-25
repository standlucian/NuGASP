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

    // Snapshot per-pad LogY state before rootCanvas->Clear() destroys sub-pads
    int wasLogy[12][12] = {};
    TCanvas *rootCanvas = canvas->getCanvas();
    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TVirtualPad *p = (maxElement_i > 1 || maxElement_j > 1)
                ? rootCanvas->GetPad((z - 1) * maxElement_j + g)
                : rootCanvas;
            if (p) wasLogy[z][g] = p->GetLogy();
            peakSearchPrimitives[z][g].clear();
        }
    }
    clearDrawnObjects();

    maxElement_j++;

    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";
            const bool isNewCell = (g == maxElement_j);

            if (isNewCell) {
                delete HijF[z][g];
                HijF[z][g] = nullptr;
                // Initialize the new column cell as a clean TracknHistogram
                const int baseColor = colors_hist.empty() ? 4 : colors_hist[0];
                if (sourceHist) {
                    HijF[z][g] = new TracknHistogram(*sourceHist);
                    HijF[z][g]->SetName(histTitle.c_str());
                    HijF[z][g]->SetTitle("");
                    HijF[z][g]->SetLineColor(baseColor);
                    while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                        HijF[z][g]->GetListOfFunctions()->RemoveLast();
                    }
                } else {
                    HijF[z][g] = new TracknHistogram(histTitle.c_str(), "", 10240, 0.0, 10240.0);
                    HijF[z][g]->SetLineColor(baseColor);
                }
                for (auto *h : HijC[z][g]) {
                    delete h;
                }
                HijC[z][g].clear();
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            TVirtualPad *pad = rootCanvas->cd(padIndex);
            if (!isNewCell && pad) {
                pad->SetLogy(wasLogy[z][g]);
            } else {
                adjustYAxisToVisibleMax(HijF[z][g], z, g);
            }
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
    CommandPrompt::logDebug("Display", QString("Added grid column -> %1x%2.").arg(maxElement_i).arg(maxElement_j));
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

    int wasLogy[12][12] = {};
    TCanvas *rootCanvas = canvas->getCanvas();
    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TVirtualPad *p = (maxElement_i > 1 || maxElement_j > 1)
                ? rootCanvas->GetPad((z - 1) * maxElement_j + g)
                : rootCanvas;
            if (p) wasLogy[z][g] = p->GetLogy();
            peakSearchPrimitives[z][g].clear();
        }
    }
    clearDrawnObjects();

    maxElement_i++;

    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);
    rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            const std::string histTitle = "h" + std::to_string(z) + std::to_string(g) + "f";
            const bool isNewCell = (z == maxElement_i);

            if (isNewCell) {
                delete HijF[z][g];
                HijF[z][g] = nullptr;
                // Initialize the new row cell as a clean TracknHistogram
                const int baseColor = colors_hist.empty() ? 4 : colors_hist[0];
                if (sourceHist) {
                    HijF[z][g] = new TracknHistogram(*sourceHist);
                    HijF[z][g]->SetName(histTitle.c_str());
                    HijF[z][g]->SetTitle("");
                    HijF[z][g]->SetLineColor(baseColor);
                    while (HijF[z][g]->GetListOfFunctions()->GetSize() > 0) {
                        HijF[z][g]->GetListOfFunctions()->RemoveLast();
                    }
                } else {
                    HijF[z][g] = new TracknHistogram(histTitle.c_str(), "", 10240, 0.0, 10240.0);
                    HijF[z][g]->SetLineColor(baseColor);
                }
                for (auto *h : HijC[z][g]) {
                    delete h;
                }
                HijC[z][g].clear();
            }

            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            TVirtualPad *pad = rootCanvas->cd(padIndex);
            if (!isNewCell && pad) {
                pad->SetLogy(wasLogy[z][g]);
            } else {
                adjustYAxisToVisibleMax(HijF[z][g], z, g);
            }
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
    CommandPrompt::logDebug("Display", QString("Added grid row -> %1x%2.").arg(maxElement_i).arg(maxElement_j));
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
    updateAxisStatusLabels();
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
        TAxis *xAxis = hist->GetXaxis();
        const Double_t xFirst = xAxis ? xAxis->GetBinLowEdge(xAxis->GetFirst()) : 0.0;
        const Double_t xLast  = xAxis ? xAxis->GetBinUpEdge(xAxis->GetLast()) : 1.0;
        const Double_t yMax   = hist->GetMaximum();

        TVirtualPad *pad = gPad;
        const bool isLog = (pad && pad->GetLogy() != 0);
        Double_t yMin = hist->GetMinimum();
        if (yMin == -1111.0 || (isLog && yMin <= 0.0)) {
            yMin = isLog ? 0.5 : 0.0;
        }

        lineR = new TLine(xFirst, yMin, xFirst, yMax);
        lineL = new TLine(xLast, yMin, xLast, yMax);
        lineU = new TLine(xFirst, yMax, xLast, yMax);
        lineD = new TLine(xFirst, yMin, xLast, yMin);

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
    int targetI = (PilgrimElement_i >= 1 && PilgrimElement_i <= maxElement_i) ? PilgrimElement_i : SelectedElement_i;
    int targetJ = (PilgrimElement_j >= 1 && PilgrimElement_j <= maxElement_j) ? PilgrimElement_j : SelectedElement_j;
    if (targetI < 1 || targetI >= 12 || targetJ < 1 || targetJ >= 12) {
        return;
    }

    TH1F *hist = HijF[targetI][targetJ];
    if (!hist || !canvas || !canvas->getCanvas()) return;

    TVirtualPad *pad = canvas->getCanvas()->cd((targetI - 1) * maxElement_j + targetJ);
    if (!pad) pad = canvas->getCanvas();
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

    const int ch = (binX > 0) ? (binX - 1) : 0;

    if (labelChannel) {
        labelChannel->setText(QString("Channel: %1").arg(ch));
    }
    if (labelEnergy) {
        TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(hist);
        if (tHist && tHist->IsCalibrated()) {
            double energy = tHist->ChannelToEnergy(static_cast<Double_t>(ch));
            labelEnergy->setText(QString("Energy: %1").arg(QString::number(energy, 'f', 2)));
        } else {
            labelEnergy->setText(QString("Energy: %1").arg(ch));
        }
    }
    if (labelCounts) {
        labelCounts->setText(QString("Counts: %1").arg(countsStr));
    }
    if (labelCursorY) {
        const Double_t cursorY = pad ? pad->PadtoY(pad->AbsPixeltoY(static_cast<Int_t>(y))) : y;
        QString yStr;
        if (std::abs(cursorY) <= 100000.0) {
            yStr = (std::abs(cursorY - std::round(cursorY)) < 1e-5)
                ? QString::number(static_cast<long long>(std::round(cursorY)))
                : QString::number(cursorY, 'f', 2);
        } else {
            yStr = QString::number(cursorY, 'e', 2);
        }
        labelCursorY->setText(QString("Y: %1").arg(yStr));
    }

    if (labelX && labelX != labelChannel) {
        labelX->setText(QString::number(ch));
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

    int wasLogy[12][12] = {};
    TCanvas *rootCanvas = canvas->getCanvas();
    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TVirtualPad *p = rootCanvas->GetPad((z - 1) * maxElement_j + g);
            if (p) wasLogy[z][g] = p->GetLogy();
            peakSearchPrimitives[z][g].clear();
        }
    }
    clearDrawnObjects();

    const int delCol = std::max(1, std::min(SelectedElement_j, maxElement_j));
    for (int z = 1; z <= maxElement_i; ++z) {
        delete HijF[z][delCol];
        HijF[z][delCol] = nullptr;
        for (auto *hist : HijC[z][delCol]) {
            delete hist;
        }
        HijC[z][delCol].clear();
        gaussCenters[z][delCol].clear();
        gaussCentersHeight[z][delCol].clear();
        peakSearchCenters[z][delCol].clear();
        peakSearchHeights[z][delCol].clear();
        for (auto *obj : autoFitMarkers[z][delCol]) {
            delete obj;
        }
        autoFitMarkers[z][delCol].clear();

        // Shift remaining columns left
        for (int g = delCol; g < maxElement_j; ++g) {
            HijF[z][g] = HijF[z][g + 1];
            HijC[z][g] = std::move(HijC[z][g + 1]);
            gaussCenters[z][g] = std::move(gaussCenters[z][g + 1]);
            gaussCentersHeight[z][g] = std::move(gaussCentersHeight[z][g + 1]);
            peakSearchCenters[z][g] = std::move(peakSearchCenters[z][g + 1]);
            peakSearchHeights[z][g] = std::move(peakSearchHeights[z][g + 1]);
            autoFitMarkers[z][g] = std::move(autoFitMarkers[z][g + 1]);
            wasLogy[z][g] = wasLogy[z][g + 1];
        }
        HijF[z][maxElement_j] = nullptr;
        HijC[z][maxElement_j].clear();
        gaussCenters[z][maxElement_j].clear();
        gaussCentersHeight[z][maxElement_j].clear();
        peakSearchCenters[z][maxElement_j].clear();
        peakSearchHeights[z][maxElement_j].clear();
        autoFitMarkers[z][maxElement_j].clear();
    }

    maxElement_j--;

    // Keep selection within valid bounds
    if (SelectedElement_j > maxElement_j) {
        SelectedElement_j = maxElement_j;
    }
    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];

    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);

    if (maxElement_i > 1 || maxElement_j > 1) {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);
    }

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            TVirtualPad *pad = (maxElement_i > 1 || maxElement_j > 1)
                ? rootCanvas->cd(padIndex)
                : rootCanvas->cd();
            if (pad) pad->SetLogy(wasLogy[z][g]);
            HijF[z][g]->Draw();

            // Re-render Gaussian center and peak search annotations
            renderPeakLabels(z, g);
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
                    if (pad) pad->cd();
                    obj->Draw("SAME");
                }
            }
        }
    }

    ColorTheFrameOfTheHistogram();
    updateAxisStatusLabels();

    rootCanvas->Modified();
    rootCanvas->Update();
    CommandPrompt::logDebug("Display", QString("Deleted column %1 -> %2x%3.").arg(delCol).arg(maxElement_i).arg(maxElement_j));
}

//==============================================================================
// QMainCanvas::DeleteLine
//==============================================================================
// Removes the selected row of sub-pads from the canvas grid (shortcut: Ctrl + Down).
// Re-divides the ROOT TCanvas into maxElement_j columns by (maxElement_i - 1) rows,
// frees resources belonging to the removed row, and re-renders remaining spectra.
//==============================================================================
void QMainCanvas::DeleteLine()
{
    if (maxElement_i <= 1) return;
    if (!canvas || !canvas->getCanvas()) return;

    int wasLogy[12][12] = {};
    TCanvas *rootCanvas = canvas->getCanvas();
    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            TVirtualPad *p = rootCanvas->GetPad((z - 1) * maxElement_j + g);
            if (p) wasLogy[z][g] = p->GetLogy();
            peakSearchPrimitives[z][g].clear();
        }
    }
    clearDrawnObjects();

    const int delRow = std::max(1, std::min(SelectedElement_i, maxElement_i));
    for (int g = 1; g <= maxElement_j; ++g) {
        delete HijF[delRow][g];
        HijF[delRow][g] = nullptr;
        for (auto *hist : HijC[delRow][g]) {
            delete hist;
        }
        HijC[delRow][g].clear();
        gaussCenters[delRow][g].clear();
        gaussCentersHeight[delRow][g].clear();
        peakSearchCenters[delRow][g].clear();
        peakSearchHeights[delRow][g].clear();
        for (auto *obj : autoFitMarkers[delRow][g]) {
            delete obj;
        }
        autoFitMarkers[delRow][g].clear();

        // Shift remaining rows up
        for (int z = delRow; z < maxElement_i; ++z) {
            HijF[z][g] = HijF[z + 1][g];
            HijC[z][g] = std::move(HijC[z + 1][g]);
            gaussCenters[z][g] = std::move(gaussCenters[z + 1][g]);
            gaussCentersHeight[z][g] = std::move(gaussCentersHeight[z + 1][g]);
            peakSearchCenters[z][g] = std::move(peakSearchCenters[z + 1][g]);
            peakSearchHeights[z][g] = std::move(peakSearchHeights[z + 1][g]);
            autoFitMarkers[z][g] = std::move(autoFitMarkers[z + 1][g]);
            wasLogy[z][g] = wasLogy[z + 1][g];
        }
        HijF[maxElement_i][g] = nullptr;
        HijC[maxElement_i][g].clear();
        gaussCenters[maxElement_i][g].clear();
        gaussCentersHeight[maxElement_i][g].clear();
        peakSearchCenters[maxElement_i][g].clear();
        peakSearchHeights[maxElement_i][g].clear();
        autoFitMarkers[maxElement_i][g].clear();
    }

    maxElement_i--;

    // Keep selection within valid bounds
    if (SelectedElement_i > maxElement_i) {
        SelectedElement_i = maxElement_i;
    }
    selectedHisto = HijF[SelectedElement_i][SelectedElement_j];

    rootCanvas->Clear();
    rootCanvas->SetBorderMode(0);
    const Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    rootCanvas->SetFillColor(rootBg);

    if (maxElement_i > 1 || maxElement_j > 1) {
        rootCanvas->Divide(maxElement_j, maxElement_i, 0, 0, 0);
    }

    for (int z = 1; z <= maxElement_i; ++z) {
        for (int g = 1; g <= maxElement_j; ++g) {
            if (!HijF[z][g]) continue;

            const int padIndex = (z - 1) * maxElement_j + g;
            TVirtualPad *pad = (maxElement_i > 1 || maxElement_j > 1)
                ? rootCanvas->cd(padIndex)
                : rootCanvas->cd();
            if (pad) pad->SetLogy(wasLogy[z][g]);
            HijF[z][g]->Draw();

            // Re-render Gaussian center and peak search annotations
            renderPeakLabels(z, g);
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
                    if (pad) pad->cd();
                    obj->Draw("SAME");
                }
            }
        }
    }

    ColorTheFrameOfTheHistogram();
    updateAxisStatusLabels();

    rootCanvas->Modified();
    rootCanvas->Update();
    CommandPrompt::logDebug("Display", QString("Deleted row %1 -> %2x%3.").arg(delRow).arg(maxElement_i).arg(maxElement_j));
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
