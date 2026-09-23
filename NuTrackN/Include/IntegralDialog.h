#ifndef INTEGRALDIALOG_H
#define INTEGRALDIALOG_H

#include <QDialog>
#include <vector>

class QSpinBox;
class QLabel;
class QMainCanvas;

class IntegralDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IntegralDialog(QMainCanvas *canvas, QWidget *parent = nullptr);
    ~IntegralDialog();

    void updateFromCanvas();

private slots:
    void recalculate();

private:
    QMainCanvas *m_canvas;
    bool m_isUpdating;

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
    QLabel *m_fwhmLabel;

    void setupUI();
};

#endif // INTEGRALDIALOG_H
