#include "Design.h"
#include "canvas.h"

#include <QFontDatabase>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QFontDialog>
#include <QColorDialog>
#include <QApplication>
#include <iostream>

#include <TCanvas.h>
#include <TPad.h>
#include <TColor.h>
#include <TStyle.h>
#include <TList.h>
#include <TIterator.h>

//==============================================================================
// Design Typography & Color Engine Implementation
//==============================================================================
namespace Design {

static QFont s_buttonPromptFont;
static QFont s_dialogFont;
static QFont s_graphFont;
static int s_rootFontFamilyIndex = 4; // 4 = Helvetica scalable
static bool s_typographyInitialized = false;

static QColor s_buttonBgColor("#e0e0e0");
static QColor s_buttonTextColor("#000000");
static QColor s_promptBgColor("#ffffff");
static QColor s_promptTextColor("#000000");

static QColor s_dialogBgColor("#1e1e1e");
static QColor s_dialogTextColor("#dcdcdc");
static QColor s_dialogAccentColor("#007acc");

static QColor s_graphBgColor("#1e1e1e");
static QColor s_spectrumColor("#3399ff");
static QColor s_peakMarkerColor("#00ffff");

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

  s_buttonBgColor = QColor("#e0e0e0");
  s_buttonTextColor = QColor("#000000");
  s_promptBgColor = QColor("#ffffff");
  s_promptTextColor = QColor("#000000");

  s_dialogBgColor = QColor("#1e1e1e");
  s_dialogTextColor = QColor("#dcdcdc");
  s_dialogAccentColor = QColor("#007acc");

  s_graphBgColor = QColor("#1e1e1e");
  s_spectrumColor = QColor("#3399ff");
  s_peakMarkerColor = QColor("#00ffff");

  loadSettings();
  s_typographyInitialized = true;
}

void resetToDefaults() {
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setPointSize(11);
  s_buttonPromptFont = mono;

  QFont dialogFont("DejaVu Sans", 11, QFont::Normal);
  dialogFont.setStyleHint(QFont::SansSerif);
  s_dialogFont = dialogFont;

  QFont graphFont("DejaVu Sans", 10, QFont::Normal);
  graphFont.setStyleHint(QFont::SansSerif);
  s_graphFont = graphFont;
  s_rootFontFamilyIndex = 4;

  s_buttonBgColor = QColor("#e0e0e0");
  s_buttonTextColor = QColor("#000000");
  s_promptBgColor = QColor("#ffffff");
  s_promptTextColor = QColor("#000000");

  s_dialogBgColor = QColor("#1e1e1e");
  s_dialogTextColor = QColor("#dcdcdc");
  s_dialogAccentColor = QColor("#007acc");

  s_graphBgColor = QColor("#1e1e1e");
  s_spectrumColor = QColor("#3399ff");
  s_peakMarkerColor = QColor("#00ffff");

  saveSettings();
}

void saveSettings() {
  QSettings settings("NuGASP", "NuTrackN");
  settings.setValue("Design/ButtonPromptFont", s_buttonPromptFont);
  settings.setValue("Design/DialogFont", s_dialogFont);
  settings.setValue("Design/GraphFont", s_graphFont);
  settings.setValue("Design/RootFontFamilyIndex", s_rootFontFamilyIndex);

  settings.setValue("Design/ButtonBgColor", s_buttonBgColor.name());
  settings.setValue("Design/ButtonTextColor", s_buttonTextColor.name());
  settings.setValue("Design/PromptBgColor", s_promptBgColor.name());
  settings.setValue("Design/PromptTextColor", s_promptTextColor.name());

  settings.setValue("Design/DialogBgColor", s_dialogBgColor.name());
  settings.setValue("Design/DialogTextColor", s_dialogTextColor.name());
  settings.setValue("Design/DialogAccentColor", s_dialogAccentColor.name());

  settings.setValue("Design/GraphBgColor", s_graphBgColor.name());
  settings.setValue("Design/SpectrumColor", s_spectrumColor.name());
  settings.setValue("Design/PeakMarkerColor", s_peakMarkerColor.name());
}

void loadSettings() {
  QSettings settings("NuGASP", "NuTrackN");
  if (settings.contains("Design/ButtonPromptFont")) {
    s_buttonPromptFont = settings.value("Design/ButtonPromptFont").value<QFont>();
  } else if (settings.contains("Typography/ButtonPromptFont")) {
    s_buttonPromptFont = settings.value("Typography/ButtonPromptFont").value<QFont>();
  }

  if (settings.contains("Design/DialogFont")) {
    s_dialogFont = settings.value("Design/DialogFont").value<QFont>();
  } else if (settings.contains("Typography/DialogFont")) {
    s_dialogFont = settings.value("Typography/DialogFont").value<QFont>();
  }

  if (settings.contains("Design/GraphFont")) {
    s_graphFont = settings.value("Design/GraphFont").value<QFont>();
  } else if (settings.contains("Typography/GraphFont")) {
    s_graphFont = settings.value("Typography/GraphFont").value<QFont>();
  }

  if (settings.contains("Design/RootFontFamilyIndex")) {
    s_rootFontFamilyIndex = settings.value("Design/RootFontFamilyIndex", 4).toInt();
  }

  if (settings.contains("Design/ButtonBgColor")) s_buttonBgColor = QColor(settings.value("Design/ButtonBgColor").toString());
  if (settings.contains("Design/ButtonTextColor")) s_buttonTextColor = QColor(settings.value("Design/ButtonTextColor").toString());
  if (settings.contains("Design/PromptBgColor")) s_promptBgColor = QColor(settings.value("Design/PromptBgColor").toString());
  if (settings.contains("Design/PromptTextColor")) s_promptTextColor = QColor(settings.value("Design/PromptTextColor").toString());

  if (settings.contains("Design/DialogBgColor")) s_dialogBgColor = QColor(settings.value("Design/DialogBgColor").toString());
  if (settings.contains("Design/DialogTextColor")) s_dialogTextColor = QColor(settings.value("Design/DialogTextColor").toString());
  if (settings.contains("Design/DialogAccentColor")) s_dialogAccentColor = QColor(settings.value("Design/DialogAccentColor").toString());

  if (settings.contains("Design/GraphBgColor")) s_graphBgColor = QColor(settings.value("Design/GraphBgColor").toString());
  if (settings.contains("Design/SpectrumColor")) s_spectrumColor = QColor(settings.value("Design/SpectrumColor").toString());
  if (settings.contains("Design/PeakMarkerColor")) s_peakMarkerColor = QColor(settings.value("Design/PeakMarkerColor").toString());
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
  saveSettings();
  if (CommandPrompt::getInstance()) {
    CommandPrompt::getInstance()->setFont(getPromptFont());
  }
}

