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
      if (!btn) continue;

      // Do NOT style buttons inside dialogs: dialog buttons must respond to Dialog options
      if (btn->window() != mainCanvas) continue;
      bool insideDialog = false;
      for (QWidget *w = btn->parentWidget(); w && w != mainCanvas; w = w->parentWidget()) {
        if (qobject_cast<QDialog*>(w)) {
          insideDialog = true;
          break;
        }
      }
      if (insideDialog) continue;

      btn->setFont(btnFont);
      btn->setStyleSheet(btnStyle);
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
      if (tc->GetListOfPrimitives()) {
        TIter next(tc->GetListOfPrimitives());
        TObject *obj = nullptr;
        while ((obj = next())) {
          if (obj && obj->InheritsFrom(TPad::Class())) {
            static_cast<TPad*>(obj)->SetFillColor(rootBg);
          }
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
// AppearanceDialog (Full Settings Dialog for Fonts, Sizes, and Colors)
//==============================================================================
class AppearanceDialog : public QDialog {
public:
  AppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget);

private:
  void setupUI();
  void addColorRow(QGridLayout *grid, int row, const QString &label, QColor *colorVar, std::function<void()> onChange);
  void updateBtnPreview();
  void updateDlgPreview();
  void updateGraphPreview();
  void updateAllSwatches();
  void refreshDialogTheme();
  void commitChanges();
  void revertChanges();

  QMainCanvas *m_canvasWidget;

  // Snapshot of original values
  QFont m_origBtnFont;
  QFont m_origDialogFont;
  QFont m_origGraphFont;
  int m_origRootFontIdx;

  QColor m_origBtnBg;
  QColor m_origBtnFg;
  QColor m_origPromptBg;
  QColor m_origPromptFg;

  QColor m_origDlgBg;
  QColor m_origDlgFg;
  QColor m_origDlgAccent;

  QColor m_origGraphBg;
  QColor m_origSpec;
  QColor m_origPeak;

  // Working values
  QFont m_curBtnFont;
  QFont m_curDialogFont;
  QFont m_curGraphFont;
  int m_curRootFontIdx;

  QColor m_curBtnBg;
  QColor m_curBtnFg;
  QColor m_curPromptBg;
  QColor m_curPromptFg;

  QColor m_curDlgBg;
  QColor m_curDlgFg;
  QColor m_curDlgAccent;

  QColor m_curGraphBg;
  QColor m_curSpec;
  QColor m_curPeak;

  // UI elements for live updates
  QComboBox *m_presetCombo{nullptr};
  QLabel *m_lblBtnFontDesc{nullptr};
  QSpinBox *m_spinBtnSize{nullptr};
  QPushButton *m_sampleBtn{nullptr};
  QLineEdit *m_samplePrompt{nullptr};

  QLabel *m_lblDlgFontDesc{nullptr};
  QSpinBox *m_spinDlgSize{nullptr};
  QFrame *m_mockDialogBox{nullptr};
  QLabel *m_mockLabel{nullptr};
  QPushButton *m_mockButton{nullptr};

  QLabel *m_lblGraphFontDesc{nullptr};
  QSpinBox *m_spinGraphSize{nullptr};
  QComboBox *m_comboRootFont{nullptr};
  QFrame *m_mockCanvasBox{nullptr};
  QLabel *m_mockPeakLabel{nullptr};

  QPushButton *m_btnReset{nullptr};
  QPushButton *m_btnApply{nullptr};
  QPushButton *m_btnOk{nullptr};
  QPushButton *m_btnCancel{nullptr};

  QList<std::function<void()>> m_swatchUpdaters;
};

AppearanceDialog::AppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget)
    : QDialog(parent), m_canvasWidget(canvasWidget)
{
  setWindowTitle("Appearance & Typography Settings (3 Fonts & Colors)");
  resize(680, 640);

  // Snapshot original settings to allow clean Cancel / revert
  m_origBtnFont = Design::getButtonPromptFont();
  m_origDialogFont = Design::getDialogFont();
  m_origGraphFont = Design::getGraphFont();
  m_origRootFontIdx = Design::getRootFontFamilyIndex();

  m_origBtnBg = Design::getButtonBackgroundColor();
  m_origBtnFg = Design::getButtonTextColor();
  m_origPromptBg = Design::getPromptBackgroundColor();
  m_origPromptFg = Design::getPromptTextColor();

  m_origDlgBg = Design::getDialogBackgroundColor();
  m_origDlgFg = Design::getDialogTextColor();
  m_origDlgAccent = Design::getDialogAccentColor();

  m_origGraphBg = Design::getGraphBackgroundColor();
  m_origSpec = Design::getSpectrumColor();
  m_origPeak = Design::getPeakMarkerColor();

  // Working copies
  m_curBtnFont = m_origBtnFont;
  m_curDialogFont = m_origDialogFont;
  m_curGraphFont = m_origGraphFont;
  m_curRootFontIdx = m_origRootFontIdx;

  m_curBtnBg = m_origBtnBg;
  m_curBtnFg = m_origBtnFg;
  m_curPromptBg = m_origPromptBg;
  m_curPromptFg = m_origPromptFg;

  m_curDlgBg = m_origDlgBg;
  m_curDlgFg = m_origDlgFg;
  m_curDlgAccent = m_origDlgAccent;

  m_curGraphBg = m_origGraphBg;
  m_curSpec = m_origSpec;
  m_curPeak = m_origPeak;

  setupUI();
  refreshDialogTheme();
}

