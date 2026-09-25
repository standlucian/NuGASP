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
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <iostream>
#include <cmath>

#include <TCanvas.h>
#include <TPad.h>
#include <TColor.h>
#include <TStyle.h>
#include <TFrame.h>
#include <TList.h>
#include <TIterator.h>

//==============================================================================
// Design Typography & Color Engine Implementation
//==============================================================================
namespace Design {

static QFont s_buttonPromptFont;
static QFont s_dialogFont;
static QFont s_graphFont;
static int s_rootFontFamilyIndex = 13; // 13 = Times (Classic Xtrackn baseline)
static bool s_typographyInitialized = false;

// Authentic Classic Xtrackn baseline (glwlib.c, trackn.F, and legacy screenshot)
static QColor s_buttonBgColor("#7790ad"); // GLW_LABELCOLOR_1
static QColor s_buttonTextColor("#00ffff"); // CYAN
static QColor s_uiBgColor("#708090");     // GLW_FRAMECOLOR
static QColor s_promptBgColor("#7790ad"); // GLW_LABELCOLOR_1
static QColor s_promptTextColor("#000000"); // BLACK

static QColor s_dialogBgColor("#708090"); // GLW_FRAMECOLOR
static QColor s_dialogTextColor("#000000"); // BLACK
static QColor s_dialogAccentColor("#2f4f4f"); // GLW_FRAMECOLOR_DARK

static QColor s_graphBgColor("#000000"); // GLW_DRAWBG = BLACK
static std::vector<QColor> s_spectrumColors = {
    QColor("#ffffff"), // 1: WHITE   (glwlib.c:2577)
    QColor("#ff0000"), // 2: RED     (glwlib.c:2578)
    QColor("#00ff00"), // 3: GREEN   (glwlib.c:2579)
    QColor("#ffff00"), // 4: YELLOW  (glwlib.c:2580)
    QColor("#0000ff"), // 5: BLUE    (glwlib.c:2581)
    QColor("#00ffff"), // 6: CYAN    (glwlib.c:2582)
    QColor("#ff00ff"), // 7: MAGENTA (glwlib.c:2583)
    QColor("#ff9900"), // 8: ORANGE
    QColor("#800080")  // 9: PURPLE
};

static QColor s_peakMarkerColor("#ffff00");     // YELLOW (trackn.F:2166, glwlib.c:2137)
static QColor s_zoomMarkerColor("#00ffff");     // CYAN   (ROI marker standard)
static QColor s_bgMarkerColor("#0000ff");       // BLUE   (trackn.F:628 CALL SHOWMARKERS(XMBGD,NMBGD,2,4))
static QColor s_integralMarkerColor("#ffff00"); // YELLOW (trackn.F:644 CALL SHOWMARKERS(XMINT,NMINT,2,3))
static QColor s_rangeMarkerColor("#ff00ff");    // MAGENTA(trackn.F:640 CALL SHOWMARKERS(XMREG,NMREG,2,6))
static QColor s_gaussMarkerColor("#ffff00");    // YELLOW (trackn.F:631 CALL SHOWMARKERS(XMGAU,NMGAU,1,3))
static QColor s_gateMarkerColor("#ff0000");     // RED    (trackn.F:665 CALL SHOWMARKERS(CGATE,NGATES*2+mod(NGNEXT,2),2,1))

void initializeTypography() {
  if (s_typographyInitialized) return;

  // Category 1: Buttons and Prompt (Times New Roman Bold matching legacy Xtrackn)
  QFont timesFont("Times New Roman", 11);
  timesFont.setStyleHint(QFont::Times);
  s_buttonPromptFont = timesFont;
  s_buttonPromptFont.setBold(true);

  // Category 2: Dialogs
  s_dialogFont = timesFont;

  // Category 3: Graph overlay & ROOT Times
  s_graphFont = timesFont;
  s_rootFontFamilyIndex = 13; // ROOT Font 13: Times

  s_buttonBgColor = QColor("#7790ad");
  s_buttonTextColor = QColor("#00ffff");
  s_uiBgColor = QColor("#708090");
  s_promptBgColor = QColor("#7790ad");
  s_promptTextColor = QColor("#000000");

  s_dialogBgColor = QColor("#708090");
  s_dialogTextColor = QColor("#000000");
  s_dialogAccentColor = QColor("#2f4f4f");

  s_graphBgColor = QColor("#000000");
  s_spectrumColors = {
      QColor("#ffffff"),
      QColor("#ff0000"),
      QColor("#00ff00"),
      QColor("#ffff00"),
      QColor("#0000ff"),
      QColor("#00ffff"),
      QColor("#ff00ff"),
      QColor("#ff9900"),
      QColor("#800080")
  };
  s_peakMarkerColor = QColor("#ffff00");
  s_zoomMarkerColor = QColor("#00ffff");
  s_bgMarkerColor = QColor("#0000ff");
  s_integralMarkerColor = QColor("#ffff00");
  s_rangeMarkerColor = QColor("#ff00ff");
  s_gaussMarkerColor = QColor("#ffff00");
  s_gateMarkerColor = QColor("#ff0000");

  loadSettings();
  s_typographyInitialized = true;
}

void resetToDefaults() {
  // Authentic Classic Xtrackn baseline (glwlib.c, trackn.F & screenshot)
  QFont timesFont("Times New Roman", 11);
  timesFont.setStyleHint(QFont::Times);
  s_buttonPromptFont = timesFont;
  s_buttonPromptFont.setBold(true);
  s_dialogFont = timesFont;
  s_graphFont = timesFont;
  s_rootFontFamilyIndex = 13; // ROOT Font 13: Times

  s_buttonBgColor = QColor("#7790ad");
  s_buttonTextColor = QColor("#00ffff");
  s_uiBgColor = QColor("#708090");
  s_promptBgColor = QColor("#7790ad");
  s_promptTextColor = QColor("#000000");

  s_dialogBgColor = QColor("#708090");
  s_dialogTextColor = QColor("#000000");
  s_dialogAccentColor = QColor("#2f4f4f");

  s_graphBgColor = QColor("#000000");
  s_spectrumColors = {
      QColor("#ffffff"),
      QColor("#ff0000"),
      QColor("#00ff00"),
      QColor("#ffff00"),
      QColor("#0000ff"),
      QColor("#00ffff"),
      QColor("#ff00ff"),
      QColor("#ff9900"),
      QColor("#800080")
  };
  s_peakMarkerColor = QColor("#ffff00");
  s_zoomMarkerColor = QColor("#00ffff");
  s_bgMarkerColor = QColor("#0000ff");
  s_integralMarkerColor = QColor("#ffff00");
  s_rangeMarkerColor = QColor("#ff00ff");
  s_gaussMarkerColor = QColor("#ffff00");
  s_gateMarkerColor = QColor("#ff0000");

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
  settings.setValue("Design/UIBgColor", s_uiBgColor.name());
  settings.setValue("Design/PromptBgColor", s_promptBgColor.name());
  settings.setValue("Design/PromptTextColor", s_promptTextColor.name());

  settings.setValue("Design/DialogBgColor", s_dialogBgColor.name());
  settings.setValue("Design/DialogTextColor", s_dialogTextColor.name());
  settings.setValue("Design/DialogAccentColor", s_dialogAccentColor.name());

  settings.setValue("Design/GraphBgColor", s_graphBgColor.name());

  for (std::size_t i = 0; i < s_spectrumColors.size(); ++i) {
    settings.setValue(QString("Design/SpectrumColor_%1").arg(i), s_spectrumColors[i].name());
  }
  if (!s_spectrumColors.empty()) {
    settings.setValue("Design/SpectrumColor", s_spectrumColors[0].name());
  }

  settings.setValue("Design/PeakMarkerColor", s_peakMarkerColor.name());
  settings.setValue("Design/ZoomMarkerColor", s_zoomMarkerColor.name());
  settings.setValue("Design/BgMarkerColor", s_bgMarkerColor.name());
  settings.setValue("Design/IntegralMarkerColor", s_integralMarkerColor.name());
  settings.setValue("Design/RangeMarkerColor", s_rangeMarkerColor.name());
  settings.setValue("Design/GaussMarkerColor", s_gaussMarkerColor.name());
  settings.setValue("Design/GateMarkerColor", s_gateMarkerColor.name());
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
    s_rootFontFamilyIndex = settings.value("Design/RootFontFamilyIndex", 13).toInt();
  }

