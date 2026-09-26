#ifndef DESIGN_H
#define DESIGN_H

#include <QFont>
#include <QColor>
#include <QPlainTextEdit>
#include <QString>
#include <vector>

// Forward declarations to avoid circular dependencies
class QMainCanvas;
class TCanvas;

namespace Design {

    /**
     * @brief Initializes typography and color settings from saved configuration or defaults.
     */
    void initializeTypography();

    /**
     * @brief Persists typography and color preferences to QSettings.
     */
    void saveSettings();

    /**
     * @brief Loads typography and color preferences from QSettings.
     */
    void loadSettings();

    /**
     * @brief Resets all fonts, sizes, and colors to factory defaults.
     */
    void resetToDefaults();

    // =========================================================================
    // Category 1: Buttons and Prompt Console
    // =========================================================================
    QFont getButtonFont();
    QFont getPromptFont();
    QFont getButtonPromptFont();
    void setButtonPromptFont(const QFont &font);

    QColor getButtonBackgroundColor();
    void setButtonBackgroundColor(const QColor &color);
    QColor getButtonTextColor();
    void setButtonTextColor(const QColor &color);

    QColor getPromptBackgroundColor();
    void setPromptBackgroundColor(const QColor &color);
    QColor getPromptTextColor();
    void setPromptTextColor(const QColor &color);

    QString getButtonStyleSheet();
    QString getPromptStyleSheet();
    QString getStatusLabelStyleSheet();

    // =========================================================================
    // Category 2: Dialogs
    // =========================================================================
    QFont getDialogFont();
    void setDialogFont(const QFont &font);

    QColor getDialogBackgroundColor();
    void setDialogBackgroundColor(const QColor &color);
    QColor getDialogTextColor();
    void setDialogTextColor(const QColor &color);
    QColor getDialogAccentColor();
    void setDialogAccentColor(const QColor &color);

    QString getDialogStyleSheet();
    QString buildDialogStyleSheet(const QFont &font, const QColor &bgCol, const QColor &fgCol, const QColor &accentCol);

    // =========================================================================
    // Category 3: Graph and Overlays (ROOT Canvas, Axes, Labels, ZoomHUD)
    // =========================================================================
    QFont getGraphFont();
    int getRootGraphFont(int precision = 2);
    int getRootFontFamilyIndex();
    void setGraphFont(const QFont &qtFont, int rootFontFamilyIndex = 4);

    QColor getUIBackgroundColor();
    void setUIBackgroundColor(const QColor &color);

    QColor getGraphBackgroundColor();
    void setGraphBackgroundColor(const QColor &color);

    std::vector<QColor> getSpectrumColors();
    QColor getSpectrumColor();
    QColor getSpectrumColor(int index);
    void setSpectrumColor(const QColor &color);
    void setSpectrumColor(int index, const QColor &color);
    void setSpectrumColors(const std::vector<QColor> &colors);

    QColor getPeakMarkerColor();
    void setPeakMarkerColor(const QColor &color);
    QColor getZoomMarkerColor();
    void setZoomMarkerColor(const QColor &color);
    QColor getBackgroundMarkerColor();
    void setBackgroundMarkerColor(const QColor &color);
    QColor getIntegralMarkerColor();
    void setIntegralMarkerColor(const QColor &color);
    QColor getRangeMarkerColor();
    void setRangeMarkerColor(const QColor &color);
    QColor getGaussMarkerColor();
    void setGaussMarkerColor(const QColor &color);
    QColor getGateMarkerColor();
    void setGateMarkerColor(const QColor &color);

    /**
     * @brief Applies graph typography to CERN ROOT gStyle.
     */
    void applyGraphTypography();

    /**
     * @brief Applies the full theme (fonts, sizes, colors) to the active main window and canvas.
     */
    void applyUITheme(QMainCanvas *mainCanvas);

    struct ThemeSettings {
        QFont buttonPromptFont;
        QFont dialogFont;
        QFont graphFont;
        int rootFontFamilyIndex{13};

        QColor buttonBgColor;
        QColor buttonTextColor;
        QColor uiBgColor;
        QColor promptBgColor;
        QColor promptTextColor;

        QColor dialogBgColor;
        QColor dialogTextColor;
        QColor dialogAccentColor;

        QColor graphBgColor;
        std::vector<QColor> spectrumColors;

        QColor peakMarkerColor;
        QColor zoomMarkerColor;
        QColor bgMarkerColor;
        QColor integralMarkerColor;
        QColor rangeMarkerColor;
        QColor gaussMarkerColor;
        QColor gateMarkerColor;
    };

    /**
     * @brief Retrieves the active theme settings snapshot.
     */
    ThemeSettings getCurrentTheme();

    /**
     * @brief Applies and stores the specified theme settings.
     */
    void setCurrentTheme(const ThemeSettings &theme);

    /**
     * @brief Exports the specified theme settings to a JSON/nugasp-theme file.
     */
    bool exportThemeToFile(const QString &filePath, const ThemeSettings &theme, QString *errorMessage = nullptr);

    /**
     * @brief Exports the active theme settings to a JSON/nugasp-theme file.
     */
    bool exportCurrentTheme(const QString &filePath, QString *errorMessage = nullptr);

    /**
     * @brief Imports theme settings from a JSON/nugasp-theme file into a ThemeSettings struct.
     */
    bool importThemeFromFile(const QString &filePath, ThemeSettings &theme, QString *errorMessage = nullptr);

    /**
     * @brief Imports theme settings from a file and applies them directly to the application.
     */
    bool importAndApplyTheme(const QString &filePath, QMainCanvas *canvasWidget = nullptr, QString *errorMessage = nullptr);

    // Aliases for compatibility
    inline void saveTypographySettings() { saveSettings(); }
    /**
     * @brief Creates the interactive appearance & typography settings dialog (non-modal).
     */
    QDialog* createAppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget);

} // namespace Design

/**
 * @brief CommandPrompt manages the embedded terminal logger and output console in NuTrackN.
 *
 * Implemented as a singleton derived from QPlainTextEdit for easy logging from any analysis module.
 */
class CommandPrompt : public QPlainTextEdit
{
    Q_OBJECT

public:
    static CommandPrompt* getInstance();
    static void setMainCanvas(QMainCanvas* m);

    virtual ~CommandPrompt() override;

private:
    explicit CommandPrompt(QWidget *parent = nullptr);

    static CommandPrompt* instance;
    static QMainCanvas* mainCanvas;

    // Prevent copying
    CommandPrompt(const CommandPrompt&) = delete;
    CommandPrompt& operator=(const CommandPrompt&) = delete;
};

/**
 * @brief Embeds the command prompt console into the main canvas window layout.
 *
 * @param mainCanvas Pointer to the main application window.
 */
void addCommandPrompt(QMainCanvas* mainCanvas);

/**
 * @brief Changes the background color of the given ROOT canvas.
 *
 * @param canvas Pointer to the ROOT TCanvas.
 */
void changeBackgroundColor(TCanvas* canvas);

/**
 * @brief Displays the interactive color/theme settings dialog.
 *
 * @param parent Parent widget for the dialog.
 * @param canvasWidget Pointer to the main canvas widget.
 */
QDialog* createAppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget);
void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget);

#endif // DESIGN_H
