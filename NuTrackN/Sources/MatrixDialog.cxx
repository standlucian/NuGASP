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
    setMinimumHeight(200);
    setMouseTracking(true);
}

void Matrix1DPreviewWidget::setSpectrum(const std::vector<double> &data, const QString &label,
                                        bool isCalib, double a0, double a1, double a2)
{
    m_data = data;
    m_label = label;
    m_isCalibrated = isCalib;
    m_calibA0 = a0;
    m_calibA1 = a1;
    m_calibA2 = a2;

    m_maxVal = 0.0;
    for (double v : m_data) {
        if (v > m_maxVal) m_maxVal = v;
    }
    m_hoverCh = -1;
    update();
}

void Matrix1DPreviewWidget::clear()
{
    m_data.clear();
    m_label.clear();
    m_maxVal = 0.0;
    m_hoverCh = -1;
    update();
}

void Matrix1DPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_data.empty()) return;

    const QRect plotArea = rect().adjusted(46, 16, -16, -26);
    int mx = event->pos().x();

    if (plotArea.contains(event->pos())) {
        int px = mx - plotArea.left();
        int ch = (px * static_cast<int>(m_data.size())) / plotArea.width();
        ch = std::max(0, std::min(ch, static_cast<int>(m_data.size()) - 1));

        m_hoverCh = ch;
        double counts = m_data[ch];
        double energy = m_isCalibrated
            ? (m_calibA0 + m_calibA1 * ch + m_calibA2 * ch * ch)
            : static_cast<double>(ch);

        emit hoverInfoChanged(ch, energy, counts);
        update();
    } else {
        m_hoverCh = -1;
        emit hoverInfoChanged(-1, 0.0, 0.0);
        update();
    }
}