  if (settings.contains("Design/ButtonBgColor")) s_buttonBgColor = QColor(settings.value("Design/ButtonBgColor").toString());
  if (settings.contains("Design/ButtonTextColor")) s_buttonTextColor = QColor(settings.value("Design/ButtonTextColor").toString());
  if (settings.contains("Design/UIBgColor")) s_uiBgColor = QColor(settings.value("Design/UIBgColor").toString());
  if (settings.contains("Design/PromptBgColor")) s_promptBgColor = QColor(settings.value("Design/PromptBgColor").toString());
  if (settings.contains("Design/PromptTextColor")) s_promptTextColor = QColor(settings.value("Design/PromptTextColor").toString());

  if (settings.contains("Design/DialogBgColor")) s_dialogBgColor = QColor(settings.value("Design/DialogBgColor").toString());
  if (settings.contains("Design/DialogTextColor")) s_dialogTextColor = QColor(settings.value("Design/DialogTextColor").toString());
  if (settings.contains("Design/DialogAccentColor")) s_dialogAccentColor = QColor(settings.value("Design/DialogAccentColor").toString());

  if (settings.contains("Design/GraphBgColor")) s_graphBgColor = QColor(settings.value("Design/GraphBgColor").toString());

  for (std::size_t i = 0; i < s_spectrumColors.size(); ++i) {
    const QString key = QString("Design/SpectrumColor_%1").arg(i);
    if (settings.contains(key)) {
      s_spectrumColors[i] = QColor(settings.value(key).toString());
    } else if (i == 0 && settings.contains("Design/SpectrumColor")) {
      s_spectrumColors[0] = QColor(settings.value("Design/SpectrumColor").toString());
    }
  }

  if (settings.contains("Design/PeakMarkerColor")) s_peakMarkerColor = QColor(settings.value("Design/PeakMarkerColor").toString());
  if (settings.contains("Design/ZoomMarkerColor")) s_zoomMarkerColor = QColor(settings.value("Design/ZoomMarkerColor").toString());
  if (settings.contains("Design/BgMarkerColor")) s_bgMarkerColor = QColor(settings.value("Design/BgMarkerColor").toString());
  if (settings.contains("Design/IntegralMarkerColor")) s_integralMarkerColor = QColor(settings.value("Design/IntegralMarkerColor").toString());
  if (settings.contains("Design/RangeMarkerColor")) s_rangeMarkerColor = QColor(settings.value("Design/RangeMarkerColor").toString());
  if (settings.contains("Design/GaussMarkerColor")) s_gaussMarkerColor = QColor(settings.value("Design/GaussMarkerColor").toString());
  if (settings.contains("Design/GateMarkerColor")) s_gateMarkerColor = QColor(settings.value("Design/GateMarkerColor").toString());
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

QColor getUIBackgroundColor() { if (!s_typographyInitialized) initializeTypography(); return s_uiBgColor; }
void setUIBackgroundColor(const QColor &color) { s_uiBgColor = color; saveSettings(); }

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

QString getStatusLabelStyleSheet() {
  if (!s_typographyInitialized) initializeTypography();
  const QString bg = s_promptBgColor.name();
  const QString fg = s_promptTextColor.name();
  const QString border = s_buttonBgColor.darker(135).name();
  return QString(
      "QLabel {"
      "  background-color: %1;"
      "  color: %2;"
      "  border: 1px solid %3;"
      "  padding: 2px 6px;"
      "}"
  ).arg(bg, fg, border);
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
      "QScrollArea { background-color: %1; border: none; }\n"
      "QScrollArea > QWidget { background-color: %1; border: none; }\n"
      "QScrollArea > QWidget > QWidget { background-color: %1; border: none; }\n"
      "QWidget#tabCanvas { background-color: %1; }\n"
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

std::vector<QColor> getSpectrumColors() {
  if (!s_typographyInitialized) initializeTypography();
  return s_spectrumColors;
}

QColor getSpectrumColor() {
  return getSpectrumColor(0);
}

QColor getSpectrumColor(int index) {
  if (!s_typographyInitialized) initializeTypography();
  if (index >= 0 && index < static_cast<int>(s_spectrumColors.size())) {
    return s_spectrumColors[index];
  }
  return s_spectrumColors.empty() ? QColor("#ffffff") : s_spectrumColors[0];
}

void setSpectrumColor(const QColor &color) {
  setSpectrumColor(0, color);
}

void setSpectrumColor(int index, const QColor &color) {
  if (!s_typographyInitialized) initializeTypography();
  if (index >= 0 && index < static_cast<int>(s_spectrumColors.size())) {
    s_spectrumColors[index] = color;
    saveSettings();
  }
}

void setSpectrumColors(const std::vector<QColor> &colors) {
  if (!s_typographyInitialized) initializeTypography();
  for (std::size_t i = 0; i < colors.size() && i < s_spectrumColors.size(); ++i) {
    s_spectrumColors[i] = colors[i];
  }
  saveSettings();
}

QColor getPeakMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_peakMarkerColor; }
void setPeakMarkerColor(const QColor &color) { s_peakMarkerColor = color; saveSettings(); }

QColor getZoomMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_zoomMarkerColor; }
void setZoomMarkerColor(const QColor &color) { s_zoomMarkerColor = color; saveSettings(); }

