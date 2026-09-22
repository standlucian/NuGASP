#ifndef TRACKFITDIALOG_H
#define TRACKFITDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QString>
#include <vector>

class QTableWidget;
class QTableWidgetItem;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QScrollArea;
class QGridLayout;
class QMainCanvas;
class TracknHistogram;

/**
 * @brief Represents a reference gamma-ray emission line.
 */
struct ReferenceGamma {
    double energy{0.0};    // Energy in keV
    QString label;         // Source label (e.g., "152Eu 121.78")
    bool enabled{true};    // Whether this line is enabled by default
};

/**
 * @brief Analysis result for a single reference peak during TrackFit / AutoTrace,
 *        including local spectrum slice and Gaussian fit parameters for visual inspection.
 */
struct TrackFitPeakResult {
    double refEnergy{0.0};       // Reference energy (keV)
    double expectedChannel{0.0};  // Channel predicted from current calibration / gain
    double fittedCentroid{0.0};   // Fitted Gaussian centroid channel
    double centroidErr{0.0};      // Centroid uncertainty
    double fwhmChannel{0.0};      // Fitted FWHM in channels
    double fwhmEnergy{0.0};       // Fitted FWHM in keV
    double netArea{0.0};          // Peak net area counts
    double netAreaErr{0.0};       // Net area uncertainty
    double calcEnergy{0.0};       // Recalibrated energy from polynomial fit (keV)
    double residualEnergy{0.0};   // Residual: E_calc - E_ref (keV)
    bool isIncluded{true};        // Included in the regression fit
    QString status;               // "Matched", "Low Counts", "Out of Range", "Excluded"

    // Visual fit parameters for interactive inspection
    int fitStartBin{0};
    int fitEndBin{0};
    std::vector<double> localCounts;
    double fitAmplitude{0.0};
    double fitSigma{0.0};
    double fitBaselineOffset{0.0};
    double fitBaselineSlope{0.0};
};

/**
 * @brief Interactive visual widget rendering a single fitted peak region
 *        with raw spectrum counts, overlaid Gaussian curve, baseline, centroid,
 *        and residual badge.
 */
class PeakFitTileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PeakFitTileWidget(int peakIndex = -1, bool isInteractive = false, QWidget *parent = nullptr);
    ~PeakFitTileWidget() override = default;

    void setPeakData(const TrackFitPeakResult &res, int peakIndex);
    void clearData();
    int getPeakIndex() const { return m_peakIndex; }
    void setShowFooter(bool show) { m_showFooter = show; update(); }
    bool getShowFooter() const { return m_showFooter; }

signals:
    void tileClicked(int peakIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_peakIndex{-1};
    bool m_isInteractive{false};
    bool m_hasData{false};
    bool m_showFooter{true};
    TrackFitPeakResult m_res;
};

/**
 * @brief Dedicated visual inspection modal dialog that opens automatically
 *        upon running AutoTrace, showing all fitted peaks simultaneously in a
 *        multi-column scrollable grid (Xtrackn style).
 */
class TrackFitInspectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrackFitInspectionDialog(const std::vector<TrackFitPeakResult> &peakResults,
                                     int polyOrder = 1,
                                     QWidget *parent = nullptr);
    ~TrackFitInspectionDialog() override = default;

    const std::vector<TrackFitPeakResult>& getResults() const { return m_peaks; }
    const std::vector<double>& getCalibCoeffs() const { return m_calibCoeffs; }
    int getPolyOrder() const { return m_polyOrder; }
    double getRmsResidual() const { return m_rmsResidual; }
    bool isAccepted() const { return m_accepted; }

private slots:
    void onTileClicked(int peakIndex);
    void onPolyOrderChanged(int orderIndex);
    void onAcceptCalibration();

