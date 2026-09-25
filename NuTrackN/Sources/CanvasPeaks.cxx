#include "canvas.h"
#include "Design.h"
#include "PeakFit.h"
#include "tracknhistogram.h"

#include <TCanvas.h>
#include <TH1F.h>
#include <TLine.h>
#include <TLatex.h>
#include <TAxis.h>
#include <TVirtualPad.h>
#include <TList.h>
#include <TColor.h>

#include <QString>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

//==============================================================================
// QMainCanvas::fitGauss
//==============================================================================
// Delegates multi-peak Gaussian fitting with background to the PeakFit module.
//==============================================================================
void QMainCanvas::fitGauss()
{
    runMultiPeakFit(this);
}

//==============================================================================
// QMainCanvas::searchPeaks
//==============================================================================
// Executes peak search on the active histogram using the current sigma and
// threshold settings.
//==============================================================================
void QMainCanvas::searchPeaks()
{
    searchPeaksWithParams(m_peakSearchSigma, m_peakSearchThreshold);
}

//==============================================================================
// QMainCanvas::searchPeaksWithParams
//==============================================================================
// Runs TSpectrum peak detection over the visible spectrum range, updates
// peak markers and GASP-style yellow labels, logs a formatted table to the
// CommandPrompt, and presents the unfocused floating parameters dialog.
//==============================================================================
void QMainCanvas::searchPeaksWithParams(double sigma, double threshold)
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    TH1F *hist = HijF[SelectedElement_i][SelectedElement_j];
    if (!hist) {
        CommandPrompt::getInstance()->appendPlainText("Peak Search: No spectrum loaded in the active pad.\n");
        return;
    }

    m_peakSearchSigma = sigma;
    m_peakSearchThreshold = threshold;

    TAxis *xAxis = hist->GetXaxis();
    const double xMin = xAxis ? xAxis->GetBinLowEdge(xAxis->GetFirst()) : 0.0;
    const double xMax = xAxis ? xAxis->GetBinUpEdge(xAxis->GetLast()) : 0.0;

    // Run TSpectrum search restricted to visible X-axis range
    std::vector<DetectedPeak> peaks = findPeaksWithTSpectrum(hist, sigma, threshold, true);

    // Update stored peaks for the current cell
    peakSearchCenters[SelectedElement_i][SelectedElement_j].clear();
    peakSearchHeights[SelectedElement_i][SelectedElement_j].clear();

    for (const auto &p : peaks) {
        peakSearchCenters[SelectedElement_i][SelectedElement_j].push_back(p.channel);
        peakSearchHeights[SelectedElement_i][SelectedElement_j].push_back(p.height);
    }

    // Render GASP-style yellow vertical tick marks and labels
    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);

    // Log table to interactive CommandPrompt
    CommandPrompt *prompt = CommandPrompt::getInstance();
    if (prompt) {
        QString header = QString("\n========== PEAK SEARCH (TSpectrum: sigma=%1, thresh=%2) ==========\n"
                                 "Pad [%3,%4] | Range: [%5, %6] | Found: %7 peak(s)\n")
                             .arg(sigma, 0, 'f', 2)
                             .arg(threshold, 0, 'f', 3)
                             .arg(SelectedElement_i)
                             .arg(SelectedElement_j)
                             .arg(xMin, 0, 'f', 1)
                             .arg(xMax, 0, 'f', 1)
                             .arg(peaks.size());
        prompt->appendPlainText(header);

        if (!peaks.empty()) {
            prompt->appendPlainText("  #       Channel     Energy (keV)          Counts\n");
            prompt->appendPlainText("--------------------------------------------------\n");
            for (const auto &p : peaks) {
                QString row;
                if (p.isCalibrated) {
                    row = QString(" %1%2%3%4\n")
                              .arg(p.index, 3)
                              .arg(p.channel, 14, 'f', 1)
                              .arg(p.energy, 17, 'f', 2)
                              .arg(static_cast<long long>(p.height), 16);
                } else {
                    row = QString(" %1%2                -%3\n")
                              .arg(p.index, 3)
                              .arg(p.channel, 14, 'f', 1)
                              .arg(static_cast<long long>(p.height), 16);
                }
                prompt->appendPlainText(row);
            }
            prompt->appendPlainText("--------------------------------------------------\n\n");
        }
    }

    // Always display the unfocused floating parameters dialog docked in the top-right corner
    showPeakSearchParamsDialog(this, sigma, threshold, peaks, xMin, xMax);
}