QColor getBackgroundMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_bgMarkerColor; }
void setBackgroundMarkerColor(const QColor &color) { s_bgMarkerColor = color; saveSettings(); }

QColor getIntegralMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_integralMarkerColor; }
void setIntegralMarkerColor(const QColor &color) { s_integralMarkerColor = color; saveSettings(); }

QColor getRangeMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_rangeMarkerColor; }
void setRangeMarkerColor(const QColor &color) { s_rangeMarkerColor = color; saveSettings(); }

QColor getGaussMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_gaussMarkerColor; }
void setGaussMarkerColor(const QColor &color) { s_gaussMarkerColor = color; saveSettings(); }

QColor getGateMarkerColor() { if (!s_typographyInitialized) initializeTypography(); return s_gateMarkerColor; }
void setGateMarkerColor(const QColor &color) { s_gateMarkerColor = color; saveSettings(); }

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
  // 1. Buttons, Status Readouts & Console Prompt
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

    // Update status readout fields (Xmin, Xmax, Ymin, Ymax, Ch, En, Cts, etc.)
    const QString labelStyle = getStatusLabelStyleSheet();
    const QList<QLabel*> labels = mainCanvas->findChildren<QLabel*>();
    for (QLabel *lbl : labels) {
      if (!lbl) continue;
      if (lbl->window() != mainCanvas) continue;
      bool insideDialog = false;
      for (QWidget *w = lbl->parentWidget(); w && w != mainCanvas; w = w->parentWidget()) {
        if (qobject_cast<QDialog*>(w)) {
          insideDialog = true;
          break;
        }
      }
      if (insideDialog) continue;

      lbl->setFont(btnFont);
      lbl->setStyleSheet(labelStyle);
    }

    // Set matching background color on toolbar container and mainCanvas
    QWidget *topContainer = mainCanvas->findChild<QWidget*>("topContainer");
    if (topContainer) {
      topContainer->setStyleSheet(QString("QWidget#topContainer { background-color: %1; }").arg(s_uiBgColor.name()));
    }
    mainCanvas->setStyleSheet(QString("QMainCanvas { background-color: %1; }").arg(s_uiBgColor.name()));
  }

  if (CommandPrompt::getInstance()) {
    CommandPrompt::getInstance()->setFont(getPromptFont());
    CommandPrompt::getInstance()->setStyleSheet(getPromptStyleSheet());
  }

  // 2. Global application font and stylesheets for dialogs (including any already open)
  if (qApp) {
    const QFont dlgFont = getDialogFont();
    const QString dlgSheet = getDialogStyleSheet();
    qApp->setFont(dlgFont);
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      if (QDialog *dlg = qobject_cast<QDialog*>(widget)) {
        dlg->setFont(dlgFont);
        dlg->setStyleSheet(dlgSheet);
        dlg->update();
      }
    }
  }

  // 3. Graph canvas & spectrum colors
  applyGraphTypography();

  if (mainCanvas) {
    const Color_t rootBg = TColor::GetColor(s_graphBgColor.name().toUtf8().constData());
    const Color_t axisCol = (s_graphBgColor.lightness() > 130) ? kBlack : kWhite;

    gStyle->SetCanvasColor(rootBg);
    gStyle->SetPadColor(rootBg);
    gStyle->SetFrameFillColor(rootBg);
    gStyle->SetFrameLineColor(axisCol);
    gStyle->SetAxisColor(axisCol, "X");
    gStyle->SetAxisColor(axisCol, "Y");
    gStyle->SetAxisColor(axisCol, "Z");
    gStyle->SetLabelColor(axisCol, "X");
    gStyle->SetLabelColor(axisCol, "Y");
    gStyle->SetLabelColor(axisCol, "Z");

    if (mainCanvas->getRootCanvas()) {
      TCanvas *tc = mainCanvas->getRootCanvas()->getCanvas();
      if (tc) {
        tc->SetFillColor(rootBg);
        if (tc->GetFrame()) {
          tc->GetFrame()->SetFillColor(rootBg);
          tc->GetFrame()->SetLineColor(axisCol);
        }
        if (tc->GetListOfPrimitives()) {
          TIter next(tc->GetListOfPrimitives());
          TObject *obj = nullptr;
          while ((obj = next())) {
            if (obj && obj->InheritsFrom(TPad::Class())) {
              TPad *pad = static_cast<TPad*>(obj);
              pad->SetFillColor(rootBg);
              pad->SetFrameFillColor(rootBg);
              if (pad->GetFrame()) {
                pad->GetFrame()->SetFillColor(rootBg);
                pad->GetFrame()->SetLineColor(axisCol);
              }
            }
          }
        }
        tc->Modified();
        tc->Update();
      }
      mainCanvas->getRootCanvas()->update();
    }

    // Synchronize all 9 spectrum colors with mainCanvas->colors_hist
    if (mainCanvas->colors_hist.size() < s_spectrumColors.size()) {
      mainCanvas->colors_hist.resize(s_spectrumColors.size());
    }
    for (std::size_t i = 0; i < s_spectrumColors.size(); ++i) {
      mainCanvas->colors_hist[i] = TColor::GetColor(s_spectrumColors[i].name().toUtf8().constData());
    }

    if (mainCanvas->selectedHisto && !mainCanvas->colors_hist.empty()) {
      mainCanvas->selectedHisto->SetLineColor(mainCanvas->colors_hist[0]);
    }

    for (int z = 0; z < 12; ++z) {
      for (int g = 0; g < 12; ++g) {
        if (mainCanvas->HijF[z][g] && !mainCanvas->colors_hist.empty()) {
          mainCanvas->HijF[z][g]->SetLineColor(mainCanvas->colors_hist[0]);
        }
        for (std::size_t k = 0; k < mainCanvas->HijC[z][g].size(); ++k) {
          if (mainCanvas->HijC[z][g][k]) {
            mainCanvas->HijC[z][g][k]->SetLineColor(mainCanvas->colors_hist[k % mainCanvas->colors_hist.size()]);
          }
        }
      }
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
    Color_t rootBg = TColor::GetColor(Design::getGraphBackgroundColor().name().toUtf8().constData());
    canvas->SetFillColor(rootBg);
    if (gPad) {
      gPad->SetFillColor(rootBg);
      gPad->SetFrameFillColor(rootBg);
    }
    canvas->Modified();
    canvas->Update();
  }
}

