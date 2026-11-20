#include "SpectrumExportDialog.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QIntValidator>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include "NuFileDialog.h"
#include "RemoteConnectDialog.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace {

void writeAscii10I9(std::ostream &file, const double *vals, size_t count)
{
    int col = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t val = static_cast<int64_t>(std::llround(vals[i]));
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
}

void writeBinarySpectrum(std::ostream &file,
                         SpectrumFormat format,
                         const std::vector<double> &outData)
{
    const size_t n = outData.size();
    if (format == SpectrumFormat::Float32) {
        std::vector<float> buf(n);
        for (size_t i = 0; i < n; ++i) buf[i] = static_cast<float>(outData[i]);
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(float));
    } else if (format == SpectrumFormat::Double64) {
        file.write(reinterpret_cast<const char*>(outData.data()), n * sizeof(double));
    } else if (format == SpectrumFormat::ShortInt16) {
        std::vector<uint16_t> buf(n);
        for (size_t i = 0; i < n; ++i) {
            double v = std::max(0.0, std::min(65535.0, std::round(outData[i])));
            buf[i] = static_cast<uint16_t>(v);
        }
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint16_t));
    } else { // LongInt32 (default GASPware L)
        std::vector<uint32_t> buf(n);
        for (size_t i = 0; i < n; ++i) {
            double v = std::max(0.0, std::min(4294967295.0, std::round(outData[i])));
            buf[i] = static_cast<uint32_t>(v);
        }
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size() * sizeof(uint32_t));
    }
}

} // namespace

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

    QDir().mkpath(QFileInfo(QString::fromStdString(filename)).absolutePath());

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
        if (mode == SpectrumExportMode::WriteAtIndex) {
            const int safeIndex = std::max(0, targetIndex);
            std::vector<double> allVals;
            if (QFileInfo::exists(QString::fromStdString(filename))) {
                std::ifstream inFile(filename);
                std::string line;
                while (std::getline(inFile, line)) {
                    if (line.empty()) continue;
                    std::size_t firstPos = line.find_first_not_of(" \t\r\n");
                    if (firstPos == std::string::npos) continue;
                    char first = line[firstPos];
                    if (first == '#' || first == '!' || first == ';' || first == '$') continue;
                    std::istringstream iss(line);
                    double val = 0.0;
                    while (iss >> val) {
                        allVals.push_back(val);
                    }
                }
            }

            const size_t requiredTotal = static_cast<size_t>(safeIndex + 1) * static_cast<size_t>(requestedChannels);
            if (allVals.size() < requiredTotal) {
                allVals.resize(requiredTotal, 0.0);
            }
            const size_t startOffset = static_cast<size_t>(safeIndex) * static_cast<size_t>(requestedChannels);
            for (int i = 0; i < requestedChannels; ++i) {
                allVals[startOffset + static_cast<size_t>(i)] = outData[static_cast<size_t>(i)];
            }

            std::ofstream outFile(filename, std::ios::out | std::ios::trunc);
            if (!outFile.is_open()) {
                if (errorMsg) *errorMsg = QObject::tr("Could not open file for writing:\n%1").arg(QString::fromStdString(filename));
                return false;
            }
            for (size_t offset = 0; offset < allVals.size(); offset += static_cast<size_t>(requestedChannels)) {
                const size_t chunk = std::min(static_cast<size_t>(requestedChannels), allVals.size() - offset);
                writeAscii10I9(outFile, allVals.data() + offset, chunk);
            }
            if (!outFile.good()) {
                if (errorMsg) *errorMsg = QObject::tr("Write error occurred while updating ASCII spectrum index #%1.").arg(safeIndex);
                return false;
            }
            return true;
        }

        bool needLeadingNewline = false;
        if (mode == SpectrumExportMode::Append) {
            std::ifstream checkFile(filename, std::ios::binary | std::ios::ate);
            if (checkFile.is_open() && checkFile.tellg() > 0) {
                checkFile.seekg(-1, std::ios::end);
                char lastCh = '\n';
                if (checkFile.get(lastCh) && lastCh != '\n' && lastCh != '\r') {
                    needLeadingNewline = true;
                }
            }
        }

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

        if (needLeadingNewline) {
            file << "\n";
        }
        writeAscii10I9(file, outData.data(), outData.size());

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
        writeBinarySpectrum(file, format, outData);

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

    writeBinarySpectrum(file, format, outData);

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
                                           QWidget *parent,
                                           bool initialIsRemote)
    : QDialog(parent), m_data(data), m_isRemoteSave(initialIsRemote)
{
    for (double v : m_data) {
        if (v < 0.0) {
            if (!m_hasNegativeCounts || v < m_minCount) {
                m_minCount = v;
            }
            m_hasNegativeCounts = true;
        }
    }

    setWindowTitle(tr("Export Spectrum (W)"));
    setMinimumWidth(560);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Title / Description
    const QFont df = Design::getDialogFont();
    auto *titleLabel = new QLabel(tr("<b>Export Active Spectrum</b>"), this);
    titleLabel->setFont(df);
    titleLabel->setStyleSheet(QString("font-size: %1pt; color: %2; font-family: \"%3\";")
        .arg(df.pointSize() > 0 ? df.pointSize() + 2 : 13)
        .arg(Design::getDialogAccentColor().name())
        .arg(df.family()));
    mainLayout->addWidget(titleLabel);

    // Destination Location Bar (Remote vs Local Switcher)
    auto *locLayout = new QHBoxLayout();
    locLayout->setSpacing(6);

    auto *lblLoc = new QLabel(tr("Destination:"), this);
    lblLoc->setFont(df);
    locLayout->addWidget(lblLoc);

    const QString tabToggleStyle = QString(
        "QPushButton:checked { background-color: %1; color: #ffffff; border: 1px solid %1; }"
    ).arg(Design::getDialogAccentColor().name());

    m_btnRemoteTab = new QPushButton(this);
    m_btnRemoteTab->setFont(df);
    m_btnRemoteTab->setCheckable(true);
    m_btnRemoteTab->setStyleSheet(tabToggleStyle);
    locLayout->addWidget(m_btnRemoteTab);

    m_btnLocalTab = new QPushButton(tr("💻 Local Filesystem"), this);
    m_btnLocalTab->setFont(df);
    m_btnLocalTab->setCheckable(true);
    m_btnLocalTab->setStyleSheet(tabToggleStyle);
    locLayout->addWidget(m_btnLocalTab);

    locLayout->addStretch(1);

    m_btnConnectSSH = new QPushButton(tr("SSH Parameters..."), this);
    m_btnConnectSSH->setFont(df);
    connect(m_btnConnectSSH, &QPushButton::clicked, this, &SpectrumExportDialog::onConnectSSHClicked);
    locLayout->addWidget(m_btnConnectSSH);

    connect(m_btnRemoteTab, &QPushButton::clicked, this, &SpectrumExportDialog::onLocationToggled);
    connect(m_btnLocalTab, &QPushButton::clicked, this, &SpectrumExportDialog::onLocationToggled);

    mainLayout->addLayout(locLayout);

    // Grid Form
    auto *formGrid = new QGridLayout();
    formGrid->setSpacing(10);

    // 1. Output File Path
    auto *pathLabel = new QLabel(tr("Output File:"), this);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Select destination file..."));
    const QString initialPath = suggestedPath.trimmed().isEmpty()
        ? QDir::current().filePath(QStringLiteral("spectrum_out"))
        : suggestedPath.trimmed();
    m_pathEdit->setText(initialPath);
    if (m_isRemoteSave) {
        m_remoteSavePath = initialPath;
    } else {
        m_localSavePath = initialPath;
    }
    connect(m_pathEdit, &QLineEdit::textEdited, this, &SpectrumExportDialog::onFilePathChanged);

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
    m_lengthCombo->setValidator(new QIntValidator(1, 1048576, m_lengthCombo));

    int curSize = static_cast<int>(m_data.size());
    if (initialLength > 0) curSize = initialLength;
    if (curSize <= 0) curSize = 10240;

    // Standard presets
    const std::vector<int> presets = {1024, 2048, 4096, 8192, 10240, 16384, 32768, 65536};
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
    connect(m_lengthCombo, &QComboBox::currentTextChanged,
            this, [this](const QString &) { updateSizeEstimate(); });

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

    // Compact status / warning line (non-redundant info only)
    const int pt = df.pointSize() > 0 ? df.pointSize() : 11;
    m_previewLabel = new QLabel(this);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setFont(df);
    m_previewLabel->setStyleSheet(QString("color: %1; font-family: \"%2\"; font-size: %3pt; padding: 4px 2px;")
        .arg(Design::getDialogTextColor().name(), df.family()).arg(pt));
    mainLayout->addWidget(m_previewLabel);

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

    updateLocationUI();
    updateSizeEstimate();
}

