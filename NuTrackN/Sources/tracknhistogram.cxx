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
      fSourceFilePath(other.fSourceFilePath) {}

TracknHistogram &TracknHistogram::operator=(const TracknHistogram &other) {
  if (this != &other) {
    TH1F::operator=(other);
    fIsCalibrated = other.fIsCalibrated;
    fCalibA0 = other.fCalibA0;
    fCalibA1 = other.fCalibA1;
    fCalibA2 = other.fCalibA2;
    fSourceFilePath = other.fSourceFilePath;
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// Handles mouse and canvas interaction events for this histogram.
///
/// Forwards to base TH1F::ExecuteEvent for standard ROOT pad interactions.
////////////////////////////////////////////////////////////////////////////////
void TracknHistogram::ExecuteEvent(Int_t event, Int_t px, Int_t py) {
  TH1F::ExecuteEvent(event, px, py);
}

void TracknHistogram::SetCalibration(Double_t a0, Double_t a1, Double_t a2) {
  fCalibA0 = a0;
  fCalibA1 = a1;
  fCalibA2 = a2;
  fIsCalibrated = (std::abs(a1) > 1e-12 || std::abs(a2) > 1e-12);
}

Double_t TracknHistogram::ChannelToEnergy(Double_t channel) const {
  if (!fIsCalibrated) {
    return channel;
  }
  return fCalibA0 + fCalibA1 * channel + fCalibA2 * channel * channel;
}

Double_t TracknHistogram::EnergyToChannel(Double_t energy) const {
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
