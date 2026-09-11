#ifndef SPECTRUMIMPORTDIALOG_H
#define SPECTRUMIMPORTDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>
#include <string>
#include <cstdint>

class QComboBox;
class QLabel;
class QSpinBox;
class QPushButton;

enum class SpectrumFormat {
    Auto = 0,
    LongInt32,   // 32-bit integer (uint32_t, 4 bytes/channel)
    ShortInt16,  // 16-bit integer (uint16_t, 2 bytes/channel)
    Ascii,       // Plain text whitespace/column delimited counts
    Float32,     // 32-bit IEEE 754 float (4 bytes/channel)
    Double64     // 64-bit IEEE 754 double (8 bytes/channel)
};

struct SpectrumDetectionResult {
    std::string    filename;
    uint64_t       fileSize{0};
    SpectrumFormat guessedFormat{SpectrumFormat::LongInt32};
    int            guessedLength{10240};
    int            totalChannelsFound{0};
    int            numSpectra{1};
    int            currentSpectrumIndex{0};
    double         maxCount{0.0};
    double         totalIntegral{0.0};
    std::vector<double> sampleCounts; // First 10..20 channels
    QString        confidenceReason;
};

// Analyzes the file size, byte patterns, and content structure to auto-detect format and length
SpectrumDetectionResult AutoDetectSpectrumFile(const std::string &filename);

// Reads spectrum data according to the selected format, channel count, and spectrum index
bool ReadSpectrumData(const std::string &filename, SpectrumFormat format, int requestedChannels,
                      int spectrumIndex, std::vector<double> &outData, QString *errorMsg = nullptr);

// Convenience overload for backwards compatibility defaulting to spectrumIndex 0
inline bool ReadSpectrumData(const std::string &filename, SpectrumFormat format, int requestedChannels,
                             std::vector<double> &outData, QString *errorMsg = nullptr)
{
    return ReadSpectrumData(filename, format, requestedChannels, 0, outData, errorMsg);
}

class QSpectrumPlotPreview : public QWidget
{
    Q_OBJECT
public:
    explicit QSpectrumPlotPreview(QWidget *parent = nullptr);
    void setSpectra(const std::vector<double> &primary, int primaryIdx,
                    const std::vector<double> &secondary = {}, int secondaryIdx = -1);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawSinglePlot(QPainter &painter, const QRect &rect,
                        const std::vector<double> &data, int specIdx,
                        const QColor &lineColor, const QColor &fillColor);

    std::vector<double> m_primaryData;
    int                 m_primaryIndex{0};
    std::vector<double> m_secondaryData;
    int                 m_secondaryIndex{-1};
};

class SpectrumImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpectrumImportDialog(const QString &filePath, QWidget *parent = nullptr);

    SpectrumFormat getSelectedFormat() const;
    int getSelectedLength() const;
    int getSelectedSpectrumIndex() const;
    int getTotalSpectraCount() const { return m_totalSpectra; }
    const std::vector<double>& getLoadedData() const { return m_loadedData; }

private slots:
    void onFormatOrLengthChanged();
    void onSpectrumIndexChanged(int index);
    void onPrevSpectrumClicked();
    void onNextSpectrumClicked();
    void updatePreview();

private:
    void recalculateSpectraCount();

    QString                 m_filePath;
    SpectrumDetectionResult m_detected;
    std::vector<double>     m_loadedData;
    std::vector<double>     m_secondaryData;
    int                     m_totalSpectra{1};

    QLabel      *m_lblFile;
    QLabel      *m_lblDetectedBanner;
    QComboBox   *m_comboFormat;
    QComboBox   *m_comboLength;
    QSpinBox    *m_spinCustomLength;

    // Multi-spectrum controls
    QWidget     *m_specIndexContainer;
    QLabel      *m_lblSpectrumIndex;
    QSpinBox    *m_spinSpectrumIndex;
    QPushButton *m_btnPrevSpec;
    QPushButton *m_btnNextSpec;
    QLabel      *m_lblSpectrumCount;

    QLabel               *m_lblPreviewSummary;
    QLabel               *m_lblPreviewChannels;
    QSpectrumPlotPreview *m_plotPreview;
    QPushButton          *m_btnLoad;
    QPushButton          *m_btnCancel;
};

#endif // SPECTRUMIMPORTDIALOG_H