//==============================================================================
// CanvasPreviewWidget (Live Simulation of ROOT Canvas, Spectra & Markers)
//==============================================================================
class CanvasPreviewWidget : public QWidget {
public:
  explicit CanvasPreviewWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void setPreviewData(const QColor &bg,
                      const std::vector<QColor> &spectra,
                      const QColor &peak,
                      const QColor &zoom,
                      const QColor &integral,
                      const QColor &gauss,
                      const QColor &gate) {
    m_bg = bg;
    m_spectra = spectra;
    m_peak = peak;
    m_zoom = zoom;
    m_integral = integral;
    m_gauss = gauss;
    m_gate = gate;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    p.fillRect(r, m_bg);

    // Subtle border around canvas
    p.setPen(QPen(m_bg.lighter(130), 1));
    p.drawRect(r.adjusted(0, 0, -1, -1));

    const int w = r.width();
    const int h = r.height();
    const int baseline = h - 24;
    const int top = 22;

    // Coordinate axes
    p.setPen(QPen(QColor(128, 128, 128, 120), 1));
    p.drawLine(35, baseline, w - 15, baseline);
    p.drawLine(35, top, 35, baseline);

    // Shaded ROI / Integral region
    const int xRoi1 = static_cast<int>(w * 0.38);
    const int xRoi2 = static_cast<int>(w * 0.60);
    QColor shadeCol = m_integral;
    shadeCol.setAlpha(45);
    p.fillRect(QRect(xRoi1, top, xRoi2 - xRoi1, baseline - top), shadeCol);

    // Vertical ROI marker lines
    p.setPen(QPen(m_zoom, 1, Qt::DashLine));
    p.drawLine(xRoi1, top, xRoi1, baseline);
    p.drawLine(xRoi2, top, xRoi2, baseline);

    // Gate marker line
    const int xGate = static_cast<int>(w * 0.78);
    p.setPen(QPen(m_gate, 2, Qt::SolidLine));
    p.drawLine(xGate, top, xGate, baseline);

    // Gauss centroid marker line
    const int xPeak = static_cast<int>(w * 0.50);
    p.setPen(QPen(m_gauss, 1, Qt::DotLine));
    p.drawLine(xPeak, top, xPeak, baseline);

    // Primary Spectrum (Spectrum 1)
    if (!m_spectra.empty()) {
      QPainterPath path1;
      path1.moveTo(35, baseline - 4);
      for (int x = 35; x <= w - 15; ++x) {
        double normX = (x - 35.0) / (w - 50.0);
        double bgLevel = 6.0 + 4.0 * (1.0 - normX);
        double peak1 = 70.0 * std::exp(-0.5 * std::pow((x - xPeak) / 16.0, 2));
        double peak2 = 28.0 * std::exp(-0.5 * std::pow((x - (w * 0.24)) / 11.0, 2));
        double yVal = baseline - (bgLevel + peak1 + peak2);
        path1.lineTo(x, yVal);
      }
      p.setPen(QPen(m_spectra[0], 2));
      p.drawPath(path1);
    }

    // Secondary Spectrum Overlay (Spectrum 2)
    if (m_spectra.size() > 1) {
      QPainterPath path2;
      path2.moveTo(35, baseline - 3);
      for (int x = 35; x <= w - 15; ++x) {
        double normX = (x - 35.0) / (w - 50.0);
        double bgLevel = 4.0 + 3.0 * (1.0 - normX);
        double peak = 45.0 * std::exp(-0.5 * std::pow((x - (xPeak + 26)) / 14.0, 2));
        double yVal = baseline - (bgLevel + peak);
        path2.lineTo(x, yVal);
      }
      p.setPen(QPen(m_spectra[1], 1, Qt::DashLine));
      p.drawPath(path2);
    }

    // Peak Marker (Arrow / indicator on top of main peak)
    p.setPen(QPen(m_peak, 2));
    p.setBrush(m_peak);
    QPolygon triangle;
    triangle << QPoint(xPeak, top + 6) << QPoint(xPeak - 5, top - 2) << QPoint(xPeak + 5, top - 2);
    p.drawPolygon(triangle);
    p.drawLine(xPeak, top + 6, xPeak, top + 18);

    // Legend
    QFont legFont("sans-serif", 8);
    p.setFont(legFont);
    p.setPen(m_bg.lightness() > 130 ? Qt::black : Qt::white);
    p.drawText(40, 15, "■ Spec 1");
    if (m_spectra.size() > 1) {
      p.setPen(m_spectra[1]);
      p.drawText(100, 15, "- - Spec 2");
    }
    p.setPen(m_peak);
    p.drawText(168, 15, "▼ Peak");
    p.setPen(m_zoom);
    p.drawText(220, 15, "| ROI");
    p.setPen(m_gauss);
    p.drawText(265, 15, ": Gauss");
    p.setPen(m_gate);
    p.drawText(320, 15, "| Gate");
  }

private:
  QColor m_bg{"#000000"};
  std::vector<QColor> m_spectra;
  QColor m_peak{"#ffff00"};
  QColor m_zoom{"#00ffff"};
  QColor m_integral{"#ffff00"};
  QColor m_gauss{"#ffff00"};
  QColor m_gate{"#ff0000"};
};

//==============================================================================
// AppearanceDialog (Full Settings Dialog for Fonts, Sizes, and Colors)
//==============================================================================
class AppearanceDialog : public QDialog {
public:
  AppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget);

