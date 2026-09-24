#ifndef EFFICIENCYDIALOG_H
#define EFFICIENCYDIALOG_H

#include <QDialog>
#include <vector>
#include <QString>

class QRadioButton;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QGroupBox;
class QMainCanvas;

enum class EfficiencyType {
    None = 0,
    Polynomial = 1,
    Spectrum = 2
};

struct EfficiencyConfig {
    EfficiencyType type{EfficiencyType::None};
    std::vector<double> coeffs; // a0..a5: ln(Eff) = sum(a_i * (ln E)^i)
    QString spectrumFile;
    std::vector<double> spectrumData;
    double scalingFactor{1.0};
};

class EfficiencyDialog : public QDialog {
    Q_OBJECT

public:
    explicit EfficiencyDialog(QMainCanvas *mainCanvas, QWidget *parent = nullptr);
    ~EfficiencyDialog() override = default;

private slots:
    void onTypeChanged();
    void updateTestCalculation();
    void onLoadFile();
    void onSaveFile();
    void onBrowseSpectrum();
    void applySettings();
    void onAccept();

private:
    void setupUI();
    void loadCurrentConfig();

    QMainCanvas        *m_mainCanvas{nullptr};

    QRadioButton       *m_radioNone{nullptr};
    QRadioButton       *m_radioPoly{nullptr};
    QRadioButton       *m_radioSpec{nullptr};

    QGroupBox          *m_grpPoly{nullptr};
    std::vector<QDoubleSpinBox*> m_spinCoeffs;

    QDoubleSpinBox     *m_spinTestEnergy{nullptr};
    QLabel             *m_lblTestResult{nullptr};

    QGroupBox          *m_grpSpec{nullptr};
    QLineEdit          *m_editSpecPath{nullptr};

    QDoubleSpinBox     *m_spinScaling{nullptr};
};

#endif // EFFICIENCYDIALOG_H