void AppearanceDialog::addColorRow(QGridLayout *grid, int row, const QString &label, QColor *colorVar, std::function<void()> onChange) {
  QLabel *lbl = new QLabel(label, this);
  grid->addWidget(lbl, row, 0);

  // Direct clickable colored button (swatch) displaying its hex value
  QPushButton *swatch = new QPushButton(this);
  swatch->setFixedSize(86, 28);
  swatch->setCursor(Qt::PointingHandCursor);
  swatch->setToolTip(QString("Click to choose %1").arg(label));

  auto updateSwatch = [swatch, colorVar]() {
    const QString hex = colorVar->name().toUpper();
    // High-contrast font color based on background luminance
    double lum = 0.299 * colorVar->red() + 0.587 * colorVar->green() + 0.114 * colorVar->blue();
    const QString textCol = lum > 140 ? "#000000" : "#ffffff";
    swatch->setText(hex);
    swatch->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; border: 2px solid #888888; border-radius: 4px; font-weight: bold; font-family: monospace; font-size: 10pt; }\n"
        "QPushButton:hover { border: 2px solid #ffffff; }\n"
    ).arg(colorVar->name(), textCol));
  };
  updateSwatch();
  m_swatchUpdaters.append(updateSwatch);

  const QString dialogTitle = QString("Select %1").arg(label);
  connect(swatch, &QPushButton::clicked, this, [this, colorVar, dialogTitle, updateSwatch, onChange]() {
    QColor c = QColorDialog::getColor(*colorVar, this, dialogTitle);
    if (c.isValid()) {
      *colorVar = c;
      updateSwatch();
      if (onChange) onChange();
    }
  });

  grid->addWidget(swatch, row, 1, Qt::AlignRight);
}

