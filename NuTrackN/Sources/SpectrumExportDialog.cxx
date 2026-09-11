#include "SpectrumExportDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

//==============================================================================
// WriteSpectrumData
//==============================================================================
bool WriteSpectrumData(const std::string &filename,
                       SpectrumFormat format,
                       const std::vector<double> &data,
                       int requestedChannels,
                       SpectrumExportMode mode,
                       int targetIndex,
                       QString *errorMsg)
{
    if (requestedChannels <= 0) {
        requestedChannels = static_cast<int>(data.size());
    }
    if (requestedChannels <= 0) {
        if (errorMsg) *errorMsg = QObject::tr("Spectrum data is empty. Nothing to export.");
        return false;
    }

    // Prepare buffer with exact requested channel count (pad with 0.0 or truncate)
    std::vector<double> outData(requestedChannels, 0.0);
    int copyCount = std::min(requestedChannels, static_cast<int>(data.size()));
    for (int i = 0; i < copyCount; ++i) {
        outData[i] = data[i];
    }

    // Determine bytes per channel for binary formats
    size_t bytesPerCh = 4;
    switch (format) {
        case SpectrumFormat::Double64:   bytesPerCh = 8; break;
        case SpectrumFormat::ShortInt16: bytesPerCh = 2; break;
        default:                         bytesPerCh = 4; break;
    }
    size_t specBytes = static_cast<size_t>(requestedChannels) * bytesPerCh;

    if (format == SpectrumFormat::Ascii) {
        std::ios_base::openmode openMode = std::ios::out;
        if (mode == SpectrumExportMode::Append) {
            openMode |= std::ios::app;
        } else {
            openMode |= std::ios::trunc;
        }

        std::ofstream file(filename, openMode);
        if (!file.is_open()) {
            if (errorMsg) *errorMsg = QObject::tr("Could not open file for writing:\n%1").arg(QString::fromStdString(filename));
            return false;
        }

        // Fortran 10I9 format: 10 integer counts per line, 9 characters wide each
        int col = 0;
        for (int i = 0; i < requestedChannels; ++i) {
            int64_t val = static_cast<int64_t>(std::llround(outData[i]));
            if (val > 999999999LL)  val = 999999999LL;
            if (val < -99999999LL)  val = -99999999LL;
            file << std::setw(9) << val;
            col++;
            if (col == 10) {
                file << "\n";
                col = 0;
            }
        }
        if (col != 0) {
            file << "\n";
        }

        if (!file.good()) {
            if (errorMsg) *errorMsg = QObject::tr("Write error occurred while writing ASCII spectrum.");
            return false;
        }
        return true;
    }

    // Binary format handling
    if (mode == SpectrumExportMode::WriteAtIndex) {
        // Open existing file in read/write binary mode; create if not existing
        std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            // File does not exist yet; create it
            file.clear();
            file.open(filename, std::ios::out | std::ios::binary);
            file.close();
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!file.is_open()) {
            if (errorMsg) *errorMsg = QObject::tr("Could not open file for direct-access write:\n%1").arg(QString::fromStdString(filename));
            return false;
        }

        std::streamoff targetOffset = static_cast<std::streamoff>(targetIndex) * specBytes;
        file.seekp(0, std::ios::end);
        std::streamoff currentSize = file.tellp();

        if (currentSize < targetOffset) {
            // Pad file with zeros up to targetOffset
            std::streamoff padBytes = targetOffset - currentSize;
            std::vector<char> zeroBuf(std::min<size_t>(static_cast<size_t>(padBytes), 65536), 0);
            while (padBytes > 0) {
                size_t toWrite = std::min<size_t>(static_cast<size_t>(padBytes), zeroBuf.size());
                file.write(zeroBuf.data(), toWrite);
                padBytes -= toWrite;
            }
        }

        file.seekp(targetOffset, std::ios::beg);

        if (format == SpectrumFormat::Float32) {
            std::vector<float> buf(requestedChannels);
            for (int i = 0; i < requestedChannels; ++i) buf[i] = static_cast<float>(outData[i]);
            file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(float));
        } else if (format == SpectrumFormat::Double64) {
            file.write(reinterpret_cast<const char*>(outData.data()), outData.size() * sizeof(double));
        } else if (format == SpectrumFormat::ShortInt16) {
            std::vector<uint16_t> buf(requestedChannels);
            for (int i = 0; i < requestedChannels; ++i) {
                double v = std::max(0.0, std::min(65535.0, std::round(outData[i])));
                buf[i] = static_cast<uint16_t>(v);
            }
            file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint16_t));
        } else { // LongInt32 (default GASPware L)
            std::vector<uint32_t> buf(requestedChannels);
            for (int i = 0; i < requestedChannels; ++i) {
                double v = std::max(0.0, std::min(4294967295.0, std::round(outData[i])));
                buf[i] = static_cast<uint32_t>(v);
            }
            file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint32_t));
        }

        if (!file.good()) {
            if (errorMsg) *errorMsg = QObject::tr("Write error occurred while updating spectrum index #%1.").arg(targetIndex);
            return false;
        }
        return true;
    }

    // NewOrOverwrite or Append
    std::ios_base::openmode openMode = std::ios::out | std::ios::binary;
    if (mode == SpectrumExportMode::Append) {
        openMode |= std::ios::app;
    } else {
        openMode |= std::ios::trunc;
    }

    std::ofstream file(filename, openMode);
    if (!file.is_open()) {
        if (errorMsg) *errorMsg = QObject::tr("Could not open file for writing:\n%1").arg(QString::fromStdString(filename));
        return false;
    }

    if (format == SpectrumFormat::Float32) {
        std::vector<float> buf(requestedChannels);
        for (int i = 0; i < requestedChannels; ++i) buf[i] = static_cast<float>(outData[i]);
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(float));
    } else if (format == SpectrumFormat::Double64) {
        file.write(reinterpret_cast<const char*>(outData.data()), outData.size() * sizeof(double));
    } else if (format == SpectrumFormat::ShortInt16) {
        std::vector<uint16_t> buf(requestedChannels);
        for (int i = 0; i < requestedChannels; ++i) {
            double v = std::max(0.0, std::min(65535.0, std::round(outData[i])));
            buf[i] = static_cast<uint16_t>(v);
        }
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint16_t));
    } else { // LongInt32 (default GASPware L)
        std::vector<uint32_t> buf(requestedChannels);
        for (int i = 0; i < requestedChannels; ++i) {
            double v = std::max(0.0, std::min(4294967295.0, std::round(outData[i])));
            buf[i] = static_cast<uint32_t>(v);
        }
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint32_t));
    }

    if (!file.good()) {
        if (errorMsg) *errorMsg = QObject::tr("Write error occurred while exporting binary spectrum.");
        return false;
    }
    return true;
}

