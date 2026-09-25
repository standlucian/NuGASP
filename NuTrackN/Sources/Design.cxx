#include "Design.h"
#include "canvas.h"

#include <QFontDatabase>
#include <QSettings>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <iostream>

#include <TCanvas.h>
#include <TColor.h>
#include <TStyle.h>

//==============================================================================
// Design Typography Implementation
//==============================================================================
namespace Design {

static QFont s_buttonPromptFont;
static QFont s_dialogFont;
static QFont s_graphFont;
static int s_rootFontFamilyIndex = 4; // 4 = Helvetica scalable
static bool s_typographyInitialized = false;

void initializeTypography() {
  if (s_typographyInitialized) return;

  // Category 1: Buttons and Prompt (Monospace / tabular clean font)
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setPointSize(11);
  s_buttonPromptFont = mono;

  // Category 2: Dialogs (Clean modern sans-serif UI font)
  QFont dialogFont("DejaVu Sans", 11, QFont::Normal);
  dialogFont.setStyleHint(QFont::SansSerif);
  s_dialogFont = dialogFont;

  // Category 3: Graph itself (ROOT scalable font 42/43 + matching Qt font)
  QFont graphFont("DejaVu Sans", 10, QFont::Normal);
  graphFont.setStyleHint(QFont::SansSerif);
  s_graphFont = graphFont;
  s_rootFontFamilyIndex = 4; // Helvetica

  // Load any previously persisted user customizations from QSettings
  loadTypographySettings();

  s_typographyInitialized = true;
}

void saveTypographySettings() {
  QSettings settings("NuGASP", "NuTrackN");
  settings.setValue("Typography/ButtonPromptFont", s_buttonPromptFont);
  settings.setValue("Typography/DialogFont", s_dialogFont);
  settings.setValue("Typography/GraphFont", s_graphFont);
  settings.setValue("Typography/RootFontFamilyIndex", s_rootFontFamilyIndex);
}

void loadTypographySettings() {
  QSettings settings("NuGASP", "NuTrackN");
  if (settings.contains("Typography/ButtonPromptFont")) {
    s_buttonPromptFont = settings.value("Typography/ButtonPromptFont").value<QFont>();
  }
  if (settings.contains("Typography/DialogFont")) {
    s_dialogFont = settings.value("Typography/DialogFont").value<QFont>();
  }
  if (settings.contains("Typography/GraphFont")) {
    s_graphFont = settings.value("Typography/GraphFont").value<QFont>();
  }
  if (settings.contains("Typography/RootFontFamilyIndex")) {
    s_rootFontFamilyIndex = settings.value("Typography/RootFontFamilyIndex", 4).toInt();
  }
}

QFont getButtonFont() {
  if (!s_typographyInitialized) initializeTypography();
  QFont btnFont = s_buttonPromptFont;
  btnFont.setBold(true);
  return btnFont;
}

QFont getPromptFont() {
  if (!s_typographyInitialized) initializeTypography();
  QFont pFont = s_buttonPromptFont;
  int pt = pFont.pointSize() > 0 ? pFont.pointSize() : 11;
  pFont.setPointSize(pt + 2); // 2pt larger for terminal readout readability
  pFont.setBold(false);
  return pFont;
}

QFont getButtonPromptFont() {
  if (!s_typographyInitialized) initializeTypography();
  return s_buttonPromptFont;
}

void setButtonPromptFont(const QFont &font) {
  s_buttonPromptFont = font;
  saveTypographySettings();
  if (CommandPrompt::getInstance()) {
    CommandPrompt::getInstance()->setFont(getPromptFont());
  }
}

QFont getDialogFont() {
  if (!s_typographyInitialized) initializeTypography();
  return s_dialogFont;
}

void setDialogFont(const QFont &font) {
  s_dialogFont = font;
  saveTypographySettings();
}

QString getDialogStyleSheet() {
  if (!s_typographyInitialized) initializeTypography();
  const QString family = s_dialogFont.family();
  const int pt = s_dialogFont.pointSize() > 0 ? s_dialogFont.pointSize() : 11;
  return QString(
      "QDialog { background-color: #1e1e1e; color: #ffffff; font-family: \"%1\"; font-size: %2pt; }\n"
      "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 10px; font-weight: bold; color: #00ffff; font-family: \"%1\"; font-size: %2pt; }\n"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }\n"
      "QLabel { color: #cccccc; font-family: \"%1\"; font-size: %2pt; }\n"
      "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 4px 6px; font-family: \"%1\"; font-size: %2pt; }\n"
      "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #007acc; }\n"
      "QCheckBox, QRadioButton { color: #ffffff; font-family: \"%1\"; font-size: %2pt; spacing: 6px; }\n"
      "QTableWidget { background-color: #252526; color: #ffffff; gridline-color: #3e3e42; border: 1px solid #3e3e42; border-radius: 4px; font-family: \"%1\"; font-size: %2pt; }\n"
      "QHeaderView::section { background-color: #333337; color: #00ffff; font-weight: bold; border: 1px solid #3e3e42; padding: 4px; font-family: \"%1\"; font-size: %2pt; }\n"
      "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 5px 14px; font-weight: bold; font-family: \"%1\"; font-size: %2pt; }\n"
      "QPushButton:hover { background-color: #4e4e52; }\n"
      "QPushButton:pressed { background-color: #007acc; }\n"
      "QPushButton:disabled { color: #888888; background-color: #2d2d30; border: 1px solid #3e3e42; }\n"
      "QTextBrowser, QTextEdit, QPlainTextEdit { background-color: #252526; color: #d4d4d4; border: 1px solid #3e3e42; border-radius: 4px; font-family: \"%1\"; font-size: %2pt; }\n"
      "QTabWidget::pane { border: 1px solid #3e3e42; background-color: #1e1e1e; }\n"
      "QTabBar::tab { background-color: #2d2d30; color: #cccccc; padding: 6px 14px; border: 1px solid #3e3e42; font-family: \"%1\"; font-size: %2pt; }\n"
      "QTabBar::tab:selected { background-color: #1e1e1e; color: #00ffff; border-bottom: 2px solid #007acc; }\n"
  ).arg(family).arg(pt);
}

QFont getGraphFont() {
  if (!s_typographyInitialized) initializeTypography();
  return s_graphFont;
}

int getRootGraphFont(int precision) {
  if (!s_typographyInitialized) initializeTypography();
  return s_rootFontFamilyIndex * 10 + precision;
}

int getRootFontFamilyIndex() {
  if (!s_typographyInitialized) initializeTypography();
  return s_rootFontFamilyIndex;
}

void setGraphFont(const QFont &qtFont, int rootFontFamilyIndex) {
  s_graphFont = qtFont;
  s_rootFontFamilyIndex = rootFontFamilyIndex;
  saveTypographySettings();
  applyGraphTypography();
}

void applyGraphTypography() {
  if (gStyle) {
    int rf = getRootGraphFont(2);
    gStyle->SetTextFont(rf);
    gStyle->SetLabelFont(rf, "x");
    gStyle->SetLabelFont(rf, "y");
    gStyle->SetLabelFont(rf, "z");
    gStyle->SetTitleFont(rf, "x");
    gStyle->SetTitleFont(rf, "y");
    gStyle->SetTitleFont(rf, "z");
    gStyle->SetStatFont(rf);
  }
}

} // namespace Design

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

  // Typography: Category 1 (Prompt Font)
  setFont(Design::getPromptFont());

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
  dialog.setFont(Design::getDialogFont());
  dialog.setStyleSheet(Design::getDialogStyleSheet());
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