void AppearanceDialog::setupUI() {
  QVBoxLayout *dialogLayout = new QVBoxLayout(this);
  dialogLayout->setSpacing(10);
  dialogLayout->setContentsMargins(14, 14, 14, 14);

  // Top header with quick theme presets dropdown
  QHBoxLayout *presetLayout = new QHBoxLayout();
  QLabel *lblPreset = new QLabel("<b>Quick Theme Preset:</b>", this);
  m_presetCombo = new QComboBox(this);
  m_presetCombo->addItem("(Custom / Keep Current)");
  m_presetCombo->addItem("Default Dark (Balanced)");
  m_presetCombo->addItem("Classic Light (Paper White)");
  m_presetCombo->addItem("Vampire (Crimson / Deep Black)");
  m_presetCombo->addItem("Cyberpunk (Neon Cyan & Pink)");
  presetLayout->addWidget(lblPreset);
  presetLayout->addWidget(m_presetCombo, 1);
  dialogLayout->addLayout(presetLayout);

  // 3-Category Tabs
  QTabWidget *tabs = new QTabWidget(this);

  // =========================================================================
  // TAB 1: Buttons & Prompt
  // =========================================================================
  QWidget *tabBtn = new QWidget();
  QVBoxLayout *tabBtnLayout = new QVBoxLayout(tabBtn);

  QGroupBox *grpBtnFont = new QGroupBox("Category 1 Typography (Buttons & Console)", tabBtn);
  QGridLayout *gridBtnFont = new QGridLayout(grpBtnFont);
  m_lblBtnFontDesc = new QLabel(tabBtn);
  QPushButton *btnChooseBtnFont = new QPushButton("Choose Font...", tabBtn);
  m_spinBtnSize = new QSpinBox(tabBtn);
  m_spinBtnSize->setRange(7, 32);
  m_spinBtnSize->setValue(m_curBtnFont.pointSize() > 0 ? m_curBtnFont.pointSize() : 11);
  m_spinBtnSize->setSuffix(" pt");

  gridBtnFont->addWidget(new QLabel("Font Family:"), 0, 0);
  gridBtnFont->addWidget(m_lblBtnFontDesc, 0, 1);
  gridBtnFont->addWidget(btnChooseBtnFont, 0, 2);
  gridBtnFont->addWidget(new QLabel("Point Size:"), 1, 0);
  gridBtnFont->addWidget(m_spinBtnSize, 1, 1);
  tabBtnLayout->addWidget(grpBtnFont);

  QGroupBox *grpBtnColors = new QGroupBox("Colors", tabBtn);
  QGridLayout *gridBtnColors = new QGridLayout(grpBtnColors);
  gridBtnColors->setColumnStretch(0, 1);
  gridBtnColors->setColumnStretch(1, 0);
  addColorRow(gridBtnColors, 0, "Button Background:", &m_curBtnBg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 1, "Button Text Color:", &m_curBtnFg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 2, "Console Prompt Background:", &m_curPromptBg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 3, "Console Prompt Text:", &m_curPromptFg, [this]() { updateBtnPreview(); });
  tabBtnLayout->addWidget(grpBtnColors);

  QGroupBox *grpBtnPreview = new QGroupBox("Live Preview (Main UI Toolbar)", tabBtn);
  QVBoxLayout *vboxBtnPreview = new QVBoxLayout(grpBtnPreview);
  m_sampleBtn = new QPushButton("EnCal", grpBtnPreview);
  m_sampleBtn->setFixedHeight(36);
  m_samplePrompt = new QLineEdit("NuTrackN Output Console: Peak 1 at 1332.5 keV (FWHM 2.1)", grpBtnPreview);
  m_samplePrompt->setReadOnly(true);
  m_samplePrompt->setFixedHeight(36);
  vboxBtnPreview->addWidget(m_sampleBtn);
  vboxBtnPreview->addWidget(m_samplePrompt);
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
  m_lblDlgFontDesc = new QLabel(tabDlg);
  QPushButton *btnChooseDlgFont = new QPushButton("Choose Font...", tabDlg);
  m_spinDlgSize = new QSpinBox(tabDlg);
  m_spinDlgSize->setRange(7, 32);
  m_spinDlgSize->setValue(m_curDialogFont.pointSize() > 0 ? m_curDialogFont.pointSize() : 11);
  m_spinDlgSize->setSuffix(" pt");

  gridDlgFont->addWidget(new QLabel("Font Family:"), 0, 0);
  gridDlgFont->addWidget(m_lblDlgFontDesc, 0, 1);
  gridDlgFont->addWidget(btnChooseDlgFont, 0, 2);
  gridDlgFont->addWidget(new QLabel("Point Size:"), 1, 0);
  gridDlgFont->addWidget(m_spinDlgSize, 1, 1);
  tabDlgLayout->addWidget(grpDlgFont);

  QGroupBox *grpDlgColors = new QGroupBox("Colors", tabDlg);
  QGridLayout *gridDlgColors = new QGridLayout(grpDlgColors);
  gridDlgColors->setColumnStretch(0, 1);
  gridDlgColors->setColumnStretch(1, 0);
  addColorRow(gridDlgColors, 0, "Dialog Background:", &m_curDlgBg, [this]() {
    updateDlgPreview();
    refreshDialogTheme();
  });
  addColorRow(gridDlgColors, 1, "Dialog Text & Labels:", &m_curDlgFg, [this]() {
    updateDlgPreview();
    refreshDialogTheme();
  });
  addColorRow(gridDlgColors, 2, "Highlight / Accent:", &m_curDlgAccent, [this]() {
    updateDlgPreview();
    refreshDialogTheme();
  });
  tabDlgLayout->addWidget(grpDlgColors);

  QGroupBox *grpDlgPreview = new QGroupBox("Live Preview (Dialog Controls)", tabDlg);
  QVBoxLayout *vboxDlgPreview = new QVBoxLayout(grpDlgPreview);
  m_mockDialogBox = new QFrame(grpDlgPreview);
  QVBoxLayout *mockLayout = new QVBoxLayout(m_mockDialogBox);
  m_mockLabel = new QLabel("Integration Range: [ 1120 .. 1450 ]", m_mockDialogBox);
  m_mockButton = new QPushButton("Apply Parameters", m_mockDialogBox);
  mockLayout->addWidget(m_mockLabel);
  mockLayout->addWidget(m_mockButton);
  vboxDlgPreview->addWidget(m_mockDialogBox);
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
  m_lblGraphFontDesc = new QLabel(tabGraph);
  QPushButton *btnChooseGraphFont = new QPushButton("Choose Font...", tabGraph);
  m_spinGraphSize = new QSpinBox(tabGraph);
  m_spinGraphSize->setRange(7, 32);
  m_spinGraphSize->setValue(m_curGraphFont.pointSize() > 0 ? m_curGraphFont.pointSize() : 10);
  m_spinGraphSize->setSuffix(" pt");

  m_comboRootFont = new QComboBox(tabGraph);
  m_comboRootFont->addItem("Helvetica (Sans-Serif - Standard ROOT Font 4)", 4);
  m_comboRootFont->addItem("Times (Serif - ROOT Font 13)", 13);
  m_comboRootFont->addItem("Courier (Monospace - ROOT Font 6)", 6);
  m_comboRootFont->addItem("Greek / Symbol (ROOT Font 12)", 12);
  int foundIdx = m_comboRootFont->findData(m_curRootFontIdx);
  if (foundIdx >= 0) m_comboRootFont->setCurrentIndex(foundIdx);

  gridGraphFont->addWidget(new QLabel("Qt Overlay Font:"), 0, 0);
  gridGraphFont->addWidget(m_lblGraphFontDesc, 0, 1);
  gridGraphFont->addWidget(btnChooseGraphFont, 0, 2);
  gridGraphFont->addWidget(new QLabel("Qt Overlay Size:"), 1, 0);
  gridGraphFont->addWidget(m_spinGraphSize, 1, 1);
  gridGraphFont->addWidget(new QLabel("ROOT Font Family:"), 2, 0);
  gridGraphFont->addWidget(m_comboRootFont, 2, 1, 1, 2);
  tabGraphLayout->addWidget(grpGraphFont);

  QGroupBox *grpGraphColors = new QGroupBox("Colors", tabGraph);
  QGridLayout *gridGraphColors = new QGridLayout(grpGraphColors);
  gridGraphColors->setColumnStretch(0, 1);
  gridGraphColors->setColumnStretch(1, 0);
  addColorRow(gridGraphColors, 0, "Canvas Background:", &m_curGraphBg, [this]() { updateGraphPreview(); });
  addColorRow(gridGraphColors, 1, "Spectrum Trace & Fill:", &m_curSpec, [this]() { updateGraphPreview(); });
  addColorRow(gridGraphColors, 2, "Peak Markers & Labels:", &m_curPeak, [this]() { updateGraphPreview(); });
  tabGraphLayout->addWidget(grpGraphColors);

  QGroupBox *grpGraphPreview = new QGroupBox("Live Preview (Graph & Canvas)", tabGraph);
  QVBoxLayout *vboxGraphPreview = new QVBoxLayout(grpGraphPreview);
  m_mockCanvasBox = new QFrame(grpGraphPreview);
  m_mockCanvasBox->setMinimumHeight(80);
  QHBoxLayout *mockCanvasLayout = new QHBoxLayout(m_mockCanvasBox);
  m_mockPeakLabel = new QLabel("[1] 1332.5 keV", m_mockCanvasBox);
  m_mockPeakLabel->setAlignment(Qt::AlignCenter);
  mockCanvasLayout->addWidget(m_mockPeakLabel);
  vboxGraphPreview->addWidget(m_mockCanvasBox);
  tabGraphLayout->addWidget(grpGraphPreview);
  tabGraphLayout->addStretch();

  tabs->addTab(tabGraph, "📈 3. Graph & Spectrum");

  dialogLayout->addWidget(tabs, 1);

  // Wire Font Choosers
  connect(btnChooseBtnFont, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, m_curBtnFont, this, "Choose Button & Prompt Font");
    if (ok) {
      m_curBtnFont = f;
      m_spinBtnSize->setValue(f.pointSize());
      updateBtnPreview();
    }
  });

  connect(m_spinBtnSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
    m_curBtnFont.setPointSize(val);
    updateBtnPreview();
  });

  connect(btnChooseDlgFont, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, m_curDialogFont, this, "Choose Dialog Typography");
    if (ok) {
      m_curDialogFont = f;
      m_spinDlgSize->setValue(f.pointSize());
      updateDlgPreview();
      refreshDialogTheme();
    }
  });

  connect(m_spinDlgSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
    m_curDialogFont.setPointSize(val);
    updateDlgPreview();
    refreshDialogTheme();
  });

  connect(btnChooseGraphFont, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, m_curGraphFont, this, "Choose Graph Overlay Typography");
    if (ok) {
      m_curGraphFont = f;
      m_spinGraphSize->setValue(f.pointSize());
      updateGraphPreview();
    }
  });

  connect(m_spinGraphSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
    m_curGraphFont.setPointSize(val);
    updateGraphPreview();
  });

  connect(m_comboRootFont, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
    m_curRootFontIdx = m_comboRootFont->itemData(idx).toInt();
  });

  // Wire Presets
  connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
    if (idx == 1) {
      // Default Dark
      m_curBtnBg = QColor("#e0e0e0"); m_curBtnFg = QColor("#000000");
      m_curPromptBg = QColor("#ffffff"); m_curPromptFg = QColor("#000000");
      m_curDlgBg = QColor("#1e1e1e"); m_curDlgFg = QColor("#dcdcdc"); m_curDlgAccent = QColor("#007acc");
      m_curGraphBg = QColor("#1e1e1e"); m_curSpec = QColor("#3399ff"); m_curPeak = QColor("#00ffff");
    } else if (idx == 2) {
      // Classic Light
      m_curBtnBg = QColor("#e8e8e8"); m_curBtnFg = QColor("#111111");
      m_curPromptBg = QColor("#ffffff"); m_curPromptFg = QColor("#000000");
      m_curDlgBg = QColor("#f4f4f4"); m_curDlgFg = QColor("#222222"); m_curDlgAccent = QColor("#0066cc");
      m_curGraphBg = QColor("#ffffff"); m_curSpec = QColor("#0033aa"); m_curPeak = QColor("#cc0000");
    } else if (idx == 3) {
      // Vampire
      m_curBtnBg = QColor("#2b1b1b"); m_curBtnFg = QColor("#ffcccc");
      m_curPromptBg = QColor("#1a0f0f"); m_curPromptFg = QColor("#ff7777");
      m_curDlgBg = QColor("#160808"); m_curDlgFg = QColor("#f0d0d0"); m_curDlgAccent = QColor("#870202");
      m_curGraphBg = QColor("#100505"); m_curSpec = QColor("#870202"); m_curPeak = QColor("#ff3333");
    } else if (idx == 4) {
      // Cyberpunk
      m_curBtnBg = QColor("#181828"); m_curBtnFg = QColor("#00ffff");
      m_curPromptBg = QColor("#0c0c16"); m_curPromptFg = QColor("#00ff99");
      m_curDlgBg = QColor("#12131f"); m_curDlgFg = QColor("#e6e6ff"); m_curDlgAccent = QColor("#ff007f");
      m_curGraphBg = QColor("#0a0a14"); m_curSpec = QColor("#00f0ff"); m_curPeak = QColor("#ff007f");
    }
    updateAllSwatches();
    refreshDialogTheme();
  });

  // Action Buttons Bar
  QHBoxLayout *btnBar = new QHBoxLayout();
  m_btnReset = new QPushButton("↺ Reset to Defaults", this);
  m_btnApply = new QPushButton("Apply", this);
  m_btnOk = new QPushButton("OK", this);
  m_btnCancel = new QPushButton("Cancel", this);

  m_btnOk->setDefault(true);

  btnBar->addWidget(m_btnReset);
  btnBar->addStretch();
  btnBar->addWidget(m_btnApply);
  btnBar->addWidget(m_btnOk);
  btnBar->addWidget(m_btnCancel);
  dialogLayout->addLayout(btnBar);

  connect(m_btnReset, &QPushButton::clicked, this, [this]() {
    Design::resetToDefaults();
    m_curBtnFont = Design::getButtonPromptFont();
    m_curDialogFont = Design::getDialogFont();
    m_curGraphFont = Design::getGraphFont();
    m_curRootFontIdx = Design::getRootFontFamilyIndex();

    m_curBtnBg = Design::getButtonBackgroundColor();
    m_curBtnFg = Design::getButtonTextColor();
    m_curPromptBg = Design::getPromptBackgroundColor();
    m_curPromptFg = Design::getPromptTextColor();

    m_curDlgBg = Design::getDialogBackgroundColor();
    m_curDlgFg = Design::getDialogTextColor();
    m_curDlgAccent = Design::getDialogAccentColor();

    m_curGraphBg = Design::getGraphBackgroundColor();
    m_curSpec = Design::getSpectrumColor();
    m_curPeak = Design::getPeakMarkerColor();

    m_spinBtnSize->setValue(m_curBtnFont.pointSize());
    m_spinDlgSize->setValue(m_curDialogFont.pointSize());
    m_spinGraphSize->setValue(m_curGraphFont.pointSize());
    int rootIdx = m_comboRootFont->findData(m_curRootFontIdx);
    if (rootIdx >= 0) m_comboRootFont->setCurrentIndex(rootIdx);
    m_presetCombo->setCurrentIndex(0);

    updateAllSwatches();
    refreshDialogTheme();
  });

  connect(m_btnApply, &QPushButton::clicked, this, &AppearanceDialog::commitChanges);

  connect(m_btnOk, &QPushButton::clicked, this, [this]() {
    commitChanges();
    accept();
  });

  connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
    revertChanges();
    reject();
  });

  // Initial render
  updateBtnPreview();
  updateDlgPreview();
  updateGraphPreview();
}