//==============================================================================
// QMainCanvas::deletePeakMarkers
//==============================================================================
// Erases all stored peak search markers and labels for the active pad (shortcut: 'Z + P').
//==============================================================================
void QMainCanvas::deletePeakMarkers()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    peakSearchCenters[SelectedElement_i][SelectedElement_j].clear();
    peakSearchHeights[SelectedElement_i][SelectedElement_j].clear();

    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);

    if (peakSearchParamsDialog && peakSearchParamsDialog->isVisible()) {
        peakSearchParamsDialog->hide();
    }

    CommandPrompt::getInstance()->appendPlainText("Peak Search markers cleared (Z+P).\n");
}

//==============================================================================
// QMainCanvas::showPeakMarkers
//==============================================================================
// Re-renders all stored peak search tick marks and labels on the active pad (shortcut: 'M + P').
//==============================================================================
void QMainCanvas::showPeakMarkers()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    renderPeakSearchLabels(SelectedElement_i, SelectedElement_j);
    CommandPrompt::getInstance()->appendPlainText("Peak Search markers redrawn (M+P).\n");
}

//==============================================================================
// QMainCanvas::transferPeaksToGaussMarkers
//==============================================================================
// Transfers detected peak centroid channels into the multi-peak Gaussian fitting
// marker list (gauss_markers) and drops pink centroid marker lines on the canvas.
//==============================================================================
void QMainCanvas::transferPeaksToGaussMarkers()
{
    if (SelectedElement_i < 1 || SelectedElement_i >= 12 ||
        SelectedElement_j < 1 || SelectedElement_j >= 12) {
        return;
    }

    const auto &centers = peakSearchCenters[SelectedElement_i][SelectedElement_j];
    if (centers.empty()) {
        CommandPrompt::getInstance()->appendPlainText("No peak search results to load into fit markers.\n");
        return;
    }

    gauss_markers.clear();
    for (double ch : centers) {
        gauss_markers.push_back(ch);
    }
    showGaussMarkers();

    CommandPrompt::getInstance()->appendPlainText(
        QString("Loaded %1 peak centroid(s) into Gaussian fit markers (gauss_markers).\n").arg(centers.size()));
}

