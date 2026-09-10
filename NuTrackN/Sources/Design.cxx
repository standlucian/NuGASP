#include "Design.h"
#include "canvas.h"

#include <QFontDatabase>
#include <QVBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <iostream>

#include <TCanvas.h>
#include <TColor.h>

// Static member definitions for the CommandPrompt singleton
CommandPrompt *CommandPrompt::instance = nullptr;
QMainCanvas *CommandPrompt::mainCanvas = nullptr;

//==============================================================================
// CommandPrompt Constructor
//==============================================================================
// Constructs the embedded terminal log widget. Configures read-only display,
// cross-platform monospace typography, a circular buffer limit, and dark styling.
//==============================================================================
CommandPrompt::CommandPrompt(QWidget *parent) : QPlainTextEdit(parent) {
  setPlaceholderText("NuTrackN Output Console...");
  setReadOnly(true);

  // Typography: Cross-platform monospace font ensuring ASCII tables and statistical
  // uncertainties (e.g. centroid, FWHM, Poisson errors) align column-by-column
  // across different operating systems (Linux, Windows, macOS).
  QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  monoFont.setPointSize(9);
  setFont(monoFont);

  // Buffer Management: Limit maximum line history to 10,000 blocks to prevent
  // memory exhaustion during heavy analysis or multi-peak scanning loops.
  setMaximumBlockCount(10000);

  // Styling: Dark terminal theme with high-contrast text and subtle border
  setStyleSheet("QPlainTextEdit {"
                "  background-color: #1e1e1e;"
                "  color: #d4d4d4;"
                "  border: 1px solid #3c3c3c;"
                "  selection-background-color: #264f78;"
                "  selection-color: #ffffff;"
                "}");
}

//==============================================================================
// CommandPrompt Destructor
//==============================================================================
// Safely resets the singleton instance pointer when the widget is destroyed.
//==============================================================================
CommandPrompt::~CommandPrompt() {
  if (instance == this) {
    instance = nullptr;
  }
}

// Sets the parent QMainCanvas pointer so the console can be constructed with the right parent
void CommandPrompt::setMainCanvas(QMainCanvas *m) { mainCanvas = m; }

//==============================================================================
// CommandPrompt::getInstance
//==============================================================================
// Returns the global singleton instance of the output console, constructing it
// lazily on the first request if not already present.
//==============================================================================
CommandPrompt *CommandPrompt::getInstance() {
  if (!instance) {
    instance = new CommandPrompt(mainCanvas);
  }
  return instance;
}

//==============================================================================
// addCommandPrompt
//==============================================================================
// Embeds the singleton CommandPrompt widget directly into the main application
// window's layout below the ROOT canvas, with proportional sizing.
//==============================================================================
void addCommandPrompt(QMainCanvas *m) {
  if (!m) {
    return;
  }

  // Bind the canvas as the owner and fetch or create the prompt instance
  CommandPrompt::setMainCanvas(m);
  CommandPrompt *prompt = CommandPrompt::getInstance();

  // Calculate proportional height (35% of total canvas height, minimum 120px)
  // to balance spectrum visibility with text log readability.
  const int canvasHeight = m->height();
  const int initialHeight =
      (canvasHeight > 0) ? static_cast<int>(canvasHeight * 0.35) : 220;
  prompt->setMinimumHeight(120);
  prompt->resize(prompt->width(), initialHeight);

  // Insert the console widget at the bottom of the main vertical layout
  QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m->layout());
  if (layout) {
    layout->addWidget(prompt);
    layout->update();
  }
}

//==============================================================================
// changeBackgroundColor
//==============================================================================
// Updates the background fill color of a CERN ROOT TCanvas instance to match
// the application's dark palette, forcing a canvas redraw.
//==============================================================================
void changeBackgroundColor(TCanvas *canvas) {
  if (canvas) {
    canvas->SetFillColor(TColor::GetColor("#1e1e1e"));
    canvas->Modified();
    canvas->Update();
  }
}

//==============================================================================
// openColorSelectionDialog
//==============================================================================
// Displays an interactive modal dialog allowing the user to select the UI/spectrum
// color scheme (e.g. Default, Dark, Vampire).
//==============================================================================
void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  // Construct dialog modal
  QDialog dialog(parent);
  dialog.setWindowTitle("Color & Theme Settings");
  dialog.setStyleSheet("background-color: #2b2b2b; color: #ffffff;");
  QFormLayout form(&dialog);

  // Theme dropdown options
  QComboBox *themeBox = new QComboBox(&dialog);
  themeBox->addItem("Default");
  themeBox->addItem("Dark");
  themeBox->addItem("Vampire");
  form.addRow("Color Theme:", themeBox);

  // Dialog action buttons (Ok / Cancel)
  QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  form.addRow(&buttonBox);

  QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  // Process user theme selection
  if (dialog.exec() == QDialog::Accepted) {
    const QString selected = themeBox->currentText();

    // Apply the selected theme color to the currently active histogram
    if (canvasWidget && selected == "Vampire") {
      if (canvasWidget->selectedHisto) {
        // Apply deep crimson fill color for Vampire theme
        canvasWidget->selectedHisto->SetFillColor(TColor::GetColor("#870202"));
      }
    }

    // Log the selection to the console
    CommandPrompt::getInstance()->appendPlainText("Theme selected: " + selected + "\n");
  }
}