void AppearanceDialog::updateBtnPreview() {
  if (!m_lblBtnFontDesc || !m_sampleBtn || !m_samplePrompt) return;
  m_lblBtnFontDesc->setText(QString("%1, %2pt, %3")
                               .arg(m_curBtnFont.family())
                               .arg(m_curBtnFont.pointSize() > 0 ? m_curBtnFont.pointSize() : 11)
                               .arg(m_curBtnFont.bold() ? "Bold" : "Normal"));
  QFont f = m_curBtnFont;
  f.setBold(true);
  m_sampleBtn->setFont(f);
  m_sampleBtn->setStyleSheet(QString(
      "QPushButton { background-color: %1; color: %2; font-weight: bold; padding: 4px; "
      "border-top: 2px solid #ffffff; border-left: 2px solid #ffffff; "
      "border-right: 2px solid #606060; border-bottom: 2px solid #606060; }"
  ).arg(m_curBtnBg.name(), m_curBtnFg.name()));

  QFont pf = m_curBtnFont;
  pf.setBold(false);
  m_samplePrompt->setFont(pf);
  m_samplePrompt->setStyleSheet(QString(
      "QLineEdit { background-color: %1; color: %2; border: 1px solid #888888; padding: 4px; }"
  ).arg(m_curPromptBg.name(), m_curPromptFg.name()));
}

