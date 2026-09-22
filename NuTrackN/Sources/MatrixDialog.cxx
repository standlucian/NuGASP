#include "MatrixDialog.h"
#include "MatrixReader.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPainter>
#include <QMouseEvent>
#include <QLocale>
#include <QFileInfo>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>

//==============================================================================
// Matrix1DPreviewWidget Implementation
//==============================================================================

Matrix1DPreviewWidget::Matrix1DPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void Matrix1DPreviewWidget::setSpectrum(const std::vector<double> &data, const QString &label,
                                        bool isCalib, double a0, double a1, double a2,
                                        const std::vector<double> &bgData)
{
    m_data = data;
    m_bgData = bgData;
    m_label = label;
    m_isCalibrated = isCalib;
    m_calibA0 = a0;
    m_calibA1 = a1;
    m_calibA2 = a2;

    m_maxVal = 0.0;
    for (double v : m_data) {
        if (v > m_maxVal) m_maxVal = v;
    }
    for (double v : m_bgData) {
        if (v > m_maxVal) m_maxVal = v;
    }

    m_hoverCh = -1;
    update();
}

void Matrix1DPreviewWidget::clear()
{
    m_data.clear();
    m_bgData.clear();
    m_label.clear();
    m_maxVal = 0.0;
    m_hoverCh = -1;
    update();
}

void Matrix1DPreviewWidget::setLogScale(bool log)
{
    if (m_logScale != log) {
        m_logScale = log;
        emit scaleModeChanged(m_logScale);
        update();
    }
}

QRect Matrix1DPreviewWidget::getPlotArea() const
{
    double effMax = (m_maxVal > 0.0) ? m_maxVal : 10.0;
    QString yMaxStr = QLocale().toString(static_cast<qlonglong>(std::round(effMax)));
    QFont tickFont = font();
    tickFont.setPointSize(8);
    QFontMetrics fm(tickFont);

    int leftMargin = std::max(64, fm.horizontalAdvance(yMaxStr) + 18);
    int rightMargin = 20;
    int topMargin = 26;
    int bottomMargin = 38;

    return rect().adjusted(leftMargin, topMargin, -rightMargin, -bottomMargin);
}

int Matrix1DPreviewWidget::pixelToChannel(int px, const QRect &plotArea) const
{
    if (m_data.empty() || plotArea.width() <= 0) return 0;
    int n = static_cast<int>(m_data.size());
    int relPx = px - plotArea.left();
    int ch = (relPx * n) / plotArea.width();
    return std::max(0, std::min(ch, n - 1));
}

int Matrix1DPreviewWidget::channelToPixel(int ch, const QRect &plotArea) const
{
    if (m_data.empty() || plotArea.width() <= 0) return plotArea.left();
    int n = static_cast<int>(m_data.size());
    if (n <= 1) return plotArea.left();
    double frac = static_cast<double>(ch) / static_cast<double>(n - 1);
    return plotArea.left() + static_cast<int>(std::round(frac * plotArea.width()));
}

void Matrix1DPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_data.empty()) return;

    const QRect plotArea = getPlotArea();
    int mx = event->pos().x();

    if (plotArea.contains(event->pos())) {
        int ch = pixelToChannel(mx, plotArea);
        m_hoverCh = ch;
        double counts = m_data[ch];
        double energy = m_isCalibrated
            ? (m_calibA0 + m_calibA1 * ch + m_calibA2 * ch * ch)
            : static_cast<double>(ch);
        double bgCounts = (!m_bgData.empty() && ch >= 0 && ch < static_cast<int>(m_bgData.size()))
            ? m_bgData[ch]
            : -1.0;

        emit hoverInfoChanged(ch, energy, counts, bgCounts);
        update();
    } else {
        m_hoverCh = -1;
        emit hoverInfoChanged(-1, 0.0, 0.0, -1.0);
        update();
    }
}

void Matrix1DPreviewWidget::leaveEvent(QEvent *)
{
    m_hoverCh = -1;
    emit hoverInfoChanged(-1, 0.0, 0.0, -1.0);
    update();
}