private:
  void setupUI();
  void addColorRow(QGridLayout *grid, int row, int colOffset, const QString &label, QColor *colorVar, std::function<void()> onChange);
  void updateBtnPreview();
  void updateDlgPreview();
  void updateGraphPreview();
  void updateCanvasPreview();
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
  QColor m_origUIBg;
  QColor m_origPromptBg;
  QColor m_origPromptFg;

  QColor m_origDlgBg;
  QColor m_origDlgFg;
  QColor m_origDlgAccent;

  QColor m_origGraphBg;
  std::vector<QColor> m_origSpectrumColors;
  QColor m_origPeak;
  QColor m_origZoom;
  QColor m_origBgMarker;
  QColor m_origIntegral;
  QColor m_origRange;
  QColor m_origGauss;
  QColor m_origGate;

  // Working values
  QFont m_curBtnFont;
  QFont m_curDialogFont;
  QFont m_curGraphFont;
  int m_curRootFontIdx;

  QColor m_curBtnBg;
  QColor m_curBtnFg;
  QColor m_curUIBg;
  QColor m_curPromptBg;
  QColor m_curPromptFg;

  QColor m_curDlgBg;
  QColor m_curDlgFg;
  QColor m_curDlgAccent;

  QColor m_curGraphBg;
  std::vector<QColor> m_curSpectrumColors;
  QColor m_curPeak;
  QColor m_curZoom;
  QColor m_curBgMarker;
  QColor m_curIntegral;
  QColor m_curRange;
  QColor m_curGauss;
  QColor m_curGate;

  // UI elements for live updates
  QComboBox *m_presetCombo{nullptr};

  // Tab 1 UI
  QLabel *m_lblBtnFontDesc{nullptr};
  QSpinBox *m_spinBtnSize{nullptr};
  QFrame *m_mockToolbarBox{nullptr};
  QPushButton *m_sampleBtn{nullptr};
  QLineEdit *m_samplePrompt{nullptr};

  // Tab 2 UI
  QLabel *m_lblDlgFontDesc{nullptr};
  QSpinBox *m_spinDlgSize{nullptr};
  QFrame *m_mockDialogBox{nullptr};
  QLabel *m_mockLabel{nullptr};
  QPushButton *m_mockButton{nullptr};

  // Tab 3 UI
  QLabel *m_lblGraphFontDesc{nullptr};
  QSpinBox *m_spinGraphSize{nullptr};
  QComboBox *m_comboRootFont{nullptr};
  QFrame *m_mockGraphBox{nullptr};
  QLabel *m_mockGraphSampleLabel{nullptr};

  // Tab 4 UI
  CanvasPreviewWidget *m_canvasPreview{nullptr};

  // Bottom action buttons
  QPushButton *m_btnReset{nullptr};
  QPushButton *m_btnApply{nullptr};
  QPushButton *m_btnOk{nullptr};
  QPushButton *m_btnCancel{nullptr};

  QList<std::function<void()>> m_swatchUpdaters;
};

AppearanceDialog::AppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget)
    : QDialog(parent), m_canvasWidget(canvasWidget)
{
  setWindowTitle("Appearance & Typography Settings (Fonts & Colors)");
  resize(720, 660);

  // Snapshot original settings to allow clean Cancel / revert
  m_origBtnFont = Design::getButtonPromptFont();
  m_origDialogFont = Design::getDialogFont();
  m_origGraphFont = Design::getGraphFont();
  m_origRootFontIdx = Design::getRootFontFamilyIndex();

  m_origBtnBg = Design::getButtonBackgroundColor();
  m_origBtnFg = Design::getButtonTextColor();
  m_origUIBg = Design::getUIBackgroundColor();
  m_origPromptBg = Design::getPromptBackgroundColor();
  m_origPromptFg = Design::getPromptTextColor();

  m_origDlgBg = Design::getDialogBackgroundColor();
  m_origDlgFg = Design::getDialogTextColor();
  m_origDlgAccent = Design::getDialogAccentColor();

  m_origGraphBg = Design::getGraphBackgroundColor();
  m_origSpectrumColors = Design::getSpectrumColors();
  m_origPeak = Design::getPeakMarkerColor();
  m_origZoom = Design::getZoomMarkerColor();
  m_origBgMarker = Design::getBackgroundMarkerColor();
  m_origIntegral = Design::getIntegralMarkerColor();
  m_origRange = Design::getRangeMarkerColor();
  m_origGauss = Design::getGaussMarkerColor();
  m_origGate = Design::getGateMarkerColor();

  // Working copies
  m_curBtnFont = m_origBtnFont;
  m_curDialogFont = m_origDialogFont;
  m_curGraphFont = m_origGraphFont;
  m_curRootFontIdx = m_origRootFontIdx;

  m_curBtnBg = m_origBtnBg;
  m_curBtnFg = m_origBtnFg;
  m_curUIBg = m_origUIBg;
  m_curPromptBg = m_origPromptBg;
  m_curPromptFg = m_origPromptFg;

  m_curDlgBg = m_origDlgBg;
  m_curDlgFg = m_origDlgFg;
  m_curDlgAccent = m_origDlgAccent;

  m_curGraphBg = m_origGraphBg;
  m_curSpectrumColors = m_origSpectrumColors;
  while (m_curSpectrumColors.size() < 9) {
    m_curSpectrumColors.push_back(QColor("#ffffff"));
  }

  m_curPeak = m_origPeak;
  m_curZoom = m_origZoom;
  m_curBgMarker = m_origBgMarker;
  m_curIntegral = m_origIntegral;
  m_curRange = m_origRange;
  m_curGauss = m_origGauss;
  m_curGate = m_origGate;

  setupUI();
  refreshDialogTheme();
}