void AppearanceDialog::updateDlgPreview() {
  if (!m_lblDlgFontDesc || !m_mockDialogBox || !m_mockLabel || !m_mockButton) return;
  m_lblDlgFontDesc->setText(QString("%1, %2pt")
                               .arg(m_curDialogFont.family())
                               .arg(m_curDialogFont.pointSize() > 0 ? m_curDialogFont.pointSize() : 11));
  m_mockDialogBox->setStyleSheet(QString(
      "QFrame { background-color: %1; border: 1px solid #555555; border-radius: 4px; }"
  ).arg(m_curDlgBg.name()));
  m_mockLabel->setFont(m_curDialogFont);
  m_mockLabel->setStyleSheet(QString("color: %1; border: none;").arg(m_curDlgFg.name()));
  m_mockButton->setFont(m_curDialogFont);
  m_mockButton->setStyleSheet(QString(
      "QPushButton { background-color: %1; color: #ffffff; border: 1px solid #777777; "
      "border-radius: 4px; padding: 4px 12px; font-weight: bold; }"
  ).arg(m_curDlgAccent.name()));
}

void AppearanceDialog::updateGraphPreview() {
  if (!m_lblGraphFontDesc || !m_mockCanvasBox || !m_mockPeakLabel) return;
  m_lblGraphFontDesc->setText(QString("%1, %2pt")
                                .arg(m_curGraphFont.family())
                                .arg(m_curGraphFont.pointSize() > 0 ? m_curGraphFont.pointSize() : 10));
  m_mockCanvasBox->setStyleSheet(QString(
      "QFrame { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
  ).arg(m_curGraphBg.name(), m_curSpec.name()));
  QFont gf = m_curGraphFont;
  gf.setBold(true);
  m_mockPeakLabel->setFont(gf);
  m_mockPeakLabel->setStyleSheet(QString("color: %1; border: none; font-weight: bold;").arg(m_curPeak.name()));
}

