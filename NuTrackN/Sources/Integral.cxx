#include "Integral.h"
#include "Design.h"
#include "tracknhistogram.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

#include <QString>
#include <QtMath>

// -----------------------------------------------------------------------------
// Calculates the gross integral and Poisson uncertainty between two marker positions.
// -----------------------------------------------------------------------------
Double_t integral_no_background(TH1F *histogram,
                                Double_t &error,
                                Double_t position_marker_left,
                                Double_t position_marker_right)
{
    error = 0.0;
    if (!histogram) {
        return 0.0;
    }

    const Int_t leftBin  = histogram->FindBin(std::min(position_marker_left, position_marker_right));
    const Int_t rightBin = histogram->FindBin(std::max(position_marker_left, position_marker_right));

    return histogram->IntegralAndError(leftBin, rightBin, error);
}

// -----------------------------------------------------------------------------
// Checks whether any of the marker intervals overlap.
// -----------------------------------------------------------------------------
bool overlapping_markers(const std::vector<Int_t>& markers)
{
    // Markers are stored in consecutive pairs: [left, right]
    for (std::size_t i = 0; i + 1 < markers.size(); i += 2) {
        const Double_t left1 = std::min(markers[i], markers[i + 1]);
        const Double_t right1 = std::max(markers[i], markers[i + 1]);

        for (std::size_t j = i + 2; j + 1 < markers.size(); j += 2) {
            const Double_t left2 = std::min(markers[j], markers[j + 1]);
            const Double_t right2 = std::max(markers[j], markers[j + 1]);

            // Open intervals overlap if they share interior points
            if (left1 < right2 && left2 < right1) {
                return true;
            }
        }
    }

    return false;
}