//==============================================================================
// SpectrumExportDialog implementation
//==============================================================================
SpectrumExportDialog::SpectrumExportDialog(const std::vector<double> &data,
                                           const QString &suggestedPath,
                                           SpectrumFormat initialFormat,
                                           int initialLength,
                                           QWidget *parent)
    : QDialog(parent), m_data(data)
{
    setWindowTitle(tr("Export Spectrum (W)"));
    setMinimumWidth(560);
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #d4d4d4; font-family: sans-serif; font-size: 12px; }"
        "QLabel { color: #cccccc; }"
        "QLineEdit { background-color: #2d2d2d; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px; }"
        "QLineEdit:focus { border: 1px solid #3b82f6; }"
        "QComboBox { background-color: #2d2d2d; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px 8px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 20px; border-left: 1px solid #3c3c3c; }"
        "QComboBox QAbstractItemView { background-color: #252526; color: #ffffff; selection-background-color: #3b82f6; selection-color: #ffffff; border: 1px solid #454545; }"
        "QSpinBox { background-color: #2d2d2d; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px; }"
        "QPushButton { background-color: #333333; color: #ffffff; border: 1px solid #444444; border-radius: 4px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #444444; border-color: #555555; }"
        "QPushButton:pressed { background-color: #222222; }"
        "QPushButton#btnExport { background-color: #15803d; border-color: #16a34a; }"
        "QPushButton#btnExport:hover { background-color: #16a34a; border-color: #22c55e; }"
    );

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Title / Description
    auto *titleLabel = new QLabel(tr("<b>Export Active Spectrum to Disk</b>"), this);
    titleLabel->setStyleSheet("font-size: 14px; color: #60a5fa;");
    mainLayout->addWidget(titleLabel);

    // Grid Form
    auto *formGrid = new QGridLayout();
    formGrid->setSpacing(10);

    // 1. Output File Path
    auto *pathLabel = new QLabel(tr("Output File:"), this);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Select destination file..."));
    if (!suggestedPath.isEmpty()) {
        m_pathEdit->setText(suggestedPath);
    }
    connect(m_pathEdit, &QLineEdit::textChanged, this, &SpectrumExportDialog::onFilePathChanged);

    m_browseBtn = new QPushButton(tr("Browse..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &SpectrumExportDialog::onBrowseClicked);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(m_browseBtn);

    formGrid->addWidget(pathLabel, 0, 0);
    formGrid->addLayout(pathLayout, 0, 1);

    // 2. Format
    auto *formatLabel = new QLabel(tr("Format:"), this);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("Long (32-bit Integer, GASPware L)"), static_cast<int>(SpectrumFormat::LongInt32));
    m_formatCombo->addItem(tr("Float (32-bit Float, GASPware R)"), static_cast<int>(SpectrumFormat::Float32));
    m_formatCombo->addItem(tr("Double (64-bit Float, GASPware D)"), static_cast<int>(SpectrumFormat::Double64));
    m_formatCombo->addItem(tr("ASCII Fortran (10I9 format, GASPware A)"), static_cast<int>(SpectrumFormat::Ascii));
    m_formatCombo->addItem(tr("Short (16-bit Integer)"), static_cast<int>(SpectrumFormat::ShortInt16));

    // Match initial format
    int fmtIdx = 0;
    for (int i = 0; i < m_formatCombo->count(); ++i) {
        if (m_formatCombo->itemData(i).toInt() == static_cast<int>(initialFormat)) {
            fmtIdx = i;
            break;
        }
    }
    m_formatCombo->setCurrentIndex(fmtIdx);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumExportDialog::onFormatChanged);

    formGrid->addWidget(formatLabel, 1, 0);
    formGrid->addWidget(m_formatCombo, 1, 1);

    // 3. Channels / Length
    auto *lengthLabel = new QLabel(tr("Channel Length:"), this);
    m_lengthCombo = new QComboBox(this);
    m_lengthCombo->setEditable(true);

    int curSize = static_cast<int>(m_data.size());
    if (initialLength > 0) curSize = initialLength;
    if (curSize <= 0) curSize = 10240;

    // Standard presets
    const std::vector<int> presets = {1024, 2048, 4096, 8192, 10240, 16384, 32768};
    bool matchedCurrent = false;
    for (int p : presets) {
        if (p == curSize) {
            m_lengthCombo->addItem(QString("%1 (Current Spectrum Channels)").arg(p), p);
            matchedCurrent = true;
        } else {
            m_lengthCombo->addItem(QString("%1 channels (%2k)").arg(p).arg(p / 1024), p);
        }
    }
    if (!matchedCurrent && curSize > 0) {
        m_lengthCombo->insertItem(0, QString("%1 (Current Spectrum Channels)").arg(curSize), curSize);
    }
    m_lengthCombo->setCurrentIndex(m_lengthCombo->findData(curSize));
    connect(m_lengthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumExportDialog::onLengthChanged);

    formGrid->addWidget(lengthLabel, 2, 0);
    formGrid->addWidget(m_lengthCombo, 2, 1);

    // 4. Mode
    auto *modeLabel = new QLabel(tr("Write Mode:"), this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("New / Overwrite File"), static_cast<int>(SpectrumExportMode::NewOrOverwrite));
    m_modeCombo->addItem(tr("Append to File as New Spectrum"), static_cast<int>(SpectrumExportMode::Append));
    m_modeCombo->addItem(tr("Write at Spectrum Index (#)"), static_cast<int>(SpectrumExportMode::WriteAtIndex));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrumExportDialog::onModeChanged);

    formGrid->addWidget(modeLabel, 3, 0);
    formGrid->addWidget(m_modeCombo, 3, 1);

    // 5. Index
    m_indexLabel = new QLabel(tr("Spectrum Index (#):"), this);
    m_indexSpin = new QSpinBox(this);
    m_indexSpin->setRange(0, 9999);
    m_indexSpin->setValue(0);
    m_indexSpin->setEnabled(false);
    m_indexLabel->setEnabled(false);

    formGrid->addWidget(m_indexLabel, 4, 0);
    formGrid->addWidget(m_indexSpin, 4, 1);

    mainLayout->addLayout(formGrid);

    // Preview / Summary Box
    auto *infoBox = new QFrame(this);
    infoBox->setStyleSheet("background-color: #252526; border: 1px solid #3c3c3c; border-radius: 4px; padding: 10px;");
    auto *infoLayout = new QVBoxLayout(infoBox);
    infoLayout->setSpacing(4);
    infoLayout->setContentsMargins(8, 8, 8, 8);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setStyleSheet("color: #9cdcfe; font-weight: bold;");
    infoLayout->addWidget(m_summaryLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet("color: #a0a0a0; font-size: 11px;");
    infoLayout->addWidget(m_previewLabel);

    mainLayout->addWidget(infoBox);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_exportBtn = new QPushButton(tr("Export Spectrum"), this);
    m_exportBtn->setObjectName("btnExport");
    connect(m_exportBtn, &QPushButton::clicked, this, &SpectrumExportDialog::onAcceptClicked);
    btnLayout->addWidget(m_exportBtn);

    mainLayout->addLayout(btnLayout);

    updateSizeEstimate();
}

