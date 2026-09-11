#ifndef SPECTRUMEXPORTDIALOG_H
#define SPECTRUMEXPORTDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>
#include <string>
#include <cstdint>
#include "SpectrumImportDialog.h" // For SpectrumFormat enum

class QLineEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;

enum class SpectrumExportMode {
    NewOrOverwrite = 0,
    Append,
    WriteAtIndex
};

/**
 * @brief Writes spectrum data to disk with format selection, channel count control,
 *        and multi-spectrum file support (overwrite, append, or indexed replacement).
 */
bool WriteSpectrumData(const std::string &filename,
                       SpectrumFormat format,
                       const std::vector<double> &data,
                       int requestedChannels,
                       SpectrumExportMode mode = SpectrumExportMode::NewOrOverwrite,
                       int targetIndex = 0,
                       QString *errorMsg = nullptr);

class SpectrumExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpectrumExportDialog(const std::vector<double> &data,
                                  const QString &suggestedPath = QString(),
                                  SpectrumFormat initialFormat = SpectrumFormat::LongInt32,
                                  int initialLength = 0,
                                  QWidget *parent = nullptr);
    ~SpectrumExportDialog() override = default;

    QString getSelectedFilePath() const;
    SpectrumFormat getSelectedFormat() const;
    int getSelectedLength() const;
    SpectrumExportMode getExportMode() const;
    int getTargetIndex() const;

private slots:
    void onBrowseClicked();
    void onFormatChanged(int index);
    void onLengthChanged(int index);
    void onModeChanged(int index);
    void onFilePathChanged(const QString &text);
    void onAcceptClicked();

private:
    void updateSizeEstimate();

    std::vector<double> m_data;

    QLineEdit   *m_pathEdit{nullptr};
    QPushButton *m_browseBtn{nullptr};
    QComboBox   *m_formatCombo{nullptr};
    QComboBox   *m_lengthCombo{nullptr};
    QComboBox   *m_modeCombo{nullptr};
    QSpinBox    *m_indexSpin{nullptr};
    QLabel      *m_indexLabel{nullptr};
    QLabel      *m_summaryLabel{nullptr};
    QLabel      *m_previewLabel{nullptr};
    QPushButton *m_exportBtn{nullptr};
    QPushButton *m_cancelBtn{nullptr};
};

#endif // SPECTRUMEXPORTDIALOG_H