void AppearanceDialog::addColorRow(QGridLayout *grid, int row, int colOffset, const QString &label, QColor *colorVar, std::function<void()> onChange) {
  QLabel *lbl = new QLabel(label, this);
  grid->addWidget(lbl, row, colOffset);

  QPushButton *swatch = new QPushButton(this);
  swatch->setFixedSize(86, 28);
  swatch->setCursor(Qt::PointingHandCursor);
  swatch->setToolTip(QString("Click to choose %1").arg(label));

  auto updateSwatch = [swatch, colorVar]() {
    const QString hex = colorVar->name().toUpper();
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

  grid->addWidget(swatch, row, colOffset + 1, Qt::AlignRight);
}

void AppearanceDialog::setupUI() {
  QVBoxLayout *dialogLayout = new QVBoxLayout(this);
  dialogLayout->setSpacing(10);
  dialogLayout->setContentsMargins(14, 14, 14, 14);

  // Top header with quick theme presets dropdown (Classic Xtrackn & Modern)
  QHBoxLayout *presetLayout = new QHBoxLayout();
  QLabel *lblPreset = new QLabel("<b>Theme Preset:</b>", this);
  m_presetCombo = new QComboBox(this);
  m_presetCombo->addItem("(Custom / Keep Current)");
  m_presetCombo->addItem("Classic (Legacy Xtrackn)");
  m_presetCombo->addItem("Modern (Deep Space Cyan)");
  presetLayout->addWidget(lblPreset);
  presetLayout->addWidget(m_presetCombo, 1);
  dialogLayout->addLayout(presetLayout);

  // 4-Category Tabs
  QTabWidget *tabs = new QTabWidget(this);

  // =========================================================================
  // TAB 1: Buttons & UI
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
  addColorRow(gridBtnColors, 0, 0, "Main UI Background (Button Panel):", &m_curUIBg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 1, 0, "Button Background:", &m_curBtnBg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 2, 0, "Button Text Color:", &m_curBtnFg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 3, 0, "Readouts & Console Background:", &m_curPromptBg, [this]() { updateBtnPreview(); });
  addColorRow(gridBtnColors, 4, 0, "Readouts & Console Text:", &m_curPromptFg, [this]() { updateBtnPreview(); });
  tabBtnLayout->addWidget(grpBtnColors);

  QGroupBox *grpBtnPreview = new QGroupBox("Live Preview (Main UI Toolbar)", tabBtn);
  QVBoxLayout *vboxBtnPreview = new QVBoxLayout(grpBtnPreview);
  m_mockToolbarBox = new QFrame(grpBtnPreview);
  QVBoxLayout *mockToolbarLayout = new QVBoxLayout(m_mockToolbarBox);
  m_sampleBtn = new QPushButton("EnCal", m_mockToolbarBox);
  m_sampleBtn->setFixedHeight(34);
  m_samplePrompt = new QLineEdit("X Min: 0.0   |   NuTrackN Output: Ready", m_mockToolbarBox);
  m_samplePrompt->setReadOnly(true);
  m_samplePrompt->setFixedHeight(34);
  mockToolbarLayout->addWidget(m_sampleBtn);
  mockToolbarLayout->addWidget(m_samplePrompt);
  vboxBtnPreview->addWidget(m_mockToolbarBox);
  tabBtnLayout->addWidget(grpBtnPreview);
  tabBtnLayout->addStretch();

  tabs->addTab(tabBtn, "1. Buttons && UI");

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
  addColorRow(gridDlgColors, 0, 0, "Dialog Background:", &m_curDlgBg, [this]() {
    updateDlgPreview();
    refreshDialogTheme();
  });
  addColorRow(gridDlgColors, 1, 0, "Dialog Text & Labels:", &m_curDlgFg, [this]() {
    updateDlgPreview();
    refreshDialogTheme();
  });
  addColorRow(gridDlgColors, 2, 0, "Highlight / Accent:", &m_curDlgAccent, [this]() {
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

  tabs->addTab(tabDlg, "2. Dialogs");

  // =========================================================================
  // TAB 3: Graph Typography
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
  m_comboRootFont->addItem("Times (Serif - ROOT Font 13, Legacy Xtrackn)", 13);
  m_comboRootFont->addItem("Helvetica (Sans-Serif - Standard ROOT Font 4)", 4);
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

  QGroupBox *grpGraphPreview = new QGroupBox("Live Preview (Graph Overlay Typography)", tabGraph);
  QVBoxLayout *vboxGraphPreview = new QVBoxLayout(grpGraphPreview);
  m_mockGraphBox = new QFrame(grpGraphPreview);
  m_mockGraphBox->setMinimumHeight(80);
  QHBoxLayout *mockGraphLayout = new QHBoxLayout(m_mockGraphBox);
  m_mockGraphSampleLabel = new QLabel("Channel: 1332.5 keV | Peak Counts: 24,510", m_mockGraphBox);
  m_mockGraphSampleLabel->setAlignment(Qt::AlignCenter);
  mockGraphLayout->addWidget(m_mockGraphSampleLabel);
  vboxGraphPreview->addWidget(m_mockGraphBox);
  tabGraphLayout->addWidget(grpGraphPreview);
  tabGraphLayout->addStretch();

  tabs->addTab(tabGraph, "3. Graph Typography");

  // =========================================================================
  // TAB 4: Canvas, Spectra & Markers
  // =========================================================================
  QWidget *tabCanvas = new QWidget();
  QVBoxLayout *tabCanvasLayout = new QVBoxLayout(tabCanvas);
  tabCanvasLayout->setSpacing(8);
  tabCanvasLayout->setContentsMargins(10, 10, 10, 10);

  // Group 1: Canvas Background
  QGroupBox *grpCanvasBg = new QGroupBox("Canvas Background", tabCanvas);
  QGridLayout *gridCanvasBg = new QGridLayout(grpCanvasBg);
  gridCanvasBg->setContentsMargins(10, 6, 10, 6);
  gridCanvasBg->setColumnStretch(0, 1);
  gridCanvasBg->setColumnStretch(1, 0);
  addColorRow(gridCanvasBg, 0, 0, "Canvas Background Color:", &m_curGraphBg, [this]() { updateCanvasPreview(); });
  tabCanvasLayout->addWidget(grpCanvasBg);

  // Group 2: Spectrum Colors (All 9 Options) - 3 Columns
  QGroupBox *grpSpectra = new QGroupBox("Spectrum Colors (Channels 1 - 9 / Overlays)", tabCanvas);
  QGridLayout *gridSpectra = new QGridLayout(grpSpectra);
  gridSpectra->setContentsMargins(10, 6, 10, 6);
  gridSpectra->setHorizontalSpacing(14);
  gridSpectra->setVerticalSpacing(4);
  for (int c = 0; c < 6; c += 2) {
    gridSpectra->setColumnStretch(c, 1);
    gridSpectra->setColumnStretch(c + 1, 0);
  }

  const QString specNames[9] = {
      "Spectrum 1 (Primary):",
      "Spectrum 2 (Overlay 1):",
      "Spectrum 3 (Overlay 2):",
      "Spectrum 4 (Overlay 3):",
      "Spectrum 5 (Overlay 4):",
      "Spectrum 6 (Overlay 5):",
      "Spectrum 7 (Overlay 6):",
      "Spectrum 8 (Overlay 7):",
      "Spectrum 9 (Overlay 8):"
  };

  for (int i = 0; i < 9; ++i) {
    int row = i % 3;
    int col = (i / 3) * 2;
    addColorRow(gridSpectra, row, col, specNames[i], &m_curSpectrumColors[i], [this]() { updateCanvasPreview(); });
  }
  tabCanvasLayout->addWidget(grpSpectra);

  // Group 3: Marker Colors - 3 Columns
  QGroupBox *grpMarkers = new QGroupBox("Marker Colors", tabCanvas);
  QGridLayout *gridMarkers = new QGridLayout(grpMarkers);
  gridMarkers->setContentsMargins(10, 6, 10, 6);
  gridMarkers->setHorizontalSpacing(14);
  gridMarkers->setVerticalSpacing(4);
  for (int c = 0; c < 6; c += 2) {
    gridMarkers->setColumnStretch(c, 1);
    gridMarkers->setColumnStretch(c + 1, 0);
  }

  addColorRow(gridMarkers, 0, 0, "Peak Search:", &m_curPeak, [this]() { updateCanvasPreview(); });
  addColorRow(gridMarkers, 1, 0, "Zoom / ROI:", &m_curZoom, [this]() { updateCanvasPreview(); });
  addColorRow(gridMarkers, 2, 0, "Background:", &m_curBgMarker, [this]() { updateCanvasPreview(); });

  addColorRow(gridMarkers, 0, 2, "Integral ROI:", &m_curIntegral, [this]() { updateCanvasPreview(); });
  addColorRow(gridMarkers, 1, 2, "Range Marker:", &m_curRange, [this]() { updateCanvasPreview(); });
  addColorRow(gridMarkers, 2, 2, "Gauss Centroid:", &m_curGauss, [this]() { updateCanvasPreview(); });

  addColorRow(gridMarkers, 0, 4, "Gate Marker:", &m_curGate, [this]() { updateCanvasPreview(); });
  tabCanvasLayout->addWidget(grpMarkers);

  // Group 4: Live Canvas Preview
  QGroupBox *grpLiveCanvas = new QGroupBox("Live Canvas Preview", tabCanvas);
  QVBoxLayout *vboxLiveCanvas = new QVBoxLayout(grpLiveCanvas);
  vboxLiveCanvas->setContentsMargins(10, 6, 10, 6);
  m_canvasPreview = new CanvasPreviewWidget(grpLiveCanvas);
  vboxLiveCanvas->addWidget(m_canvasPreview);
  tabCanvasLayout->addWidget(grpLiveCanvas);

  tabCanvasLayout->addStretch();

  tabs->addTab(tabCanvas, "4. Canvas, Spectra && Markers");

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
      // 1. Classic (Authentic Legacy Xtrackn)
      QFont timesFont("Times New Roman", 11);
      timesFont.setStyleHint(QFont::Times);
      m_curBtnFont = timesFont;
      m_curBtnFont.setBold(true);
      m_curDialogFont = timesFont;
      m_curGraphFont = timesFont;
      m_curRootFontIdx = 13; // ROOT Font 13: Times

      m_curUIBg = QColor("#708090");
      m_curBtnBg = QColor("#7790ad");
      m_curBtnFg = QColor("#00ffff");
      m_curPromptBg = QColor("#7790ad");
      m_curPromptFg = QColor("#000000");

      m_curDlgBg = QColor("#708090");
      m_curDlgFg = QColor("#000000");
      m_curDlgAccent = QColor("#2f4f4f");

      m_curGraphBg = QColor("#000000");
      m_curSpectrumColors = {
          QColor("#ffffff"), // WHITE   (glwlib.c:2577)
          QColor("#ff0000"), // RED     (glwlib.c:2578)
          QColor("#00ff00"), // GREEN   (glwlib.c:2579)
          QColor("#ffff00"), // YELLOW  (glwlib.c:2580)
          QColor("#0000ff"), // BLUE    (glwlib.c:2581)
          QColor("#00ffff"), // CYAN    (glwlib.c:2582)
          QColor("#ff00ff"), // MAGENTA (glwlib.c:2583)
          QColor("#ff9900"), // ORANGE
          QColor("#800080")  // PURPLE
      };

      m_curPeak = QColor("#ffff00");     // YELLOW (trackn.F:2166, glwlib.c:2137)
      m_curZoom = QColor("#00ffff");     // CYAN   (ROI standard)
      m_curBgMarker = QColor("#0000ff"); // BLUE   (trackn.F:628)
      m_curIntegral = QColor("#ffff00"); // YELLOW (trackn.F:644)
      m_curRange = QColor("#ff00ff");    // MAGENTA(trackn.F:640)
      m_curGauss = QColor("#ffff00");    // YELLOW (trackn.F:631)
      m_curGate = QColor("#ff0000");     // RED    (trackn.F:665)

      m_spinBtnSize->setValue(11);
      m_spinDlgSize->setValue(11);
      m_spinGraphSize->setValue(11);
      int rootIdx = m_comboRootFont->findData(13);
      if (rootIdx >= 0) m_comboRootFont->setCurrentIndex(rootIdx);
    } else if (idx == 2) {
      // 2. Modern (Deep Space Cyan)
      QFont modernFont("DejaVu Sans", 10);
      modernFont.setStyleHint(QFont::SansSerif);
      m_curBtnFont = modernFont;
      m_curBtnFont.setBold(true);
      m_curDialogFont = modernFont;
      m_curGraphFont = modernFont;
      m_curRootFontIdx = 4; // ROOT Font 4: Helvetica / Sans-Serif

      m_curUIBg = QColor("#0f172a");
      m_curBtnBg = QColor("#1e2638");
      m_curBtnFg = QColor("#e2e8f0");
      m_curPromptBg = QColor("#121824");
      m_curPromptFg = QColor("#38bdf8");

      m_curDlgBg = QColor("#151b26");
      m_curDlgFg = QColor("#e2e8f0");
      m_curDlgAccent = QColor("#00d2ff");

      m_curGraphBg = QColor("#0c1017");
      m_curSpectrumColors = {
          QColor("#00f0ff"), // Cyan
          QColor("#ff3366"), // Coral
          QColor("#10b981"), // Emerald
          QColor("#fbbf24"), // Amber
          QColor("#a855f7"), // Purple
          QColor("#38bdf8"), // Sky Blue
          QColor("#f97316"), // Orange
          QColor("#4ade80"), // Mint
          QColor("#e879f9")  // Fuchsia
      };

      m_curPeak = QColor("#ff2d75");
      m_curZoom = QColor("#00f0ff");
      m_curBgMarker = QColor("#3b82f6");
      m_curIntegral = QColor("#f59e0b");
      m_curRange = QColor("#fb923c");
      m_curGauss = QColor("#f472b6");
      m_curGate = QColor("#a855f7");

      m_spinBtnSize->setValue(10);
      m_spinDlgSize->setValue(10);
      m_spinGraphSize->setValue(10);
      int rootIdx = m_comboRootFont->findData(4);
      if (rootIdx >= 0) m_comboRootFont->setCurrentIndex(rootIdx);
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
    m_curUIBg = Design::getUIBackgroundColor();
    m_curPromptBg = Design::getPromptBackgroundColor();
    m_curPromptFg = Design::getPromptTextColor();

    m_curDlgBg = Design::getDialogBackgroundColor();
    m_curDlgFg = Design::getDialogTextColor();
    m_curDlgAccent = Design::getDialogAccentColor();

    m_curGraphBg = Design::getGraphBackgroundColor();
    m_curSpectrumColors = Design::getSpectrumColors();
    while (m_curSpectrumColors.size() < 9) {
      m_curSpectrumColors.push_back(QColor("#ffffff"));
    }

    m_curPeak = Design::getPeakMarkerColor();
    m_curZoom = Design::getZoomMarkerColor();
    m_curBgMarker = Design::getBackgroundMarkerColor();
    m_curIntegral = Design::getIntegralMarkerColor();
    m_curRange = Design::getRangeMarkerColor();
    m_curGauss = Design::getGaussMarkerColor();
    m_curGate = Design::getGateMarkerColor();

    m_spinBtnSize->setValue(m_curBtnFont.pointSize());
    m_spinDlgSize->setValue(m_curDialogFont.pointSize());
    m_spinGraphSize->setValue(m_curGraphFont.pointSize());
    int rootIdx = m_comboRootFont->findData(m_curRootFontIdx);
    if (rootIdx >= 0) m_comboRootFont->setCurrentIndex(rootIdx);
    m_presetCombo->setCurrentIndex(1);

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
  updateCanvasPreview();
}

void AppearanceDialog::updateBtnPreview() {
  if (!m_lblBtnFontDesc || !m_sampleBtn || !m_samplePrompt || !m_mockToolbarBox) return;
  m_lblBtnFontDesc->setText(QString("%1, %2pt, %3")
                               .arg(m_curBtnFont.family())
                               .arg(m_curBtnFont.pointSize() > 0 ? m_curBtnFont.pointSize() : 11)
                               .arg(m_curBtnFont.bold() ? "Bold" : "Normal"));

  m_mockToolbarBox->setStyleSheet(QString(
      "QFrame { background-color: %1; border: 1px solid #777777; border-radius: 4px; padding: 6px; }"
  ).arg(m_curUIBg.name()));

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
  if (!m_lblGraphFontDesc || !m_mockGraphBox || !m_mockGraphSampleLabel) return;
  m_lblGraphFontDesc->setText(QString("%1, %2pt")
                                .arg(m_curGraphFont.family())
                                .arg(m_curGraphFont.pointSize() > 0 ? m_curGraphFont.pointSize() : 10));
  m_mockGraphBox->setStyleSheet(QString(
      "QFrame { background-color: %1; border: 1px solid #666666; border-radius: 4px; }"
  ).arg(m_curGraphBg.name()));

  m_mockGraphSampleLabel->setFont(m_curGraphFont);
  m_mockGraphSampleLabel->setStyleSheet(QString("color: %1; border: none;").arg(
      m_curSpectrumColors.empty() ? "#ffffff" : m_curSpectrumColors[0].name()));
}

void AppearanceDialog::updateCanvasPreview() {
  if (m_canvasPreview) {
    m_canvasPreview->setPreviewData(m_curGraphBg,
                                   m_curSpectrumColors,
                                   m_curPeak,
                                   m_curZoom,
                                   m_curIntegral,
                                   m_curGauss,
                                   m_curGate);
  }
  updateGraphPreview();
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
      "QScrollArea { background-color: %1; border: none; }\n"
      "QScrollArea > QWidget { background-color: %1; border: none; }\n"
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
  updateCanvasPreview();
}

void AppearanceDialog::commitChanges() {
  Design::setButtonPromptFont(m_curBtnFont);
  Design::setButtonBackgroundColor(m_curBtnBg);
  Design::setButtonTextColor(m_curBtnFg);
  Design::setUIBackgroundColor(m_curUIBg);
  Design::setPromptBackgroundColor(m_curPromptBg);
  Design::setPromptTextColor(m_curPromptFg);

  Design::setDialogFont(m_curDialogFont);
  Design::setDialogBackgroundColor(m_curDlgBg);
  Design::setDialogTextColor(m_curDlgFg);
  Design::setDialogAccentColor(m_curDlgAccent);

  Design::setGraphFont(m_curGraphFont, m_curRootFontIdx);
  Design::setGraphBackgroundColor(m_curGraphBg);
  Design::setSpectrumColors(m_curSpectrumColors);

  Design::setPeakMarkerColor(m_curPeak);
  Design::setZoomMarkerColor(m_curZoom);
  Design::setBackgroundMarkerColor(m_curBgMarker);
  Design::setIntegralMarkerColor(m_curIntegral);
  Design::setRangeMarkerColor(m_curRange);
  Design::setGaussMarkerColor(m_curGauss);
  Design::setGateMarkerColor(m_curGate);

  Design::saveSettings();
  Design::applyUITheme(m_canvasWidget);

  refreshDialogTheme();
}

void AppearanceDialog::revertChanges() {
  Design::setButtonPromptFont(m_origBtnFont);
  Design::setButtonBackgroundColor(m_origBtnBg);
  Design::setButtonTextColor(m_origBtnFg);
  Design::setUIBackgroundColor(m_origUIBg);
  Design::setPromptBackgroundColor(m_origPromptBg);
  Design::setPromptTextColor(m_origPromptFg);

  Design::setDialogFont(m_origDialogFont);
  Design::setDialogBackgroundColor(m_origDlgBg);
  Design::setDialogTextColor(m_origDlgFg);
  Design::setDialogAccentColor(m_origDlgAccent);

  Design::setGraphFont(m_origGraphFont, m_origRootFontIdx);
  Design::setGraphBackgroundColor(m_origGraphBg);
  Design::setSpectrumColors(m_origSpectrumColors);

  Design::setPeakMarkerColor(m_origPeak);
  Design::setZoomMarkerColor(m_origZoom);
  Design::setBackgroundMarkerColor(m_origBgMarker);
  Design::setIntegralMarkerColor(m_origIntegral);
  Design::setRangeMarkerColor(m_origRange);
  Design::setGaussMarkerColor(m_origGauss);
  Design::setGateMarkerColor(m_origGate);

  Design::saveSettings();
  Design::applyUITheme(m_canvasWidget);

  m_curDialogFont = m_origDialogFont;
  m_curDlgBg = m_origDlgBg;
  m_curDlgFg = m_origDlgFg;
  m_curDlgAccent = m_origDlgAccent;
  refreshDialogTheme();
}

namespace Design {
QDialog* createAppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  return new AppearanceDialog(parent, canvasWidget);
}
}

QDialog* createAppearanceDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  return Design::createAppearanceDialog(parent, canvasWidget);
}

void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  AppearanceDialog dialog(parent, canvasWidget);
  dialog.exec();
}