void SpectrumExportDialog::onBrowseClicked()
{
    QString initialDir;
    if (!m_pathEdit->text().isEmpty()) {
        initialDir = QFileInfo(m_pathEdit->text()).absolutePath();
    }

    QString filter;
    SpectrumFormat fmt = getSelectedFormat();
    if (fmt == SpectrumFormat::Ascii) {
        filter = tr("ASCII Spectra (*.txt *.asc *.dat);;All Files (*)");
    } else {
        filter = tr("Binary Spectra (*.spe *.dat);;All Files (*)");
    }

    QString sel = QFileDialog::getSaveFileName(this, tr("Export Spectrum File"), initialDir, filter);
    if (!sel.isEmpty()) {
        m_pathEdit->setText(sel);
    }
}

void SpectrumExportDialog::onFormatChanged(int)
{
    updateSizeEstimate();
}

void SpectrumExportDialog::onLengthChanged(int)
{
    updateSizeEstimate();
}

void SpectrumExportDialog::onModeChanged(int)
{
    SpectrumExportMode m = getExportMode();
    bool isIndexMode = (m == SpectrumExportMode::WriteAtIndex);
    m_indexLabel->setEnabled(isIndexMode);
    m_indexSpin->setEnabled(isIndexMode);
    updateSizeEstimate();
}

