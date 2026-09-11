#ifndef TRACKNHISTOGRAM_H
#define TRACKNHISTOGRAM_H

#include "TH1F.h"
#include <string>

/**
 * @brief TracknHistogram extends ROOT's TH1F with gamma-spectroscopy metadata,
 *        calibration management, and event handling for NuTrackN.
 */
class TracknHistogram : public TH1F
{
public:
    TracknHistogram();
    TracknHistogram(const char *name, const char *title, Int_t nbins, Double_t xlow, Double_t xup);
    TracknHistogram(const TracknHistogram &other);
    TracknHistogram &operator=(const TracknHistogram &other);
    virtual ~TracknHistogram() override = default;

    /**
     * @brief Handles mouse and canvas interaction events for this histogram.
     */
    void ExecuteEvent(Int_t event, Int_t px, Int_t py) override;

    /**
     * @brief Sets the polynomial energy calibration coefficients: E(ch) = a0 + a1*ch + a2*ch^2
     */
    void SetCalibration(Double_t a0, Double_t a1, Double_t a2 = 0.0);

    /**
     * @brief Converts a spectrum channel number to energy (keV) using current calibration.
     */
    Double_t ChannelToEnergy(Double_t channel) const;

    /**
     * @brief Converts energy (keV) to the corresponding spectrum channel number.
     */
    Double_t EnergyToChannel(Double_t energy) const;

    bool IsCalibrated() const { return fIsCalibrated; }
    Double_t GetCalibA0() const { return fCalibA0; }
    Double_t GetCalibA1() const { return fCalibA1; }
    Double_t GetCalibA2() const { return fCalibA2; }

    void SetSourceFilePath(const std::string &path) { fSourceFilePath = path; }
    const std::string &GetSourceFilePath() const { return fSourceFilePath; }

    /**
     * @brief Loads spectrum data from a file (supports binary uint32 and ASCII formats).
     * @param filename Path to the spectrum file.
     * @return True if loading was successful, false otherwise.
     */
    bool LoadFromFile(const std::string &filename);

    /**
     * @brief Loads spectrum data from an in-memory vector of channel counts.
     * @param data Vector of channel count values.
     * @param sourcePath Optional path to the source file for display/provenance.
     * @return True if data was non-empty and loaded, false otherwise.
     */
    bool LoadFromData(const std::vector<double> &data, const std::string &sourcePath = "");

private:
    bool fIsCalibrated{false};
    Double_t fCalibA0{0.0};
    Double_t fCalibA1{1.0};
    Double_t fCalibA2{0.0};
    std::string fSourceFilePath;
};

#endif // TRACKNHISTOGRAM_H
