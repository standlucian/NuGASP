#include "SpectrumImportDialog.h"

#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QLinearGradient>

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>

//==============================================================================
// AutoDetectSpectrumFile
//==============================================================================
// Examines file size, binary vs. ASCII heuristics, and byte distributions to
// determine the most probable spectrum format and channel count.
//==============================================================================
SpectrumDetectionResult AutoDetectSpectrumFile(const std::string &filename)
{
    SpectrumDetectionResult res;
    res.filename = filename;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        res.confidenceReason = "Could not open file for reading.";
        return res;
    }

    const uint64_t fileSize = static_cast<uint64_t>(file.tellg());
    res.fileSize = fileSize;
    file.seekg(0, std::ios::beg);

    if (fileSize == 0) {
        res.confidenceReason = "File is empty (0 bytes).";
        return res;
    }

    // Read up to 8192 bytes to probe encoding
    const std::size_t probeSize = std::min(fileSize, static_cast<uint64_t>(8192));
    std::vector<char> probeBuffer(probeSize);
    file.read(probeBuffer.data(), probeSize);

    // 1. Evaluate ASCII characteristics
    std::size_t printableCount = 0;
    std::size_t whitespaceCount = 0;
    std::size_t nullByteCount = 0;

    for (std::size_t i = 0; i < probeSize; ++i) {
        unsigned char uc = static_cast<unsigned char>(probeBuffer[i]);
        if (uc == 0) {
            nullByteCount++;
        }
        if (std::isprint(uc)) {
            printableCount++;
        } else if (std::isspace(uc)) {
            whitespaceCount++;
        }
    }

    const double textRatio = static_cast<double>(printableCount + whitespaceCount) / probeSize;
    const bool isLikelyAscii = (nullByteCount == 0 && textRatio > 0.98);

    if (isLikelyAscii) {
        // Count numbers in full ASCII file
        file.seekg(0, std::ios::beg);
        std::string line;
        int countTokens = 0;
        std::vector<double> allAscii;

        while (std::getline(file, line)) {
            // Skip common comment lines
            if (line.empty()) continue;
            char first = line.find_first_not_of(" \t\r\n") != std::string::npos
                             ? line[line.find_first_not_of(" \t\r\n")]
                             : ' ';
            if (first == '#' || first == '!' || first == ';' || first == '$') {
                continue;
            }

            std::istringstream iss(line);
            double val;
            while (iss >> val) {
                countTokens++;
                if (allAscii.size() < 100000) {
                    allAscii.push_back(val);
                }
            }
        }

        res.guessedFormat = SpectrumFormat::Ascii;
        res.totalChannelsFound = countTokens;

        // Determine nearest standard 1024 multiple or exact count
        if (countTokens <= 1024) res.guessedLength = 1024;
        else if (countTokens <= 2048) res.guessedLength = 2048;
        else if (countTokens <= 4096) res.guessedLength = 4096;
        else if (countTokens <= 8192) res.guessedLength = 8192;
        else if (countTokens <= 10240) res.guessedLength = 10240;
        else if (countTokens <= 16384) res.guessedLength = 16384;
        else if (countTokens <= 32768) res.guessedLength = 32768;
        else res.guessedLength = countTokens;

        res.numSpectra = std::max(1, countTokens / res.guessedLength);
        if (res.numSpectra > 1) {
            res.confidenceReason = QString("Multi-spectrum ASCII: %1 spectra of %2 channels (%3 values total)")
                                       .arg(res.numSpectra).arg(res.guessedLength).arg(countTokens);
        } else {
            res.confidenceReason = QString("ASCII text: found %1 numeric values").arg(countTokens);
        }
        ReadSpectrumData(filename, SpectrumFormat::Ascii, res.guessedLength, 0, res.sampleCounts);
        return res;
    }

    // 2. Binary File Heuristics
    // Check known 32-bit integer size multiples (4 bytes per channel)
    int candidateChannels32 = static_cast<int>(fileSize / 4);

    bool matchesStandard32 = false;
    if (fileSize == 4096)   { res.guessedLength = 1024; matchesStandard32 = true; }
    else if (fileSize == 8192)  { res.guessedLength = 2048; matchesStandard32 = true; }
    else if (fileSize == 16384) { res.guessedLength = 4096; matchesStandard32 = true; }
    else if (fileSize == 32768) { res.guessedLength = 8192; matchesStandard32 = true; }
    else if (fileSize == 40960) { res.guessedLength = 10240; matchesStandard32 = true; } // NuTrackN/XTrackN standard
    else if (fileSize == 65536) { res.guessedLength = 16384; matchesStandard32 = true; }
    else if (fileSize == 131072){ res.guessedLength = 32768; matchesStandard32 = true; }

    if (matchesStandard32) {
        res.guessedFormat = SpectrumFormat::LongInt32;
        res.totalChannelsFound = res.guessedLength;
        res.numSpectra = 1;
        res.confidenceReason = QString("Binary: exact match for %1-channel 32-bit integer spectrum (%2 bytes)")
                                   .arg(res.guessedLength).arg(fileSize);
        ReadSpectrumData(filename, SpectrumFormat::LongInt32, res.guessedLength, 0, res.sampleCounts);
        return res;
    }

    // Check multi-spectrum 32-bit files (divisible by 4 bytes/channel)
    if (fileSize % 4 == 0 && candidateChannels32 > 10240) {
        res.guessedFormat = SpectrumFormat::LongInt32;
        res.totalChannelsFound = candidateChannels32;
        // Check standard nuclear spectroscopy multi-spectrum detector sizes: 4096 (4k), 2048 (2k), 1024 (1k), 8192 (8k)
        if (fileSize % (4096 * 4) == 0) {
            res.guessedLength = 4096;
            res.numSpectra = candidateChannels32 / 4096;
        } else if (fileSize % (2048 * 4) == 0) {
            res.guessedLength = 2048;
            res.numSpectra = candidateChannels32 / 2048;
        } else if (fileSize % (1024 * 4) == 0) {
            res.guessedLength = 1024;
            res.numSpectra = candidateChannels32 / 1024;
        } else if (fileSize % (8192 * 4) == 0) {
            res.guessedLength = 8192;
            res.numSpectra = candidateChannels32 / 8192;
        } else if (fileSize % (10240 * 4) == 0) {
            res.guessedLength = 10240;
            res.numSpectra = candidateChannels32 / 10240;
        } else {
            res.guessedLength = 4096;
            res.numSpectra = std::max(1, candidateChannels32 / 4096);
        }

        res.confidenceReason = QString("Multi-spectrum binary: %1 spectra of %2 channels (32-bit int, %3 bytes)")
                                   .arg(res.numSpectra).arg(res.guessedLength).arg(fileSize);
        ReadSpectrumData(filename, SpectrumFormat::LongInt32, res.guessedLength, 0, res.sampleCounts);
        return res;
    }

    // Check 16-bit integer size multiples (2 bytes per channel)
    if (fileSize % 2 == 0 && fileSize % 4 != 0) {
        res.guessedFormat = SpectrumFormat::ShortInt16;
        res.totalChannelsFound = static_cast<int>(fileSize / 2);
        res.numSpectra = 1;
        res.guessedLength = res.totalChannelsFound;
        res.confidenceReason = QString("Binary: 16-bit integer spectrum (%1 channels, %2 bytes)")
                                   .arg(res.totalChannelsFound).arg(fileSize);
        ReadSpectrumData(filename, SpectrumFormat::ShortInt16, res.guessedLength, 0, res.sampleCounts);
        return res;
    }

    // Default binary fallback to LongInt32
    if (fileSize % 4 == 0) {
        res.guessedFormat = SpectrumFormat::LongInt32;
        res.totalChannelsFound = candidateChannels32;
        res.numSpectra = 1;
        res.guessedLength = (candidateChannels32 >= 10240) ? 10240 : candidateChannels32;
        res.confidenceReason = QString("Binary: divisible by 4 bytes/channel (%1 channels)")
                                   .arg(candidateChannels32);
        ReadSpectrumData(filename, SpectrumFormat::LongInt32, res.guessedLength, 0, res.sampleCounts);
        return res;
    }

    // Generic fallback
    res.guessedFormat = SpectrumFormat::Ascii;
    res.guessedLength = 10240;
    res.numSpectra = 1;
    res.confidenceReason = "Unrecognized byte size; defaulted to standard 10k ASCII.";
    ReadSpectrumData(filename, SpectrumFormat::Ascii, res.guessedLength, 0, res.sampleCounts);
    return res;
}