void Matrix1DPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background
    p.fillRect(rect(), QColor("#14161a"));
    p.setPen(QPen(QColor("#2b303c"), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const QRect plotArea = getPlotArea();

    // Plot Border
    p.setPen(QPen(QColor("#3d4452"), 1));
    p.drawRect(plotArea);

    if (m_data.empty() || m_maxVal <= 0.0) {
        p.setPen(QColor("#777e8c"));
        p.drawText(plotArea, Qt::AlignCenter, tr("No spectrum data available for preview."));
        return;
    }

    const int nVis = static_cast<int>(m_data.size());
    const int pW = plotArea.width();
    const int pH = plotArea.height();

    double effMax = (m_maxVal > 0.0) ? m_maxVal : 10.0;
    const double logMax = std::log10(1.0 + effMax);

    // Subtle grid lines (3 horizontal, 3 vertical)
    p.setPen(QPen(QColor(255, 255, 255, 18), 1, Qt::DotLine));
    for (int step = 1; step <= 3; ++step) {
        int y = plotArea.bottom() - (pH * step) / 4;
        p.drawLine(plotArea.left(), y, plotArea.right(), y);

        int x = plotArea.left() + (pW * step) / 4;
        p.drawLine(x, plotArea.top(), x, plotArea.bottom());
    }

    // Build curve points and fill polygon
    QPolygonF linePoly;
    QPolygonF fillPoly;
    fillPoly << QPointF(plotArea.left(), plotArea.bottom());

    for (int px = 0; px < pW; ++px) {
        int c1 = (px * nVis) / pW;
        int c2 = ((px + 1) * nVis) / pW;
        if (c2 <= c1) c2 = c1 + 1;
        if (c2 > nVis) c2 = nVis;

        double cMax = m_data[c1];
        for (int c = c1 + 1; c < c2; ++c) {
            if (m_data[c] > cMax) cMax = m_data[c];
        }

        double normY = 0.0;
        if (m_logScale) {
            normY = (cMax > 0.0 && logMax > 0.0)
                ? (std::log10(1.0 + cMax) / logMax)
                : 0.0;
        } else {
            normY = cMax / effMax;
        }
        normY = std::max(0.0, std::min(1.0, normY));

        double yPos = plotArea.bottom() - normY * (pH - 2);
        double screenX = plotArea.left() + px;

        linePoly << QPointF(screenX, yPos);
        fillPoly << QPointF(screenX, yPos);
    }
    fillPoly << QPointF(plotArea.right(), plotArea.bottom());

    // Shaded fill under curve
    QLinearGradient grad(0, plotArea.top(), 0, plotArea.bottom());
    grad.setColorAt(0.0, QColor(0, 224, 255, 110));
    grad.setColorAt(1.0, QColor(0, 136, 204, 15));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawPolygon(fillPoly);

    // Spectrum curve line
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#00e0ff"), 1.2));
    p.drawPolyline(linePoly);

    // Background curve line (if present)
    if (!m_bgData.empty()) {
        QPolygonF bgLinePoly;
        int nBg = static_cast<int>(m_bgData.size());
        for (int px = 0; px < pW; ++px) {
            int c1 = (px * nBg) / pW;
            int c2 = ((px + 1) * nBg) / pW;
            if (c2 <= c1) c2 = c1 + 1;
            if (c2 > nBg) c2 = nBg;

            double cMax = m_bgData[c1];
            for (int c = c1 + 1; c < c2; ++c) {
                if (m_bgData[c] > cMax) cMax = m_bgData[c];
            }

            double normY = 0.0;
            if (m_logScale) {
                normY = (cMax > 0.0 && logMax > 0.0)
                    ? (std::log10(1.0 + cMax) / logMax)
                    : 0.0;
            } else {
                normY = cMax / effMax;
            }
            normY = std::max(0.0, std::min(1.0, normY));

            double yPos = plotArea.bottom() - normY * (pH - 2);
            double screenX = plotArea.left() + px;
            bgLinePoly << QPointF(screenX, yPos);
        }
        p.setPen(QPen(QColor("#ff4d4d"), 1.6));
        p.drawPolyline(bgLinePoly);
    }

    // Hover hair-line
    if (m_hoverCh >= 0 && m_hoverCh < nVis) {
        int hx = channelToPixel(m_hoverCh, plotArea);
        p.setPen(QPen(QColor("#ffaa00"), 1, Qt::DashLine));
        p.drawLine(hx, plotArea.top(), hx, plotArea.bottom());
    }

    // Top Badges & Text
    QFont badgeFont = font();
    badgeFont.setPointSize(9);
    badgeFont.setBold(true);
    p.setFont(badgeFont);

    // Label badge (top-left inside plot)
    p.setPen(QColor("#00e0ff"));
    QString dispLabel = m_label;
    if (m_logScale) dispLabel += " [Log Y]";
    p.drawText(plotArea.left() + 8, plotArea.top() + 16, dispLabel);

    if (!m_bgData.empty()) {
        int labelWidth = p.fontMetrics().horizontalAdvance(dispLabel);
        p.setPen(QColor("#ff4d4d"));
        p.drawText(plotArea.left() + 16 + labelWidth, plotArea.top() + 16, tr("[Auto BG: Red]"));
    }

    // Max counts readout (top-right inside plot)
    QFont infoFont = font();
    infoFont.setPointSize(8);
    infoFont.setBold(false);
    p.setFont(infoFont);
    p.setPen(QColor("#9da4b0"));
    QString maxStr = QString("Scale Max: %1").arg(QLocale().toString(static_cast<qlonglong>(std::round(effMax))));
    p.drawText(plotArea.right() - p.fontMetrics().horizontalAdvance(maxStr) - 8, plotArea.top() + 16, maxStr);

    // Axis Labels at Bottom
    QFont tickFont = font();
    tickFont.setPointSize(8);
    p.setFont(tickFont);
    p.setPen(QColor("#8c94a4"));

    QString startLabel = QString("Ch 0");
    if (m_isCalibrated) {
        double e0 = m_calibA0;
        startLabel += QString(" (%1 keV)").arg(e0, 0, 'f', 0);
    }
    p.drawText(plotArea.left(), plotArea.bottom() + 17, startLabel);

    int midCh = nVis / 2;
    QString midLabel = QString("Ch %1").arg(midCh);
    p.drawText(plotArea.left() + pW / 2 - p.fontMetrics().horizontalAdvance(midLabel) / 2, plotArea.bottom() + 17, midLabel);

    int endCh = nVis - 1;
    QString endLabel = QString("Ch %1").arg(endCh);
    if (m_isCalibrated) {
        double e1 = m_calibA0 + m_calibA1 * endCh + m_calibA2 * endCh * endCh;
        endLabel += QString(" (%1 keV)").arg(e1, 0, 'f', 0);
    }
    p.drawText(plotArea.right() - p.fontMetrics().horizontalAdvance(endLabel), plotArea.bottom() + 17, endLabel);

    // Y Axis Ticks (drawn to the left of plotArea border)
    QString yMaxValStr = QLocale().toString(static_cast<qlonglong>(std::round(effMax)));
    int yMaxW = p.fontMetrics().horizontalAdvance(yMaxValStr);
    p.drawText(plotArea.left() - yMaxW - 6, plotArea.top() + 12, yMaxValStr);

    if (m_logScale) {
        p.drawText(plotArea.left() - p.fontMetrics().horizontalAdvance("1") - 6, plotArea.bottom() - 1, "1");
    } else {
        p.drawText(plotArea.left() - p.fontMetrics().horizontalAdvance("0") - 6, plotArea.bottom() - 1, "0");
    }
}

//==============================================================================
// MatrixDialog (Open CM) Implementation
//==============================================================================

MatrixDialog::MatrixDialog(std::shared_ptr<MatrixReader> reader,
                           bool isCalibrated,
                           double calibA0,
                           double calibA1,
                           double calibA2,
                           QWidget *parent)
    : QDialog(parent),
      m_reader(reader),
      m_isCalibrated(isCalibrated),
      m_calibA0(calibA0),
      m_calibA1(calibA1),
      m_calibA2(calibA2)
{
    setWindowTitle(tr("Open Compressed Matrix (CM)"));
    resize(880, 680);
    setMinimumSize(780, 580);

    setStyleSheet(
        "QDialog { background-color: #24262b; color: #ffffff; }"
        "QLabel { color: #e6e6e6; font-size: 14px; }"
        "QLabel:disabled { color: #585f6d; }"
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00e0ff; border: 1px solid #444955; border-radius: 6px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QRadioButton { font-size: 14px; color: #ffffff; spacing: 8px; }"
        "QRadioButton::indicator { width: 18px; height: 18px; }"
        "QCheckBox { font-size: 13px; color: #ffffff; spacing: 8px; }"
        "QComboBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox:disabled { background-color: #1a1c22; color: #585f6d; border: 1px solid #2d313b; }"
        "QComboBox::drop-down:disabled { border: none; background-color: transparent; }"
        "QComboBox QAbstractItemView { background-color: #24262b; color: #ffffff; selection-background-color: #0077b6; }"
        "QDoubleSpinBox, QSpinBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QDoubleSpinBox:disabled, QSpinBox:disabled { background-color: #1a1c22; color: #585f6d; border: 1px solid #2d313b; }"
        "QPushButton { font-size: 14px; font-weight: bold; border-radius: 4px; padding: 7px 18px; }"
    );

    setupUI();
    updatePreview();
}

void MatrixDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // 1. Header Banner with File Name & Symmetry Status
    QFrame *headerFrame = new QFrame(this);
    headerFrame->setStyleSheet("background-color: #1a2332; border: 1px solid #23486a; border-radius: 6px; padding: 10px 14px;");
    QVBoxLayout *headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setSpacing(6);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QString titleText = m_reader ? m_reader->getFileName() : tr("Unknown Matrix");
    QLabel *lblTitle = new QLabel(QString("<b>Matrix File:</b> %1").arg(titleText), headerFrame);
    lblTitle->setStyleSheet("font-size: 16px; color: #ffffff;");
    headerLayout->addWidget(lblTitle);

    // Symmetry notice
    m_lblSymmetryBanner = new QLabel(headerFrame);
    if (m_reader && m_reader->isSymmetric()) {
        m_lblSymmetryBanner->setStyleSheet(
            "background-color: #133827; color: #44ffaa; border: 1px solid #287a53; "
            "border-radius: 4px; padding: 6px 12px; font-size: 13px; font-weight: bold;"
        );
        m_lblSymmetryBanner->setText(
            QString("✓ Symmetrical Matrix (%1 x %2 channels) — Projection X and Projection Y are identical.")
                .arg(m_reader->getResolutionX())
                .arg(m_reader->getResolutionY())
        );
    } else if (m_reader) {
        m_lblSymmetryBanner->setStyleSheet(
            "background-color: #3b2810; color: #ffb84d; border: 1px solid #7d501a; "
            "border-radius: 4px; padding: 6px 12px; font-size: 13px; font-weight: bold;"
        );
        m_lblSymmetryBanner->setText(
            QString("⇄ Non-Symmetrical Matrix (%1 x %2 channels) — Asymmetrical axes with independent projections.")
                .arg(m_reader->getResolutionX())
                .arg(m_reader->getResolutionY())
        );
    }
    headerLayout->addWidget(m_lblSymmetryBanner);
    mainLayout->addWidget(headerFrame);

    // 2. Projection Selection Card (For non-symmetrical matrix)
    QGroupBox *grpSelection = new QGroupBox(tr("1D Projection Selection"), this);
    QVBoxLayout *selectionLayout = new QVBoxLayout(grpSelection);
    selectionLayout->setSpacing(8);
    selectionLayout->setContentsMargins(14, 16, 14, 12);

    m_projGroup = new QButtonGroup(this);

    if (m_reader && !m_reader->isSymmetric()) {
        QLabel *lblPrompt = new QLabel(tr("Select which projection spectrum to load into the active pad:"), grpSelection);
        lblPrompt->setStyleSheet("color: #b0b8c6; font-size: 13px;");
        selectionLayout->addWidget(lblPrompt);

        QHBoxLayout *radioLayout = new QHBoxLayout();
        radioLayout->setSpacing(24);

        m_radioProjX = new QRadioButton(
            QString("Projection X (Horizontal Axis — %1 channels)").arg(m_reader->getResolutionX()), grpSelection);
        m_radioProjX->setChecked(true);
        m_projGroup->addButton(m_radioProjX, 0);
        radioLayout->addWidget(m_radioProjX);

        m_radioProjY = new QRadioButton(
            QString("Projection Y (Vertical Axis — %1 channels)").arg(m_reader->getResolutionY()), grpSelection);
        m_projGroup->addButton(m_radioProjY, 1);
        radioLayout->addWidget(m_radioProjY);
        radioLayout->addStretch(1);

        connect(m_radioProjX, &QRadioButton::toggled, this, &MatrixDialog::onProjectionSelectionChanged);
        connect(m_radioProjY, &QRadioButton::toggled, this, &MatrixDialog::onProjectionSelectionChanged);

        selectionLayout->addLayout(radioLayout);
    } else if (m_reader) {
        QLabel *lblSym = new QLabel(
            QString("Total 1D Projection (%1 channels, %2 total integral counts)")
                .arg(m_reader->getResolutionX())
                .arg(QLocale().toString(static_cast<qlonglong>(m_reader->getTotalCounts()))), grpSelection);
        lblSym->setStyleSheet("color: #00e0ff; font-weight: bold; font-size: 14px;");
        selectionLayout->addWidget(lblSym);
    }
    mainLayout->addWidget(grpSelection);

    // 3. 1D Spectrum Plot Preview
    QGroupBox *grpPreview = new QGroupBox(tr("1D Projection Spectrum Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(grpPreview);
    previewLayout->setSpacing(6);
    previewLayout->setContentsMargins(12, 12, 12, 10);

    // Toolbar above preview
    QHBoxLayout *previewBar = new QHBoxLayout();
    previewBar->setSpacing(8);

    m_btnLogScale = new QPushButton(tr("Log Y"), grpPreview);
    m_btnLogScale->setCheckable(true);
    m_btnLogScale->setChecked(false);
    m_btnLogScale->setStyleSheet(
        "QPushButton { background-color: #2b303c; color: #00e0ff; border: 1px solid #414856; border-radius: 3px; padding: 4px 12px; font-size: 12px; font-weight: bold; } "
        "QPushButton:checked { background-color: #0077b6; color: #ffffff; border: 1px solid #0096c7; } "
        "QPushButton:hover { background-color: #3d4452; }"
    );
    previewBar->addWidget(m_btnLogScale);
    previewBar->addStretch(1);

    m_lblHoverReadout = new QLabel(tr("Hover over spectrum to inspect channel counts"), grpPreview);
    m_lblHoverReadout->setStyleSheet("font-size: 12px; color: #88909e; font-family: monospace;");
    previewBar->addWidget(m_lblHoverReadout);

    previewLayout->addLayout(previewBar);

    m_plotPreview = new Matrix1DPreviewWidget(grpPreview);
    connect(m_plotPreview, &Matrix1DPreviewWidget::hoverInfoChanged,
            this, &MatrixDialog::onHoverInfoChanged);
    previewLayout->addWidget(m_plotPreview, 1);

    connect(m_btnLogScale, &QPushButton::toggled, m_plotPreview, &Matrix1DPreviewWidget::setLogScale);

    mainLayout->addWidget(grpPreview, 1);

    // 4. Coincidence Background Subtraction Configuration (Xtrackn)
    QGroupBox *grpBg = new QGroupBox(tr("Coincidence Background Subtraction (Xtrackn)"), this);
    QVBoxLayout *bgLayout = new QVBoxLayout(grpBg);
    bgLayout->setSpacing(8);
    bgLayout->setContentsMargins(14, 14, 14, 12);

    QHBoxLayout *bgTopRow = new QHBoxLayout();
    bgTopRow->setSpacing(16);

    m_chkEnableBg = new QCheckBox(tr("Enable Background Subtraction"), grpBg);
    m_chkEnableBg->setChecked(m_reader ? m_reader->getBackgroundConfig().enabled : true);
    m_chkEnableBg->setStyleSheet("font-weight: bold; color: #ffffff;");
    bgTopRow->addWidget(m_chkEnableBg);

    m_lblBgModePrompt = new QLabel(tr("Mode:"), grpBg);
    bgTopRow->addWidget(m_lblBgModePrompt);

    m_comboBgMode = new QComboBox(grpBg);
    // User requested order: Normal -> Auto -> Common
    m_comboBgMode->addItem(tr("Normal / Local (Gate boundary baseline)"), static_cast<int>(MatrixBgMode::Normal));
    m_comboBgMode->addItem(tr("Auto / SNIP (Iterative peak clipping)"), static_cast<int>(MatrixBgMode::Auto));
    m_comboBgMode->addItem(tr("Common / Projection (GASPware default)"), static_cast<int>(MatrixBgMode::Common));

    if (m_reader) {
        MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
        int idx = (mode == MatrixBgMode::Auto) ? 1 :
                  (mode == MatrixBgMode::Common) ? 2 : 0;
        m_comboBgMode->setCurrentIndex(idx);
    }
    bgTopRow->addWidget(m_comboBgMode, 1);

    m_lblCorrFactorPrompt = new QLabel(tr("Correction Factor:"), grpBg);
    bgTopRow->addWidget(m_lblCorrFactorPrompt);

    m_spinCorrFactor = new QDoubleSpinBox(grpBg);
    m_spinCorrFactor->setRange(0.0, 10.0);
    m_spinCorrFactor->setSingleStep(0.05);
    m_spinCorrFactor->setDecimals(2);
    m_spinCorrFactor->setValue(m_reader ? m_reader->getBackgroundConfig().correctionFactor : 1.00);
    m_spinCorrFactor->setFixedWidth(80);
    bgTopRow->addWidget(m_spinCorrFactor);

    bgLayout->addLayout(bgTopRow);

    m_lblBgHelp = new QLabel(grpBg);
    m_lblBgHelp->setWordWrap(true);
    m_lblBgHelp->setStyleSheet("color: #9cb3c9; font-size: 12px; font-style: italic;");
    bgLayout->addWidget(m_lblBgHelp);

    connect(m_chkEnableBg, &QCheckBox::toggled, this, &MatrixDialog::onBackgroundConfigChanged);
    connect(m_comboBgMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatrixDialog::onBackgroundConfigChanged);
    connect(m_spinCorrFactor, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MatrixDialog::onBackgroundConfigChanged);

    onBackgroundConfigChanged();

    mainLayout->addWidget(grpBg);

    // 5. Matrix Storage Details & Metrics
    if (m_reader && m_reader->isOpen()) {
        const double compRatio = m_reader->getCompressionRatio();
        const double spaceSavings = (1.0 - 1.0 / compRatio) * 100.0;
        QString details = QString("File Size: %1 MB | Uncompressed: %2 MB (%3x compression, %4% saved) | Grid: %5x%6 tiles")
            .arg(m_reader->getFileSize() / 1048576.0, 0, 'f', 1)
            .arg(m_reader->getUncompressedSize() / 1048576.0, 0, 'f', 1)
            .arg(compRatio, 0, 'f', 1)
            .arg(spaceSavings, 0, 'f', 1)
            .arg(m_reader->getNumDivX())
            .arg(m_reader->getNumDivY());

        m_lblDetails = new QLabel(details, this);
        m_lblDetails->setStyleSheet("color: #8c93a1; font-size: 12px;");
        m_lblDetails->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(m_lblDetails);
    }

    // 5. Bottom Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    bottomLayout->addStretch(1);

    m_btnLoadProjection = new QPushButton(tr("Load Projection into Active Pad"), this);
    m_btnLoadProjection->setStyleSheet(
        "QPushButton { background-color: #0077b6; color: #ffffff; border: 1px solid #0096c7; font-size: 15px; padding: 8px 24px; } "
        "QPushButton:hover { background-color: #0096c7; }"
    );
    connect(m_btnLoadProjection, &QPushButton::clicked, this, &MatrixDialog::onLoadProjectionClicked);
    bottomLayout->addWidget(m_btnLoadProjection);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setStyleSheet(
        "QPushButton { background-color: #3e4452; color: #ffffff; border: 1px solid #5a6275; padding: 8px 20px; } "
        "QPushButton:hover { background-color: #4f5769; }"
    );
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);
}

void MatrixDialog::updatePreview()
{
    if (!m_reader || !m_reader->isOpen() || !m_plotPreview) return;

    bool useProjY = (m_radioProjY && m_radioProjY->isChecked());
    const std::vector<double> &data = useProjY ? m_reader->getProjectionY() : m_reader->getProjectionX();
    QString label = useProjY ? tr("Projection Y (Vertical)") : tr("Projection X (Horizontal)");
    if (m_reader->isSymmetric()) {
        label = tr("Total 1D Projection");
    }

    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : false;
    bool isAutoBg = (bgEnabled && m_comboBgMode && static_cast<MatrixBgMode>(m_comboBgMode->currentData().toInt()) == MatrixBgMode::Auto);

    if (isAutoBg && !data.empty()) {
        double corrFactor = m_spinCorrFactor ? m_spinCorrFactor->value() : 1.0;
        std::vector<double> bgData = m_reader->computeSnipBackground(data, 20, corrFactor);
        m_plotPreview->setSpectrum(data, label, m_isCalibrated, m_calibA0, m_calibA1, m_calibA2, bgData);
    } else {
        m_plotPreview->setSpectrum(data, label, m_isCalibrated, m_calibA0, m_calibA1, m_calibA2);
    }
}

void MatrixDialog::onProjectionSelectionChanged()
{
    updatePreview();
}

void MatrixDialog::onBackgroundConfigChanged()
{
    if (!m_chkEnableBg || !m_comboBgMode || !m_spinCorrFactor || !m_lblBgHelp) return;

    bool enabled = m_chkEnableBg->isChecked();
    if (m_lblBgModePrompt) m_lblBgModePrompt->setEnabled(enabled);
    m_comboBgMode->setEnabled(enabled);
    if (m_lblCorrFactorPrompt) m_lblCorrFactorPrompt->setEnabled(enabled);
    m_spinCorrFactor->setEnabled(enabled);

    if (!enabled) {
        m_lblBgHelp->setText(tr("Background subtraction disabled. Coincidence cuts will extract raw, unsubtracted slices."));
        saveBackgroundConfig();
        updatePreview();
        return;
    }

    int modeVal = m_comboBgMode->currentData().toInt();
    if (modeVal == static_cast<int>(MatrixBgMode::Normal)) {
        m_lblBgHelp->setText(tr("Normal/Local background: Estimates local baseline between gate boundaries and subtracts scaled projection."));
    } else if (modeVal == static_cast<int>(MatrixBgMode::Auto)) {
        m_lblBgHelp->setText(tr("Auto/SNIP background: Non-linear iterative peak-clipping filter stripping continuum background from cut spectrum."));
    } else {
        m_lblBgHelp->setText(tr("Common background (GASPware trackn.F): Subtracts total projection scaled by fraction of counts in gate × correction factor: pfacs = (Gate Counts / Total Projection) × %1.")
            .arg(m_spinCorrFactor->value(), 0, 'f', 2));
    }

    saveBackgroundConfig();
    updatePreview();
}

void MatrixDialog::saveBackgroundConfig()
{
    if (!m_reader || !m_chkEnableBg || !m_comboBgMode || !m_spinCorrFactor) return;

    MatrixBackgroundConfig cfg;
    cfg.enabled = m_chkEnableBg->isChecked();
    cfg.mode = static_cast<MatrixBgMode>(m_comboBgMode->currentData().toInt());
    cfg.correctionFactor = m_spinCorrFactor->value();
    m_reader->setBackgroundConfig(cfg);
}

void MatrixDialog::onHoverInfoChanged(int ch, double energy, double counts, double bgCounts)
{
    if (ch < 0) {
        m_lblHoverReadout->setText(tr("Hover over spectrum to inspect channel counts"));
        return;
    }

    QString info = QString("Channel: %1").arg(ch);
    if (m_isCalibrated) {
        info += QString(" | Energy: %1 keV").arg(energy, 0, 'f', 1);
    }
    info += QString(" | Counts: %1").arg(QLocale().toString(static_cast<qlonglong>(std::round(counts))));
    if (bgCounts >= 0.0) {
        info += QString(" | BG: %1").arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))));
    }
    m_lblHoverReadout->setText(info);
}

