#ifndef RUNBYRUNMANAGER_H
#define RUNBYRUNMANAGER_H

#include <QDialog>
#include <QString>
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
    void onAddPresetAnchor(double energy, double windowCh, double minCounts);
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
    void updatePlot(int detId);
    void showIssueReportModal(int totalSuccess, int totalFailures);

    QMainCanvas            *m_mainCanvas{nullptr};
    std::unique_ptr<RunByRunEngine> m_engine;
    QThread                *m_workerThread{nullptr};

    // Run discovery
    std::vector<QString>    m_runFilePaths;
    QString                 m_commonPrefix;

    // UI Widgets - Configuration
    QLabel                 *m_lblRunsSummary{nullptr};
    QComboBox              *m_comboRefRun{nullptr};
    QSpinBox               *m_spinDetStart{nullptr};
    QSpinBox               *m_spinDetEnd{nullptr};
    QComboBox              *m_comboChannelLength{nullptr};
    QCheckBox              *m_chkQuadratic{nullptr};
    QDoubleSpinBox         *m_spinDriftTolerance{nullptr};

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
    QTableWidget           *m_tablePlotData{nullptr}; // Numeric trend data & diagnostics

    // Cached results
    std::vector<DetectorTrajectory> m_completedTrajectories;
    bool m_isRunning{false};
};

#endif // RUNBYRUNMANAGER_H
