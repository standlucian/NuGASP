#ifndef RUNBYRUNENGINE_H
#define RUNBYRUNENGINE_H

#include <QObject>
#include <QString>
#include <vector>
#include <memory>
#include <atomic>
#include "SpectrumImportDialog.h"

// Status for an individual (Detector, Run) calibration
enum class RunCalibStatus {
    Success,
    NoPeakInWindow,
    LowStatistics,
    BadFWHM,
    BadChi2,
    ExcessiveDrift,
    FileError,
    RefAnchorFailed
};

// Configuration for a single anchor line
struct AnchorPeakDef {
    double physicalEnergy{0.0};    // Known physical energy in keV (e.g. 1460.8)
    double energyError{0.0};       // Known energy error in keV
    double initialChannel{0.0};    // Approximate reference channel (0 = auto-find around energy / gain)
    double searchWindowCh{15.0};   // +- delta channel window
    double minCounts{25.0};        // Minimum peak area or amplitude
    double expectedFwhmCh{5.0};    // Expected FWHM in channels
};

// Results of fitting a single anchor line in a given run
struct FittedAnchor {
    double energy{0.0};
    double energyErr{0.0};
    double centroidCh{0.0};
    double centroidErrCh{0.0};
    double fwhmCh{0.0};
    double area{0.0};
    double chi2{0.0};
    bool   isValid{false};
};

// Result for a specific (Run, Detector)
struct RunCalibResult {
    int runNumber{0};
    QString runFileName;
    QString fullFilePath;
    int detectorId{0};
    RunCalibStatus status{RunCalibStatus::Success};
    QString failureReason;
    bool isFallbackFromPrevious{false};
    int fallbackSourceRun{0};

    // Calibration coefficients: E = a0 + a1*ch (+ a2*ch^2)
    int numCoeffs{2};
    double a0{0.0};
    double a1{1.0};
    double a2{0.0};

    // Uncertainty and goodness-of-fit metrics (for .ucal)
    double sigmaA0{0.0};
    double sigmaA1{0.0};
    double sigmaA2{0.0};
    double covA0A1{0.0};
    double sRes{0.0};         // Residual standard error (in keV)
    double chi2NDF{0.0};

    std::vector<FittedAnchor> fittedAnchors;
};

// Summary of an entire detector's trajectory through all runs
struct DetectorTrajectory {
    int detectorId{0};
    std::vector<RunCalibResult> runResults;
    std::vector<double> lastValidCentroids;
    int successCount{0};
    int failureCount{0};
};

// Configuration passed into the batch engine
struct RunByRunConfig {
    std::vector<QString> runFilePaths; // In sequence order (e.g. Run 1 to Run N)
    int detectorStart{0};              // e.g. 0
    int detectorEnd{24};               // e.g. 24
    int spectrumChannels{65536};       // e.g. 65536 (64k), 4096 (4k), etc.
    SpectrumFormat spectrumFormat{SpectrumFormat::LongInt32};
    std::vector<AnchorPeakDef> anchors;
    bool useQuadratic{false};          // true = quadratic (a0, a1, a2); false = linear (a0, a1)
    double maxAllowedDriftCh{25.0};    // Max allowed drift from last valid centroid before failure
    double maxAllowedChi2NDF{5.0};     // Max allowed reduced Chi2/ndf before flagging/rejection
};

/**
 * @brief RunByRunEngine executes detector-by-detector trajectory tracking across all runs.
 */
class RunByRunEngine : public QObject {
    Q_OBJECT

public:
    explicit RunByRunEngine(QObject *parent = nullptr);
    ~RunByRunEngine() override;

    void setConfig(const RunByRunConfig &config);
    const RunByRunConfig& getConfig() const { return m_config; }

public slots:
    // Start batch calibration (can be called asynchronously)
    void run();

public:
    // Request non-blocking cancellation
    void requestStop();
    bool isStopRequested() const { return m_stopRequested.load(); }

    // Retrieve results after completion
    const std::vector<DetectorTrajectory>& getTrajectories() const { return m_trajectories; }
    std::vector<RunCalibResult> getAllFailures() const;

    // Helper to fit a single anchor peak within a window
    static FittedAnchor fitAnchorPeak(const std::vector<double> &spectrum,
                                     double centerCh, double windowCh,
                                     double minCounts, double energy);

    // Helper to compute polynomial calibration coefficients and uncertainties
    static bool computeCalibration(const std::vector<FittedAnchor> &anchors,
                                   bool useQuadratic,
                                   RunCalibResult &outResult,
                                   double maxAllowedChi2NDF = 5.0);

signals:
    void detectorStarted(int detectorId, int totalDetectors);
    void runProcessed(int detectorId, int runIndex, int totalRuns, const RunCalibResult &result);
    void detectorFinished(int detectorId, int successCount, int failureCount);
    void progressUpdated(int overallPercent, const QString &statusText);
    void batchFinished(bool cancelled, int totalSuccess, int totalFailures);

private:
    RunByRunConfig m_config;
    std::atomic<bool> m_stopRequested{false};
    std::vector<DetectorTrajectory> m_trajectories;
};

Q_DECLARE_METATYPE(RunCalibResult)

#endif // RUNBYRUNENGINE_H
