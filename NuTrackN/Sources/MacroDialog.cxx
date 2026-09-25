#include "MacroDialog.h"
#include "canvas.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QTextBrowser>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QLabel>

MacroDialog::MacroDialog(QMainCanvas *mainCanvas, QWidget *parent)
    : QDialog(parent), m_mainCanvas(mainCanvas)
{
    setWindowTitle(tr("Automatic Command Strings & Macros (Block 4)"));
    resize(740, 680);
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #ffffff; }"
        "QGroupBox { border: 1px solid #3e3e42; border-radius: 4px; margin-top: 10px; font-weight: bold; color: #00ffff; font-size: 13px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #cccccc; font-size: 12px; }"
        "QLineEdit, QSpinBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; border-radius: 3px; padding: 3px 6px; font-size: 12px; }"
        "QLineEdit:focus, QSpinBox:focus { border: 1px solid #007acc; }"
        "QTableWidget { background-color: #252526; color: #ffffff; gridline-color: #3e3e42; border: 1px solid #3e3e42; border-radius: 4px; font-size: 12px; }"
        "QHeaderView::section { background-color: #333337; color: #00ffff; font-weight: bold; border: 1px solid #3e3e42; padding: 4px; }"
        "QPushButton { background-color: #3e3e42; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: #4e4e52; }"
        "QPushButton:pressed { background-color: #007acc; }"
        "QTextBrowser { background-color: #252526; color: #d4d4d4; border: 1px solid #3e3e42; border-radius: 4px; font-size: 11px; }"
    );

    setupUI();
    loadMacros();
}

void MacroDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. Top description banner
    QLabel *lblDesc = new QLabel(
        tr("<b>Xtrackn Command Strings & Macros:</b> Configure sequence strings for slots 0–9.<br>"
           "Trigger via keyboard: <b>[0-9]</b> to run once, <b>D+[0-9]</b> to define, <b>C+[0-9]</b> to cycle, "
           "<b>M+[0-9]</b> to show, <b>Z+[0-9]</b> to erase."), this);
    lblDesc->setStyleSheet("color: #9cdcfe; margin-bottom: 4px;");
    mainLayout->addWidget(lblDesc);

    // 2. Table of Macros (Slots 0 to 9)
    m_table = new QTableWidget(10, 5, this);
    m_table->setHorizontalHeaderLabels({
        tr("Slot"), tr("Command String"), tr("Cycles"), tr("Description / Note"), tr("Actions")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(1, 150);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    for (int i = 0; i < 10; ++i) {
        // Slot label
        QTableWidgetItem *itemSlot = new QTableWidgetItem(QString("  #%1  ").arg(i));
        itemSlot->setTextAlignment(Qt::AlignCenter);
        itemSlot->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, 0, itemSlot);

        // Command string editor
        QLineEdit *editCmd = new QLineEdit(m_table);
        editCmd->setPlaceholderText(tr("e.g. NFF, NCP, AG"));
        m_table->setCellWidget(i, 1, editCmd);

        // Cycles spinbox
        QSpinBox *spinCycles = new QSpinBox(m_table);
        spinCycles->setRange(1, 10000);
        spinCycles->setValue(1);
        m_table->setCellWidget(i, 2, spinCycles);

        // Description
        QLineEdit *editDesc = new QLineEdit(m_table);
        editDesc->setPlaceholderText(tr("Optional label"));
        m_table->setCellWidget(i, 3, editDesc);

        // Action buttons container
        QWidget *actionWidget = new QWidget(m_table);
        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(4, 2, 4, 2);
        actionLayout->setSpacing(4);

        QPushButton *btnRun = new QPushButton(tr("Run (1x)"), actionWidget);
        btnRun->setToolTip(tr("Execute this macro once"));
        connect(btnRun, &QPushButton::clicked, this, [this, i]() { onExecuteClicked(i); });
        actionLayout->addWidget(btnRun);

        QPushButton *btnCycle = new QPushButton(tr("Cycle"), actionWidget);
        btnCycle->setToolTip(tr("Run this macro in a loop"));
        connect(btnCycle, &QPushButton::clicked, this, [this, i]() { onCycleClicked(i); });
        actionLayout->addWidget(btnCycle);

        QPushButton *btnClear = new QPushButton(tr("Clear"), actionWidget);
        btnClear->setToolTip(tr("Erase this macro string"));
        connect(btnClear, &QPushButton::clicked, this, [this, i]() { onClearClicked(i); });
        actionLayout->addWidget(btnClear);

        m_table->setCellWidget(i, 4, actionWidget);
        m_table->setRowHeight(i, 36);
    }
    mainLayout->addWidget(m_table, 1);

    // 3. Command Syntax Quick Reference / Cheat Sheet
    QGroupBox *grpHelp = new QGroupBox(tr("Command Syntax & Tokens Reference"), this);
    QVBoxLayout *helpLayout = new QVBoxLayout(grpHelp);
    helpLayout->setContentsMargins(8, 8, 8, 8);

    QTextBrowser *helpBrowser = new QTextBrowser(grpHelp);
    helpBrowser->setFixedHeight(120);
    helpBrowser->setHtml(
        "<table style='width:100%; font-family:monospace;'>"
        "<tr>"
        "<td><b>N / N+</b>: Next Spectrum</td>"
        "<td><b>N-</b>: Prev Spectrum</td>"
        "<td><b>*1..*4</b>: Fast Spectrum Steps</td>"
        "<td><b>FF</b>: Full Zoom (FX+FY)</td>"
        "</tr><tr>"
        "<td><b>FX</b>: Full X (keep Y)</td>"
        "<td><b>FY</b>: Full Y (autoscale)</td>"
        "<td><b>L</b>: Lin/Log Toggle</td>"
        "<td><b>&lt; / &gt;</b>: Pan Left/Right 75%</td>"
        "</tr><tr>"
        "<td><b>CP</b>: Peak Search</td>"
        "<td><b>MP</b>: Show Peaks</td>"
        "<td><b>ZP</b>: Clear Peaks</td>"
        "<td><b>CB</b>: Fit Background</td>"
        "</tr><tr>"
        "<td><b>CI</b>: Integrate</td>"
        "<td><b>CJ</b>: BG + Integrate</td>"
        "<td><b>CG / CV</b>: Gauss Multi-Fit</td>"
        "<td><b>AG / AJ</b>: Auto Gauss / Integral</td>"
        "</tr><tr>"
        "<td><b>Q</b>: Matrix Projection</td>"
        "<td><b>=</b>: Redraw Screen</td>"
        "<td><b>CW</b>: Gate Cut</td>"
        "<td><b>ZA</b>: Delete All Markers</td>"
        "</tr>"
        "</table>"
    );
    helpLayout->addWidget(helpBrowser);
    mainLayout->addWidget(grpHelp);

    // 4. Bottom Button Controls
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnLoad = new QPushButton(tr("Load from File..."), this);
    QPushButton *btnSave = new QPushButton(tr("Save to File..."), this);
    connect(btnLoad, &QPushButton::clicked, this, &MacroDialog::onLoadFromFile);
    connect(btnSave, &QPushButton::clicked, this, &MacroDialog::onSaveToFile);

    bottomLayout->addWidget(btnLoad);
    bottomLayout->addWidget(btnSave);
    bottomLayout->addStretch(1);

    QPushButton *btnApply = new QPushButton(tr("Apply"), this);
    QPushButton *btnOk = new QPushButton(tr("OK"), this);
    QPushButton *btnCancel = new QPushButton(tr("Close"), this);
    btnOk->setStyleSheet("QPushButton { background-color: #0e639c; } QPushButton:hover { background-color: #1177bb; }");

    connect(btnApply, &QPushButton::clicked, this, &MacroDialog::applySettings);
    connect(btnOk, &QPushButton::clicked, this, &MacroDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottomLayout->addWidget(btnApply);
    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);
}

void MacroDialog::loadMacros()
{
    if (!m_mainCanvas) return;
    const auto &macros = m_mainCanvas->getMacros();

    for (int i = 0; i < 10; ++i) {
        auto it = macros.find(i);
        QLineEdit *editCmd = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 1));
        QSpinBox *spinCycles = qobject_cast<QSpinBox*>(m_table->cellWidget(i, 2));
        QLineEdit *editDesc = qobject_cast<QLineEdit*>(m_table->cellWidget(i, 3));

        if (it != macros.end()) {
            if (editCmd) editCmd->setText(it->second.commandString);
            if (spinCycles) spinCycles->setValue(it->second.cycles > 0 ? it->second.cycles : 1);
            if (editDesc) editDesc->setText(it->second.description);
        } else {
            if (editCmd) editCmd->clear();
            if (spinCycles) spinCycles->setValue(1);
            if (editDesc) editDesc->clear();
        }
    }
}