void AppearanceDialog::refreshDialogTheme() {
  setFont(m_curDialogFont);

  const QString family = m_curDialogFont.family();
  const int pt = m_curDialogFont.pointSize() > 0 ? m_curDialogFont.pointSize() : 11;
  const QString bg = m_curDlgBg.name();
  const QString fg = m_curDlgFg.name();
  const QString accent = m_curDlgAccent.name();
  const QString panelBg = m_curDlgBg.lighter(118).name();
  const QString border = m_curDlgBg.lighter(140).name();
  const QString inputBg = m_curDlgBg.lighter(110).name();

  setStyleSheet(QString(
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
  ).arg(bg, fg, family).arg(pt).arg(border, accent, inputBg, panelBg));

  if (m_btnApply) {
    m_btnApply->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #ffffff; border: 1px solid %2; border-radius: 4px; padding: 5px 14px; font-weight: bold; font-family: \"%3\"; font-size: %4pt; }\n"
        "QPushButton:hover { background-color: %5; }\n"
    ).arg(accent, border, family).arg(pt).arg(m_curDlgAccent.lighter(120).name()));
  }
  if (m_btnOk) {
    m_btnOk->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #ffffff; border: 1px solid %2; border-radius: 4px; padding: 5px 14px; font-weight: bold; font-family: \"%3\"; font-size: %4pt; }\n"
        "QPushButton:hover { background-color: %5; }\n"
    ).arg(accent, border, family).arg(pt).arg(m_curDlgAccent.lighter(120).name()));
  }
}

