#ifndef DESIGN_H
#define DESIGN_H

#include <QPlainTextEdit>
#include <QString>

// Forward declarations to avoid circular dependencies
class QMainCanvas;
class TCanvas;

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
void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget);

#endif // DESIGN_H
