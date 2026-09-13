#include "tracknhistogram.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

TracknHistogram::TracknHistogram() : TH1F() {}

TracknHistogram::TracknHistogram(const char *name, const char *title,
                                 Int_t nbins, Double_t xlow, Double_t xup)
    : TH1F(name, title, nbins, xlow, xup) {}

TracknHistogram::TracknHistogram(const TracknHistogram &other)
    : TH1F(other), fIsCalibrated(other.fIsCalibrated), fCalibA0(other.fCalibA0),
      fCalibA1(other.fCalibA1), fCalibA2(other.fCalibA2),
      fSourceFilePath(other.fSourceFilePath),
      fCalibSegments(other.fCalibSegments) {}

TracknHistogram &TracknHistogram::operator=(const TracknHistogram &other) {
  if (this != &other) {
    TH1F::operator=(other);
    fIsCalibrated = other.fIsCalibrated;
    fCalibA0 = other.fCalibA0;
    fCalibA1 = other.fCalibA1;
    fCalibA2 = other.fCalibA2;
    fSourceFilePath = other.fSourceFilePath;
    fCalibSegments = other.fCalibSegments;
  }
  return *this;
}

TObject *TracknHistogram::Clone(const char *newname) const {
  TracknHistogram *copy = new TracknHistogram(*this);
  if (newname && newname[0]) {
    copy->SetName(newname);
  }
  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// Handles mouse and canvas interaction events for this histogram.
///
/// Forwards to base TH1F::ExecuteEvent for standard ROOT pad interactions.
////////////////////////////////////////////////////////////////////////////////
void TracknHistogram::ExecuteEvent(Int_t event, Int_t px, Int_t py) {
  TH1F::ExecuteEvent(event, px, py);
}

//==============================================================================
// TracknHistogram::SetCalibration
//==============================================================================
// Stores polynomial energy calibration coefficients:
//   E(channel) = a0 + a1 * channel + a2 * channel^2
//
// Automatically updates the fIsCalibrated flag if any slope or quadratic terms
// are non-zero.
//==============================================================================
void TracknHistogram::SetCalibration(Double_t a0, Double_t a1, Double_t a2) {
  fCalibA0 = a0;
  fCalibA1 = a1;
  fCalibA2 = a2;
  fCalibSegments.clear();
  CalibSegment seg;
  seg.maxChannel = 1e9;
  seg.coeffs = {a0, a1, a2};
  fCalibSegments.push_back(seg);
  fIsCalibrated = (std::abs(a1) > 1e-12 || std::abs(a2) > 1e-12);
}

//==============================================================================
// TracknHistogram::SetSegmentedCalibration
//==============================================================================
// Stores piecewise polynomial energy calibration segments (e.g. from GASP .mcal).
//==============================================================================
void TracknHistogram::SetSegmentedCalibration(const std::vector<CalibSegment> &segments) {
  fCalibSegments = segments;
  if (!fCalibSegments.empty()) {
    const auto &c = fCalibSegments[0].coeffs;
    fCalibA0 = (c.size() > 0) ? c[0] : 0.0;
    fCalibA1 = (c.size() > 1) ? c[1] : 1.0;
    fCalibA2 = (c.size() > 2) ? c[2] : 0.0;
    fIsCalibrated = true;
  } else {
    ClearCalibration();
  }
}

//==============================================================================
// TracknHistogram::ClearCalibration
//==============================================================================
// Clears / disables energy calibration, reverting readouts to raw channels.
//==============================================================================
void TracknHistogram::ClearCalibration() {
  fCalibA0 = 0.0;
  fCalibA1 = 1.0;
  fCalibA2 = 0.0;
  fCalibSegments.clear();
  fIsCalibrated = false;
}

//==============================================================================
// TracknHistogram::ChannelToEnergy
//==============================================================================
// Converts a spectrum channel number into calibrated physical energy (keV)
// using either piecewise segments or quadratic calibration polynomial.
//
// If uncalibrated, returns the channel number unchanged.
//==============================================================================
Double_t TracknHistogram::ChannelToEnergy(Double_t channel) const {
  if (!fIsCalibrated) {
    return channel;
  }
  if (!fCalibSegments.empty()) {
    const CalibSegment *activeSeg = &fCalibSegments.back();
    for (const auto &seg : fCalibSegments) {
      if (channel <= seg.maxChannel) {
        activeSeg = &seg;
        break;
      }
    }
    const auto &c = activeSeg->coeffs;
    if (c.empty()) return channel;
    // Horner's method: E = c0 + ch * (c1 + ch * (c2 + ...))
    Double_t energy = c.back();
    for (int i = static_cast<int>(c.size()) - 2; i >= 0; --i) {
      energy = c[i] + channel * energy;
    }
    return energy;
  }
  return fCalibA0 + fCalibA1 * channel + fCalibA2 * channel * channel;
}

//==============================================================================
// TracknHistogram::EnergyToChannel
//==============================================================================
// Inverts the energy calibration to find the channel corresponding to a given energy.
// For piecewise segments, locates the appropriate segment and solves via Newton-Raphson.
//==============================================================================
Double_t TracknHistogram::EnergyToChannel(Double_t energy) const {
  if (!fIsCalibrated) {
    return energy;
  }
  if (!fCalibSegments.empty()) {
    // Locate the matching segment by comparing energy to segment maxChannel energy
    const CalibSegment *activeSeg = &fCalibSegments.back();
    for (const auto &seg : fCalibSegments) {
      // Evaluate energy at seg.maxChannel
      const auto &c = seg.coeffs;
      if (c.empty()) continue;
      Double_t eMax = c.back();
      for (int i = static_cast<int>(c.size()) - 2; i >= 0; --i) {
        eMax = c[i] + seg.maxChannel * eMax;
      }
      if (energy <= eMax) {
        activeSeg = &seg;
        break;
      }
    }

    const auto &c = activeSeg->coeffs;
    if (c.empty()) return energy;
    if (c.size() == 1) return 0.0;

    // Initial guess from linear part: ch0 = (energy - c[0]) / c[1]
    Double_t ch = (std::abs(c[1]) > 1e-12) ? (energy - c[0]) / c[1] : energy;

    // Newton-Raphson iteration
    for (int iter = 0; iter < 20; ++iter) {
      Double_t p = c.back();
      Double_t p_prime = 0.0;
      for (int i = static_cast<int>(c.size()) - 2; i >= 0; --i) {
        p_prime = p + ch * p_prime;
        p = c[i] + ch * p;
      }
      Double_t diff = p - energy;
      if (std::abs(diff) < 1e-7) {
        return ch;
      }
      if (std::abs(p_prime) < 1e-12) {
        break;
      }
      ch -= diff / p_prime;
    }
    return ch;
  }

  // Fallback for simple quadratic
  if (std::abs(fCalibA2) > 1e-12) {
    const Double_t c = fCalibA0 - energy;
    const Double_t disc = fCalibA1 * fCalibA1 - 4.0 * fCalibA2 * c;
    if (disc >= 0.0) {
      return (-fCalibA1 + std::sqrt(disc)) / (2.0 * fCalibA2);
    }
  }
  if (std::abs(fCalibA1) > 1e-12) {
    return (energy - fCalibA0) / fCalibA1;
  }
  return energy;
}

//==============================================================================
// isAsciiFile
//==============================================================================
// Helper function to detect whether a spectrum file is plain ASCII text or binary.
// Checks if all characters fall within the standard 7-bit ASCII range (0..127).
//==============================================================================
static bool isAsciiFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    return false;
  }
  char c;
  while (file.get(c)) {
    if (static_cast<unsigned char>(c) > 127) {
      return false;
    }
  }
  return true;
}

