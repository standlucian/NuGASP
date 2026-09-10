#ifndef CALIB_H
#define CALIB_H

#include <vector>
#include "RtypesCore.h"

class QWidget;
class TracknHistogram;

/**
 * @brief Computes linear energy calibration coefficients A0 (intercept) and A1 (slope)
 *        from two reference channel points and their known energies.
 */
void TwoPointCalibration(const std::vector<Float_t>& puncte_calib2p, double energie1, double energie2);

/**
 * @brief Launches the interactive 2-point calibration dialog, prompts the user for
 *        energies corresponding to the last two peaks, and applies the calibration
 *        to the active spectrum.
 */
void runTwoPointCalibrationDialog(QWidget *parent,
                                  const std::vector<Float_t> &puncte_calib2p,
                                  TracknHistogram *activeHistogram = nullptr);

#endif // CALIB_H