QColor getButtonBackgroundColor() { if (!s_typographyInitialized) initializeTypography(); return s_buttonBgColor; }
void setButtonBackgroundColor(const QColor &color) { s_buttonBgColor = color; saveSettings(); }

QColor getButtonTextColor() { if (!s_typographyInitialized) initializeTypography(); return s_buttonTextColor; }
void setButtonTextColor(const QColor &color) { s_buttonTextColor = color; saveSettings(); }

QColor getPromptBackgroundColor() { if (!s_typographyInitialized) initializeTypography(); return s_promptBgColor; }
void setPromptBackgroundColor(const QColor &color) { s_promptBgColor = color; saveSettings(); }

QColor getPromptTextColor() { if (!s_typographyInitialized) initializeTypography(); return s_promptTextColor; }
void setPromptTextColor(const QColor &color) { s_promptTextColor = color; saveSettings(); }

QString getButtonStyleSheet() {
  if (!s_typographyInitialized) initializeTypography();
  const QString bg = s_buttonBgColor.name();
  const QString fg = s_buttonTextColor.name();
  const QString pressedBg = s_buttonBgColor.darker(115).name();
  const QString topBorder = s_buttonBgColor.lighter(130).name();
  const QString bottomBorder = s_buttonBgColor.darker(140).name();

  return QString(
      "QPushButton {"
      "  background-color: %1;"
      "  color: %2;"
      "  border-top: 2px solid %3;"
      "  border-left: 2px solid %3;"
      "  border-right: 2px solid %4;"
      "  border-bottom: 2px solid %4;"
      "  padding: 3px 6px;"
      "}"
      "QPushButton:pressed {"
      "  background-color: %5;"
      "  border-top: 2px solid %4;"
      "  border-left: 2px solid %4;"
      "  border-right: 2px solid %3;"
      "  border-bottom: 2px solid %3;"
      "}"
      "QPushButton:disabled {"
      "  color: #888888;"
      "  background-color: #e8e8e8;"
      "  border: 1px solid #a0a0a0;"
      "}"
  ).arg(bg, fg, topBorder, bottomBorder, pressedBg);
}

QString getPromptStyleSheet() {
  if (!s_typographyInitialized) initializeTypography();
  return QString(
      "QPlainTextEdit {"
      "  background-color: %1;"
      "  color: %2;"
      "  border: 1px solid #cccccc;"
      "  selection-background-color: %3;"
      "  selection-color: #ffffff;"
      "}"
  ).arg(s_promptBgColor.name(), s_promptTextColor.name(), s_dialogAccentColor.name());
}

QFont getDialogFont() {
  if (!s_typographyInitialized) initializeTypography();
  return s_dialogFont;
}

void setDialogFont(const QFont &font) {
  s_dialogFont = font;
  saveSettings();
}

QColor getDialogBackgroundColor() { if (!s_typographyInitialized) initializeTypography(); return s_dialogBgColor; }
void setDialogBackgroundColor(const QColor &color) { s_dialogBgColor = color; saveSettings(); }

QColor getDialogTextColor() { if (!s_typographyInitialized) initializeTypography(); return s_dialogTextColor; }
void setDialogTextColor(const QColor &color) { s_dialogTextColor = color; saveSettings(); }

QColor getDialogAccentColor() { if (!s_typographyInitialized) initializeTypography(); return s_dialogAccentColor; }
void setDialogAccentColor(const QColor &color) { s_dialogAccentColor = color; saveSettings(); }