void MatrixDialog::onLoadProjectionClicked()
{
    if (!m_reader || !m_reader->isOpen()) return;

    saveBackgroundConfig();

    bool useProjY = (m_radioProjY && m_radioProjY->isChecked());
    const std::vector<double> &data = useProjY ? m_reader->getProjectionY() : m_reader->getProjectionX();
    if (data.empty()) return;

    QString suffix = m_reader->isSymmetric()
        ? "[CM Total]"
        : (useProjY ? "[CM ProjY]" : "[CM ProjX]");

    QString title = QString("%1 %2").arg(suffix).arg(m_reader->getFileName());

    QString bgDesc = "Disabled";
    if (m_reader->getBackgroundConfig().enabled) {
        MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
        bgDesc = (mode == MatrixBgMode::Common) ? QString("Common (factor %1)").arg(m_reader->getBackgroundConfig().correctionFactor, 0, 'f', 2) :
                 (mode == MatrixBgMode::Normal) ? "Normal/Local" :
                 (mode == MatrixBgMode::Auto) ? "Auto/SNIP" : "None";
    }
    CommandPrompt *prompt = CommandPrompt::getInstance();
    if (prompt) {
        prompt->appendPlainText(
            QString("Matrix loaded: %1. Coincidence background subtraction: %2. Use 'W' to place gate markers.\n")
                .arg(m_reader->getFileName())
                .arg(bgDesc));
    }

    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : false;
    bool isAutoBg = (bgEnabled && m_reader->getBackgroundConfig().mode == MatrixBgMode::Auto);

    if (isAutoBg) {
        std::vector<double> bgData = m_reader->computeSnipBackground(data, 20, m_reader->getBackgroundConfig().correctionFactor);
        QString bgTitle = QString("%1 [Auto BG] %2").arg(suffix).arg(m_reader->getFileName());
        emit loadProjectionRequested(data, title, bgData, bgTitle);
    } else {
        emit loadProjectionRequested(data, title);
    }
    accept();
}