void SpectrumExportDialog::onFilePathChanged(const QString &)
{
    updateSizeEstimate();
}

void SpectrumExportDialog::updateSizeEstimate()
{
    int channels = getSelectedLength();
    SpectrumFormat fmt = getSelectedFormat();

    size_t bytesPerCh = 4;
    QString fmtName;
    switch (fmt) {
        case SpectrumFormat::Double64:
            bytesPerCh = 8;
            fmtName = "Double (64-bit float)";
            break;
        case SpectrumFormat::ShortInt16:
            bytesPerCh = 2;
            fmtName = "Short (16-bit int)";
            break;
        case SpectrumFormat::Float32:
            bytesPerCh = 4;
            fmtName = "Float (32-bit float)";
            break;
        case SpectrumFormat::Ascii:
            // 9 chars per number + 1 newline every 10 values
            bytesPerCh = 0;
            fmtName = "ASCII (Fortran 10I9)";
            break;
        default:
            bytesPerCh = 4;
            fmtName = "Long (32-bit int)";
            break;
    }

    size_t specBytes = 0;
    if (fmt == SpectrumFormat::Ascii) {
        size_t fullRows = channels / 10;
        size_t remainder = channels % 10;
        specBytes = fullRows * (10 * 9 + 1) + (remainder > 0 ? (remainder * 9 + 1) : 0);
    } else {
        specBytes = static_cast<size_t>(channels) * bytesPerCh;
    }

    QString sizeStr;
    if (specBytes < 1024) {
        sizeStr = QString("%1 B").arg(specBytes);
    } else if (specBytes < 1024 * 1024) {
        sizeStr = QString("%1 KB (%2 bytes)").arg(specBytes / 1024.0, 0, 'f', 1).arg(specBytes);
    } else {
        sizeStr = QString("%1 MB (%2 bytes)").arg(specBytes / (1024.0 * 1024.0), 0, 'f', 2).arg(specBytes);
    }

    m_summaryLabel->setText(QString("Spectrum: %1 channels | Format: %2 | Spectrum Size: %3")
                            .arg(channels).arg(fmtName).arg(sizeStr));

    QString note;
    int srcSize = static_cast<int>(m_data.size());
    if (channels < srcSize) {
        note = QString("Note: Spectrum will be truncated from %1 down to %2 channels.").arg(srcSize).arg(channels);
    } else if (channels > srcSize) {
        note = QString("Note: Spectrum will be padded with zeros from %1 to %2 channels.").arg(srcSize).arg(channels);
    } else {
        note = QString("Channels match source spectrum exactly (0 to %1).").arg(channels - 1);
    }

    QString path = m_pathEdit->text().trimmed();
    if (!path.isEmpty()) {
        QFileInfo fi(path);
        if (fi.exists()) {
            SpectrumExportMode m = getExportMode();
            if (m == SpectrumExportMode::NewOrOverwrite) {
                note += QString(" [File exists: %1 bytes will be replaced]").arg(fi.size());
            } else if (m == SpectrumExportMode::Append) {
                note += QString(" [File exists: will append to current %1 bytes]").arg(fi.size());
            } else {
                note += QString(" [File exists: will overwrite spectrum at index #%1]").arg(m_indexSpin->value());
            }
        }
    }
    m_previewLabel->setText(note);
}

