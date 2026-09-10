#include "tracknhistogram.h"
#include <cmath>

TracknHistogram::TracknHistogram()
    : TH1F()
{
}

TracknHistogram::TracknHistogram(const char *name, const char *title, Int_t nbins, Double_t xlow, Double_t xup)
    : TH1F(name, title, nbins, xlow, xup)
{
}

TracknHistogram::TracknHistogram(const TracknHistogram &other)
    : TH1F(other),
      fIsCalibrated(other.fIsCalibrated),
      fCalibA0(other.fCalibA0),
      fCalibA1(other.fCalibA1),
      fCalibA2(other.fCalibA2),
      fSourceFilePath(other.fSourceFilePath)
{
}

TracknHistogram &TracknHistogram::operator=(const TracknHistogram &other)
{
    if (this != &other) {
        TH1F::operator=(other);
        fIsCalibrated   = other.fIsCalibrated;
        fCalibA0        = other.fCalibA0;
        fCalibA1        = other.fCalibA1;
        fCalibA2        = other.fCalibA2;
        fSourceFilePath = other.fSourceFilePath;
    }
    return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// Handles mouse and canvas interaction events for this histogram.
///
/// Forwards to base TH1F::ExecuteEvent for standard ROOT pad interactions.
////////////////////////////////////////////////////////////////////////////////
void TracknHistogram::ExecuteEvent(Int_t event, Int_t px, Int_t py)
{
    TH1F::ExecuteEvent(event, px, py);
}

void TracknHistogram::SetCalibration(Double_t a0, Double_t a1, Double_t a2)
{
    fCalibA0 = a0;
    fCalibA1 = a1;
    fCalibA2 = a2;
    fIsCalibrated = (std::abs(a1) > 1e-12 || std::abs(a2) > 1e-12);
}

Double_t TracknHistogram::ChannelToEnergy(Double_t channel) const
{
    if (!fIsCalibrated) {
        return channel;
    }
    return fCalibA0 + fCalibA1 * channel + fCalibA2 * channel * channel;
}

Double_t TracknHistogram::EnergyToChannel(Double_t energy) const
{
    if (!fIsCalibrated) {
        return energy;
    }
    // For quadratic calibration: a2 * ch^2 + a1 * ch + (a0 - E) = 0
    if (std::abs(fCalibA2) > 1e-12) {
        const Double_t c = fCalibA0 - energy;
        const Double_t disc = fCalibA1 * fCalibA1 - 4.0 * fCalibA2 * c;
        if (disc >= 0.0) {
            return (-fCalibA1 + std::sqrt(disc)) / (2.0 * fCalibA2);
        }
    }
    // Linear fallback: ch = (E - a0) / a1
    if (std::abs(fCalibA1) > 1e-12) {
        return (energy - fCalibA0) / fCalibA1;
    }
    return energy;
}