QString getDialogStyleSheet() {
  if (!s_typographyInitialized) initializeTypography();
  const QString family = s_dialogFont.family();
  const int pt = s_dialogFont.pointSize() > 0 ? s_dialogFont.pointSize() : 11;
  const QString bg = s_dialogBgColor.name();
  const QString fg = s_dialogTextColor.name();
  const QString accent = s_dialogAccentColor.name();
  const QString panelBg = s_dialogBgColor.lighter(118).name();
  const QString border = s_dialogBgColor.lighter(140).name();
  const QString inputBg = s_dialogBgColor.lighter(110).name();

  return QString(
      "QDialog { background-color: %1; color: %2; font-family: \"%3\"; font-size: %4pt; }\n"
      "QGroupBox { border: 1px solid %5; border-radius: 4px; margin-top: 10px; font-weight: bold; color: %6; font-family: \"%3\"; font-size: %4pt; }\n"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }\n"
      "QLabel { color: %2; font-family: \"%3\"; font-size: %4pt; }\n"
      "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background-color: %7; color: %2; border: 1px solid %5; border-radius: 3px; padding: 4px 6px; font-family: \"%3\"; font-size: %4pt; }\n"
      "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid %6; }\n"
      "QCheckBox, QRadioButton { color: %2; font-family: \"%3\"; font-size: %4pt; spacing: 6px; }\n"
      "QTableWidget { background-color: %8; color: %2; gridline-color: %5; border: 1px solid %5; border-radius: 4px; font-family: \"%3\"; font-size: %4pt; }\n"
      "QHeaderView::section { background-color: %8; color: %6; font-weight: bold; border: 1px solid %5; padding: 4px; font-family: \"%3\"; font-size: %4pt; }\n"
      "QPushButton { background-color: %7; color: %2; border: 1px solid %5; border-radius: 4px; padding: 5px 14px; font-weight: bold; font-family: \"%3\"; font-size: %4pt; }\n"
      "QPushButton:hover { background-color: %5; }\n"
      "QPushButton:pressed { background-color: %6; color: #ffffff; }\n"
      "QPushButton:disabled { color: #888888; background-color: %8; border: 1px solid %5; }\n"
      "QTextBrowser, QTextEdit, QPlainTextEdit { background-color: %8; color: %2; border: 1px solid %5; border-radius: 4px; font-family: \"%3\"; font-size: %4pt; }\n"
      "QTabWidget::pane { border: 1px solid %5; background-color: %1; }\n"
      "QTabBar::tab { background-color: %8; color: %2; padding: 6px 14px; border: 1px solid %5; font-family: \"%3\"; font-size: %4pt; }\n"
      "QTabBar::tab:selected { background-color: %1; color: %6; border-bottom: 2px solid %6; }\n"
  ).arg(bg, fg, family).arg(pt).arg(border, accent, inputBg, panelBg);
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
  saveSettings();
  applyGraphTypography();
}

QColor getGraphBackgroundColor() { if (!s_typographyInitialized) initializeTypography(); return s_graphBgColor; }
void setGraphBackgroundColor(const QColor &color) { s_graphBgColor = color; saveSettings(); }

QColor getSpectrumColor() { if (!s_typographyInitialized) initializeTypography(); return s_spectrumColor; }
void setSpectrumColor(const QColor &color) { s_spectrumColor = color; saveSettings(); }

QColor getPeakMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_peakMarkerColor; }
void setPeakMarkerColor(const QColor &color) { s_peakMarkerColor = color; saveSettings(); }

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

void applyUITheme(QMainCanvas *mainCanvas) {
  // 1. Buttons & Prompt
  if (mainCanvas) {
    const QString btnStyle = getButtonStyleSheet();
    const QFont btnFont = getButtonFont();
    const QList<QPushButton*> buttons = mainCanvas->findChildren<QPushButton*>();
    for (QPushButton *btn : buttons) {
      if (btn && btn->icon().isNull() && btn->text() != "MAC") {
        btn->setFont(btnFont);
        btn->setStyleSheet(btnStyle);
      }
    }
  }

  if (CommandPrompt::getInstance()) {
    CommandPrompt::getInstance()->setFont(getPromptFont());
    CommandPrompt::getInstance()->setStyleSheet(getPromptStyleSheet());
  }

  // 2. Global application font for dialogs
  if (qApp) {
    qApp->setFont(getDialogFont());
  }

  // 3. Graph canvas
  applyGraphTypography();

  if (mainCanvas && mainCanvas->getRootCanvas()) {
    TCanvas *tc = mainCanvas->getRootCanvas()->getCanvas();
    if (tc) {
      Color_t rootBg = TColor::GetColor(s_graphBgColor.name().toUtf8().constData());
      tc->SetFillColor(rootBg);
      TIter next(tc->GetListOfPrimitives());
      TObject *obj = nullptr;
      while ((obj = next())) {
        if (obj && obj->InheritsFrom(TPad::Class())) {
          static_cast<TPad*>(obj)->SetFillColor(rootBg);
        }
      }
      tc->Modified();
      tc->Update();
    }

    Color_t rootSpecColor = TColor::GetColor(s_spectrumColor.name().toUtf8().constData());
    if (!mainCanvas->colors_hist.empty()) {
      mainCanvas->colors_hist[0] = rootSpecColor;
    }
    if (mainCanvas->selectedHisto) {
      mainCanvas->selectedHisto->SetLineColor(rootSpecColor);
    }
    mainCanvas->RefreshScreen();
  }
}

} // namespace Design

// Static member definitions for the CommandPrompt singleton
CommandPrompt *CommandPrompt::instance = nullptr;
QMainCanvas *CommandPrompt::mainCanvas = nullptr;

//==============================================================================
// CommandPrompt Constructor
//==============================================================================
CommandPrompt::CommandPrompt(QWidget *parent) : QPlainTextEdit(parent) {
  setPlaceholderText("NuTrackN Output Console...");
  setReadOnly(true);

  // Typography & Styling: Category 1
  setFont(Design::getPromptFont());
  setStyleSheet(Design::getPromptStyleSheet());
  setMaximumBlockCount(10000);
}

//==============================================================================
// CommandPrompt Destructor
//==============================================================================
CommandPrompt::~CommandPrompt() {
  if (instance == this) {
    instance = nullptr;
  }
}

