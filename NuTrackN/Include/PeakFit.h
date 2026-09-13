#ifndef PEAKFIT_H
#define PEAKFIT_H

#include <vector>
#include "RtypesCore.h"

class TH1F;
class QMainCanvas;

/**
 * @brief Finds the minimum bin content within the specified channel range.
 *
 * Automatically validates and clamps range to [1, hist->GetNbinsX()].
 *
 * @param hist Pointer to ROOT 1D histogram.
 * @param intervalStart Lower channel boundary.
 * @param intervalFinish Upper channel boundary.
 * @return Minimum bin content, or 0.0 if hist is null.
 */
Double_t findMinValueInInterval(TH1F *hist, int intervalStart, int intervalFinish);

/**
 * @brief Finds the maximum bin content within the specified channel range.
 *
 * Automatically validates and clamps range to [1, hist->GetNbinsX()].
 *
 * @param hist Pointer to ROOT 1D histogram.
 * @param intervalStart Lower channel boundary.
 * @param intervalFinish Upper channel boundary.
 * @return Maximum bin content, or 0.0 if hist is null.
 */
Double_t findMaxValueInInterval(TH1F *hist, int intervalStart, int intervalFinish);

/**
 * @brief Validates and sanitizes background marker intervals.
 *
 * Ensures an even number of markers and resolves overlapping intervals.
 *
 * @param background_markers Vector of background markers (channels).
 */
void checkBackgrounds(std::vector<Int_t> &background_markers);

/**
 * @brief Validates fit range markers.
 *
 * Ensures exactly two ordered boundary markers [min, max].
 *
 * @param range_markers Vector of range markers (channels).
 * @return True if valid range markers exist, false otherwise.
 */
bool checkRanges(std::vector<Double_t> &range_markers);

/**
 * @brief Validates peak centroid markers against the fit range.
 *
 * Erases any centroid markers falling outside [range_markers[0], range_markers[1]].
 *
 * @param gauss_markers Vector of Gaussian centroid markers.
 * @param range_markers Vector of range markers.
 * @return True if at least one valid centroid marker remains, false otherwise.
 */
bool checkGauss(std::vector<Double_t> &gauss_markers, const std::vector<Double_t> &range_markers);

/**
 * @brief Helper that fits linear background across marked intervals and plots baseline.
 *
 * @param mainCanvas Pointer to the main application window.
 */
void fitBackgroundHelper(QMainCanvas *mainCanvas);

/**
 * @brief Identifies the peak apex and determines optimal fit boundaries using 3-point smoothing.
 *
 * Scans a local window around clickedBin on a 3-point triangular smoothed representation
 * to locate the true local apex, then walks left and right down the slopes to detect the
 * valley floors (or baseline level). Returns the apex and the recommended [xMin, xMax]
 * fitting range with background padding, without modifying the underlying raw histogram.
 *
 * @param hist Pointer to ROOT 1D histogram.
 * @param clickedBin Channel bin corresponding to the user click.
 * @param outApex Output refined peak apex channel.
 * @param outXMin Output lower fitting boundary channel.
 * @param outXMax Output upper fitting boundary channel.
 */
void findPeakBoundariesWithSmoothing(TH1F *hist, int clickedBin, int &outApex, Double_t &outXMin, Double_t &outXMax);

/**
 * @brief Runs the automated single-peak Gaussian fit workflow on the active histogram pad.
 *
 * Fits a Gaussian peak + linear background around the clicked channel coordinate,
 * adaptively widening the range if the peak is broad. Formats and logs the results.
 *
 * @param mainCanvas Pointer to the main application window.
 * @param x Pixel X coordinate of click.
 * @param y Pixel Y coordinate of click.
 */
void runAutoFit(QMainCanvas *mainCanvas, int x, int y);

/**
 * @brief Runs the multi-peak Gaussian fit workflow over marked range on the active histogram pad.
 *
 * Fits linear background over background markers, builds a composite multi-Gaussian
 * function over range markers, runs Minuit minimization, propagates uncertainties,
 * and prints a detailed analysis table.
 *
 * @param mainCanvas Pointer to the main application window.
 */
void runMultiPeakFit(QMainCanvas *mainCanvas);

class QString;

/**
 * @brief Container for fitted peak results passed to the interactive parameters dialog.
 */
struct FittedPeakData {
    int peakIndex{0};
    double centroid{0.0};
    double centroidErr{0.0};
    double amplitude{0.0};
    double amplitudeErr{0.0};
    double width{0.0}; // FWHM in display units (keV if calib, ch if uncalib)
    double widthErr{0.0};
    double netArea{0.0};
    double netAreaErr{0.0};
    bool isCalibrated{false};
};

/**
 * @brief Displays an unfocused floating parameters dialog in the top-right corner.
 *
 * Shows background values, fit quality, and interactive peak cards for centroids,
 * amplitudes, and widths with fix constraints.
 *
 * @param mainCanvas Pointer to the main application window.
 * @param title Window title.
 * @param htmlContent Formatted HTML text for fit quality.
 * @param peaks Vector of fitted peak data.
 */
void showFitParametersDialog(QMainCanvas *mainCanvas, const QString &title, const QString &htmlContent, const std::vector<FittedPeakData> &peaks = {});

/**
 * @brief Container for peak search results detected via TSpectrum.
 */
struct DetectedPeak {
    int index{0};
    double channel{0.0};
    double energy{0.0};
    double height{0.0};
    bool isCalibrated{false};
};

/**
 * @brief Performs peak finding on a histogram using ROOT's TSpectrum::Search.
 *
 * Operates on the visible X-axis range of the histogram if useVisibleRange is true.
 * Sorts detected peaks in ascending channel order and applies energy calibration
 * if the histogram is a calibrated TracknHistogram.
 *
 * @param hist Pointer to ROOT histogram.
 * @param sigma Expected peak standard deviation in channels (default: 2.5).
 * @param threshold Relative threshold (fraction of max peak) (default: 0.05).
 * @param useVisibleRange If true, restricts search to visible X-axis range.
 * @return Vector of detected peaks sorted by channel.
 */
std::vector<DetectedPeak> findPeaksWithTSpectrum(
    TH1F *hist,
    double sigma = 2.5,
    double threshold = 0.05,
    bool useVisibleRange = true
);

/**
 * @brief Displays an unfocused floating peak search dialog docked in the top-right corner.
 *
 * Shows interactive spinboxes for sigma and threshold, search range, number of peaks,
 * and a table of detected peaks. Automatically closes on any external key or mouse click.
 *
 * @param mainCanvas Pointer to the main application window.
 * @param sigma Current sigma parameter.
 * @param threshold Current threshold parameter.
 * @param peaks Vector of detected peaks.
 * @param xMin Lower channel/energy bound searched.
 * @param xMax Upper channel/energy bound searched.
 */
void showPeakSearchParamsDialog(
    QMainCanvas *mainCanvas,
    double sigma,
    double threshold,
    const std::vector<DetectedPeak> &peaks,
    double xMin,
    double xMax
);

#endif // PEAKFIT_H