//==============================================================================
// ReadSpectrumData
//==============================================================================
// Reads binary or ASCII spectrum files into a floating-point vector according
// to the requested format, channel limit, and spectrum index.
//==============================================================================
bool ReadSpectrumData(const std::string &filename, SpectrumFormat format, int requestedChannels,
                      int spectrumIndex, std::vector<double> &outData, QString *errorMsg)
{
    outData.clear();
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        if (errorMsg) *errorMsg = "Could not open spectrum file.";
        return false;
    }

    const int targetLen = (requestedChannels > 0) ? requestedChannels : 1048576;
    const int specIdx = std::max(0, spectrumIndex);

    switch (format) {
        case SpectrumFormat::LongInt32: {
            const uint64_t byteOffset = static_cast<uint64_t>(specIdx) * targetLen * sizeof(uint32_t);
            file.seekg(byteOffset, std::ios::beg);
            uint32_t val = 0;
            while (file.read(reinterpret_cast<char*>(&val), sizeof(val))) {
                outData.push_back(static_cast<double>(val));
                if (static_cast<int>(outData.size()) >= targetLen) break;
            }
            break;
        }
        case SpectrumFormat::ShortInt16: {
            const uint64_t byteOffset = static_cast<uint64_t>(specIdx) * targetLen * sizeof(uint16_t);
            file.seekg(byteOffset, std::ios::beg);
            uint16_t val = 0;
            while (file.read(reinterpret_cast<char*>(&val), sizeof(val))) {
                outData.push_back(static_cast<double>(val));
                if (static_cast<int>(outData.size()) >= targetLen) break;
            }
            break;
        }
        case SpectrumFormat::Float32: {
            const uint64_t byteOffset = static_cast<uint64_t>(specIdx) * targetLen * sizeof(float);
            file.seekg(byteOffset, std::ios::beg);
            float val = 0.0f;
            while (file.read(reinterpret_cast<char*>(&val), sizeof(val))) {
                if (std::isnan(val) || std::isinf(val)) val = 0.0f;
                outData.push_back(static_cast<double>(val));
                if (static_cast<int>(outData.size()) >= targetLen) break;
            }
            break;
        }
        case SpectrumFormat::Double64: {
            const uint64_t byteOffset = static_cast<uint64_t>(specIdx) * targetLen * sizeof(double);
            file.seekg(byteOffset, std::ios::beg);
            double val = 0.0;
            while (file.read(reinterpret_cast<char*>(&val), sizeof(val))) {
                if (std::isnan(val) || std::isinf(val)) val = 0.0;
                outData.push_back(val);
                if (static_cast<int>(outData.size()) >= targetLen) break;
            }
            break;
        }
        case SpectrumFormat::Ascii:
        case SpectrumFormat::Auto: {
            file.close();
            std::ifstream asciiFile(filename);
            std::string line;
            double val = 0.0;
            int tokenIndex = 0;
            const int skipTokens = specIdx * targetLen;

            while (std::getline(asciiFile, line)) {
                if (line.empty()) continue;
                char first = line.find_first_not_of(" \t\r\n") != std::string::npos
                                 ? line[line.find_first_not_of(" \t\r\n")]
                                 : ' ';
                if (first == '#' || first == '!' || first == ';' || first == '$') {
                    continue;
                }

                std::istringstream iss(line);
                while (iss >> val) {
                    if (tokenIndex >= skipTokens) {
                        outData.push_back(val);
                        if (static_cast<int>(outData.size()) >= targetLen) break;
                    }
                    tokenIndex++;
                }
                if (static_cast<int>(outData.size()) >= targetLen) break;
            }
            break;
        }
    }

    if (outData.empty()) {
        if (errorMsg) *errorMsg = "File contains no readable numeric data for selected format.";
        return false;
    }

    // Pad with zeros if requestedChannels is explicitly greater than available
    if (requestedChannels > 0 && static_cast<int>(outData.size()) < requestedChannels) {
        outData.resize(requestedChannels, 0.0);
    }

    return true;
}