//==============================================================================
// QMainCanvas::renderPeakSearchLabels
//==============================================================================
// Replicates GASPware's DrawPeakLabel / xtpSHOWPEAKS: renders bright yellow vertical
// tick marks directly above detected peak apexes and yellow energy (or channel)
// text labels centered horizontally right above the ticks.
//==============================================================================
void QMainCanvas::renderPeakSearchLabels(int z, int g)
{
    if (z < 1 || z >= 12 || g < 1 || g >= 12) return;
    if (!HijF[z][g] || !canvas || !canvas->getCanvas()) return;

    if (maxElement_i > 1 || maxElement_j > 1) {
        int padIndex = (z - 1) * maxElement_j + g;
        canvas->getCanvas()->cd(padIndex);
    } else {
        canvas->getCanvas()->cd();
    }
    TVirtualPad *pad = gPad;
    if (!pad) return;

    // Clean up previously drawn peak search primitives for this cell
    for (auto *obj : peakSearchPrimitives[z][g]) {
        if (obj) {
            listOfObjectsDrawnOnScreen.Remove(obj);
            if (pad->GetListOfPrimitives()) {
                pad->GetListOfPrimitives()->Remove(obj);
            }
            delete obj;
        }
    }
    peakSearchPrimitives[z][g].clear();

    if (peakSearchCenters[z][g].empty()) {
        pad->Modified();
        pad->Update();
        return;
    }

    TH1F *hist = HijF[z][g];
    TracknHistogram *tHist = dynamic_cast<TracknHistogram*>(hist);
    const bool isCalib = (tHist && tHist->IsCalibrated());

    TAxis *xAxis = hist->GetXaxis();
    double xFirst = xAxis ? xAxis->GetBinLowEdge(xAxis->GetFirst()) : 0.0;
    double xLast = xAxis ? xAxis->GetBinUpEdge(xAxis->GetLast()) : 0.0;
    if (pad->GetUxmin() < pad->GetUxmax()) {
        xFirst = std::min(xFirst, static_cast<double>(pad->GetUxmin()));
        xLast = std::max(xLast, static_cast<double>(pad->GetUxmax()));
    }
    const double xRange = xLast - xFirst;

    const bool isLogY = (pad->GetLogy() != 0);

    double yMin = 0.0;
    double yMax = 1.0;
    if (isLogY) {
        yMin = std::pow(10.0, pad->GetUymin());
        yMax = std::pow(10.0, pad->GetUymax());
    } else {
        yMin = pad->GetUymin();
        yMax = pad->GetUymax();
    }
    if (yMax <= yMin) {
        yMin = hist->GetMinimum();
        if (yMin < 0.0 || yMin == -1111.0) yMin = 0.0;
        yMax = hist->GetMaximum();
        if (yMax <= yMin || yMax == -1111.0) {
            yMax = hist->GetBinContent(hist->GetMaximumBin()) * 1.10;
            if (yMax <= yMin) yMax = yMin + 10.0;
        }
    }
    const double yRange = yMax - yMin;

    // Peak marker color configured via Design system
    Color_t peakColor = TColor::GetColor(Design::getPeakMarkerColor().name().toUtf8().constData());

    for (size_t k = 0; k < peakSearchCenters[z][g].size(); ++k) {
        const double center = peakSearchCenters[z][g][k];
        if (center < xFirst || center > xLast) continue; // Only render peaks within visible zoom window

        double apexY = (k < peakSearchHeights[z][g].size()) ? peakSearchHeights[z][g][k] : 0.0;
        if (apexY <= 0.0) {
            apexY = hist->GetBinContent(hist->FindBin(center));
        }

        double yTickLow = apexY;
        double yTickHigh = apexY;
        double yText = apexY;

        if (isLogY) {
            const double safeApex = std::max(1.0, apexY);
            if (safeApex * 1.25 * 1.08 <= yMax) {
                // Peak apex is within visible vertical range
                yTickLow = safeApex;
                yTickHigh = safeApex * 1.25;
                yText = yTickHigh * 1.08;
            } else {
                // Peak reaches or exceeds top border: pin tick and label neatly right below top border
                yText = yMax * 0.94;
                yTickHigh = yText / 1.08;
                yTickLow = std::max(1.0, yTickHigh / 1.25);
            }
        } else {
            const double tickHeight = yRange * 0.05;
            if (apexY + tickHeight + yRange * 0.04 <= yMax) {
                // Peak apex is within visible vertical range
                yTickLow = apexY;
                yTickHigh = apexY + tickHeight;
                yText = yTickHigh + yRange * 0.012;
            } else {
                // Peak reaches or exceeds top border: pin tick and label neatly right below top border
                yText = yMax * 0.95;
                yTickHigh = yText - yRange * 0.012;
                yTickLow = std::max(yMin, yTickHigh - tickHeight);
            }
        }

        TLine *tick = new TLine(center, yTickLow, center, yTickHigh);
        tick->SetLineColor(peakColor);
        tick->SetLineWidth(2);
        tick->Draw("SAME");
        peakSearchPrimitives[z][g].push_back(tick);
        listOfObjectsDrawnOnScreen.Add(tick);

        // Format label text: energy if calibrated, channel if uncalibrated (GASP formatted %-.1f)
        char buffer[64];
        if (isCalib) {
            snprintf(buffer, sizeof(buffer), "%.1f", tHist->ChannelToEnergy(center));
        } else {
            snprintf(buffer, sizeof(buffer), "%.1f", center);
        }

        TLatex *lbl = new TLatex(center, yText, buffer);
        lbl->SetName("PeakSearchLabel");
        lbl->SetTextFont(Design::getRootGraphFont(3)); // Fixed screen pixel font from Design
        lbl->SetTextSize(14); // 14px crisp screen text
        int align = 21; // Centered horizontally at peak center, bottom-aligned
        if (xRange > 0.0) {
            if (center - xFirst < xRange * 0.035) {
                align = 11; // Left-align near left pad edge to avoid clipping
            } else if (xLast - center < xRange * 0.035) {
                align = 31; // Right-align near right pad edge to avoid clipping
            }
        }
        lbl->SetTextAlign(align);
        lbl->SetTextColor(peakColor);
        lbl->Draw("SAME");
        peakSearchPrimitives[z][g].push_back(lbl);
        listOfObjectsDrawnOnScreen.Add(lbl);
    }

    pad->Modified();
    pad->Update();
}