void SpectrumExportDialog::onAcceptClicked()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Missing File Path"), tr("Please specify a destination file path."));
        m_pathEdit->setFocus();
        return;
    }

    QFileInfo fi(path);
    if (fi.exists() && getExportMode() == SpectrumExportMode::NewOrOverwrite) {
        auto reply = QMessageBox::question(
            this, tr("Overwrite File?"),
            tr("The file \"%1\" already exists.\nDo you want to overwrite it?").arg(fi.fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QString errorMsg;
    bool ok = WriteSpectrumData(path.toStdString(),
                                getSelectedFormat(),
                                m_data,
                                getSelectedLength(),
                                getExportMode(),
                                getTargetIndex(),
                                &errorMsg);

    if (!ok) {
        QMessageBox::critical(this, tr("Export Failed"), errorMsg);
        return;
    }

    accept();
}

QString SpectrumExportDialog::getSelectedFilePath() const
{
    return m_pathEdit->text().trimmed();
}

SpectrumFormat SpectrumExportDialog::getSelectedFormat() const
{
    return static_cast<SpectrumFormat>(m_formatCombo->currentData().toInt());
}

int SpectrumExportDialog::getSelectedLength() const
{
    bool ok = false;
    int val = m_lengthCombo->currentText().split(' ').first().toInt(&ok);
    if (ok && val > 0) return val;
    int d = m_lengthCombo->currentData().toInt();
    if (d > 0) return d;
    return static_cast<int>(m_data.size());
}

SpectrumExportMode SpectrumExportDialog::getExportMode() const
{
    return static_cast<SpectrumExportMode>(m_modeCombo->currentData().toInt());
}

int SpectrumExportDialog::getTargetIndex() const
{
    return m_indexSpin->value();
}