//==============================================================================
// TracknHistogram::LoadFromFile
//==============================================================================
// Loads spectrum data from a file into this histogram.
// Supports both:
//   1. Plain ASCII text files (space/newline separated integers).
//   2. Binary files containing raw 32-bit unsigned integers (uint32_t),
//      as typically produced by MCA/ADC acquisition systems (.spe, .dat).
//
// Automatically expands histogram bin count if the input data length exceeds
// the current number of bins, clears old content, resets axes, and stores
// the file path for provenance.
//==============================================================================
bool TracknHistogram::LoadFromFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::in | std::ios::binary);
  if (!file) {
    std::cerr << "Error: could not open spectrum file: " << filename
              << std::endl;
    return false;
  }

  std::vector<uint32_t> data;
  uint32_t value = 0;
  std::string line;

  if (isAsciiFile(filename)) {
    file.close();
    std::ifstream asciiFile(filename);
    while (std::getline(asciiFile, line)) {
      std::istringstream iss(line);
      while (iss >> value) {
        data.push_back(value);
      }
    }
  } else {
    while (file.read(reinterpret_cast<char *>(&value), sizeof(value))) {
      data.push_back(value);
    }
  }

  if (data.empty()) {
    std::cerr << "Warning: spectrum file contains no data: " << filename
              << std::endl;
    return false;
  }

  Reset();

  // Adjust binning if incoming spectrum is larger than default
  if (static_cast<Int_t>(data.size()) > GetNbinsX()) {
    SetBins(static_cast<Int_t>(data.size()), 0.0,
            static_cast<Double_t>(data.size()));
  }

  for (std::size_t i = 0; i < data.size(); ++i) {
    SetBinContent(static_cast<Int_t>(i + 1), data[i]);
  }

  GetXaxis()->UnZoom();
  GetYaxis()->UnZoom();
  SetSourceFilePath(filename);

  return true;
}

//==============================================================================
// TracknHistogram::LoadFromData
//==============================================================================
// Loads spectrum data directly from an in-memory vector of channel counts.
// Adjusts the histogram bins to exactly match the data size, resets axes,
// and optionally records the file path.
//==============================================================================
bool TracknHistogram::LoadFromData(const std::vector<double> &data, const std::string &sourcePath) {
  if (data.empty()) {
    return false;
  }

  Reset();

  // Resize histogram bins to match incoming spectrum length
  SetBins(static_cast<Int_t>(data.size()), 0.0, static_cast<Double_t>(data.size()));

  for (std::size_t i = 0; i < data.size(); ++i) {
    SetBinContent(static_cast<Int_t>(i + 1), data[i]);
  }

  GetXaxis()->UnZoom();
  GetYaxis()->UnZoom();
  if (!sourcePath.empty()) {
    SetSourceFilePath(sourcePath);
  }

  return true;
}

//==============================================================================
// TracknHistogram::GetBinData
//==============================================================================
// Retrieves all channel counts from bin 1 through NbinsX into a std::vector.
//==============================================================================
std::vector<double> TracknHistogram::GetBinData() const {
  Int_t nBins = GetNbinsX();
  std::vector<double> data;
  data.reserve(nBins);
  for (Int_t i = 1; i <= nBins; ++i) {
    data.push_back(GetBinContent(i));
  }
  return data;
}
