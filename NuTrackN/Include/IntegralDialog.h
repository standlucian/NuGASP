#ifndef INTEGRALDIALOG_H
#define INTEGRALDIALOG_H

#include <QDialog>
#include <vector>

class QSpinBox;
class QLabel;
class QGroupBox;
class QComboBox;
class QMainCanvas;
struct IntegratedPeak;

class IntegralDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IntegralDialog(QMainCanvas *canvas, QWidget *parent = nullptr);
    ~IntegralDialog();

    void updateFromCanvas();

protected:
    void closeEvent(QCloseEvent *event) override;
    void done(int r) override;

private slots:
    void recalculate();

private:
    QMainCanvas *m_canvas;
    bool m_isUpdating;
    bool m_hasUserModified{false};
    bool m_hasLeftBg{false};
    bool m_hasRightBg{false};
    int m_selectedPeakIdx{0};
    int m_selectedRightBgIdx{1};

    QGroupBox *m_bgLeftGroup{nullptr};
    QGroupBox *m_intGroup{nullptr};
    QGroupBox *m_bgRightGroup{nullptr};
    QComboBox *m_peakCombo{nullptr};
    QComboBox *m_bgRightCombo{nullptr};
    QComboBox *m_bgMethodCombo{nullptr};

    QSpinBox *m_bgLeftStart;
    QSpinBox *m_bgLeftEnd;
    QSpinBox *m_intStart;
    QSpinBox *m_intEnd;
    QSpinBox *m_bgRightStart;
    QSpinBox *m_bgRightEnd;

    QLabel *m_grossAreaLabel;
    QLabel *m_bgAreaLabel;
    QLabel *m_netAreaLabel;
    QLabel *m_centroidLabel;
    QLabel *m_fwhmTitleLabel{nullptr};
    QLabel *m_fwhmLabel;

    void setupUI();
    void updateResultLabels(const std::vector<IntegratedPeak> &peaks);
};

#endif // INTEGRALDIALOG_H