void SpectrumExportDialog::updateLocationUI()
{
    auto sess = RemoteSession::globalSession();
    const bool connected = (sess && sess->isConnected());
    if (connected) {
        const RemoteProfile &prof = sess->getProfile();
        m_btnRemoteTab->setText(QString("🌐 Remote: %1@%2").arg(prof.user, prof.host));
    } else {
        m_btnRemoteTab->setText(tr("🌐 Remote Server (Offline)"));
    }

    if (m_isRemoteSave && !connected) {
        m_isRemoteSave = false;
        m_remoteSavePath.clear();
    }

    m_btnRemoteTab->setChecked(m_isRemoteSave);
    m_btnLocalTab->setChecked(!m_isRemoteSave);
}

void SpectrumExportDialog::onConnectSSHClicked()
{
    auto sess = RemoteSession::globalSession();
    RemoteConnectDialog dlg(sess, this);
    if (dlg.exec() == QDialog::Accepted && sess && sess->isConnected()) {
        m_isRemoteSave = true;
        const QString currentName = QFileInfo(m_pathEdit->text().trimmed()).fileName();
        const QString fileName = currentName.isEmpty() ? QStringLiteral("spectrum_out") : currentName;
        QString remoteDir = sess->getCurrentRemoteDir();
        if (remoteDir.isEmpty()) remoteDir = sess->getProfile().lastUsedPath;
        if (remoteDir.isEmpty()) remoteDir = QStringLiteral("/");
        m_remoteSavePath = remoteDir.endsWith('/') ? (remoteDir + fileName) : (remoteDir + "/" + fileName);
        m_pathEdit->setText(m_remoteSavePath);
        CommandPrompt::logDebug("Export", QString("Export destination set to Remote: %1").arg(m_remoteSavePath));
    }
    updateLocationUI();
    updateSizeEstimate();
}

