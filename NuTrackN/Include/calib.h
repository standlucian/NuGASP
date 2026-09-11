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
 * @brief Computes first-order linear energy calibration coefficients A0 (intercept) and A1 (slope)
 *        using linear regression over N reference points (channel, energy).
 */
void LinearCalibration(const std::vector<double>& channels, const std::vector<double>& energies);

/**
 * @brief Launches the interactive N-point first-order calibration dialog, prompting the user for
 *        the number of points and physical energies corresponding to the reference channels,
 *        and applies the calibration to the active spectrum.
 */
void runTwoPointCalibrationDialog(QWidget *parent,
                                  const std::vector<Float_t> &puncte_calib2p,
                                  TracknHistogram *activeHistogram = nullptr);

#endif // CALIB_H