private:
    void recalculateRegression();
    void updateHeaderAndTiles();

    std::vector<TrackFitPeakResult> m_peaks;
    std::vector<double> m_calibCoeffs;
    int m_polyOrder{1};
    double m_rmsResidual{0.0};
    double m_fwhmIntercept{0.0};
    double m_fwhmSlope{0.0};
    bool m_accepted{false};

    QLabel *m_lblSummary{nullptr};
    QLabel *m_lblEquation{nullptr};
    QLabel *m_lblRms{nullptr};
    QLabel *m_lblFwhm{nullptr};
    QComboBox *m_comboPolyOrder{nullptr};
    std::vector<PeakFitTileWidget*> m_tiles;
};

/**
 * @brief AutoTrace / TrackFit calibration manager dialog (DT).
 *
 * Automates reference peak search, Gaussian + background fitting,
 * multi-order polynomial energy calibration, resolution (FWHM) calibration,
 * visual fit inspection, and residuals analysis.
 */
class TrackFitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrackFitDialog(QMainCanvas *mainCanvas,
                            TracknHistogram *activeHistogram,
                            int currentDetId = 0,
                            QWidget *parent = nullptr);
    ~TrackFitDialog() override = default;

public slots:
    void onAutoTraceClicked();

private slots:
    void onSourcePresetChanged(int index);
    void onAddCustomLine();
    void onRemoveSelectedLine();
    void onViewAllFitsClicked();
    void onPolyOrderChanged(int order);
    void onTableItemChanged(QTableWidgetItem *item);
    void onTableSelectionChanged();
    void onTileClicked(int peakIndex);
    void onToggleCurrentInclude();
    void onApplyActiveClicked();
    void onApplyAllClicked();
    void onSaveClicked();

private:
    void initSourcePresets();
    void updateReferenceList(const std::vector<ReferenceGamma> &lines);
    void refreshTableDisplay();
    void updateInspectorView(int row);
    void refreshGridTiles();
    bool fitPeakAtChannel(double expectedCh, double searchTolCh, double fitWindowFwhm,
                          TrackFitPeakResult &outRes);
    void performPolynomialFit();

    QMainCanvas *m_mainCanvas{nullptr};
    TracknHistogram *m_activeHist{nullptr};
    int m_currentDetId{0};

    // UI Widgets - Controls
    QComboBox *m_comboPreset{nullptr};
    QDoubleSpinBox *m_spinInitGain{nullptr};
    QDoubleSpinBox *m_spinInitOffset{nullptr};
    QSpinBox *m_spinSearchWindow{nullptr};
    QDoubleSpinBox *m_spinFitWidth{nullptr};
    QComboBox *m_comboPolyOrder{nullptr};

    // Table & Live Inspector (Tab 1)
    QTableWidget *m_tablePeaks{nullptr};
    PeakFitTileWidget *m_inspectorTile{nullptr};
    QLabel *m_lblInspectorDetails{nullptr};
    QPushButton *m_btnToggleInclude{nullptr};

    // Multi-Peak Fit Grid (Tab 2)
    QScrollArea *m_gridScrollArea{nullptr};
    QWidget *m_gridContainer{nullptr};
    QGridLayout *m_gridLayout{nullptr};
    std::vector<PeakFitTileWidget*> m_gridTiles;
    QLabel *m_lblGridSummary{nullptr};

    // Regression summary
    QLabel *m_lblEquation{nullptr};
    QLabel *m_lblRmsResidual{nullptr};
    QLabel *m_lblFwhmEquation{nullptr};

    // State
    std::vector<ReferenceGamma> m_currentRefLines;
    std::vector<TrackFitPeakResult> m_peakResults;
    std::vector<double> m_calibCoeffs; // [A0, A1, A2, ...]
    double m_fwhmIntercept{0.0};
    double m_fwhmSlope{0.0};
    double m_rmsResidualKeV{0.0};
    bool m_fitSuccess{false};
    int m_selectedPeakIndex{0};
};

#endif // TRACKFITDIALOG_H
