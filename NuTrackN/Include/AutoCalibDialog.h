#ifndef AUTOCALIBDIALOG_H
#define AUTOCALIBDIALOG_H

#include <QDialog>
#include <vector>
#include <QString>

class QComboBox;
class QRadioButton;
class QLineEdit;
class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;
class QMainCanvas;
class TracknHistogram;

struct StandardIsotope {
    QString name;
    std::vector<double> energies;
    std::vector<double> errors;
};

class AutoCalibDialog : public QDialog {
    Q_OBJECT

public:
    explicit AutoCalibDialog(QMainCanvas *mainCanvas, QWidget *parent = nullptr);
    ~AutoCalibDialog() override = default;

private slots:
    void onSourceTypeChanged();
    void onSourceSelected(int index);
    void onBrowseFile();
    void runSearchAndMatch();
    void applyCalibration();

private:
    void setupUI();
    void initIsotopes();
    std::vector<double> getSelectedReferenceEnergies() const;

    QMainCanvas            *m_mainCanvas{nullptr};
    std::vector<StandardIsotope> m_isotopes;

    QRadioButton           *m_radioSource{nullptr};
    QRadioButton           *m_radioManual{nullptr};
    QRadioButton           *m_radioFile{nullptr};

    QComboBox              *m_comboSources{nullptr};
    QLineEdit              *m_editManualEnergies{nullptr};
    QLineEdit              *m_editFilePath{nullptr};

    QDoubleSpinBox         *m_spinMinSlope{nullptr};
    QDoubleSpinBox         *m_spinMaxSlope{nullptr};
    QDoubleSpinBox         *m_spinMaxOffset{nullptr};

    QDoubleSpinBox         *m_spinSigma{nullptr};
    QDoubleSpinBox         *m_spinThreshold{nullptr};

    QTableWidget           *m_tableMatches{nullptr};
    QLabel                 *m_lblResultStatus{nullptr};
    QPushButton            *m_btnApply{nullptr};

    double m_bestA0{0.0};
    double m_bestA1{1.0};
    double m_bestA2{0.0};
    int    m_matchedCount{0};
};

#endif // AUTOCALIBDIALOG_H
