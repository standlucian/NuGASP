#ifndef DISPLAYPARAMSDIALOG_H
#define DISPLAYPARAMSDIALOG_H

#include <QDialog>

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QRadioButton;
class QMainCanvas;

class DisplayParamsDialog : public QDialog {
    Q_OBJECT

public:
    explicit DisplayParamsDialog(QMainCanvas *mainCanvas, QWidget *parent = nullptr);
    ~DisplayParamsDialog() override = default;

private slots:
    void applySettings();
    void onAccept();

private:
    void setupUI();
    void loadCurrentValues();

    QMainCanvas    *m_mainCanvas{nullptr};

    QDoubleSpinBox *m_spinLinearHeadroom{nullptr};
    QDoubleSpinBox *m_spinLogHeadroom{nullptr};
    QCheckBox      *m_chkGridX{nullptr};
    QCheckBox      *m_chkGridY{nullptr};
    QSpinBox       *m_spinZoomWidth{nullptr};
    QRadioButton   *m_radioLinearY{nullptr};
    QRadioButton   *m_radioLogY{nullptr};
    QDoubleSpinBox *m_spinXMin{nullptr};
    QDoubleSpinBox *m_spinXMax{nullptr};
    QDoubleSpinBox *m_spinYMin{nullptr};
    QDoubleSpinBox *m_spinYMax{nullptr};
};

#endif // DISPLAYPARAMSDIALOG_H