void AppearanceDialog::updateAllSwatches() {
  for (const auto &fn : m_swatchUpdaters) {
    fn();
  }
  updateBtnPreview();
  updateDlgPreview();
  updateGraphPreview();
}

void AppearanceDialog::commitChanges() {
  Design::setButtonPromptFont(m_curBtnFont);
  Design::setButtonBackgroundColor(m_curBtnBg);
  Design::setButtonTextColor(m_curBtnFg);
  Design::setPromptBackgroundColor(m_curPromptBg);
  Design::setPromptTextColor(m_curPromptFg);

  Design::setDialogFont(m_curDialogFont);
  Design::setDialogBackgroundColor(m_curDlgBg);
  Design::setDialogTextColor(m_curDlgFg);
  Design::setDialogAccentColor(m_curDlgAccent);

  Design::setGraphFont(m_curGraphFont, m_curRootFontIdx);
  Design::setGraphBackgroundColor(m_curGraphBg);
  Design::setSpectrumColor(m_curSpec);
  Design::setPeakMarkerColor(m_curPeak);

  Design::saveSettings();
  Design::applyUITheme(m_canvasWidget);

  refreshDialogTheme();
}

void AppearanceDialog::revertChanges() {
  Design::setButtonPromptFont(m_origBtnFont);
  Design::setButtonBackgroundColor(m_origBtnBg);
  Design::setButtonTextColor(m_origBtnFg);
  Design::setPromptBackgroundColor(m_origPromptBg);
  Design::setPromptTextColor(m_origPromptFg);

  Design::setDialogFont(m_origDialogFont);
  Design::setDialogBackgroundColor(m_origDlgBg);
  Design::setDialogTextColor(m_origDlgFg);
  Design::setDialogAccentColor(m_origDlgAccent);

  Design::setGraphFont(m_origGraphFont, m_origRootFontIdx);
  Design::setGraphBackgroundColor(m_origGraphBg);
  Design::setSpectrumColor(m_origSpec);
  Design::setPeakMarkerColor(m_origPeak);

  Design::saveSettings();
  Design::applyUITheme(m_canvasWidget);

  m_curDialogFont = m_origDialogFont;
  m_curDlgBg = m_origDlgBg;
  m_curDlgFg = m_origDlgFg;
  m_curDlgAccent = m_origDlgAccent;
  refreshDialogTheme();
}

void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  AppearanceDialog dialog(parent, canvasWidget);
  dialog.exec();
}