void SpectrumExportDialog::onLocationToggled()
{
    auto sess = RemoteSession::globalSession();
    if (sender() == m_btnRemoteTab) {
        if (!sess || !sess->isConnected()) {
            CommandPrompt::logWarning("Export", "Remote destination selected while SSH is offline; opening SSH connection dialog.");
            QMessageBox::information(this, tr("SSH Not Connected"),
                tr("There is currently no active SSH connection.\nPlease configure host parameters and connect."));
            onConnectSSHClicked();
            return;
        }
        m_isRemoteSave = true;
        if (m_remoteSavePath.isEmpty()) {
            const QString currentName = QFileInfo(m_pathEdit->text().trimmed()).fileName();
            const QString fileName = currentName.isEmpty() ? QStringLiteral("spectrum_out") : currentName;
            QString remoteDir = sess->getCurrentRemoteDir();
            if (remoteDir.isEmpty()) remoteDir = sess->getProfile().lastUsedPath;
            if (remoteDir.isEmpty()) remoteDir = QStringLiteral("/");
            m_remoteSavePath = remoteDir.endsWith('/') ? (remoteDir + fileName) : (remoteDir + "/" + fileName);
        }
        m_pathEdit->setText(m_remoteSavePath);
        CommandPrompt::logDebug("Export", QString("Switched export destination to Remote: %1").arg(m_remoteSavePath));
    } else {
        m_isRemoteSave = false;
        const QString currentName = QFileInfo(m_pathEdit->text().trimmed()).fileName();
        const QString fileName = currentName.isEmpty() ? QStringLiteral("spectrum_out") : currentName;
        if (m_localSavePath.isEmpty() || m_localSavePath == m_remoteSavePath) {
            m_localSavePath = QDir::current().filePath(fileName);
        }
        m_remoteSavePath.clear();
        m_pathEdit->setText(m_localSavePath);
        CommandPrompt::logDebug("Export", QString("Switched export destination to Local: %1").arg(m_localSavePath));
    }
    updateLocationUI();
    updateSizeEstimate();
}