//==============================================================================
// MatrixGateDialog (Gate CM) Implementation
//==============================================================================

MatrixGateDialog::MatrixGateDialog(std::shared_ptr<MatrixReader> reader,
                                   bool isCalibrated,
                                   double calibA0,
                                   double calibA1,
                                   double calibA2,
                                   const std::vector<MatrixGateRegion> &initialGates,
                                   QWidget *parent)
    : QDialog(parent),
      m_reader(reader),
      m_isCalibrated(isCalibrated),
      m_calibA0(calibA0),
      m_calibA1(calibA1),
      m_calibA2(calibA2),
      m_gates(initialGates),
      m_peakGateIndex(0)
{
    setWindowTitle(tr("Coincidence Gate (Gate CM)"));
    resize(880, 680);
    setMinimumSize(780, 580);

    setStyleSheet(
        "QDialog { background-color: #24262b; color: #ffffff; }"
        "QLabel { color: #e6e6e6; font-size: 14px; }"
        "QLabel:disabled { color: #585f6d; }"
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00e0ff; border: 1px solid #444955; border-radius: 6px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QSpinBox { background-color: #323640; color: #ffffff; font-size: 14px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QRadioButton { font-size: 14px; color: #ffffff; spacing: 8px; }"
        "QCheckBox { font-size: 13px; color: #ffffff; spacing: 8px; }"
        "QComboBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox:disabled { background-color: #1a1c22; color: #585f6d; border: 1px solid #2d313b; }"
        "QComboBox::drop-down:disabled { border: none; background-color: transparent; }"
        "QComboBox QAbstractItemView { background-color: #24262b; color: #ffffff; selection-background-color: #0077b6; }"
        "QDoubleSpinBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QDoubleSpinBox:disabled, QSpinBox:disabled { background-color: #1a1c22; color: #585f6d; border: 1px solid #2d313b; }"
        "QPushButton { font-size: 14px; font-weight: bold; border-radius: 4px; padding: 7px 18px; }"
    );

    int maxCh = (m_reader && m_reader->isOpen()) ? (m_reader->getResolutionY() - 1) : 10239;
    if (m_gates.empty()) {
        m_gates.push_back({150, 160});
    }
    for (auto &g : m_gates) {
        if (g.minCh > g.maxCh) std::swap(g.minCh, g.maxCh);
        g.minCh = std::max(0, std::min(g.minCh, maxCh));
        g.maxCh = std::max(0, std::min(g.maxCh, maxCh));
    }

    // Auto-select peak gate with highest projection count density if multiple gates exist
    if (m_gates.size() > 1 && m_reader && m_reader->isOpen()) {
        const std::vector<double> &proj = m_reader->getProjectionY();
        if (!proj.empty()) {
            double bestDensity = -1.0;
            int bestIdx = 0;
            for (size_t i = 0; i < m_gates.size(); ++i) {
                double sum = 0.0;
                int w = m_gates[i].maxCh - m_gates[i].minCh + 1;
                for (int c = m_gates[i].minCh; c <= m_gates[i].maxCh && c < static_cast<int>(proj.size()); ++c) {
                    sum += proj[c];
                }
                double density = (w > 0) ? (sum / w) : 0.0;
                if (density > bestDensity) {
                    bestDensity = density;
                    bestIdx = static_cast<int>(i);
                }
            }
            m_peakGateIndex = bestIdx;
        }
    }

    setupUI();

    if (m_reader && m_reader->isOpen()) {
        if (m_peakGateIndex >= 0 && m_peakGateIndex < static_cast<int>(m_gates.size())) {
            m_spinGateMin->blockSignals(true);
            m_spinGateMin->setValue(m_gates[m_peakGateIndex].minCh);
            m_spinGateMin->blockSignals(false);

            m_spinGateMax->blockSignals(true);
            m_spinGateMax->setValue(m_gates[m_peakGateIndex].maxCh);
            m_spinGateMax->blockSignals(false);
        }
        syncGateSpinboxes();
        onGateParametersChanged();
    }
}

