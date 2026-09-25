#ifndef DESIGN_H
#define DESIGN_H

#include <QFont>
#include <QColor>
#include <QPlainTextEdit>
#include <QString>

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

    // =========================================================================
    // Category 3: Graph and Overlays (ROOT Canvas, Axes, Labels, ZoomHUD)
    // =========================================================================
    QFont getGraphFont();
    int getRootGraphFont(int precision = 2);
    int getRootFontFamilyIndex();
    void setGraphFont(const QFont &qtFont, int rootFontFamilyIndex = 4);

    QColor getGraphBackgroundColor();
    void setGraphBackgroundColor(const QColor &color);
    QColor getSpectrumColor();
    void setSpectrumColor(const QColor &color);
    QColor getPeakMarkerColor();
    void setPeakMarkerColor(const QColor &color);

    /**
     * @brief Applies graph typography to CERN ROOT gStyle.
     */
    void applyGraphTypography();

    /**
     * @brief Applies the full theme (fonts, sizes, colors) to the active main window and canvas.
     */
    void applyUITheme(QMainCanvas *mainCanvas);

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