void Matrix1DPreviewWidget::leaveEvent(QEvent *)
{
    m_hoverCh = -1;
    emit hoverInfoChanged(-1, 0.0, 0.0);
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

    const QRect plotArea = rect().adjusted(46, 16, -16, -26);

    // Plot Border
    p.setPen(QPen(QColor("#3d4452"), 1));
    p.drawRect(plotArea);

    if (m_data.empty() || m_maxVal <= 0.0) {
        p.setPen(QColor("#777e8c"));
        p.drawText(plotArea, Qt::AlignCenter, tr("No spectrum data available for preview."));
        return;
    }

    const int nCh = static_cast<int>(m_data.size());
    const int pW = plotArea.width();
    const int pH = plotArea.height();

    // Subtle grid lines
    p.setPen(QPen(QColor(255, 255, 255, 18), 1, Qt::DotLine));
    for (int step = 1; step <= 3; ++step) {
        int y = plotArea.bottom() - (pH * step) / 4;
        p.drawLine(plotArea.left(), y, plotArea.right(), y);

        int x = plotArea.left() + (pW * step) / 4;
        p.drawLine(x, plotArea.top(), x, plotArea.bottom());
    }

    // Build curve polylines
    QPolygonF linePoly;
    QPolygonF fillPoly;
    fillPoly << QPointF(plotArea.left(), plotArea.bottom());

    for (int px = 0; px < pW; ++px) {
        int ch1 = (px * nCh) / pW;
        int ch2 = ((px + 1) * nCh) / pW;
        if (ch2 <= ch1) ch2 = ch1 + 1;
        if (ch2 > nCh) ch2 = nCh;

        double cMin = m_data[ch1];
        double cMax = m_data[ch1];
        for (int c = ch1 + 1; c < ch2; ++c) {
            if (m_data[c] < cMin) cMin = m_data[c];
            if (m_data[c] > cMax) cMax = m_data[c];
        }

        double yTop = plotArea.bottom() - (cMax / m_maxVal) * (pH - 2);
        double yBot = plotArea.bottom() - (cMin / m_maxVal) * (pH - 2);
        double screenX = plotArea.left() + px;

        linePoly << QPointF(screenX, yTop);
        if (std::abs(yBot - yTop) > 1.0) {
            linePoly << QPointF(screenX, yBot);
        }
    }

    for (int i = 0; i < linePoly.size(); ++i) {
        fillPoly << linePoly[i];
    }
    fillPoly << QPointF(plotArea.right(), plotArea.bottom());

    // Shaded fill under curve
    QLinearGradient grad(0, plotArea.top(), 0, plotArea.bottom());
    grad.setColorAt(0.0, QColor(0, 224, 255, 90));
    grad.setColorAt(1.0, QColor(0, 136, 204, 10));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawPolygon(fillPoly);

    // Spectrum curve line
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#00e0ff"), 1.2));
    p.drawPolyline(linePoly);

    // Hover hair-line
    if (m_hoverCh >= 0 && m_hoverCh < nCh) {
        int hx = plotArea.left() + (m_hoverCh * pW) / nCh;
        p.setPen(QPen(QColor("#ffaa00"), 1, Qt::DashLine));
        p.drawLine(hx, plotArea.top(), hx, plotArea.bottom());
    }

    // Top Badges & Text
    QFont badgeFont = font();
    badgeFont.setPointSize(9);
    badgeFont.setBold(true);
    p.setFont(badgeFont);

    // Label badge (top-left)
    p.setPen(QColor("#00e0ff"));
    p.drawText(plotArea.left() + 8, plotArea.top() + 15, m_label);

    // Max counts readout (top-right)
    QFont infoFont = font();
    infoFont.setPointSize(8);
    infoFont.setBold(false);
    p.setFont(infoFont);
    p.setPen(QColor("#9da4b0"));
    QString maxStr = QString("Max: %1 counts").arg(QLocale().toString(static_cast<qlonglong>(std::round(m_maxVal))));
    p.drawText(plotArea.right() - p.fontMetrics().horizontalAdvance(maxStr) - 6, plotArea.top() + 15, maxStr);

    // Axis Labels
    p.drawText(plotArea.left(), plotArea.bottom() + 16, "Ch 0");
    QString midCh = QString("Ch %1").arg(nCh / 2);
    p.drawText(plotArea.left() + pW / 2 - p.fontMetrics().horizontalAdvance(midCh) / 2, plotArea.bottom() + 16, midCh);
    QString endCh = QString("Ch %1").arg(nCh - 1);
    p.drawText(plotArea.right() - p.fontMetrics().horizontalAdvance(endCh), plotArea.bottom() + 16, endCh);

    // Y Axis Ticks
    p.drawText(4, plotArea.top() + 10, QString::number(static_cast<qlonglong>(std::round(m_maxVal))));
    p.drawText(20, plotArea.bottom(), "0");
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
    resize(740, 520);
    setMinimumSize(680, 480);

    setStyleSheet(
        "QDialog { background-color: #24262b; color: #ffffff; }"
        "QLabel { color: #e6e6e6; font-size: 14px; }"
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00e0ff; border: 1px solid #444955; border-radius: 6px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QRadioButton { font-size: 14px; color: #ffffff; spacing: 8px; }"
        "QRadioButton::indicator { width: 18px; height: 18px; }"
        "QCheckBox { font-size: 13px; color: #ffffff; spacing: 8px; }"
        "QComboBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background-color: #24262b; color: #ffffff; selection-background-color: #0077b6; }"
        "QDoubleSpinBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
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
    previewLayout->setContentsMargins(12, 14, 12, 10);

    m_plotPreview = new Matrix1DPreviewWidget(grpPreview);
    connect(m_plotPreview, &Matrix1DPreviewWidget::hoverInfoChanged,
            this, &MatrixDialog::onHoverInfoChanged);
    previewLayout->addWidget(m_plotPreview, 1);

    m_lblHoverReadout = new QLabel(tr("Hover over spectrum to inspect channel counts"), grpPreview);
    m_lblHoverReadout->setStyleSheet("font-size: 12px; color: #88909e; font-family: monospace;");
    m_lblHoverReadout->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_lblHoverReadout);

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

    bgTopRow->addWidget(new QLabel(tr("Mode:"), grpBg));
    m_comboBgMode = new QComboBox(grpBg);
    m_comboBgMode->addItem(tr("Common / Projection (GASPware default)"), 1);
    m_comboBgMode->addItem(tr("Normal / Local (Gate boundary baseline)"), 2);
    m_comboBgMode->addItem(tr("Auto / SNIP (Iterative peak clipping)"), 3);
    m_comboBgMode->addItem(tr("None (Raw coincidence slices)"), 0);

    if (m_reader) {
        MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
        int idx = (mode == MatrixBgMode::Common) ? 0 :
                  (mode == MatrixBgMode::Normal) ? 1 :
                  (mode == MatrixBgMode::Auto) ? 2 : 3;
        m_comboBgMode->setCurrentIndex(idx);
    }
    bgTopRow->addWidget(m_comboBgMode, 1);

    bgTopRow->addWidget(new QLabel(tr("Correction Factor:"), grpBg));
    m_spinCorrFactor = new QDoubleSpinBox(grpBg);
    m_spinCorrFactor->setRange(0.0, 10.0);
    m_spinCorrFactor->setSingleStep(0.05);
    m_spinCorrFactor->setDecimals(2);
    m_spinCorrFactor->setValue(m_reader ? m_reader->getBackgroundConfig().correctionFactor : 1.00);
    m_spinCorrFactor->setFixedWidth(80);
    bgTopRow->addWidget(m_spinCorrFactor);

    bgLayout->addLayout(bgTopRow);

    m_lblBgHelp = new QLabel(grpBg);
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

    m_plotPreview->setSpectrum(data, label, m_isCalibrated, m_calibA0, m_calibA1, m_calibA2);
}