MatrixGateDialog::MatrixGateDialog(std::shared_ptr<MatrixReader> reader,
                                   bool isCalibrated,
                                   double calibA0,
                                   double calibA1,
                                   double calibA2,
                                   int initialGateMin,
                                   int initialGateMax,
                                   QWidget *parent)
    : MatrixGateDialog(reader, isCalibrated, calibA0, calibA1, calibA2,
                       (initialGateMin >= 0 && initialGateMax >= 0)
                           ? std::vector<MatrixGateRegion>{{initialGateMin, initialGateMax}}
                           : std::vector<MatrixGateRegion>{{150, 160}},
                       parent)
{
}

void MatrixGateDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // 1. Header Frame
    QFrame *headerFrame = new QFrame(this);
    headerFrame->setStyleSheet("background-color: #1a2332; border: 1px solid #23486a; border-radius: 6px; padding: 8px 12px;");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QString titleText = m_reader ? m_reader->getFileName() : tr("Unknown Matrix");
    QLabel *lblTitle = new QLabel(QString("<b>Coincidence Gating:</b> %1").arg(titleText), headerFrame);
    lblTitle->setStyleSheet("font-size: 16px; color: #00e0ff;");
    headerLayout->addWidget(lblTitle);
    headerLayout->addStretch(1);

    if (m_reader) {
        QLabel *lblSym = new QLabel(
            m_reader->isSymmetric()
                ? tr("Symmetric Matrix")
                : tr("Non-Symmetrical Matrix"), headerFrame);
        lblSym->setStyleSheet(
            "background-color: #123d2e; color: #44ffaa; border: 1px solid #2e7752; "
            "border-radius: 4px; padding: 3px 8px; font-size: 12px; font-weight: bold;"
        );
        headerLayout->addWidget(lblSym);
    }
    mainLayout->addWidget(headerFrame);

    // 2. Gate Parameters GroupBox
    QGroupBox *grpGate = new QGroupBox(tr("Coincidence Gate Settings"), this);
    QVBoxLayout *gateLayout = new QVBoxLayout(grpGate);
    gateLayout->setSpacing(10);
    gateLayout->setContentsMargins(14, 16, 14, 12);

    // If asymmetric, allow axis selection
    if (m_reader && !m_reader->isSymmetric()) {
        QHBoxLayout *axisLayout = new QHBoxLayout();
        axisLayout->setSpacing(16);

        m_radioGateY = new QRadioButton(tr("Gate on Y-axis (slice horizontal spectrum)"), grpGate);
        m_radioGateY->setChecked(true);
        axisLayout->addWidget(m_radioGateY);

        m_radioGateX = new QRadioButton(tr("Gate on X-axis (slice vertical spectrum)"), grpGate);
        axisLayout->addWidget(m_radioGateX);
        axisLayout->addStretch(1);

        connect(m_radioGateY, &QRadioButton::toggled, this, &MatrixGateDialog::onGateParametersChanged);
        connect(m_radioGateX, &QRadioButton::toggled, this, &MatrixGateDialog::onGateParametersChanged);
        gateLayout->addLayout(axisLayout);
    }

    if (m_gates.size() > 1) {
        QHBoxLayout *peakSelectLayout = new QHBoxLayout();
        peakSelectLayout->setSpacing(10);
        m_lblPeakPrompt = new QLabel(tr("Peak Gate:"), grpGate);
        m_lblPeakPrompt->setStyleSheet("font-weight: bold; color: #00e0ff; font-size: 13px;");
        peakSelectLayout->addWidget(m_lblPeakPrompt);

        m_comboPeakGate = new QComboBox(grpGate);
        m_comboPeakGate->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        for (size_t i = 0; i < m_gates.size(); ++i) {
            QString gTxt = QString("Gate %1: [%2 - %3 ch]").arg(i + 1).arg(m_gates[i].minCh).arg(m_gates[i].maxCh);
            if (m_isCalibrated) {
                gTxt += QString(" (%1 - %2 keV)").arg(channelToEnergy(m_gates[i].minCh), 0, 'f', 1)
                                                 .arg(channelToEnergy(m_gates[i].maxCh), 0, 'f', 1);
            }
            m_comboPeakGate->addItem(gTxt);
        }
        m_comboPeakGate->setCurrentIndex(m_peakGateIndex);
        connect(m_comboPeakGate, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MatrixGateDialog::onPeakGateChanged);
        peakSelectLayout->addWidget(m_comboPeakGate);
        peakSelectLayout->addStretch(1);
        gateLayout->addLayout(peakSelectLayout);

        m_lblGatesDetail = new QLabel(grpGate);
        m_lblGatesDetail->setWordWrap(true);
        m_lblGatesDetail->setStyleSheet("color: #b0c4de; font-size: 12px; font-family: monospace;");
        gateLayout->addWidget(m_lblGatesDetail);
    }

    QGridLayout *paramGrid = new QGridLayout();
    paramGrid->setSpacing(10);

    paramGrid->addWidget(new QLabel(m_gates.size() > 1 ? tr("Peak Channel Min:") : tr("Gate Channel Min:"), grpGate), 0, 0);
    m_spinGateMin = new QSpinBox(grpGate);
    m_spinGateMin->setRange(0, m_reader ? m_reader->getResolutionY() - 1 : 10239);
    connect(m_spinGateMin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MatrixGateDialog::onGateParametersChanged);
    paramGrid->addWidget(m_spinGateMin, 0, 1);

    paramGrid->addWidget(new QLabel(m_gates.size() > 1 ? tr("Peak Channel Max:") : tr("Gate Channel Max:"), grpGate), 0, 2);
    m_spinGateMax = new QSpinBox(grpGate);
    m_spinGateMax->setRange(0, m_reader ? m_reader->getResolutionY() - 1 : 10239);
    connect(m_spinGateMax, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MatrixGateDialog::onGateParametersChanged);
    paramGrid->addWidget(m_spinGateMax, 0, 3);

    m_lblGateWidth = new QLabel(grpGate);
    m_lblGateWidth->setStyleSheet("color: #b0b8c6; font-size: 12px;");
    paramGrid->addWidget(m_lblGateWidth, 1, 0, 1, 2);

    m_lblGateEnergy = new QLabel(grpGate);
    m_lblGateEnergy->setStyleSheet("color: #44ffaa; font-weight: bold; font-size: 13px;");
    paramGrid->addWidget(m_lblGateEnergy, 1, 2, 1, 2);

    gateLayout->addLayout(paramGrid);

    // Background Subtraction Controls Row in Gate CM
    QHBoxLayout *bgRow = new QHBoxLayout();
    bgRow->setSpacing(12);

    m_chkEnableBg = new QCheckBox(tr("Background Subtraction:"), grpGate);
    m_chkEnableBg->setChecked(m_reader ? m_reader->getBackgroundConfig().enabled : true);
    m_chkEnableBg->setStyleSheet("font-weight: bold; color: #ffffff;");
    bgRow->addWidget(m_chkEnableBg);

    m_lblBgModePrompt = new QLabel(tr("Mode:"), grpGate);
    bgRow->addWidget(m_lblBgModePrompt);

    m_comboBgMode = new QComboBox(grpGate);
    // User requested order: Normal -> Auto -> Common
    m_comboBgMode->addItem(tr("Normal (Local Baseline)"), static_cast<int>(MatrixBgMode::Normal));
    m_comboBgMode->addItem(tr("Auto (SNIP Filter)"), static_cast<int>(MatrixBgMode::Auto));
    m_comboBgMode->addItem(tr("Common (Projection)"), static_cast<int>(MatrixBgMode::Common));

    if (m_reader) {
        MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
        int idx = (mode == MatrixBgMode::Auto) ? 1 :
                  (mode == MatrixBgMode::Common) ? 2 : 0;
        m_comboBgMode->setCurrentIndex(idx);
    }
    bgRow->addWidget(m_comboBgMode);

    m_lblCorrFactorPrompt = new QLabel(tr("Factor:"), grpGate);
    bgRow->addWidget(m_lblCorrFactorPrompt);

    m_spinCorrFactor = new QDoubleSpinBox(grpGate);
    m_spinCorrFactor->setRange(0.0, 10.0);
    m_spinCorrFactor->setSingleStep(0.05);
    m_spinCorrFactor->setDecimals(2);
    m_spinCorrFactor->setValue(m_reader ? m_reader->getBackgroundConfig().correctionFactor : 1.00);
    m_spinCorrFactor->setFixedWidth(80);
    bgRow->addWidget(m_spinCorrFactor);

    bgRow->addStretch(1);
    gateLayout->addLayout(bgRow);

    m_lblBgStats = new QLabel(grpGate);
    m_lblBgStats->setWordWrap(true);
    m_lblBgStats->setStyleSheet("color: #44ffaa; font-size: 12px; font-family: monospace;");
    gateLayout->addWidget(m_lblBgStats);

    connect(m_chkEnableBg, &QCheckBox::toggled, this, &MatrixGateDialog::onBackgroundSettingsChanged);
    connect(m_comboBgMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatrixGateDialog::onBackgroundSettingsChanged);
    connect(m_spinCorrFactor, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MatrixGateDialog::onBackgroundSettingsChanged);

    onBackgroundSettingsChanged();

    mainLayout->addWidget(grpGate);

    // 3. Sliced Gate 1D Preview
    QGroupBox *grpPreview = new QGroupBox(tr("Gated Coincidence Spectrum Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(grpPreview);
    previewLayout->setSpacing(10);
    previewLayout->setContentsMargins(12, 12, 12, 10);

    // Toolbar above preview
    QHBoxLayout *previewBar = new QHBoxLayout();
    previewBar->setSpacing(8);

    m_btnLogScale = new QPushButton(tr("Log Y"), grpPreview);
    m_btnLogScale->setCheckable(true);
    m_btnLogScale->setChecked(false);
    m_btnLogScale->setStyleSheet(
        "QPushButton { background-color: #2b303c; color: #00e0ff; border: 1px solid #414856; border-radius: 3px; padding: 4px 12px; font-size: 12px; font-weight: bold; } "
        "QPushButton:checked { background-color: #0077b6; color: #ffffff; border: 1px solid #0096c7; } "
        "QPushButton:hover { background-color: #3d4452; }"
    );
    previewBar->addWidget(m_btnLogScale);
    previewBar->addStretch(1);

    m_lblHoverReadout = new QLabel(tr("Hover over spectrum to inspect channel counts"), grpPreview);
    m_lblHoverReadout->setStyleSheet("font-size: 12px; color: #88909e; font-family: monospace;");
    previewBar->addWidget(m_lblHoverReadout);

    previewLayout->addLayout(previewBar);

    m_plotPreview = new Matrix1DPreviewWidget(grpPreview);
    connect(m_plotPreview, &Matrix1DPreviewWidget::hoverInfoChanged,
            this, &MatrixGateDialog::onHoverInfoChanged);
    previewLayout->addWidget(m_plotPreview, 1);

    connect(m_btnLogScale, &QPushButton::toggled, m_plotPreview, &Matrix1DPreviewWidget::setLogScale);

    mainLayout->addWidget(grpPreview, 1);

    // 4. Bottom Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    bottomLayout->addStretch(1);

    m_btnSliceGate = new QPushButton(tr("Slice && Load into Pad"), this);
    m_btnSliceGate->setStyleSheet(
        "QPushButton { background-color: #1e7040; color: #ffffff; border: 1px solid #2e9e5d; font-size: 14px; padding: 7px 20px; } "
        "QPushButton:hover { background-color: #278d52; }"
    );
    connect(m_btnSliceGate, &QPushButton::clicked, this, &MatrixGateDialog::onSliceGateClicked);
    bottomLayout->addWidget(m_btnSliceGate);

    m_btnOverlayGate = new QPushButton(tr("Overlay on Pad"), this);
    m_btnOverlayGate->setStyleSheet(
        "QPushButton { background-color: #7b4f12; color: #ffffff; border: 1px solid #b8751b; font-size: 14px; padding: 7px 20px; } "
        "QPushButton:hover { background-color: #946016; }"
    );
    connect(m_btnOverlayGate, &QPushButton::clicked, this, &MatrixGateDialog::onOverlayGateClicked);
    bottomLayout->addWidget(m_btnOverlayGate);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setStyleSheet(
        "QPushButton { background-color: #3e4452; color: #ffffff; border: 1px solid #5a6275; padding: 7px 18px; } "
        "QPushButton:hover { background-color: #4f5769; }"
    );
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);
}