void CommandPrompt::setMainCanvas(QMainCanvas *m) { mainCanvas = m; }

CommandPrompt *CommandPrompt::getInstance() {
  if (!instance) {
    instance = new CommandPrompt(mainCanvas);
  }
  return instance;
}

//==============================================================================
// addCommandPrompt
//==============================================================================
void addCommandPrompt(QMainCanvas *m) {
  if (!m) return;

  CommandPrompt::setMainCanvas(m);
  CommandPrompt *prompt = CommandPrompt::getInstance();

  QSplitter *splitter = m->getMainSplitter();
  if (splitter) {
    prompt->setParent(splitter);
    splitter->addWidget(prompt);
    prompt->setMinimumHeight(60);

    const int totalHeight = (m->height() > 0) ? m->height() : 720;
    const int promptHeight = std::max(60, static_cast<int>(totalHeight * 0.20));
    const int topHeight = totalHeight - promptHeight;

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << topHeight << promptHeight);
  } else {
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
void changeBackgroundColor(TCanvas *canvas) {
  if (canvas) {
    canvas->SetFillColor(TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData()));
    canvas->Modified();
    canvas->Update();
  }
}

//==============================================================================
// openColorSelectionDialog (Settings Dialog for Fonts, Sizes, and Colors)
//==============================================================================
void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  // Snapshot initial settings to allow clean Cancel / revert
  QFont origBtnFont = Design::getButtonPromptFont();
  QFont origDialogFont = Design::getDialogFont();
  QFont origGraphFont = Design::getGraphFont();
  int origRootFontIdx = Design::getRootFontFamilyIndex();

  QColor origBtnBg = Design::getButtonBackgroundColor();
  QColor origBtnFg = Design::getButtonTextColor();
  QColor origPromptBg = Design::getPromptBackgroundColor();
  QColor origPromptFg = Design::getPromptTextColor();

  QColor origDlgBg = Design::getDialogBackgroundColor();
  QColor origDlgFg = Design::getDialogTextColor();
  QColor origDlgAccent = Design::getDialogAccentColor();

  QColor origGraphBg = Design::getGraphBackgroundColor();
  QColor origSpec = Design::getSpectrumColor();
  QColor origPeak = Design::getPeakMarkerColor();

  // Working copies for the interactive modal
  QFont curBtnFont = origBtnFont;
  QFont curDialogFont = origDialogFont;
  QFont curGraphFont = origGraphFont;
  int curRootFontIdx = origRootFontIdx;

  QColor curBtnBg = origBtnBg;
  QColor curBtnFg = origBtnFg;
  QColor curPromptBg = origPromptBg;
  QColor curPromptFg = origPromptFg;

  QColor curDlgBg = origDlgBg;
  QColor curDlgFg = origDlgFg;
  QColor curDlgAccent = origDlgAccent;

  QColor curGraphBg = origGraphBg;
  QColor curSpec = origSpec;
  QColor curPeak = origPeak;

  // Build the settings dialog
  QDialog dialog(parent);
  dialog.setWindowTitle("Appearance & Typography Settings (3 Fonts & Colors)");
  dialog.resize(680, 640);
  dialog.setFont(curDialogFont);
  dialog.setStyleSheet(Design::getDialogStyleSheet());

  QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
  dialogLayout->setSpacing(10);
  dialogLayout->setContentsMargins(14, 14, 14, 14);

  // Top header with quick theme presets dropdown
  QHBoxLayout *presetLayout = new QHBoxLayout();
  QLabel *lblPreset = new QLabel("<b>Quick Theme Preset:</b>", &dialog);
  QComboBox *presetCombo = new QComboBox(&dialog);
  presetCombo->addItem("(Custom / Keep Current)");
  presetCombo->addItem("Default Dark (Balanced)");
  presetCombo->addItem("Classic Light (Paper White)");
  presetCombo->addItem("Vampire (Crimson / Deep Black)");
  presetCombo->addItem("Cyberpunk (Neon Cyan & Pink)");
  presetLayout->addWidget(lblPreset);
  presetLayout->addWidget(presetCombo, 1);
  dialogLayout->addLayout(presetLayout);

  // 3-Category Tabs
  QTabWidget *tabs = new QTabWidget(&dialog);

  // Helper lambda for color pickers with real-time swatch preview
  auto addColorRow = [&](QGridLayout *grid, int row, const QString &label, QColor &colorVar, std::function<void()> onChange) {
    grid->addWidget(new QLabel(label, &dialog), row, 0);

    QPushButton *swatch = new QPushButton(&dialog);
    swatch->setFixedSize(54, 26);
    auto updateSwatch = [swatch](const QColor &c) {
      swatch->setStyleSheet(QString("background-color: %1; border: 2px solid #888888; border-radius: 4px;").arg(c.name()));
    };
    updateSwatch(colorVar);

    QPushButton *pickBtn = new QPushButton("Pick...", &dialog);
    pickBtn->setFixedWidth(75);

    auto pickAction = [&, swatch, updateSwatch, onChange]() {
      QColor c = QColorDialog::getColor(colorVar, &dialog, QString("Select %1").arg(label));
      if (c.isValid()) {
        colorVar = c;
        updateSwatch(c);
        if (onChange) onChange();
      }
    };
    QObject::connect(swatch, &QPushButton::clicked, pickAction);
    QObject::connect(pickBtn, &QPushButton::clicked, pickAction);

    grid->addWidget(swatch, row, 1);
    grid->addWidget(pickBtn, row, 2);
    return swatch;
  };

  // Forward declare preview update routines
  std::function<void()> updateBtnPreview;
  std::function<void()> updateDlgPreview;
  std::function<void()> updateGraphPreview;

  // =========================================================================
  // TAB 1: Buttons and Prompt
  // =========================================================================
  QWidget *tabBtn = new QWidget();
  QVBoxLayout *tabBtnLayout = new QVBoxLayout(tabBtn);

  QGroupBox *grpBtnFont = new QGroupBox("Category 1 Typography (Buttons & Console)", tabBtn);
  QGridLayout *gridBtnFont = new QGridLayout(grpBtnFont);
  QLabel *lblBtnFontDesc = new QLabel(tabBtn);
  QPushButton *btnChooseBtnFont = new QPushButton("Choose Font...", tabBtn);
  QSpinBox *spinBtnSize = new QSpinBox(tabBtn);
  spinBtnSize->setRange(7, 32);
  spinBtnSize->setValue(curBtnFont.pointSize() > 0 ? curBtnFont.pointSize() : 11);
  spinBtnSize->setSuffix(" pt");

  gridBtnFont->addWidget(new QLabel("Font Family:"), 0, 0);
  gridBtnFont->addWidget(lblBtnFontDesc, 0, 1);
  gridBtnFont->addWidget(btnChooseBtnFont, 0, 2);
  gridBtnFont->addWidget(new QLabel("Point Size:"), 1, 0);
  gridBtnFont->addWidget(spinBtnSize, 1, 1);
  tabBtnLayout->addWidget(grpBtnFont);

  QGroupBox *grpBtnColors = new QGroupBox("Colors", tabBtn);
  QGridLayout *gridBtnColors = new QGridLayout(grpBtnColors);
  QPushButton *swatchBtnBg = addColorRow(gridBtnColors, 0, "Button Background:", curBtnBg, [&]() { if (updateBtnPreview) updateBtnPreview(); });
  QPushButton *swatchBtnFg = addColorRow(gridBtnColors, 1, "Button Text Color:", curBtnFg, [&]() { if (updateBtnPreview) updateBtnPreview(); });
  QPushButton *swatchPromptBg = addColorRow(gridBtnColors, 2, "Console Prompt Background:", curPromptBg, [&]() { if (updateBtnPreview) updateBtnPreview(); });
  QPushButton *swatchPromptFg = addColorRow(gridBtnColors, 3, "Console Prompt Text:", curPromptFg, [&]() { if (updateBtnPreview) updateBtnPreview(); });
  tabBtnLayout->addWidget(grpBtnColors);

  QGroupBox *grpBtnPreview = new QGroupBox("Live Preview", tabBtn);
  QVBoxLayout *vboxBtnPreview = new QVBoxLayout(grpBtnPreview);
  QPushButton *sampleBtn = new QPushButton("EnCal", grpBtnPreview);
  sampleBtn->setFixedHeight(36);
  QLineEdit *samplePrompt = new QLineEdit("NuTrackN Output Console: Peak 1 at 1332.5 keV (FWHM 2.1)", grpBtnPreview);
  samplePrompt->setReadOnly(true);
  samplePrompt->setFixedHeight(36);
  vboxBtnPreview->addWidget(sampleBtn);
  vboxBtnPreview->addWidget(samplePrompt);
  tabBtnLayout->addWidget(grpBtnPreview);
  tabBtnLayout->addStretch();

  tabs->addTab(tabBtn, "🔘 1. Buttons & Prompt");

  // =========================================================================
  // TAB 2: Dialogs
  // =========================================================================
  QWidget *tabDlg = new QWidget();
  QVBoxLayout *tabDlgLayout = new QVBoxLayout(tabDlg);

  QGroupBox *grpDlgFont = new QGroupBox("Category 2 Typography (All Parameter & Analysis Dialogs)", tabDlg);
  QGridLayout *gridDlgFont = new QGridLayout(grpDlgFont);
  QLabel *lblDlgFontDesc = new QLabel(tabDlg);
  QPushButton *btnChooseDlgFont = new QPushButton("Choose Font...", tabDlg);
  QSpinBox *spinDlgSize = new QSpinBox(tabDlg);
  spinDlgSize->setRange(7, 32);
  spinDlgSize->setValue(curDialogFont.pointSize() > 0 ? curDialogFont.pointSize() : 11);
  spinDlgSize->setSuffix(" pt");

  gridDlgFont->addWidget(new QLabel("Font Family:"), 0, 0);
  gridDlgFont->addWidget(lblDlgFontDesc, 0, 1);
  gridDlgFont->addWidget(btnChooseDlgFont, 0, 2);
  gridDlgFont->addWidget(new QLabel("Point Size:"), 1, 0);
  gridDlgFont->addWidget(spinDlgSize, 1, 1);
  tabDlgLayout->addWidget(grpDlgFont);

  QGroupBox *grpDlgColors = new QGroupBox("Colors", tabDlg);
  QGridLayout *gridDlgColors = new QGridLayout(grpDlgColors);
  QPushButton *swatchDlgBg = addColorRow(gridDlgColors, 0, "Dialog Background:", curDlgBg, [&]() { if (updateDlgPreview) updateDlgPreview(); });
  QPushButton *swatchDlgFg = addColorRow(gridDlgColors, 1, "Dialog Text & Labels:", curDlgFg, [&]() { if (updateDlgPreview) updateDlgPreview(); });
  QPushButton *swatchDlgAccent = addColorRow(gridDlgColors, 2, "Highlight / Accent:", curDlgAccent, [&]() { if (updateDlgPreview) updateDlgPreview(); });
  tabDlgLayout->addWidget(grpDlgColors);

  QGroupBox *grpDlgPreview = new QGroupBox("Live Preview", tabDlg);
  QVBoxLayout *vboxDlgPreview = new QVBoxLayout(grpDlgPreview);
  QFrame *mockDialogBox = new QFrame(grpDlgPreview);
  QVBoxLayout *mockLayout = new QVBoxLayout(mockDialogBox);
  QLabel *mockLabel = new QLabel("Integration Range: [ 1120 .. 1450 ]", mockDialogBox);
  QPushButton *mockButton = new QPushButton("Apply Parameters", mockDialogBox);
  mockLayout->addWidget(mockLabel);
  mockLayout->addWidget(mockButton);
  vboxDlgPreview->addWidget(mockDialogBox);
  tabDlgLayout->addWidget(grpDlgPreview);
  tabDlgLayout->addStretch();

  tabs->addTab(tabDlg, "💬 2. Dialogs");

  // =========================================================================
  // TAB 3: Graph & Spectrum
  // =========================================================================
  QWidget *tabGraph = new QWidget();
  QVBoxLayout *tabGraphLayout = new QVBoxLayout(tabGraph);

  QGroupBox *grpGraphFont = new QGroupBox("Category 3 Typography (ROOT Graph, Labels, ZoomHUD)", tabGraph);
  QGridLayout *gridGraphFont = new QGridLayout(grpGraphFont);
  QLabel *lblGraphFontDesc = new QLabel(tabGraph);
  QPushButton *btnChooseGraphFont = new QPushButton("Choose Font...", tabGraph);
  QSpinBox *spinGraphSize = new QSpinBox(tabGraph);
  spinGraphSize->setRange(7, 32);
  spinGraphSize->setValue(curGraphFont.pointSize() > 0 ? curGraphFont.pointSize() : 10);
  spinGraphSize->setSuffix(" pt");

  QComboBox *comboRootFont = new QComboBox(tabGraph);
  comboRootFont->addItem("Helvetica (Sans-Serif - Standard ROOT Font 4)", 4);
  comboRootFont->addItem("Times (Serif - ROOT Font 13)", 13);
  comboRootFont->addItem("Courier (Monospace - ROOT Font 6)", 6);
  comboRootFont->addItem("Greek / Symbol (ROOT Font 12)", 12);
  int foundIdx = comboRootFont->findData(curRootFontIdx);
  if (foundIdx >= 0) comboRootFont->setCurrentIndex(foundIdx);

  gridGraphFont->addWidget(new QLabel("Qt Overlay Font:"), 0, 0);
  gridGraphFont->addWidget(lblGraphFontDesc, 0, 1);
  gridGraphFont->addWidget(btnChooseGraphFont, 0, 2);
  gridGraphFont->addWidget(new QLabel("Qt Overlay Size:"), 1, 0);
  gridGraphFont->addWidget(spinGraphSize, 1, 1);
  gridGraphFont->addWidget(new QLabel("ROOT Font Family:"), 2, 0);
  gridGraphFont->addWidget(comboRootFont, 2, 1, 1, 2);
  tabGraphLayout->addWidget(grpGraphFont);

  QGroupBox *grpGraphColors = new QGroupBox("Colors", tabGraph);
  QGridLayout *gridGraphColors = new QGridLayout(grpGraphColors);
  QPushButton *swatchGraphBg = addColorRow(gridGraphColors, 0, "Canvas Background:", curGraphBg, [&]() { if (updateGraphPreview) updateGraphPreview(); });
  QPushButton *swatchSpec = addColorRow(gridGraphColors, 1, "Spectrum Trace & Fill:", curSpec, [&]() { if (updateGraphPreview) updateGraphPreview(); });
  QPushButton *swatchPeak = addColorRow(gridGraphColors, 2, "Peak Markers & Labels:", curPeak, [&]() { if (updateGraphPreview) updateGraphPreview(); });
  tabGraphLayout->addWidget(grpGraphColors);

  QGroupBox *grpGraphPreview = new QGroupBox("Live Preview", tabGraph);
  QVBoxLayout *vboxGraphPreview = new QVBoxLayout(grpGraphPreview);
  QFrame *mockCanvasBox = new QFrame(grpGraphPreview);
  mockCanvasBox->setMinimumHeight(80);
  QHBoxLayout *mockCanvasLayout = new QHBoxLayout(mockCanvasBox);
  QLabel *mockPeakLabel = new QLabel("[1] 1332.5 keV", mockCanvasBox);
  mockPeakLabel->setAlignment(Qt::AlignCenter);
  mockCanvasLayout->addWidget(mockPeakLabel);
  vboxGraphPreview->addWidget(mockCanvasBox);
  tabGraphLayout->addWidget(grpGraphPreview);
  tabGraphLayout->addStretch();

  tabs->addTab(tabGraph, "📈 3. Graph & Spectrum");

  dialogLayout->addWidget(tabs, 1);

  // Update Preview Implementations
  updateBtnPreview = [&]() {
    lblBtnFontDesc->setText(QString("%1, %2pt, %3")
                                .arg(curBtnFont.family())
                                .arg(curBtnFont.pointSize() > 0 ? curBtnFont.pointSize() : 11)
                                .arg(curBtnFont.bold() ? "Bold" : "Normal"));
    QFont f = curBtnFont;
    f.setBold(true);
    sampleBtn->setFont(f);
    sampleBtn->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; font-weight: bold; padding: 4px; "
        "border-top: 2px solid #ffffff; border-left: 2px solid #ffffff; "
        "border-right: 2px solid #606060; border-bottom: 2px solid #606060; }"
    ).arg(curBtnBg.name(), curBtnFg.name()));

    QFont pf = curBtnFont;
    pf.setBold(false);
    samplePrompt->setFont(pf);
    samplePrompt->setStyleSheet(QString(
        "QLineEdit { background-color: %1; color: %2; border: 1px solid #888888; padding: 4px; }"
    ).arg(curPromptBg.name(), curPromptFg.name()));
  };

  updateDlgPreview = [&]() {
    lblDlgFontDesc->setText(QString("%1, %2pt")
                                .arg(curDialogFont.family())
                                .arg(curDialogFont.pointSize() > 0 ? curDialogFont.pointSize() : 11));
    mockDialogBox->setStyleSheet(QString(
        "QFrame { background-color: %1; border: 1px solid #555555; border-radius: 4px; }"
    ).arg(curDlgBg.name()));
    mockLabel->setFont(curDialogFont);
    mockLabel->setStyleSheet(QString("color: %1; border: none;").arg(curDlgFg.name()));
    mockButton->setFont(curDialogFont);
    mockButton->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #ffffff; border: 1px solid #777777; "
        "border-radius: 4px; padding: 4px 12px; font-weight: bold; }"
    ).arg(curDlgAccent.name()));
  };

  updateGraphPreview = [&]() {
    lblGraphFontDesc->setText(QString("%1, %2pt")
                                  .arg(curGraphFont.family())
                                  .arg(curGraphFont.pointSize() > 0 ? curGraphFont.pointSize() : 10));
    mockCanvasBox->setStyleSheet(QString(
        "QFrame { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
    ).arg(curGraphBg.name(), curSpec.name()));
    QFont gf = curGraphFont;
    gf.setBold(true);
    mockPeakLabel->setFont(gf);
    mockPeakLabel->setStyleSheet(QString("color: %1; border: none; font-weight: bold;").arg(curPeak.name()));
  };

  // Wire Font Choosers
  QObject::connect(btnChooseBtnFont, &QPushButton::clicked, [&]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, curBtnFont, &dialog, "Choose Button & Prompt Font");
    if (ok) {
      curBtnFont = f;
      spinBtnSize->setValue(f.pointSize());
      updateBtnPreview();
    }
  });

  QObject::connect(spinBtnSize, QOverload<int>::of(&QSpinBox::valueChanged), [&](int val) {
    curBtnFont.setPointSize(val);
    updateBtnPreview();
  });

  QObject::connect(btnChooseDlgFont, &QPushButton::clicked, [&]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, curDialogFont, &dialog, "Choose Dialog Typography");
    if (ok) {
      curDialogFont = f;
      spinDlgSize->setValue(f.pointSize());
      updateDlgPreview();
    }
  });

  QObject::connect(spinDlgSize, QOverload<int>::of(&QSpinBox::valueChanged), [&](int val) {
    curDialogFont.setPointSize(val);
    updateDlgPreview();
  });

  QObject::connect(btnChooseGraphFont, &QPushButton::clicked, [&]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, curGraphFont, &dialog, "Choose Graph Overlay Typography");
    if (ok) {
      curGraphFont = f;
      spinGraphSize->setValue(f.pointSize());
      updateGraphPreview();
    }
  });

  QObject::connect(spinGraphSize, QOverload<int>::of(&QSpinBox::valueChanged), [&](int val) {
    curGraphFont.setPointSize(val);
    updateGraphPreview();
  });

  QObject::connect(comboRootFont, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx) {
    curRootFontIdx = comboRootFont->itemData(idx).toInt();
  });

  // Wire Presets
  auto updateAllSwatches = [&]() {
    swatchBtnBg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curBtnBg.name()));
    swatchBtnFg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curBtnFg.name()));
    swatchPromptBg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curPromptBg.name()));
    swatchPromptFg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curPromptFg.name()));
    swatchDlgBg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curDlgBg.name()));
    swatchDlgFg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curDlgFg.name()));
    swatchDlgAccent->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curDlgAccent.name()));
    swatchGraphBg->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curGraphBg.name()));
    swatchSpec->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curSpec.name()));
    swatchPeak->setStyleSheet(QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(curPeak.name()));
    updateBtnPreview();
    updateDlgPreview();
    updateGraphPreview();
  };

  QObject::connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx) {
    if (idx == 1) {
      // Default Dark
      curBtnBg = QColor("#e0e0e0"); curBtnFg = QColor("#000000");
      curPromptBg = QColor("#ffffff"); curPromptFg = QColor("#000000");
      curDlgBg = QColor("#1e1e1e"); curDlgFg = QColor("#dcdcdc"); curDlgAccent = QColor("#007acc");
      curGraphBg = QColor("#1e1e1e"); curSpec = QColor("#3399ff"); curPeak = QColor("#00ffff");
    } else if (idx == 2) {
      // Classic Light
      curBtnBg = QColor("#e8e8e8"); curBtnFg = QColor("#111111");
      curPromptBg = QColor("#ffffff"); curPromptFg = QColor("#000000");
      curDlgBg = QColor("#f4f4f4"); curDlgFg = QColor("#222222"); curDlgAccent = QColor("#0066cc");
      curGraphBg = QColor("#ffffff"); curSpec = QColor("#0033aa"); curPeak = QColor("#cc0000");
    } else if (idx == 3) {
      // Vampire
      curBtnBg = QColor("#2b1b1b"); curBtnFg = QColor("#ffcccc");
      curPromptBg = QColor("#1a0f0f"); curPromptFg = QColor("#ff7777");
      curDlgBg = QColor("#160808"); curDlgFg = QColor("#f0d0d0"); curDlgAccent = QColor("#870202");
      curGraphBg = QColor("#100505"); curSpec = QColor("#870202"); curPeak = QColor("#ff3333");
    } else if (idx == 4) {
      // Cyberpunk
      curBtnBg = QColor("#181828"); curBtnFg = QColor("#00ffff");
      curPromptBg = QColor("#0c0c16"); curPromptFg = QColor("#00ff99");
      curDlgBg = QColor("#12131f"); curDlgFg = QColor("#e6e6ff"); curDlgAccent = QColor("#ff007f");
      curGraphBg = QColor("#0a0a14"); curSpec = QColor("#00f0ff"); curPeak = QColor("#ff007f");
    }
    updateAllSwatches();
  });

  // Action Buttons Bar
  QHBoxLayout *btnBar = new QHBoxLayout();
  QPushButton *btnReset = new QPushButton("↺ Reset to Defaults", &dialog);
  QPushButton *btnApply = new QPushButton("Apply", &dialog);
  QPushButton *btnOk = new QPushButton("OK", &dialog);
  QPushButton *btnCancel = new QPushButton("Cancel", &dialog);

  btnOk->setDefault(true);
  btnApply->setStyleSheet("QPushButton { background-color: #0e639c; color: #ffffff; font-weight: bold; }");
  btnOk->setStyleSheet("QPushButton { background-color: #107c41; color: #ffffff; font-weight: bold; }");

  btnBar->addWidget(btnReset);
  btnBar->addStretch();
  btnBar->addWidget(btnApply);
  btnBar->addWidget(btnOk);
  btnBar->addWidget(btnCancel);
  dialogLayout->addLayout(btnBar);

  auto commitChanges = [&]() {
    Design::setButtonPromptFont(curBtnFont);
    Design::setButtonBackgroundColor(curBtnBg);
    Design::setButtonTextColor(curBtnFg);
    Design::setPromptBackgroundColor(curPromptBg);
    Design::setPromptTextColor(curPromptFg);

    Design::setDialogFont(curDialogFont);
    Design::setDialogBackgroundColor(curDlgBg);
    Design::setDialogTextColor(curDlgFg);
    Design::setDialogAccentColor(curDlgAccent);

    Design::setGraphFont(curGraphFont, curRootFontIdx);
    Design::setGraphBackgroundColor(curGraphBg);
    Design::setSpectrumColor(curSpec);
    Design::setPeakMarkerColor(curPeak);

    Design::saveSettings();
    Design::applyUITheme(canvasWidget);

    // Refresh dialog's own styling
    dialog.setFont(curDialogFont);
    dialog.setStyleSheet(Design::getDialogStyleSheet());
  };

  QObject::connect(btnReset, &QPushButton::clicked, [&]() {
    Design::resetToDefaults();
    curBtnFont = Design::getButtonPromptFont();
    curDialogFont = Design::getDialogFont();
    curGraphFont = Design::getGraphFont();
    curRootFontIdx = Design::getRootFontFamilyIndex();

    curBtnBg = Design::getButtonBackgroundColor();
    curBtnFg = Design::getButtonTextColor();
    curPromptBg = Design::getPromptBackgroundColor();
    curPromptFg = Design::getPromptTextColor();

    curDlgBg = Design::getDialogBackgroundColor();
    curDlgFg = Design::getDialogTextColor();
    curDlgAccent = Design::getDialogAccentColor();

    curGraphBg = Design::getGraphBackgroundColor();
    curSpec = Design::getSpectrumColor();
    curPeak = Design::getPeakMarkerColor();

    spinBtnSize->setValue(curBtnFont.pointSize());
    spinDlgSize->setValue(curDialogFont.pointSize());
    spinGraphSize->setValue(curGraphFont.pointSize());
    int rootIdx = comboRootFont->findData(curRootFontIdx);
    if (rootIdx >= 0) comboRootFont->setCurrentIndex(rootIdx);
    presetCombo->setCurrentIndex(0);

    updateAllSwatches();
  });

  QObject::connect(btnApply, &QPushButton::clicked, commitChanges);

  QObject::connect(btnOk, &QPushButton::clicked, [&]() {
    commitChanges();
    dialog.accept();
  });

  QObject::connect(btnCancel, &QPushButton::clicked, [&]() {
    // Revert to original settings in case Apply was previously pressed
    Design::setButtonPromptFont(origBtnFont);
    Design::setButtonBackgroundColor(origBtnBg);
    Design::setButtonTextColor(origBtnFg);
    Design::setPromptBackgroundColor(origPromptBg);
    Design::setPromptTextColor(origPromptFg);

    Design::setDialogFont(origDialogFont);
    Design::setDialogBackgroundColor(origDlgBg);
    Design::setDialogTextColor(origDlgFg);
    Design::setDialogAccentColor(origDlgAccent);

    Design::setGraphFont(origGraphFont, origRootFontIdx);
    Design::setGraphBackgroundColor(origGraphBg);
    Design::setSpectrumColor(origSpec);
    Design::setPeakMarkerColor(origPeak);

    Design::saveSettings();
    Design::applyUITheme(canvasWidget);
    dialog.reject();
  });

  // Initial trigger to render previews
  updateBtnPreview();
  updateDlgPreview();
  updateGraphPreview();

  dialog.exec();
}
