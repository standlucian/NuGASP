#ifndef INTEGRAL_H
#define INTEGRAL_H

#include "TH1F.h"
#include <vector>

/**
 * @brief Calculates the gross integral and Poisson uncertainty between two
 * marker positions.
 *
 * Automatically normalizes marker order and clamps coordinates to the valid
 * histogram range.
 *
 * @param histogram Pointer to the ROOT 1D histogram.
 * @param error Reference receiving the computed Poisson error of the integral.
 * @param position_marker_left First boundary marker (channel/bin).
 * @param position_marker_right Second boundary marker (channel/bin).
 * @return Gross counts between the markers, or 0.0 if histogram is null or
 * range is invalid.
 */
Double_t integral_no_background(TH1F *histogram, Double_t &error,
                                Double_t position_marker_left,
                                Double_t position_marker_right);

/**
 * @brief Checks whether any of the marker interval pairs overlap with each
 * other.
 *
 * Markers are assumed to be organized as consecutive pairs [M[2k], M[2k+1]].
 *
 * @param markers Vector of channel/bin marker positions.
 * @return True if at least two intervals overlap (sharing more than a boundary
 * point), false otherwise.
 */
bool overlapping_markers(const std::vector<Int_t> &markers);

/**
 * @brief Computes the least-squares linear background fit (y = slope * x +
 * yIntercept) over all specified background regions.
 *
 * @param histogram Pointer to the ROOT 1D histogram.
 * @param background_markers Vector of background marker pairs defining the
 * background intervals.
 * @param slope Reference receiving the calculated slope.
 * @param yIntercept Reference receiving the calculated y-intercept.
 */
void get_best_fitted_line(TH1F *histogram,
                          const std::vector<Int_t> &background_markers,
                          Double_t &slope, Double_t &yIntercept);

/**
 * @brief Performs ROI peak integration with background subtraction, computing
 * net area, peak centroid, and FWHM along with propagated statistical
 * uncertainties.
 *
 * Outputs the results to both the standard output and the GUI command prompt
 * terminal.
 *
 * @param histogram Pointer to the ROOT 1D histogram.
 * @param integral_markers Vector of marker pairs defining the peak integration
 * ROI(s).
 * @param background_markers Vector of marker pairs defining the background
 * estimation regions.
 * @param slope Reference receiving the background slope.
 * @param addition Reference receiving the background intercept (named
 * 'addition' for legacy compatibility).
 */
void integral_function(TH1F *histogram,
                       const std::vector<Int_t> &integral_markers,
                       const std::vector<Int_t> &background_markers,
                       Double_t &slope, Double_t &addition);

#endif // INTEGRAL_H