double MatrixGateDialog::channelToEnergy(double ch) const
{
    if (!m_isCalibrated) return ch;
    return m_calibA0 + m_calibA1 * ch + m_calibA2 * ch * ch;
}

void MatrixGateDialog::syncGateSpinboxes()
{
    if (m_gates.size() > 1 && m_lblGatesDetail) {
        QString detail = QString("<b>Peak:</b> Gate %1 [%2-%3 ch, w=%4] | <b>Background:</b> ")
                             .arg(m_peakGateIndex + 1)
                             .arg(m_gates[m_peakGateIndex].minCh)
                             .arg(m_gates[m_peakGateIndex].maxCh)
                             .arg(m_gates[m_peakGateIndex].maxCh - m_gates[m_peakGateIndex].minCh + 1);
        int totalBgW = 0;
        bool first = true;
        for (size_t i = 0; i < m_gates.size(); ++i) {
            if (static_cast<int>(i) == m_peakGateIndex) continue;
            int w = m_gates[i].maxCh - m_gates[i].minCh + 1;
            totalBgW += w;
            if (!first) detail += ", ";
            detail += QString("Gate %1 [%2-%3 ch, w=%4]").arg(i + 1).arg(m_gates[i].minCh).arg(m_gates[i].maxCh).arg(w);
            first = false;
        }
        detail += QString(" (Total BG w=%1)").arg(totalBgW);
        m_lblGatesDetail->setText(detail);

        if (m_comboPeakGate && m_peakGateIndex >= 0 && m_peakGateIndex < m_comboPeakGate->count()) {
            QString gTxt = QString("Gate %1: [%2 - %3 ch]")
                               .arg(m_peakGateIndex + 1)
                               .arg(m_gates[m_peakGateIndex].minCh)
                               .arg(m_gates[m_peakGateIndex].maxCh);
            if (m_isCalibrated) {
                gTxt += QString(" (%1 - %2 keV)")
                            .arg(channelToEnergy(m_gates[m_peakGateIndex].minCh), 0, 'f', 1)
                            .arg(channelToEnergy(m_gates[m_peakGateIndex].maxCh), 0, 'f', 1);
            }
            m_comboPeakGate->setItemText(m_peakGateIndex, gTxt);
        }
    }
}