//==============================================================================
// QSpectrumPlotPreview Implementation
//==============================================================================
QSpectrumPlotPreview::QSpectrumPlotPreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void QSpectrumPlotPreview::setSpectra(const std::vector<double> &primary, int primaryIdx,
                                      const std::vector<double> &secondary, int secondaryIdx)
{
    m_primaryData = primary;
    m_primaryIndex = primaryIdx;
    m_secondaryData = secondary;
    m_secondaryIndex = secondaryIdx;
    update();
}

void QSpectrumPlotPreview::clear()
{
    m_primaryData.clear();
    m_secondaryData.clear();
    update();
}

void QSpectrumPlotPreview::drawSinglePlot(QPainter &painter, const QRect &rect,
                                          const std::vector<double> &data, int specIdx,
                                          const QColor &lineColor, const QColor &fillColor)
{
    if (data.empty()) {
        painter.setPen(QColor("#777e8c"));
        painter.drawText(rect, Qt::AlignCenter, tr("No data"));
        return;
    }

    const int leftPad = 52;
    const int rightPad = 12;
    const int topPad = 20;
    const int botPad = 16;

    QRect plotArea(rect.left() + leftPad, rect.top() + topPad,
                   rect.width() - leftPad - rightPad,
                   rect.height() - topPad - botPad);

    if (plotArea.width() <= 10 || plotArea.height() <= 10) return;

    // Fill background of plot canvas
    painter.fillRect(plotArea, QColor("#111317"));
    painter.setPen(QPen(QColor("#2c313d"), 1));
    painter.drawRect(plotArea);

    double maxVal = 1.0;
    for (double v : data) {
        if (v > maxVal) maxVal = v;
    }

    // Grid lines (horizontal divisions)
    painter.setPen(QPen(QColor("#1c2028"), 1, Qt::DotLine));
    for (int g = 1; g <= 3; ++g) {
        int y = plotArea.bottom() - (plotArea.height() * g / 4);
        painter.drawLine(plotArea.left(), y, plotArea.right(), y);
    }

    const int plotW = plotArea.width();
    const int nCh = static_cast<int>(data.size());

    QPolygonF fillPoly;
    fillPoly << QPointF(plotArea.left(), plotArea.bottom());

    QPolygonF linePoly;
    for (int px = 0; px < plotW; ++px) {
        int chStart = static_cast<int>(static_cast<int64_t>(px) * nCh / plotW);
        int chEnd = static_cast<int>(static_cast<int64_t>(px + 1) * nCh / plotW);
        if (chEnd <= chStart) chEnd = chStart + 1;
        if (chEnd > nCh) chEnd = nCh;

        double cMin = data[chStart];
        double cMax = data[chStart];
        for (int c = chStart + 1; c < chEnd; ++c) {
            if (data[c] < cMin) cMin = data[c];
            if (data[c] > cMax) cMax = data[c];
        }

        double yTop = plotArea.bottom() - (cMax / maxVal) * (plotArea.height() - 2);
        double yBot = plotArea.bottom() - (cMin / maxVal) * (plotArea.height() - 2);
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
    grad.setColorAt(0.0, fillColor);
    grad.setColorAt(1.0, QColor(fillColor.red(), fillColor.green(), fillColor.blue(), 8));
    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(fillPoly);

    // Outline curve
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(lineColor, 1.2));
    painter.drawPolyline(linePoly);

    // Badges & Readouts
    QFont badgeFont = painter.font();
    badgeFont.setPointSize(9);
    badgeFont.setBold(true);
    painter.setFont(badgeFont);

    // Spec badge (top-left)
    painter.setPen(lineColor);
    QString badge = QString("Spectrum #%1 (%2 ch)").arg(specIdx).arg(nCh);
    painter.drawText(plotArea.left() + 6, rect.top() + 15, badge);

    // Max counts (top-right)
    QFont infoFont = painter.font();
    infoFont.setBold(false);
    infoFont.setPointSize(8);
    painter.setFont(infoFont);
    painter.setPen(QColor("#9da4b0"));
    QString maxStr = QString("Max: %1 cts").arg(QLocale().toString(static_cast<long long>(std::round(maxVal))));
    painter.drawText(plotArea.right() - painter.fontMetrics().horizontalAdvance(maxStr) - 6, rect.top() + 15, maxStr);

    // Channel range labels (bottom)
    painter.drawText(plotArea.left(), rect.bottom() - 2, "Ch 0");
    QString endStr = QString("Ch %1").arg(nCh - 1);
    painter.drawText(plotArea.right() - painter.fontMetrics().horizontalAdvance(endStr), rect.bottom() - 2, endStr);

    // Y axis labels (left)
    painter.drawText(rect.left() + 2, plotArea.top() + 10, QString::number(static_cast<long long>(std::round(maxVal))));
    painter.drawText(rect.left() + 2, plotArea.bottom(), "0");
}

void QSpectrumPlotPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.fillRect(rect(), QColor("#14161a"));
    painter.setPen(QPen(QColor("#2b303c"), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    if (m_primaryData.empty()) {
        painter.setPen(QColor("#777e8c"));
        painter.drawText(rect(), Qt::AlignCenter, tr("No spectrum data loaded for preview."));
        return;
    }

    if (m_secondaryData.empty()) {
        drawSinglePlot(painter, rect(), m_primaryData, m_primaryIndex,
                       QColor("#00f0ff"), QColor(0, 240, 255, 45));
    } else {
        const int midY = rect().height() / 2;
        QRect topRect(rect().left(), rect().top(), rect().width(), midY - 1);
        QRect botRect(rect().left(), rect().top() + midY + 1, rect().width(), rect().height() - midY - 1);

        drawSinglePlot(painter, topRect, m_primaryData, m_primaryIndex,
                       QColor("#00f0ff"), QColor(0, 240, 255, 45));
        drawSinglePlot(painter, botRect, m_secondaryData, m_secondaryIndex,
                       QColor("#ffb830"), QColor(255, 184, 48, 45));

        // Draw separator
        painter.setPen(QPen(QColor("#2d323e"), 1));
        painter.drawLine(rect().left() + 8, rect().top() + midY, rect().right() - 8, rect().top() + midY);
    }
}

//==============================================================================
// SpectrumImportDialog Implementation
//==============================================================================
SpectrumImportDialog::SpectrumImportDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_plotPreview(nullptr)
{
    setWindowTitle(tr("Open Spectrum - Format & Length"));
    setMinimumWidth(680);
    resize(720, 620);

    setStyleSheet(
        "QDialog { background-color: #24262b; color: #ffffff; }"
        "QLabel { color: #e6e6e6; font-size: 15px; }"
        "QGroupBox { font-size: 15px; font-weight: bold; color: #00e0ff; border: 1px solid #444955; border-radius: 6px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QComboBox { background-color: #323640; color: #ffffff; font-size: 15px; border: 1px solid #555b68; border-radius: 4px; padding: 5px 10px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #2b2f38; color: #ffffff; selection-background-color: #0088cc; }"
        "QSpinBox { background-color: #323640; color: #ffffff; font-size: 15px; border: 1px solid #555b68; border-radius: 4px; padding: 4px 8px; }"
        "QPushButton { font-size: 15px; font-weight: bold; border-radius: 4px; padding: 6px 18px; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // 1. File Info Card
    QFileInfo fi(filePath);
    m_lblFile = new QLabel(QString("<b>File:</b> %1 (%2 bytes)")
                               .arg(fi.fileName())
                               .arg(QLocale().toString(static_cast<qulonglong>(fi.size()))), this);
    m_lblFile->setStyleSheet("font-size: 16px; color: #ffffff;");
    mainLayout->addWidget(m_lblFile);

    // 2. Auto-Detection Banner
    m_detected = AutoDetectSpectrumFile(filePath.toStdString());

    m_lblDetectedBanner = new QLabel(this);
    m_lblDetectedBanner->setStyleSheet(
        "background-color: #1a3328; color: #44ffaa; border: 1px solid #2e7752; "
        "border-radius: 5px; padding: 8px 12px; font-size: 14px; font-weight: bold;"
    );
    m_lblDetectedBanner->setText(QString("🎯 Auto-Detected: %1").arg(m_detected.confidenceReason));
    mainLayout->addWidget(m_lblDetectedBanner);

    // 3. Settings GroupBox
    QGroupBox *settingsGroup = new QGroupBox(tr("Spectrum Configuration"), this);
    QGridLayout *settingsLayout = new QGridLayout(settingsGroup);
    settingsLayout->setSpacing(10);
    settingsLayout->setContentsMargins(14, 16, 14, 14);

    // Format selection
    QLabel *lblFormat = new QLabel(tr("Data Format:"), settingsGroup);
    m_comboFormat = new QComboBox(settingsGroup);
    m_comboFormat->addItem(tr("32-bit Integer (Long / uint32_t)"), static_cast<int>(SpectrumFormat::LongInt32));
    m_comboFormat->addItem(tr("16-bit Integer (Short / uint16_t)"), static_cast<int>(SpectrumFormat::ShortInt16));
    m_comboFormat->addItem(tr("ASCII Text (Whitespace/Column)"), static_cast<int>(SpectrumFormat::Ascii));
    m_comboFormat->addItem(tr("32-bit Float (Real / IEEE-754)"), static_cast<int>(SpectrumFormat::Float32));
    m_comboFormat->addItem(tr("64-bit Double (Float / IEEE-754)"), static_cast<int>(SpectrumFormat::Double64));

    // Pre-select guessed format
    int formatIdx = m_comboFormat->findData(static_cast<int>(m_detected.guessedFormat));
    if (formatIdx >= 0) {
        m_comboFormat->setCurrentIndex(formatIdx);
    }

    settingsLayout->addWidget(lblFormat, 0, 0);
    settingsLayout->addWidget(m_comboFormat, 0, 1);

    // Length selection
    QLabel *lblLength = new QLabel(tr("Channels (Length):"), settingsGroup);
    m_comboLength = new QComboBox(settingsGroup);
    m_comboLength->addItem(tr("1024 channels (1k)"), 1024);
    m_comboLength->addItem(tr("2048 channels (2k)"), 2048);
    m_comboLength->addItem(tr("4096 channels (4k)"), 4096);
    m_comboLength->addItem(tr("8192 channels (8k)"), 8192);
    m_comboLength->addItem(tr("10240 channels (10k)"), 10240);
    m_comboLength->addItem(tr("16384 channels (16k)"), 16384);
    m_comboLength->addItem(tr("32768 channels (32k)"), 32768);
    m_comboLength->addItem(tr("All Channels in File"), -1);
    m_comboLength->addItem(tr("Custom..."), 0);

    // Custom length spinbox
    m_spinCustomLength = new QSpinBox(settingsGroup);
    m_spinCustomLength->setRange(16, 1048576);
    m_spinCustomLength->setValue(m_detected.guessedLength);
    m_spinCustomLength->setVisible(false);

    // Pre-select guessed length
    int lenIdx = m_comboLength->findData(m_detected.guessedLength);
    if (lenIdx >= 0) {
        m_comboLength->setCurrentIndex(lenIdx);
    } else {
        // Select custom
        int customIdx = m_comboLength->findData(0);
        m_comboLength->setCurrentIndex(customIdx);
        m_spinCustomLength->setVisible(true);
        m_spinCustomLength->setValue(m_detected.guessedLength);
    }

    settingsLayout->addWidget(lblLength, 1, 0);
    settingsLayout->addWidget(m_comboLength, 1, 1);
    settingsLayout->addWidget(m_spinCustomLength, 2, 1);

    // Multi-Spectrum Selection row
    m_specIndexContainer = new QWidget(settingsGroup);
    QHBoxLayout *specIndexLayout = new QHBoxLayout(m_specIndexContainer);
    specIndexLayout->setContentsMargins(0, 0, 0, 0);
    specIndexLayout->setSpacing(6);

    m_lblSpectrumIndex = new QLabel(tr("Spectrum Index:"), settingsGroup);

    m_btnPrevSpec = new QPushButton(tr("◀"), m_specIndexContainer);
    m_btnPrevSpec->setFixedWidth(32);
    m_btnPrevSpec->setFixedHeight(28);
    m_btnPrevSpec->setStyleSheet("background-color: #383c45; color: #00f0ff; font-weight: bold; border-radius: 3px;");

    m_spinSpectrumIndex = new QSpinBox(m_specIndexContainer);
    m_spinSpectrumIndex->setRange(0, 0);
    m_spinSpectrumIndex->setValue(0);
    m_spinSpectrumIndex->setPrefix("# ");
    m_spinSpectrumIndex->setFixedWidth(90);

    m_btnNextSpec = new QPushButton(tr("▶"), m_specIndexContainer);
    m_btnNextSpec->setFixedWidth(32);
    m_btnNextSpec->setFixedHeight(28);
    m_btnNextSpec->setStyleSheet("background-color: #383c45; color: #00f0ff; font-weight: bold; border-radius: 3px;");

    m_lblSpectrumCount = new QLabel(m_specIndexContainer);
    m_lblSpectrumCount->setStyleSheet("color: #a0a0a0; font-size: 13px; font-style: italic;");

    specIndexLayout->addWidget(m_btnPrevSpec);
    specIndexLayout->addWidget(m_spinSpectrumIndex);
    specIndexLayout->addWidget(m_btnNextSpec);
    specIndexLayout->addWidget(m_lblSpectrumCount);
    specIndexLayout->addStretch(1);

    settingsLayout->addWidget(m_lblSpectrumIndex, 3, 0);
    settingsLayout->addWidget(m_specIndexContainer, 3, 1);

    mainLayout->addWidget(settingsGroup);

    // 4. Live Data Preview Box
    QGroupBox *previewGroup = new QGroupBox(tr("Live Data Preview"), this);
    QVBoxLayout *prevLayout = new QVBoxLayout(previewGroup);
    prevLayout->setSpacing(8);
    prevLayout->setContentsMargins(14, 16, 14, 14);

    m_lblPreviewSummary = new QLabel(previewGroup);
    m_lblPreviewSummary->setStyleSheet("font-weight: bold; color: #00f0ff; font-size: 14px;");

    m_plotPreview = new QSpectrumPlotPreview(previewGroup);

    prevLayout->addWidget(m_lblPreviewSummary);
    prevLayout->addWidget(m_plotPreview);
    mainLayout->addWidget(previewGroup);

    // 5. Dialog Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setStyleSheet("background-color: #4a4e58; color: #ffffff; border: 1px solid #666c7a;");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnLoad = new QPushButton(tr("Load Spectrum"), this);
    m_btnLoad->setStyleSheet("background-color: #007acc; color: #ffffff; border: 1px solid #0099ff;");
    m_btnLoad->setDefault(true);
    connect(m_btnLoad, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnLoad);
    mainLayout->addLayout(btnLayout);

    // Connect change signals for live update
    connect(m_comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumImportDialog::onFormatOrLengthChanged);
    connect(m_comboLength, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumImportDialog::onFormatOrLengthChanged);
    connect(m_spinCustomLength, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpectrumImportDialog::onFormatOrLengthChanged);

    connect(m_spinSpectrumIndex, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpectrumImportDialog::onSpectrumIndexChanged);
    connect(m_btnPrevSpec, &QPushButton::clicked,
            this, &SpectrumImportDialog::onPrevSpectrumClicked);
    connect(m_btnNextSpec, &QPushButton::clicked,
            this, &SpectrumImportDialog::onNextSpectrumClicked);

    // Initial setup
    recalculateSpectraCount();
    updatePreview();
}

SpectrumFormat SpectrumImportDialog::getSelectedFormat() const
{
    return static_cast<SpectrumFormat>(m_comboFormat->currentData().toInt());
}

int SpectrumImportDialog::getSelectedLength() const
{
    int val = m_comboLength->currentData().toInt();
    if (val == 0) {
        return m_spinCustomLength->value();
    }
    return val;
}

int SpectrumImportDialog::getSelectedSpectrumIndex() const
{
    return m_spinSpectrumIndex ? m_spinSpectrumIndex->value() : 0;
}

void SpectrumImportDialog::recalculateSpectraCount()
{
    const int len = getSelectedLength();
    if (len <= 0 || m_detected.totalChannelsFound <= 0) {
        m_totalSpectra = 1;
    } else {
        m_totalSpectra = std::max(1, m_detected.totalChannelsFound / len);
    }

    const bool hasMultiple = (m_totalSpectra > 1);
    m_lblSpectrumIndex->setVisible(hasMultiple);
    m_specIndexContainer->setVisible(hasMultiple);

    m_spinSpectrumIndex->blockSignals(true);
    m_spinSpectrumIndex->setRange(0, m_totalSpectra - 1);
    if (m_spinSpectrumIndex->value() >= m_totalSpectra) {
        m_spinSpectrumIndex->setValue(0);
    }
    m_spinSpectrumIndex->blockSignals(false);

    const int currentIdx = m_spinSpectrumIndex->value();
    m_lblSpectrumCount->setText(QString(tr("(Spectrum %1 of %2)"))
                                    .arg(currentIdx + 1)
                                    .arg(m_totalSpectra));

    m_btnPrevSpec->setEnabled(currentIdx > 0);
    m_btnNextSpec->setEnabled(currentIdx < m_totalSpectra - 1);
}

void SpectrumImportDialog::onFormatOrLengthChanged()
{
    int val = m_comboLength->currentData().toInt();
    m_spinCustomLength->setVisible(val == 0);
    recalculateSpectraCount();
    updatePreview();
}

void SpectrumImportDialog::onSpectrumIndexChanged(int index)
{
    m_btnPrevSpec->setEnabled(index > 0);
    m_btnNextSpec->setEnabled(index < m_totalSpectra - 1);
    m_lblSpectrumCount->setText(QString(tr("(Spectrum %1 of %2)"))
                                    .arg(index + 1)
                                    .arg(m_totalSpectra));
    updatePreview();
}

void SpectrumImportDialog::onPrevSpectrumClicked()
{
    if (m_spinSpectrumIndex->value() > 0) {
        m_spinSpectrumIndex->setValue(m_spinSpectrumIndex->value() - 1);
    }
}

void SpectrumImportDialog::onNextSpectrumClicked()
{
    if (m_spinSpectrumIndex->value() < m_totalSpectra - 1) {
        m_spinSpectrumIndex->setValue(m_spinSpectrumIndex->value() + 1);
    }
}

void SpectrumImportDialog::updatePreview()
{
    SpectrumFormat fmt = getSelectedFormat();
    int len = getSelectedLength();
    int specIdx = getSelectedSpectrumIndex();

    QString err;
    bool ok = ReadSpectrumData(m_filePath.toStdString(), fmt, len, specIdx, m_loadedData, &err);

    if (!ok || m_loadedData.empty()) {
        m_lblPreviewSummary->setText(tr("⚠️ Failed to parse spectrum with chosen settings"));
        if (m_plotPreview) m_plotPreview->clear();
        m_btnLoad->setEnabled(false);
        return;
    }

    m_btnLoad->setEnabled(true);

    double maxCount = 0.0;
    double totalArea = 0.0;
    for (double val : m_loadedData) {
        if (val > maxCount) maxCount = val;
        totalArea += val;
    }

    if (m_totalSpectra > 1) {
        int secIdx = (specIdx + 1 < m_totalSpectra) ? specIdx + 1 : specIdx - 1;
        ReadSpectrumData(m_filePath.toStdString(), fmt, len, secIdx, m_secondaryData);

        double secMax = 0.0;
        double secArea = 0.0;
        for (double v : m_secondaryData) {
            if (v > secMax) secMax = v;
            secArea += v;
        }

        m_lblPreviewSummary->setText(
            QString("Spec #%1: Max %2 cts, Total %3  |  Spec #%4: Max %5 cts, Total %6  (%7 spectra in file)")
                .arg(specIdx)
                .arg(QLocale().toString(static_cast<long long>(std::round(maxCount))))
                .arg(QLocale().toString(static_cast<long long>(std::round(totalArea))))
                .arg(secIdx)
                .arg(QLocale().toString(static_cast<long long>(std::round(secMax))))
                .arg(QLocale().toString(static_cast<long long>(std::round(secArea))))
                .arg(m_totalSpectra)
        );

        if (m_plotPreview) {
            m_plotPreview->setSpectra(m_loadedData, specIdx, m_secondaryData, secIdx);
        }
    } else {
        m_lblPreviewSummary->setText(
            QString("Channels: %1 | Max Peak: %2 cts | Total Counts: %3")
                .arg(QLocale().toString(static_cast<int>(m_loadedData.size())))
                .arg(QLocale().toString(static_cast<long long>(std::round(maxCount))))
                .arg(QLocale().toString(static_cast<long long>(std::round(totalArea))))
        );
        m_secondaryData.clear();
        if (m_plotPreview) {
            m_plotPreview->setSpectra(m_loadedData, specIdx);
        }
    }
}

