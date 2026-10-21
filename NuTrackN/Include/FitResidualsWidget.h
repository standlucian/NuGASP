#ifndef FITRESIDUALSWIDGET_H
#define FITRESIDUALSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include <QString>

class TH1F;
class TF1;
class QMainCanvas;

struct ResidualPoint {
    double channel{0.0};
    double energy{-1.0};
    double dataCount{0.0};
    double fitCount{0.0};
    double rawResidual{0.0}; // data - fit
    double error{1.0};       // Poisson sigma
    double pull{0.0};        // (data - fit) / error
};

class QFitResidualsWidget : public QWidget
{
    Q_OBJECT
public:
    enum class DisplayMode {
        Pulls,      // Standardized pulls: (Data - Fit) / Sigma
        RawCounts   // Raw residual: Data - Fit (counts)
    };

    explicit QFitResidualsWidget(QWidget *parent = nullptr);
    virtual ~QFitResidualsWidget() = default;

    void setMainCanvas(QMainCanvas *canvas) { m_mainCanvas = canvas; }

    void updateResiduals(TH1F *hist, TF1 *fitFunc, double xMin, double xMax,
                         const QString &title = QString(), double chi2 = 0.0, int ndf = 1);
    void clearResiduals();

    bool hasData() const { return !m_points.empty(); }
    DisplayMode displayMode() const { return m_mode; }
    void setDisplayMode(DisplayMode mode);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setupUI();
    void updateHeaderReadout();

    QMainCanvas *m_mainCanvas{nullptr};
    DisplayMode m_mode{DisplayMode::Pulls};

    QWidget *m_headerWidget{nullptr};
    QLabel *m_lblTitle{nullptr};
    QLabel *m_lblMetrics{nullptr};
    QPushButton *m_btnMode{nullptr};
    QPushButton *m_btnClose{nullptr};

    QString m_fitTitle;
    double m_chi2{0.0};
    int m_ndf{1};
    double m_redChi2{0.0};
    double m_rmsResidual{0.0};
    double m_meanRelPct{0.0};
    double m_maxAbsPull{0.0};
    double m_maxAbsRaw{0.0};
    bool m_isCalibrated{false};
    double m_xMin{0.0};
    double m_xMax{0.0};

    std::vector<ResidualPoint> m_points;

    int m_hoveredIndex{-1};
    QRectF m_plotRect;
};

#endif // FITRESIDUALSWIDGET_H