void MatrixGateDialog::onPeakGateChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(m_gates.size())) return;
    m_peakGateIndex = index;

    m_spinGateMin->blockSignals(true);
    m_spinGateMin->setValue(m_gates[m_peakGateIndex].minCh);
    m_spinGateMin->blockSignals(false);

    m_spinGateMax->blockSignals(true);
    m_spinGateMax->setValue(m_gates[m_peakGateIndex].maxCh);
    m_spinGateMax->blockSignals(false);

    syncGateSpinboxes();
    onGateParametersChanged();
}

void MatrixGateDialog::onBackgroundSettingsChanged()
{
    bool enabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : true;
    if (m_lblBgModePrompt) m_lblBgModePrompt->setEnabled(enabled);
    if (m_comboBgMode) m_comboBgMode->setEnabled(enabled);
    if (m_lblCorrFactorPrompt) m_lblCorrFactorPrompt->setEnabled(enabled);
    if (m_spinCorrFactor) m_spinCorrFactor->setEnabled(enabled);

    if (m_reader && m_chkEnableBg && m_comboBgMode && m_spinCorrFactor) {
        MatrixBackgroundConfig cfg;
        cfg.enabled = enabled;
        cfg.mode = static_cast<MatrixBgMode>(m_comboBgMode->currentData().toInt());
        cfg.correctionFactor = m_spinCorrFactor->value();
        m_reader->setBackgroundConfig(cfg);
    }

    updateGatePreview();
}