void SpectrumExportDialog::onBrowseClicked()
{
    const QString initialTarget = m_pathEdit->text().trimmed();

    QString remotePath;
    bool isRemote = false;
    QString sel = NuFileDialog::getSaveFileName(
        this,
        tr("Export Spectrum File"),
        initialTarget,
        QString(),
        nullptr,
        &remotePath,
        &isRemote,
        m_isRemoteSave ? 1 : 0);
    if (!sel.isEmpty()) {
        m_isRemoteSave = isRemote;
        m_remoteSavePath = isRemote ? remotePath : QString();
        m_localSavePath = sel;
        m_pathEdit->setText(isRemote ? remotePath : sel);
        updateLocationUI();
        updateSizeEstimate();
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

void SpectrumExportDialog::onFilePathChanged(const QString &newText)
{
    const QString trimmed = newText.trimmed();
    if (m_isRemoteSave) {
        m_remoteSavePath = trimmed;
    } else {
        m_remoteSavePath.clear();
        m_localSavePath = trimmed;
    }
    updateSizeEstimate();
}

void SpectrumExportDialog::updateSizeEstimate()
{
    int channels = getSelectedLength();
    SpectrumFormat fmt = getSelectedFormat();

    size_t bytesPerCh = 4;
    switch (fmt) {
        case SpectrumFormat::Double64:   bytesPerCh = 8; break;
        case SpectrumFormat::ShortInt16: bytesPerCh = 2; break;
        case SpectrumFormat::Ascii:      bytesPerCh = 0; break;
        default:                         bytesPerCh = 4; break;
    }

    size_t specBytes = 0;
    if (fmt == SpectrumFormat::Ascii) {
        size_t fullRows = static_cast<size_t>(channels) / 10U;
        size_t remainder = static_cast<size_t>(channels) % 10U;
        specBytes = fullRows * (10U * 9U + 1U) + (remainder > 0U ? (remainder * 9U + 1U) : 0U);
    } else {
        specBytes = static_cast<size_t>(channels) * bytesPerCh;
    }

    QString sizeStr;
    if (specBytes < 1024) {
        sizeStr = QString("%1 B").arg(specBytes);
    } else if (specBytes < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(specBytes / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 MB").arg(specBytes / (1024.0 * 1024.0), 0, 'f', 2);
    }

    QString note = QString("Estimated Size: %1").arg(sizeStr);
    int srcSize = static_cast<int>(m_data.size());
    if (channels < srcSize) {
        note += QString(" (truncated from %1 ch)").arg(srcSize);
    } else if (channels > srcSize) {
        note += QString(" (zero-padded from %1 ch)").arg(srcSize);
    }

    if (m_hasNegativeCounts && (fmt == SpectrumFormat::LongInt32 || fmt == SpectrumFormat::ShortInt16)) {
        note += QString("\nWarning: Negative counts (min %1) will be clamped to 0 in unsigned integer format; use Float32, Double64, or ASCII to preserve them.")
                    .arg(m_minCount, 0, 'g', 4);
    }

    QString path = m_pathEdit->text().trimmed();
    if (!m_isRemoteSave && !path.isEmpty()) {
        QFileInfo fi(path);
        if (fi.exists()) {
            SpectrumExportMode m = getExportMode();
            if (m == SpectrumExportMode::NewOrOverwrite) {
                note += QString("  |  Existing file (%1 B) will be overwritten").arg(fi.size());
            } else if (m == SpectrumExportMode::Append) {
                note += QString("  |  Will append to existing file (%1 B)").arg(fi.size());
            } else {
                note += QString("  |  Will overwrite spectrum #%1").arg(m_indexSpin->value());
            }
            if (m != SpectrumExportMode::NewOrOverwrite && fmt != SpectrumFormat::Ascii &&
                specBytes > 0 && (fi.size() % static_cast<qint64>(specBytes)) != 0) {
                note += QString("\nWarning: Existing file size (%1 bytes) is not a multiple of %2 bytes/spectrum.")
                            .arg(fi.size())
                            .arg(specBytes);
            }
        }
    }
    m_previewLabel->setText(note);
}

void SpectrumExportDialog::onAcceptClicked()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        CommandPrompt::logWarning("Export", "Destination file path cannot be empty.");
        QMessageBox::warning(this, tr("Missing File Path"), tr("Please specify a destination file path."));
        m_pathEdit->setFocus();
        return;
    }

    bool lenOk = false;
    const QString firstToken = m_lengthCombo->currentText().trimmed().split(' ').first();
    int requestedLen = firstToken.toInt(&lenOk);
    if (!lenOk) {
        const int d = m_lengthCombo->currentData().toInt();
        if (d > 0) {
            requestedLen = d;
            lenOk = true;
        }
    }
    if (!lenOk || requestedLen < 1 || requestedLen > 1048576) {
        CommandPrompt::logWarning("Export", QString("Invalid export channel length '%1'.").arg(m_lengthCombo->currentText().trimmed()));
        QMessageBox::warning(
            this, tr("Invalid Channel Length"),
            tr("Please enter a valid channel count between 1 and 1,048,576."));
        m_lengthCombo->setFocus();
        return;
    }

    QString writePath = path;
    bool targetExists = QFileInfo::exists(writePath);

    if (m_isRemoteSave && !m_remoteSavePath.isEmpty()) {
        auto sess = RemoteSession::globalSession();
        if (!sess || !sess->isConnected()) {
            const QString err = QString("Cannot export to remote destination '%1': SSH session is not active.").arg(m_remoteSavePath);
            CommandPrompt::logError("Export", err);
            QMessageBox::warning(this, tr("SSH Not Connected"), err);
            return;
        }
        writePath = sess->getLocalCachePath(m_remoteSavePath);
        const bool remoteExists = sess->fileExists(m_remoteSavePath);
        targetExists = remoteExists;

        // If appending or writing at a specific index on an existing remote file,
        // synchronize the remote file to the local cache first to prevent data loss!
        if (remoteExists && getExportMode() != SpectrumExportMode::NewOrOverwrite) {
            QString syncErr;
            QString syncedPath;
            if (!sess->syncRemoteFileToCache(m_remoteSavePath, syncedPath, nullptr, &syncErr, nullptr, false)) {
                CommandPrompt::logError(
                    "Export",
                    QString("Failed to synchronize remote file '%1' before export: %2").arg(m_remoteSavePath, syncErr));
                QMessageBox::critical(this, tr("Remote Sync Failed"),
                    tr("Could not synchronize existing remote file '%1' before modifying:\n%2")
                        .arg(m_remoteSavePath, syncErr));
                return;
            }
            writePath = syncedPath;
        }
    }

    if (targetExists && getExportMode() == SpectrumExportMode::NewOrOverwrite) {
        const QString displayTarget = (m_isRemoteSave && !m_remoteSavePath.isEmpty())
            ? QFileInfo(m_remoteSavePath).fileName()
            : QFileInfo(writePath).fileName();
        auto reply = QMessageBox::question(
            this, tr("Overwrite File?"),
            tr("The file \"%1\" already exists.\nDo you want to overwrite it?").arg(displayTarget),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            CommandPrompt::logDebug("Export", QString("Overwrite declined for '%1'.").arg(displayTarget));
            return;
        }
    }

    QString errorMsg;
    bool ok = WriteSpectrumData(writePath.toStdString(),
                                getSelectedFormat(),
                                m_data,
                                requestedLen,
                                getExportMode(),
                                getTargetIndex(),
                                &errorMsg);

    if (!ok) {
        CommandPrompt::logError("Export", errorMsg);
        QMessageBox::critical(this, tr("Export Failed"), errorMsg);
        return;
    }

    m_localSavePath = writePath;

    if (m_isRemoteSave && !m_remoteSavePath.isEmpty()) {
        QString uploadErr;
        if (!NuFileDialog::finishRemoteSave(writePath, m_remoteSavePath, &uploadErr, this)) {
            CommandPrompt::logError("Export", QString("Remote upload failed for '%1': %2").arg(m_remoteSavePath, uploadErr));
            return;
        }
    }

    CommandPrompt::logDebug(
        "Export",
        QString("Wrote spectrum file '%1' (format=%2, ch=%3, mode=%4, index=%5, remote=%6).")
            .arg(writePath, spectrumFormatToString(getSelectedFormat()))
            .arg(requestedLen)
            .arg(static_cast<int>(getExportMode()))
            .arg(getTargetIndex())
            .arg(m_isRemoteSave ? m_remoteSavePath : QStringLiteral("local")));

    accept();
}

QString SpectrumExportDialog::getSelectedFilePath() const
{
    return m_localSavePath.isEmpty() ? m_pathEdit->text().trimmed() : m_localSavePath;
}

SpectrumFormat SpectrumExportDialog::getSelectedFormat() const
{
    return static_cast<SpectrumFormat>(m_formatCombo->currentData().toInt());
}

int SpectrumExportDialog::getSelectedLength() const
{
    bool ok = false;
    int val = m_lengthCombo->currentText().trimmed().split(' ').first().toInt(&ok);
    if (ok && val >= 1 && val <= 1048576) return val;
    int d = m_lengthCombo->currentData().toInt();
    if (d >= 1 && d <= 1048576) return d;
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