void MacroDialog::saveRowToMacro(int row)
{
    if (!m_mainCanvas || row < 0 || row >= 10) return;
    QLineEdit *editCmd = qobject_cast<QLineEdit*>(m_table->cellWidget(row, 1));
    QSpinBox *spinCycles = qobject_cast<QSpinBox*>(m_table->cellWidget(row, 2));
    QLineEdit *editDesc = qobject_cast<QLineEdit*>(m_table->cellWidget(row, 3));

    QMainCanvas::MacroDefinition def;
    def.id = row;
    def.commandString = editCmd ? editCmd->text().trimmed().toUpper() : "";
    def.cycles = spinCycles ? spinCycles->value() : 1;
    def.description = editDesc ? editDesc->text().trimmed() : "";

    m_mainCanvas->setMacro(row, def);
}

void MacroDialog::applySettings()
{
    for (int i = 0; i < 10; ++i) {
        saveRowToMacro(i);
    }
    CommandPrompt::getInstance()->appendPlainText(tr("Macro settings updated.\n"));
}

void MacroDialog::onAccept()
{
    applySettings();
    accept();
}

void MacroDialog::onExecuteClicked(int macroId)
{
    saveRowToMacro(macroId);
    if (m_mainCanvas) {
        m_mainCanvas->executeMacro(macroId);
    }
}

void MacroDialog::onCycleClicked(int macroId)
{
    saveRowToMacro(macroId);
    if (!m_mainCanvas) return;
    QSpinBox *spinCycles = qobject_cast<QSpinBox*>(m_table->cellWidget(macroId, 2));
    int cycles = spinCycles ? spinCycles->value() : 1;
    m_mainCanvas->cycleMacro(macroId, cycles);
}

void MacroDialog::onClearClicked(int macroId)
{
    QLineEdit *editCmd = qobject_cast<QLineEdit*>(m_table->cellWidget(macroId, 1));
    QLineEdit *editDesc = qobject_cast<QLineEdit*>(m_table->cellWidget(macroId, 3));
    if (editCmd) editCmd->clear();
    if (editDesc) editDesc->clear();
    saveRowToMacro(macroId);
    if (m_mainCanvas) {
        m_mainCanvas->clearMacro(macroId);
    }
}

void MacroDialog::onSaveToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save Macro Definitions"), "",
        tr("Macro Files (*.mac *.txt);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not write macro file: %1").arg(fileName));
        return;
    }

    applySettings();
    QTextStream out(&file);
    out << "# NuTrackN Macro Definitions (Block 4)\n";
    out << "# Format: ID|COMMAND_STRING|CYCLES|DESCRIPTION\n";
    const auto &macros = m_mainCanvas->getMacros();
    for (const auto &pair : macros) {
        if (!pair.second.commandString.isEmpty() || !pair.second.description.isEmpty()) {
            out << QString("%1|%2|%3|%4\n")
                .arg(pair.first)
                .arg(pair.second.commandString)
                .arg(pair.second.cycles)
                .arg(pair.second.description);
        }
    }
    CommandPrompt::getInstance()->appendPlainText(
        QString("Macros saved to file: %1\n").arg(fileName));
}

void MacroDialog::onLoadFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Load Macro Definitions"), "",
        tr("Macro Files (*.mac *.txt);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not read macro file: %1").arg(fileName));
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        QStringList parts = line.split('|');
        if (parts.size() >= 2) {
            bool ok = false;
            int id = parts[0].toInt(&ok);
            if (ok && id >= 0 && id < 10) {
                QMainCanvas::MacroDefinition def;
                def.id = id;
                def.commandString = parts[1].trimmed().toUpper();
                def.cycles = (parts.size() >= 3) ? std::max(1, parts[2].toInt()) : 1;
                def.description = (parts.size() >= 4) ? parts[3].trimmed() : "";
                m_mainCanvas->setMacro(id, def);
            }
        }
    }
    loadMacros();
    CommandPrompt::getInstance()->appendPlainText(
        QString("Macros loaded from file: %1\n").arg(fileName));
}