void MatrixGateDialog::updateGatePreview()
{
    if (!m_reader || !m_reader->isOpen() || !m_plotPreview) return;

    int gateAxis = 1;
    if (m_radioGateX && m_radioGateX->isChecked()) {
        gateAxis = 0;
    }

    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : true;
    double backfac = 0.0;
    double bgCounts = 0.0;
    std::vector<double> bgSlice;

    m_currentSlice = m_reader->getMultiGateSlice(
        m_gates, m_peakGateIndex, gateAxis, bgEnabled,
        &backfac, &bgCounts, &bgSlice);

    int pMin = m_gates.empty() ? m_spinGateMin->value() : m_gates[m_peakGateIndex].minCh;
    int pMax = m_gates.empty() ? m_spinGateMax->value() : m_gates[m_peakGateIndex].maxCh;
    if (pMin > pMax) std::swap(pMin, pMax);

    std::vector<double> rawSlice = m_reader->getRawGateSlice(pMin, pMax, gateAxis);
    double rawCounts = 0.0;
    for (double v : rawSlice) rawCounts += v;
    double netCounts = 0.0;
    for (double v : m_currentSlice) netCounts += v;

    if (m_lblBgStats) {
        if (bgEnabled && m_reader->getBackgroundConfig().mode != MatrixBgMode::None) {
            MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
            if (mode == MatrixBgMode::Auto) {
                m_lblBgStats->setText(
                    QString("BG Mode: Auto (SNIP) | Subtracted BG: %1 cts | Net: %2 cts")
                        .arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))))
                        .arg(QLocale().toString(static_cast<qlonglong>(std::round(netCounts))))
                );
            } else if (mode == MatrixBgMode::Common) {
                m_lblBgStats->setText(
                    QString("BG Mode: Common Projection | Subtracted BG: %1 cts | Net: %2 cts")
                        .arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))))
                        .arg(QLocale().toString(static_cast<qlonglong>(std::round(netCounts))))
                );
            } else {
                // Normal Mode
                if (m_gates.size() > 1) {
                    m_lblBgStats->setText(
                        QString("Normal BG (%1 gates): Peak Gate %2 [%3-%4 ch] | backfac = %5 (%6%) | Subtracted BG: %7 cts | Net: %8 cts")
                            .arg(m_gates.size())
                            .arg(m_peakGateIndex + 1)
                            .arg(pMin).arg(pMax)
                            .arg(backfac, 0, 'f', 4)
                            .arg(backfac * 100.0, 0, 'f', 2)
                            .arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))))
                            .arg(QLocale().toString(static_cast<qlonglong>(std::round(netCounts))))
                    );
                } else {
                    m_lblBgStats->setText(
                        QString("Normal BG (Single gate trapezoid) | pfacs = %1 (%2%) | Subtracted BG: %3 cts | Net: %4 cts")
                            .arg(backfac, 0, 'f', 4)
                            .arg(backfac * 100.0, 0, 'f', 2)
                            .arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))))
                            .arg(QLocale().toString(static_cast<qlonglong>(std::round(netCounts))))
                    );
                }
            }
        } else {
            m_lblBgStats->setText(
                QString("Raw Coincidence Gate (No Subtraction) | Counts: %1")
                    .arg(QLocale().toString(static_cast<qlonglong>(std::round(rawCounts))))
            );
        }
    }

    QString label;
    if (m_gates.size() > 1) {
        label = QString("Peak Gate %1 [%2 - %3 ch]").arg(m_peakGateIndex + 1).arg(pMin).arg(pMax);
    } else {
        label = QString("Gate [%1 - %2 ch]").arg(pMin).arg(pMax);
    }
    if (m_isCalibrated) {
        label += QString(" (%1 - %2 keV)").arg(channelToEnergy(pMin), 0, 'f', 1).arg(channelToEnergy(pMax), 0, 'f', 1);
    }
    if (bgEnabled && m_reader->getBackgroundConfig().mode != MatrixBgMode::None) {
        label += " [Net BG-Sub]";
    }

    m_plotPreview->setSpectrum(m_currentSlice, label, m_isCalibrated, m_calibA0, m_calibA1, m_calibA2);
}

void MatrixGateDialog::onGateParametersChanged()
{
    int ch1 = m_spinGateMin->value();
    int ch2 = m_spinGateMax->value();
    if (ch1 > ch2) std::swap(ch1, ch2);

    if (!m_gates.empty() && m_peakGateIndex >= 0 && m_peakGateIndex < static_cast<int>(m_gates.size())) {
        m_gates[m_peakGateIndex].minCh = ch1;
        m_gates[m_peakGateIndex].maxCh = ch2;
    }

    int width = ch2 - ch1 + 1;
    m_lblGateWidth->setText(tr("Width: %1 channels").arg(width));

    if (m_isCalibrated) {
        double e1 = channelToEnergy(ch1);
        double e2 = channelToEnergy(ch2);
        m_lblGateEnergy->setText(QString("Energy: %1 - %2 keV")
                                     .arg(e1, 0, 'f', 1)
                                     .arg(e2, 0, 'f', 1));
    } else {
        m_lblGateEnergy->setText(tr("Uncalibrated"));
    }

    syncGateSpinboxes();
    updateGatePreview();
}

void MatrixGateDialog::onHoverInfoChanged(int ch, double energy, double counts, double bgCounts)
{
    Q_UNUSED(bgCounts);
    if (ch < 0) {
        m_lblHoverReadout->setText(tr("Hover over spectrum to inspect channel counts"));
        return;
    }

    QString info = QString("Channel: %1").arg(ch);
    if (m_isCalibrated) {
        info += QString(" | Energy: %1 keV").arg(energy, 0, 'f', 1);
    }
    info += QString(" | Counts: %1").arg(QLocale().toString(static_cast<qlonglong>(std::round(counts))));
    m_lblHoverReadout->setText(info);
}

void MatrixGateDialog::onSliceGateClicked()
{
    if (m_currentSlice.empty()) return;

    int pMin = m_gates.empty() ? m_spinGateMin->value() : m_gates[m_peakGateIndex].minCh;
    int pMax = m_gates.empty() ? m_spinGateMax->value() : m_gates[m_peakGateIndex].maxCh;
    if (pMin > pMax) std::swap(pMin, pMax);

    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : true;
    QString bgTag;
    if (bgEnabled && m_reader && m_reader->getBackgroundConfig().mode != MatrixBgMode::None) {
        bgTag = (m_reader->getBackgroundConfig().mode == MatrixBgMode::Auto) ? " [Auto BG]" :
                (m_reader->getBackgroundConfig().mode == MatrixBgMode::Common) ? " [Common BG]" : " [Net BG]";
    }

    QString title;
    if (m_isCalibrated) {
        title = QString("[Gate %1-%2 keV%3] %4")
                    .arg(channelToEnergy(pMin), 0, 'f', 1)
                    .arg(channelToEnergy(pMax), 0, 'f', 1)
                    .arg(bgTag)
                    .arg(m_reader->getFileName());
    } else {
        title = QString("[Gate %1-%2 ch%3] %4")
                    .arg(pMin)
                    .arg(pMax)
                    .arg(bgTag)
                    .arg(m_reader->getFileName());
    }

    emit loadGateSliceRequested(m_currentSlice, title, false);
    accept();
}

void MatrixGateDialog::onOverlayGateClicked()
{
    if (m_currentSlice.empty()) return;

    int pMin = m_gates.empty() ? m_spinGateMin->value() : m_gates[m_peakGateIndex].minCh;
    int pMax = m_gates.empty() ? m_spinGateMax->value() : m_gates[m_peakGateIndex].maxCh;
    if (pMin > pMax) std::swap(pMin, pMax);

    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : true;
    QString bgTag;
    if (bgEnabled && m_reader && m_reader->getBackgroundConfig().mode != MatrixBgMode::None) {
        bgTag = (m_reader->getBackgroundConfig().mode == MatrixBgMode::Auto) ? " [Auto BG]" :
                (m_reader->getBackgroundConfig().mode == MatrixBgMode::Common) ? " [Common BG]" : " [Net BG]";
    }

    QString title;
    if (m_isCalibrated) {
        title = QString("[Gate %1-%2 keV%3] %4")
                    .arg(channelToEnergy(pMin), 0, 'f', 1)
                    .arg(channelToEnergy(pMax), 0, 'f', 1)
                    .arg(bgTag)
                    .arg(m_reader->getFileName());
    } else {
        title = QString("[Gate %1-%2 ch%3] %4")
                    .arg(pMin)
                    .arg(pMax)
                    .arg(bgTag)
                    .arg(m_reader->getFileName());
    }

    emit loadGateSliceRequested(m_currentSlice, title, true);
    accept();
}
