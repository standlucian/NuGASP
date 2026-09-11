#include "Design.h"
#include "canvas.h"

#include <QFontDatabase>
#include <QVBoxLayout>
#include <QSplitter>
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
// cross-platform monospace typography, a circular buffer limit, and a clean
// white background theme.
//==============================================================================
CommandPrompt::CommandPrompt(QWidget *parent) : QPlainTextEdit(parent) {
  setPlaceholderText("NuTrackN Output Console...");
  setReadOnly(true);

  // Typography: Cross-platform monospace font ensuring ASCII tables and statistical
  // uncertainties (e.g. centroid, FWHM, Poisson errors) align column-by-column
  // across different operating systems (Linux, Windows, macOS).
  QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  monoFont.setPointSize(14);
  setFont(monoFont);

  // Buffer Management: Limit maximum line history to 10,000 blocks to prevent
  // memory exhaustion during heavy analysis or multi-peak scanning loops.
  setMaximumBlockCount(10000);

  // Styling: White background theme with high-contrast text and clean border
  setStyleSheet("QPlainTextEdit {"
                "  background-color: #ffffff;"
                "  color: #000000;"
                "  border: 1px solid #cccccc;"
                "  selection-background-color: #0078d7;"
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
// Embeds the singleton CommandPrompt widget into the main application window's
// vertical QSplitter below the ROOT spectrum canvas, enabling dynamic mouse
// resizing. Defaults the prompt console height to 20% of the total window height.
//==============================================================================
void addCommandPrompt(QMainCanvas *m) {
  if (!m) {
    return;
  }

  // Bind the canvas as the owner and fetch or create the prompt instance
  CommandPrompt::setMainCanvas(m);
  CommandPrompt *prompt = CommandPrompt::getInstance();

  QSplitter *splitter = m->getMainSplitter();
  if (splitter) {
    prompt->setParent(splitter);
    splitter->addWidget(prompt);
    prompt->setMinimumHeight(60);

    // Calculate default height: 20% of total window height for prompt,
    // remaining 80% for top spectrum and controls area.
    const int totalHeight = (m->height() > 0) ? m->height() : 720;
    const int promptHeight = std::max(60, static_cast<int>(totalHeight * 0.20));
    const int topHeight = totalHeight - promptHeight;

    splitter->setStretchFactor(0, 4); // 80% stretch for spectrum / controls
    splitter->setStretchFactor(1, 1); // 20% stretch for output prompt
    splitter->setSizes(QList<int>() << topHeight << promptHeight);
  } else {
    // Fallback: insert directly into vertical layout if splitter is unavailable
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m->layout());
    if (layout) {
      layout->addWidget(prompt);
      layout->update();
    }
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
  dialog.setStyleSheet(
      "QDialog { background-color: #2b2b2b; color: #ffffff; font-size: 16px; }"
      "QLabel { color: #ffffff; font-size: 16px; }"
      "QComboBox { background-color: #ffffff; color: #000000; font-size: 16px; padding: 4px 8px; border-radius: 3px; }"
      "QPushButton { background-color: #4a4a4a; color: #ffffff; border: 1px solid #707070; "
      "border-radius: 4px; padding: 6px 20px; font-weight: bold; font-size: 16px; min-height: 28px; }"
      "QPushButton:hover { background-color: #5a5a5a; }"
  );
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