void MatrixDialog::onProjectionSelectionChanged()
{
    updatePreview();
}

void MatrixDialog::onBackgroundConfigChanged()
{
    if (!m_chkEnableBg || !m_comboBgMode || !m_spinCorrFactor || !m_lblBgHelp) return;

    bool enabled = m_chkEnableBg->isChecked();
    m_comboBgMode->setEnabled(enabled);
    m_spinCorrFactor->setEnabled(enabled);

    if (!enabled) {
        m_lblBgHelp->setText(tr("Background subtraction disabled. Coincidence cuts will extract raw, unsubtracted slices."));
        saveBackgroundConfig();
        return;
    }

    int modeIdx = m_comboBgMode->currentIndex();
    if (modeIdx == 0) {
        m_lblBgHelp->setText(tr("Common background (GASPware trackn.F): Subtracts total projection scaled by fraction of counts in gate × correction factor: pfacs = (Gate Counts / Total Projection) × %1.")
            .arg(m_spinCorrFactor->value(), 0, 'f', 2));
    } else if (modeIdx == 1) {
        m_lblBgHelp->setText(tr("Normal/Local background: Estimates local baseline between gate boundaries and subtracts scaled projection."));
    } else if (modeIdx == 2) {
        m_lblBgHelp->setText(tr("Auto/SNIP background: Non-linear iterative peak-clipping filter stripping continuum background from cut spectrum."));
    } else {
        m_lblBgHelp->setText(tr("No background subtraction will be applied."));
    }

    saveBackgroundConfig();
}

void MatrixDialog::saveBackgroundConfig()
{
    if (!m_reader || !m_chkEnableBg || !m_comboBgMode || !m_spinCorrFactor) return;

    MatrixBackgroundConfig cfg;
    cfg.enabled = m_chkEnableBg->isChecked();
    int modeIdx = m_comboBgMode->currentIndex();
    cfg.mode = (modeIdx == 0) ? MatrixBgMode::Common :
               (modeIdx == 1) ? MatrixBgMode::Normal :
               (modeIdx == 2) ? MatrixBgMode::Auto : MatrixBgMode::None;
    cfg.correctionFactor = m_spinCorrFactor->value();
    m_reader->setBackgroundConfig(cfg);
}