static bool solveWeightedLinearBackground(
    TH1F *histogram,
    const std::vector<Int_t> &background_markers,
    BackgroundFitMethod method,
    Double_t initSlope,
    Double_t initIntercept,
    Double_t &slope,
    Double_t &yIntercept,
    Double_t *outVarSlope,
    Double_t *outVarIntercept,
    Double_t *outCov)
{
    auto weightAt = [&](Double_t ch, Double_t y) -> Double_t {
        if (method == BackgroundFitMethod::PoissonWLS) {
            return (y > 0.0) ? (1.0 / y) : 1.0;
        }
        if (method == BackgroundFitMethod::ModelWLS) {
            const Double_t yModel = initSlope * ch + initIntercept;
            return 1.0 / std::max(yModel, 1.0);
        }
        return 1.0;
    };

    Double_t S1 = 0.0;
    Double_t SX = 0.0;
    Double_t SY = 0.0;

    for (std::size_t i = 0; i + 1 < background_markers.size(); i += 2) {
        const Int_t startCh  = std::min(background_markers[i], background_markers[i + 1]);
        const Int_t endCh    = std::max(background_markers[i], background_markers[i + 1]);
        const Int_t startBin = histogram->FindBin(startCh);
        const Int_t endBin   = histogram->FindBin(endCh);

        for (Int_t bin = startBin; bin <= endBin; ++bin) {
            const Double_t ch = histogram->GetBinCenter(bin);
            const Double_t y  = histogram->GetBinContent(bin);
            const Double_t w  = weightAt(ch, y);
            S1 += w;
            SX += w * ch;
            SY += w * y;
        }
    }

    if (S1 <= 0.0) {
        return false;
    }

    const Double_t xMean = SX / S1;
    const Double_t yMean = SY / S1;

    Double_t numerator = 0.0;
    Double_t denominator = 0.0;

    for (std::size_t i = 0; i + 1 < background_markers.size(); i += 2) {
        const Int_t startCh  = std::min(background_markers[i], background_markers[i + 1]);
        const Int_t endCh    = std::max(background_markers[i], background_markers[i + 1]);
        const Int_t startBin = histogram->FindBin(startCh);
        const Int_t endBin   = histogram->FindBin(endCh);

        for (Int_t bin = startBin; bin <= endBin; ++bin) {
            const Double_t ch = histogram->GetBinCenter(bin);
            const Double_t y  = histogram->GetBinContent(bin);
            const Double_t w  = weightAt(ch, y);
            const Double_t dx = ch - xMean;
            numerator += w * dx * (y - yMean);
            denominator += w * dx * dx;
        }
    }

    if (std::abs(denominator) > 1e-12) {
        slope = numerator / denominator;
        yIntercept = yMean - slope * xMean;
        if (outVarSlope) *outVarSlope = 1.0 / denominator;
        if (outVarIntercept) *outVarIntercept = (1.0 / S1) + (xMean * xMean) / denominator;
        if (outCov) *outCov = -xMean / denominator;
    } else {
        slope = 0.0;
        yIntercept = yMean;
        if (outVarSlope) *outVarSlope = 0.0;
        if (outVarIntercept) *outVarIntercept = 1.0 / S1;
        if (outCov) *outCov = 0.0;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Calculates the linear background (y = slope * x + yIntercept)
// across all designated background intervals using OLS, 2-pass IRLS, or Poisson WLS.
// -----------------------------------------------------------------------------
void get_best_fitted_line(TH1F* histogram,
                          const std::vector<Int_t>& background_markers,
                          Double_t& slope,
                          Double_t& yIntercept,
                          BackgroundFitMethod method,
                          Double_t* outVarSlope,
                          Double_t* outVarIntercept,
                          Double_t* outCov)
{
    slope = 0.0;
    yIntercept = 0.0;
    if (outVarSlope) *outVarSlope = 0.0;
    if (outVarIntercept) *outVarIntercept = 0.0;
    if (outCov) *outCov = 0.0;

    if (!histogram || background_markers.empty()) {
        return;
    }

    if (method == BackgroundFitMethod::ModelWLS) {
        Double_t initSlope = 0.0, initIntercept = 0.0;
        if (!solveWeightedLinearBackground(histogram, background_markers, BackgroundFitMethod::OLS,
                                           0.0, 0.0, initSlope, initIntercept, nullptr, nullptr, nullptr)) {
            return;
        }
        solveWeightedLinearBackground(histogram, background_markers, BackgroundFitMethod::ModelWLS,
                                      initSlope, initIntercept, slope, yIntercept,
                                      outVarSlope, outVarIntercept, outCov);
    } else {
        solveWeightedLinearBackground(histogram, background_markers, method,
                                      0.0, 0.0, slope, yIntercept,
                                      outVarSlope, outVarIntercept, outCov);
    }
}

// -----------------------------------------------------------------------------
// Calculates the net peak area, centroid, and FWHM for each integral marker interval
// after subtracting the linear background.
// The algorithm employs statistical moments and error propagation consistent
// with legacy Xtrackn / GASPWare standards.
// -----------------------------------------------------------------------------
void integral_function(TH1F* histogram,
                       const std::vector<Int_t>& integral_markers,
                       const std::vector<Int_t>& background_markers,
                       Double_t& slope,
                       Double_t& addition,
                       std::vector<IntegratedPeak>* outPeaks,
                       bool quiet,
                       BackgroundFitMethod bgMethod)
{
    if (outPeaks) {
        outPeaks->clear();
    }

    if (!histogram) {
        if (!quiet) {
            const QString errMsg = "Error: Histogram pointer is null in integral_function.";
            CommandPrompt::getInstance()->appendPlainText(errMsg);
            std::cerr << errMsg.toStdString() << std::endl;
        }
        return;
    }

    // Default to zero background
    slope = 0.0;
    addition = 0.0;

    // Process background markers if provided
    if (!background_markers.empty()) {
        if (background_markers.size() % 2 != 0) {
            if (!quiet) {
                const QString msg = "Background markers must be placed in pairs (multiples of 2).\n";
                CommandPrompt::getInstance()->appendPlainText(msg);
                std::cout << msg.toStdString() << std::endl;
            }
            return;
        }

        if (!quiet && overlapping_markers(background_markers)) {
            const QString warning = "Warning: Some background marker intervals overlap. "
                                    "Check marker positions for accuracy.\n";
            CommandPrompt::getInstance()->appendPlainText(warning);
            std::cout << warning.toStdString() << std::endl;
        }

        get_best_fitted_line(histogram, background_markers, slope, addition, bgMethod);
    }

    // Validate integral markers
    if (integral_markers.empty()) {
        if (!quiet) {
            const QString msg = "Place markers for the integral to be calculated.\n";
            CommandPrompt::getInstance()->appendPlainText(msg);
            std::cout << msg.toStdString() << std::endl;
        }
        return;
    }

    if (integral_markers.size() % 2 != 0) {
        if (!quiet) {
            const QString msg = "Integral markers must be placed in pairs (multiples of 2).\n";
            CommandPrompt::getInstance()->appendPlainText(msg);
            std::cout << msg.toStdString() << std::endl;
        }
        return;
    }

    if (!quiet) {
        // Print table header to terminal and GUI command prompt
        std::cout << std::left
                  << std::setw(10) << "Integral#"
                  << std::setw(10) << "Channel"
                  << std::setw(15) << "Energy"
                  << std::setw(25) << "Area"
                  << std::setw(10) << "Width" << std::endl;

        const QString headerRow = QString("%1%2%3%4%5")
            .arg("Integral#", -10, QChar(' '))
            .arg("Channel",   -10, QChar(' '))
            .arg("Energy",    -15, QChar(' '))
            .arg("Area",      -25, QChar(' '))
            .arg("Width",     -10, QChar(' '));
        CommandPrompt::getInstance()->appendPlainText(headerRow);
    }

    const bool hasBackground = !background_markers.empty();

    // Loop through each pair of integral markers
    for (std::size_t i = 0; i + 1 < integral_markers.size(); i += 2) {
        const Int_t leftCh  = std::min(integral_markers[i], integral_markers[i + 1]);
        const Int_t rightCh = std::max(integral_markers[i], integral_markers[i + 1]);
        const Int_t leftBin  = histogram->FindBin(leftCh);
        const Int_t rightBin = histogram->FindBin(rightCh);

        const Double_t midCh = 0.5 * (leftCh + rightCh);

        Double_t xc0 = 0.0; // 0th moment: Net Area
        Double_t xc1 = 0.0; // 1st moment: Centroid offset
        Double_t xc2 = 0.0; // 2nd moment: Peak dispersion
        Double_t xd0 = 0.0; // Variance of net area
        Double_t xd1 = 0.0;
        Double_t xd2 = 0.0;
        Double_t xd4 = 0.0;

        Double_t xGross0 = 0.0;
        Double_t xBg0 = 0.0;
        Double_t xdGross0 = 0.0;
        Double_t xdBg0 = 0.0;

        for (Int_t bin = leftBin; bin <= rightBin; ++bin) {
            const Double_t ch = histogram->GetBinCenter(bin);
            const Double_t w = ch - midCh;
            const Double_t y = histogram->GetBinContent(bin);
            const Double_t varY = std::max(y, 1.0); // Poisson variance of gross counts

            Double_t yNet = y;
            Double_t varNet = varY;
            xGross0 += y;
            xdGross0 += varY;

            if (hasBackground) {
                const Double_t bg = slope * ch + addition;
                yNet -= bg;
                const Double_t varBg = std::max(bg, 0.0);
                varNet += varBg; // Net variance = Var(Gross) + Var(Background)
                
                xBg0 += bg;
                xdBg0 += varBg;
            }

            xc0 += yNet;
            xc1 += yNet * w;
            xc2 += yNet * w * w;

            xd0 += varNet;
            xd1 += varNet * w;
            xd2 += varNet * w * w;
            xd4 += varNet * w * w * w * w;
        }

        const Double_t area = xc0;
        const Double_t areaError = std::sqrt(std::max(xd0, 0.0));

        Double_t centroid = midCh;
        Double_t centroidError = 0.0;
        Double_t fwhm = 0.0;
        Double_t fwhmError = 0.0;

        // Compute centroid and FWHM using statistical moments (Xtrackn algorithm)
        if (xc0 > 0.0) {
            const Double_t xPos = xc1 / xc0;
            centroid = midCh + xPos;

            const Double_t varPos = (xd2 - 2.0 * xPos * xd1 + xPos * xPos * xd0) / (xc0 * xc0);
            if (varPos > 0.0) {
                centroidError = std::sqrt(varPos);
            }

            const Double_t peakVar = (xc2 / xc0) - (xPos * xPos);
            if (peakVar > 0.0) {
                fwhm = 2.354820 * std::sqrt(peakVar);

                // Error propagation for FWHM from legacy Xtrackn
                const Double_t dfwhmSq = 4.0 * xPos * xPos * varPos
                                       + (xd4 / (xc0 * xc0))
                                       - (2.0 * xc2 * xd2 / (xc0 * xc0 * xc0))
                                       + (xc2 * xc2 * xd0 / (xc0 * xc0 * xc0 * xc0));
                if (dfwhmSq > 0.0) {
                    fwhmError = (std::sqrt(dfwhmSq) / (2.0 * peakVar)) * fwhm;
                }
            }
        } else {
            // Fallback for flat or negative ROI
            centroid = midCh;
            centroidError = 0.0;
            fwhm = static_cast<Double_t>(rightCh - leftCh);
            fwhmError = 0.0;
        }

        const int peakIndex = static_cast<int>(i / 2 + 1);

        TracknHistogram *trackHist = dynamic_cast<TracknHistogram*>(histogram);
        const bool isCalib = (trackHist && trackHist->IsCalibrated());

        Double_t energy = 0.0;
        Double_t energyError = 0.0;
        Double_t widthDisp = fwhm;
        Double_t widthDispError = fwhmError;

        if (isCalib) {
            energy = trackHist->ChannelToEnergy(centroid);
            const Double_t eLow = trackHist->ChannelToEnergy(std::max(0.0, centroid - centroidError * 0.5));
            const Double_t eHigh = trackHist->ChannelToEnergy(centroid + centroidError * 0.5);
            energyError = std::abs(eHigh - eLow);

            const Double_t wLow = trackHist->ChannelToEnergy(std::max(0.0, centroid - fwhm * 0.5));
            const Double_t wHigh = trackHist->ChannelToEnergy(centroid + fwhm * 0.5);
            widthDisp = std::abs(wHigh - wLow);
            if (fwhm > 0.0) {
                widthDispError = (fwhmError / fwhm) * widthDisp;
            }
        }

        // Format row strings with standard bracketed uncertainty notation
        const QString numberStr   = QString::number(peakIndex);
        const QString centroidStr = QString::number(centroid, 'f', 2);
        QString energyStr;
        if (isCalib) {
            energyStr = QString("%1(%2)")
                            .arg(energy, 0, 'f', 2)
                            .arg(qCeil(energyError * 100));
        } else {
            energyStr = QString("%1(%2)")
                            .arg(centroid, 0, 'f', 2)
                            .arg(qCeil(centroidError * 100));
        }
        const QString areaStr     = QString("%1(%2)")
                                        .arg(area, 0, 'f', 0)
                                        .arg(qCeil(areaError));
        const QString fwhmStr     = QString("%1(%2)")
                                        .arg(widthDisp, 0, 'f', 2)
                                        .arg(qCeil(widthDispError * 100));

        if (outPeaks) {
            IntegratedPeak p;
            p.index = peakIndex;
            p.centroid = centroid;
            p.centroidError = centroidError;
            p.grossArea = xGross0;
            p.grossAreaError = std::sqrt(std::max(xdGross0, 0.0));
            p.bgArea = xBg0;
            p.bgAreaError = std::sqrt(std::max(xdBg0, 0.0));
            p.area = area;
            p.areaError = areaError;
            p.fwhm = fwhm;
            p.fwhmError = fwhmError;
            p.energy = isCalib ? energy : centroid;
            p.energyError = isCalib ? energyError : centroidError;
            p.energyFwhm = widthDisp;
            p.energyFwhmError = widthDispError;
            p.isCalibrated = isCalib;
            outPeaks->push_back(p);
        }

        if (!quiet) {
            // Output to terminal
            std::cout << std::left
                      << std::setw(10) << numberStr.toStdString()
                      << std::setw(10) << centroidStr.toStdString()
                      << std::setw(15) << energyStr.toStdString()
                      << std::setw(25) << areaStr.toStdString()
                      << std::setw(10) << fwhmStr.toStdString() << std::endl;

            // Output to application command prompt
            const QString dataRow = QString("%1%2%3%4%5")
                .arg(numberStr,   -10, QChar(' '))
                .arg(centroidStr, -10, QChar(' '))
                .arg(energyStr,   -15, QChar(' '))
                .arg(areaStr,     -25, QChar(' '))
                .arg(fwhmStr,     -10, QChar(' '));
            CommandPrompt::getInstance()->appendPlainText(dataRow);

            // Record in session disk log
            CommandPrompt::logDebug(
                "Integration",
                QString("%1 Peak #%2: ch=%3, E=%4, Area=%5, FWHM=%6")
                    .arg(hasBackground ? "CJ" : "CI")
                    .arg(numberStr, centroidStr, energyStr, areaStr, fwhmStr));
        }
    }

    if (!quiet) {
        std::cout << std::endl;
        CommandPrompt::getInstance()->appendPlainText("");
    }
}
