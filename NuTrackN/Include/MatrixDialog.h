#ifndef MATRIXDIALOG_H
#define MATRIXDIALOG_H

#include <QDialog>
#include <memory>
#include "MatrixReader.h"
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QRadioButton;
class QButtonGroup;

/**
 * @brief Matrix1DPreviewWidget paints a high-performance 1D spectrum plot preview
 *        with axes, labels, curve fill, and interactive mouse-over readouts.
 */
class Matrix1DPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit Matrix1DPreviewWidget(QWidget *parent = nullptr);
    void setSpectrum(const std::vector<double> &data, const QString &label,
                     bool isCalib = false, double a0 = 0.0, double a1 = 1.0, double a2 = 0.0,
                     const std::vector<double> &bgData = {});
    void clear();

    bool isLogScale() const { return m_logScale; }
    void setLogScale(bool log);

signals:
    void hoverInfoChanged(int ch, double energy, double counts, double bgCounts = -1.0);
    void scaleModeChanged(bool isLog);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect getPlotArea() const;
    int pixelToChannel(int px, const QRect &plotArea) const;
    int channelToPixel(int ch, const QRect &plotArea) const;

    std::vector<double> m_data;
    std::vector<double> m_bgData;
    QString m_label;
    bool m_isCalibrated{false};
    double m_calibA0{0.0};
    double m_calibA1{1.0};
    double m_calibA2{0.0};
    double m_maxVal{0.0};
    int m_hoverCh{-1};

    bool m_logScale{false};
};

/**
 * @brief MatrixDialog handles opening compressed matrices (Open CM),
 *        displays symmetry information, allows projection selection (for non-symmetrical),
 *        configures coincidence background subtraction, and provides 1D preview before loading to pad.
 */
class MatrixDialog : public QDialog {
    Q_OBJECT
public:
    explicit MatrixDialog(std::shared_ptr<MatrixReader> reader,
                          bool isCalibrated = false,
                          double calibA0 = 0.0,
                          double calibA1 = 1.0,
                          double calibA2 = 0.0,
                          QWidget *parent = nullptr);

    ~MatrixDialog() override = default;

signals:
    void loadProjectionRequested(const std::vector<double> &data, const QString &title,
                                 const std::vector<double> &bgData = {}, const QString &bgTitle = QString());

private slots:
    void onProjectionSelectionChanged();
    void onBackgroundConfigChanged();
    void onLoadProjectionClicked();
    void onHoverInfoChanged(int ch, double energy, double counts, double bgCounts = -1.0);

private:
    void setupUI();
    void updatePreview();
    void saveBackgroundConfig();

    std::shared_ptr<MatrixReader> m_reader;
    bool m_isCalibrated{false};
    double m_calibA0{0.0};
    double m_calibA1{1.0};
    double m_calibA2{0.0};

    // UI elements
    QLabel *m_lblSymmetryBanner{nullptr};
    QRadioButton *m_radioProjX{nullptr};
    QRadioButton *m_radioProjY{nullptr};
    QButtonGroup *m_projGroup{nullptr};

    // Background Subtraction controls
    QCheckBox *m_chkEnableBg{nullptr};
    QLabel *m_lblBgModePrompt{nullptr};
    QComboBox *m_comboBgMode{nullptr};
    QLabel *m_lblCorrFactorPrompt{nullptr};
    QDoubleSpinBox *m_spinCorrFactor{nullptr};
    QLabel *m_lblBgHelp{nullptr};

    // 1D Preview Controls
    QPushButton *m_btnLogScale{nullptr};

    Matrix1DPreviewWidget *m_plotPreview{nullptr};
    QLabel *m_lblHoverReadout{nullptr};
    QLabel *m_lblDetails{nullptr};

    QPushButton *m_btnLoadProjection{nullptr};
    QPushButton *m_btnClose{nullptr};
};

/**
 * @brief MatrixGateDialog provides dedicated coincidence gating controls
 *        for the active matrix (Gate CM), showing 1D preview of the sliced cut
 *        with live background subtraction configuration and statistics.
 */
class MatrixGateDialog : public QDialog {
    Q_OBJECT
public:
    explicit MatrixGateDialog(std::shared_ptr<MatrixReader> reader,
                              bool isCalibrated = false,
                              double calibA0 = 0.0,
                              double calibA1 = 1.0,
                              double calibA2 = 0.0,
                              const std::vector<MatrixGateRegion> &initialGates = {},
                              QWidget *parent = nullptr);

    explicit MatrixGateDialog(std::shared_ptr<MatrixReader> reader,
                              bool isCalibrated,
                              double calibA0,
                              double calibA1,
                              double calibA2,
                              int initialGateMin,
                              int initialGateMax,
                              QWidget *parent = nullptr);

    ~MatrixGateDialog() override = default;

signals:
    void loadGateSliceRequested(const std::vector<double> &data, const QString &title, bool asOverlay);

private slots:
    void onGateParametersChanged();
    void onPeakGateChanged(int index);
    void onBackgroundSettingsChanged();
    void onSliceGateClicked();
    void onOverlayGateClicked();
    void onHoverInfoChanged(int ch, double energy, double counts, double bgCounts = -1.0);

private:
    void setupUI();
    void updateGatePreview();
    double channelToEnergy(double ch) const;
    void syncGateSpinboxes();

    std::shared_ptr<MatrixReader> m_reader;
    bool m_isCalibrated{false};
    double m_calibA0{0.0};
    double m_calibA1{1.0};
    double m_calibA2{0.0};

    std::vector<MatrixGateRegion> m_gates;
    int m_peakGateIndex{0};

    QRadioButton *m_radioGateY{nullptr}; // Gate on Y -> project X
    QRadioButton *m_radioGateX{nullptr}; // Gate on X -> project Y
    QLabel *m_lblPeakPrompt{nullptr};
    QComboBox *m_comboPeakGate{nullptr};
    QLabel *m_lblGatesDetail{nullptr};
    QSpinBox *m_spinGateMin{nullptr};
    QSpinBox *m_spinGateMax{nullptr};
    QLabel *m_lblGateWidth{nullptr};
    QLabel *m_lblGateEnergy{nullptr};

    // Background Subtraction controls & stats
    QCheckBox *m_chkEnableBg{nullptr};
    QLabel *m_lblBgModePrompt{nullptr};
    QComboBox *m_comboBgMode{nullptr};
    QLabel *m_lblCorrFactorPrompt{nullptr};
    QDoubleSpinBox *m_spinCorrFactor{nullptr};
    QLabel *m_lblBgStats{nullptr};

    // 1D Preview Controls
    QPushButton *m_btnLogScale{nullptr};

    Matrix1DPreviewWidget *m_plotPreview{nullptr};
    QLabel *m_lblHoverReadout{nullptr};

    QPushButton *m_btnSliceGate{nullptr};
    QPushButton *m_btnOverlayGate{nullptr};
    QPushButton *m_btnClose{nullptr};

    std::vector<double> m_currentSlice;
};

#endif // MATRIXDIALOG_H
