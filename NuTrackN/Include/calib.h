#ifndef CALIB_H
#define CALIB_H
#include <vector>
#include "RtypesCore.h"

// Performs a two-point energy calibration using two reference energies.
void TwoPointCalibration(const std::vector<Float_t>& puncte_calib2p,double energie1,double energie2);

#endif