void MatrixDialog::onHoverInfoChanged(int ch, double energy, double counts)
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
    CommandPrompt::getInstance()->appendPlainText(
        QString("Matrix loaded: %1. Coincidence background subtraction: %2. Use 'W' to place gate markers.\n")
            .arg(m_reader->getFileName())
            .arg(bgDesc));

    emit loadProjectionRequested(data, title);
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
                                   int initialGateMin,
                                   int initialGateMax,
                                   QWidget *parent)
    : QDialog(parent),
      m_reader(reader),
      m_isCalibrated(isCalibrated),
      m_calibA0(calibA0),
      m_calibA1(calibA1),
      m_calibA2(calibA2)
{
    setWindowTitle(tr("Coincidence Gate (Gate CM)"));
    resize(760, 560);
    setMinimumSize(700, 500);

    setStyleSheet(
        "QDialog { background-color: #24262b; color: #ffffff; }"
        "QLabel { color: #e6e6e6; font-size: 14px; }"
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00e0ff; border: 1px solid #444955; border-radius: 6px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QSpinBox { background-color: #323640; color: #ffffff; font-size: 14px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QRadioButton { font-size: 14px; color: #ffffff; spacing: 8px; }"
        "QCheckBox { font-size: 13px; color: #ffffff; spacing: 8px; }"
        "QComboBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background-color: #24262b; color: #ffffff; selection-background-color: #0077b6; }"
        "QDoubleSpinBox { background-color: #323640; color: #ffffff; font-size: 13px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QPushButton { font-size: 14px; font-weight: bold; border-radius: 4px; padding: 7px 18px; }"
    );

    setupUI();

    if (m_reader && m_reader->isOpen()) {
        int maxCh = m_reader->getResolutionY() - 1;
        if (initialGateMin >= 0 && initialGateMax >= 0) {
            m_spinGateMin->setValue(std::min(initialGateMin, maxCh));
            m_spinGateMax->setValue(std::min(initialGateMax, maxCh));
        } else {
            m_spinGateMin->setValue(150);
            m_spinGateMax->setValue(160);
        }
        onGateParametersChanged();
    }
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

    QGridLayout *paramGrid = new QGridLayout();
    paramGrid->setSpacing(10);

    paramGrid->addWidget(new QLabel(tr("Gate Channel Min:"), grpGate), 0, 0);
    m_spinGateMin = new QSpinBox(grpGate);
    m_spinGateMin->setRange(0, m_reader ? m_reader->getResolutionY() - 1 : 10239);
    connect(m_spinGateMin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MatrixGateDialog::onGateParametersChanged);
    paramGrid->addWidget(m_spinGateMin, 0, 1);

    paramGrid->addWidget(new QLabel(tr("Gate Channel Max:"), grpGate), 0, 2);
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

    m_comboBgMode = new QComboBox(grpGate);
    m_comboBgMode->addItem(tr("Common (Projection)"), 1);
    m_comboBgMode->addItem(tr("Normal (Local Baseline)"), 2);
    m_comboBgMode->addItem(tr("Auto (SNIP Filter)"), 3);
    m_comboBgMode->addItem(tr("None (Raw)"), 0);

    if (m_reader) {
        MatrixBgMode mode = m_reader->getBackgroundConfig().mode;
        int idx = (mode == MatrixBgMode::Common) ? 0 :
                  (mode == MatrixBgMode::Normal) ? 1 :
                  (mode == MatrixBgMode::Auto) ? 2 : 3;
        m_comboBgMode->setCurrentIndex(idx);
    }
    bgRow->addWidget(m_comboBgMode);

    bgRow->addWidget(new QLabel(tr("Factor:"), grpGate));
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
    m_lblBgStats->setStyleSheet("color: #44ffaa; font-size: 12px; font-family: monospace;");
    gateLayout->addWidget(m_lblBgStats);

    connect(m_chkEnableBg, &QCheckBox::toggled, this, &MatrixGateDialog::onBackgroundSettingsChanged);
    connect(m_comboBgMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatrixGateDialog::onBackgroundSettingsChanged);
    connect(m_spinCorrFactor, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MatrixGateDialog::onBackgroundSettingsChanged);

    mainLayout->addWidget(grpGate);

    // 3. Sliced Gate 1D Preview
    QGroupBox *grpPreview = new QGroupBox(tr("Gated Coincidence Spectrum Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(grpPreview);
    previewLayout->setSpacing(6);
    previewLayout->setContentsMargins(12, 14, 12, 10);

    m_plotPreview = new Matrix1DPreviewWidget(grpPreview);
    connect(m_plotPreview, &Matrix1DPreviewWidget::hoverInfoChanged,
            this, &MatrixGateDialog::onHoverInfoChanged);
    previewLayout->addWidget(m_plotPreview, 1);

    m_lblHoverReadout = new QLabel(tr("Hover over spectrum to inspect channel counts"), grpPreview);
    m_lblHoverReadout->setStyleSheet("font-size: 12px; color: #88909e; font-family: monospace;");
    m_lblHoverReadout->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_lblHoverReadout);

    mainLayout->addWidget(grpPreview, 1);

    // 4. Bottom Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    bottomLayout->addStretch(1);

    m_btnSliceGate = new QPushButton(tr("Slice & Load into Pad"), this);
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

void MatrixGateDialog::onBackgroundSettingsChanged()
{
    if (m_reader && m_chkEnableBg && m_comboBgMode && m_spinCorrFactor) {
        MatrixBackgroundConfig cfg;
        cfg.enabled = m_chkEnableBg->isChecked();
        int modeIdx = m_comboBgMode->currentIndex();
        cfg.mode = (modeIdx == 0) ? MatrixBgMode::Common :
                   (modeIdx == 1) ? MatrixBgMode::Normal :
                   (modeIdx == 2) ? MatrixBgMode::Auto : MatrixBgMode::None;
        cfg.correctionFactor = m_spinCorrFactor->value();
        m_reader->setBackgroundConfig(cfg);
    }

    if (m_comboBgMode && m_spinCorrFactor && m_chkEnableBg) {
        m_comboBgMode->setEnabled(m_chkEnableBg->isChecked());
        m_spinCorrFactor->setEnabled(m_chkEnableBg->isChecked());
    }

    updateGatePreview();
}

void MatrixGateDialog::updateGatePreview()
{
    if (!m_reader || !m_reader->isOpen() || !m_plotPreview) return;

    int ch1 = m_spinGateMin->value();
    int ch2 = m_spinGateMax->value();
    if (ch1 > ch2) std::swap(ch1, ch2);

    int gateAxis = 1;
    if (m_radioGateX && m_radioGateX->isChecked()) {
        gateAxis = 0;
    }

    double pfacs = 0.0;
    bool bgEnabled = m_chkEnableBg ? m_chkEnableBg->isChecked() : true;
    m_currentSlice = m_reader->getGateSlice(ch1, ch2, gateAxis, bgEnabled);
    std::vector<double> rawSlice = m_reader->getRawGateSlice(ch1, ch2, gateAxis);
    std::vector<double> bgSlice = m_reader->computeBackgroundSlice(ch1, ch2, gateAxis, &pfacs);

    double rawCounts = 0.0;
    for (double v : rawSlice) rawCounts += v;
    double bgCounts = 0.0;
    for (double v : bgSlice) bgCounts += v;
    double netCounts = 0.0;
    for (double v : m_currentSlice) netCounts += v;

    if (m_lblBgStats) {
        if (bgEnabled && m_reader->getBackgroundConfig().mode != MatrixBgMode::None) {
            m_lblBgStats->setText(
                QString("BG Mode: %1 | pfacs = %2 (%3%) | Subtracted BG: %4 cts | Net: %5 cts")
                    .arg(m_comboBgMode ? m_comboBgMode->currentText() : "Common")
                    .arg(pfacs, 0, 'f', 4)
                    .arg(pfacs * 100.0, 0, 'f', 2)
                    .arg(QLocale().toString(static_cast<qlonglong>(std::round(bgCounts))))
                    .arg(QLocale().toString(static_cast<qlonglong>(std::round(netCounts))))
            );
        } else {
            m_lblBgStats->setText(
                QString("Raw Coincidence Gate (No Subtraction) | Counts: %1")
                    .arg(QLocale().toString(static_cast<qlonglong>(std::round(rawCounts))))
            );
        }
    }

    QString label = QString("Gate [%1 - %2 ch]").arg(ch1).arg(ch2);
    if (m_isCalibrated) {
        label += QString(" (%1 - %2 keV)").arg(channelToEnergy(ch1), 0, 'f', 1).arg(channelToEnergy(ch2), 0, 'f', 1);
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

    updateGatePreview();
}

void MatrixGateDialog::onHoverInfoChanged(int ch, double energy, double counts)
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
    m_lblHoverReadout->setText(info);
}

void MatrixGateDialog::onSliceGateClicked()
{
    if (m_currentSlice.empty()) return;

    int ch1 = m_spinGateMin->value();
    int ch2 = m_spinGateMax->value();
    if (ch1 > ch2) std::swap(ch1, ch2);

    QString title;
    if (m_isCalibrated) {
        title = QString("[Gate %1-%2 keV] %3")
                    .arg(channelToEnergy(ch1), 0, 'f', 1)
                    .arg(channelToEnergy(ch2), 0, 'f', 1)
                    .arg(m_reader->getFileName());
    } else {
        title = QString("[Gate %1-%2 ch] %3")
                    .arg(ch1)
                    .arg(ch2)
                    .arg(m_reader->getFileName());
    }

    emit loadGateSliceRequested(m_currentSlice, title, false);
    accept();
}

void MatrixGateDialog::onOverlayGateClicked()
{
    if (m_currentSlice.empty()) return;

    int ch1 = m_spinGateMin->value();
    int ch2 = m_spinGateMax->value();
    if (ch1 > ch2) std::swap(ch1, ch2);

    QString title;
    if (m_isCalibrated) {
        title = QString("[Gate %1-%2 keV] %3")
                    .arg(channelToEnergy(ch1), 0, 'f', 1)
                    .arg(channelToEnergy(ch2), 0, 'f', 1)
                    .arg(m_reader->getFileName());
    } else {
        title = QString("[Gate %1-%2 ch] %3")
                    .arg(ch1)
                    .arg(ch2)
                    .arg(m_reader->getFileName());
    }

    emit loadGateSliceRequested(m_currentSlice, title, true);
    accept();
}
