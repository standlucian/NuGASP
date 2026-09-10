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

CommandPrompt *CommandPrompt::instance = nullptr;
QMainCanvas *CommandPrompt::mainCanvas = nullptr;

CommandPrompt::CommandPrompt(QWidget *parent) : QPlainTextEdit(parent) {
  setPlaceholderText("NuTrackN Output Console...");
  setReadOnly(true);

  // Cross-platform monospace font ensuring ASCII tables and uncertainties align perfectly
  QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  monoFont.setPointSize(9);
  setFont(monoFont);

  // Cap line buffer to prevent memory exhaustion during continuous data acquisition
  setMaximumBlockCount(10000);

  // Professional dark terminal theme
  setStyleSheet("QPlainTextEdit {"
                "  background-color: #1e1e1e;"
                "  color: #d4d4d4;"
                "  border: 1px solid #3c3c3c;"
                "  selection-background-color: #264f78;"
                "  selection-color: #ffffff;"
                "}");
}

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

void addCommandPrompt(QMainCanvas *m) {
  if (!m) {
    return;
  }

  CommandPrompt::setMainCanvas(m);
  CommandPrompt *prompt = CommandPrompt::getInstance();

  // Proportional height with minimum constraints to allow flexible window resizing
  const int canvasHeight = m->height();
  const int initialHeight =
      (canvasHeight > 0) ? static_cast<int>(canvasHeight * 0.35) : 220;
  prompt->setMinimumHeight(120);
  prompt->resize(prompt->width(), initialHeight);

  QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m->layout());
  if (layout) {
    layout->addWidget(prompt);
    layout->update();
  }
}

void changeBackgroundColor(TCanvas *canvas) {
  if (canvas) {
    canvas->SetFillColor(TColor::GetColor("#1e1e1e"));
    canvas->Modified();
    canvas->Update();
  }
}

void openColorSelectionDialog(QWidget *parent, QMainCanvas *canvasWidget) {
  QDialog dialog(parent);
  dialog.setWindowTitle("Color & Theme Settings");
  dialog.setStyleSheet("background-color: #2b2b2b; color: #ffffff;");
  QFormLayout form(&dialog);

  QComboBox *themeBox = new QComboBox(&dialog);
  themeBox->addItem("Default");
  themeBox->addItem("Dark");
  themeBox->addItem("Vampire");
  form.addRow("Color Theme:", themeBox);

  QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  form.addRow(&buttonBox);

  QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted) {
    const QString selected = themeBox->currentText();
    if (canvasWidget && selected == "Vampire") {
      if (canvasWidget->selectedHisto) {
        canvasWidget->selectedHisto->SetFillColor(TColor::GetColor("#870202"));
      }
    }
    CommandPrompt::getInstance()->appendPlainText("Theme selected: " + selected + "\n");
  }
}
