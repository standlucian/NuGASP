#ifndef CALIB_H
#define CALIB_H

#include <vector>
#include <QString>
#include "RtypesCore.h"
#include "tracknhistogram.h"

class QWidget;
class QMainCanvas;
class TracknHistogram;

struct CalibDetector {
    int group{1};
    int detectorId{0};
    int numSegments{0};
    std::vector<CalibSegment> segments;
};

/**
 * @brief Parses calibration files in GASP multi-detector .mcal format or simple .cal / .dat formats.
 * @param filePath Path to the calibration file.
 * @param outDetectors List of parsed detectors and their calibration segments.
 * @param outFormatInfo Summary description of parsed file.
 * @return True if parsing succeeded and at least one detector/calibration was loaded.
 */
bool ParseCalibrationFile(const QString &filePath,
                          std::vector<CalibDetector> &outDetectors,
                          QString &outFormatInfo);

/**
 * @brief Saves calibration to a file (.mcal or simple .cal format).
 */
bool SaveCalibrationFile(const QString &filePath,
                         const std::vector<CalibDetector> &detectors,
                         bool isMcalFormat);

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

/**
 * @brief Launches the full Energy Calibration Manager dialog (EnCal).
 *        Supports GASP multi-detector .mcal files, simple .cal files, manual polynomial entry,
 *        and multi-point linear regression.
 */
void runEnergyCalibrationDialog(QMainCanvas *mainCanvas, int currentDetId = 0);

#endif // CALIB_H
