#ifndef RUNBYRUNMANAGER_H
#define RUNBYRUNMANAGER_H

#include <QDialog>
#include <QString>
#include <QWidget>
#include <vector>
#include <memory>
#include "RunByRunEngine.h"

class QTableWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QMainCanvas;
class QThread;
class QSplitter;

// Data point for detector drift trend visualization
struct DriftPlotPoint {
    int runIndex{0};
    int runNumber{0};
    QString runFileName;
    double gain{0.0};
    double chi2ndf{0.0};
    RunCalibStatus status{RunCalibStatus::Success};
    bool isFallback{false};
    int fallbackSourceRun{0};
    bool isValid{false};
};

/**
 * @brief DetectorDriftPlotWidget renders interactive dual-trend curves:
 * 1) Gain a1 (keV/ch) vs Run Number
 * 2) Reduced Chi2 / ndf vs Run Number
 * Supports tooltips, hover crosshair, and double-click to inspect spectra.
 */
class DetectorDriftPlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetectorDriftPlotWidget(QWidget *parent = nullptr);

    void setData(int detectorId, const std::vector<DriftPlotPoint> &points);
    void clearData();

signals:
    void runPointDoubleClicked(int runIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int m_detectorId{-1};
    std::vector<DriftPlotPoint> m_points;
    int m_hoveredIndex{-1};
    QPoint m_mousePos;
};

/**
 * @brief RunByRunManager provides the complete interactive UI dashboard
 * for in-beam run-by-run detector drift alignment, quality monitoring,
 * and legacy Xtrackn .cal / extended .ucal export.
 */
class RunByRunManager : public QDialog {
    Q_OBJECT

public:
    explicit RunByRunManager(QMainCanvas *mainCanvas, QWidget *parent = nullptr);
    ~RunByRunManager() override;

private slots:
    void onBrowseRunsClicked();
    void onAddAnchorClicked();
    void onRemoveAnchorClicked();
    void onClearAnchorsClicked();
    void onReadPeaksFromFileClicked();
    void onGrabParametersClicked();
    void onStartCalibrationClicked();
    void onCancelCalibrationClicked();
    void onExportCalClicked();
    void onExportUcalClicked();
    void onCellDoubleClicked(int row, int col);
    void onDetectorPlotSelectionChanged(int detIdx);

    // Engine worker slots
    void onDetectorStarted(int detId, int totalDetectors);
    void onRunProcessed(int detId, int runIndex, int totalRuns, const RunCalibResult &result);
    void onDetectorFinished(int detId, int successCount, int failureCount);
    void onProgressUpdated(int percent, const QString &statusText);
    void onBatchFinished(bool cancelled, int totalSuccess, int totalFailures);

private:
    void setupUI();
    void discoverRuns();
    void populateAnchorTable();
    void syncAnchorsFromTable();
    void updatePlot(int detId);
    void showIssueReportModal(int totalSuccess, int totalFailures);
    void inspectSpectrumOnCanvas(int row, int col);
    void openInterventionDialog(int row, int col);

    QMainCanvas            *m_mainCanvas{nullptr};
    std::unique_ptr<RunByRunEngine> m_engine;
    QThread                *m_workerThread{nullptr};

    // Run discovery
    std::vector<QString>    m_runFilePaths;
    QString                 m_commonPrefix;

    // UI Widgets - Configuration
    QLabel                 *m_lblRunsSummary{nullptr};
    QSpinBox               *m_spinDetStart{nullptr};
    QSpinBox               *m_spinDetEnd{nullptr};
    QComboBox              *m_comboChannelLength{nullptr};
    QCheckBox              *m_chkQuadratic{nullptr};
    QDoubleSpinBox         *m_spinDriftTolerance{nullptr};
    QDoubleSpinBox         *m_spinMaxChi2{nullptr};

    // Anchors Table
    QTableWidget           *m_tableAnchors{nullptr};
    std::vector<AnchorPeakDef> m_anchors;

    // Execution Controls
    QPushButton            *m_btnStart{nullptr};
    QPushButton            *m_btnCancel{nullptr};
    QPushButton            *m_btnExportCal{nullptr};
    QPushButton            *m_btnExportUcal{nullptr};
    QProgressBar           *m_progressBar{nullptr};
    QLabel                 *m_lblStatusText{nullptr};

    // 2D Status Grid (Rows = Runs, Cols = Detectors)
    QTableWidget           *m_gridMatrix{nullptr};

    // Drift Trend View
    QComboBox              *m_comboPlotDet{nullptr};
    DetectorDriftPlotWidget *m_plotWidget{nullptr};
    QTableWidget           *m_tablePlotData{nullptr}; // Numeric trend data & diagnostics

    // Cached results
    std::vector<DetectorTrajectory> m_completedTrajectories;
    bool m_isRunning{false};
};

#endif // RUNBYRUNMANAGER_H
