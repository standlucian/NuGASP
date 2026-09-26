#include "HelpDialog.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>

struct ShortcutEntry {
    QString category;
    QString shortcut;
    QString action;
    QString description;
};

static const std::vector<ShortcutEntry> s_shortcuts = {
    // 1. I/O
    {"Spectrum I/O", "N", "New Spectrum", "Open and load a new 1D spectrum file (.spe, .chn, .root, ASCII)"},
    {"Spectrum I/O", "OS", "Output Spectrum", "Export active spectrum data to ASCII / CSV"},
    {"Spectrum I/O", "O=", "Output Plot", "Generate vector Postscript / PDF print of current view"},
    {"Spectrum I/O", "1 - 9", "Select Spectrum", "Switch active spectrum buffer / toggle overlay"},
    {"Spectrum I/O", "Ctrl + Right", "Add Column", "Add an additional spectrum display column to canvas"},
    {"Spectrum I/O", "Ctrl + Left", "Delete Column", "Remove last spectrum display column from canvas"},
    {"Spectrum I/O", "Ctrl + Up", "Add Row", "Add an additional spectrum display row"},
    {"Spectrum I/O", "Ctrl + Down", "Delete Row", "Remove last spectrum display row"},

    // 2. Navigation & Scaling
    {"Navigation", "L", "Lin / Log Scale", "Toggle vertical count axis between Linear and Logarithmic display"},
    {"Navigation", "E", "Expand / Zoom", "Zoom view to fit region between the last two markers"},
    {"Navigation", "X", "Expand at Cursor", "Zoom in centered at current mouse cursor position"},
    {"Navigation", "FF", "Full Display", "Reset view to full spectrum range (both X and Y)"},
    {"Navigation", "FX", "Full X Range", "Reset horizontal axis to full energy/channel range"},
    {"Navigation", "FY", "Full Y Range", "Reset vertical axis to full auto-scaled count height"},
    {"Navigation", "<", "Pan Left", "Shift visible spectrum window 3/4 screen to the left"},
    {"Navigation", ">", "Pan Right", "Shift visible spectrum window 3/4 screen to the right"},
    {"Navigation", "=", "Redraw / Refresh", "Refresh canvas display and clear temporary lines"},
    {"Navigation", "FO", "Set Y-Max", "Set vertical maximum to cursor/marker count level"},
    {"Navigation", "FU", "Set Y-Min", "Set vertical minimum to cursor/marker count level"},
    {"Navigation", "SX / SY", "Sync Scales", "Synchronize X or Y axis ranges across all sub-windows"},
    {"Navigation", "MZ", "Mark Zero", "Draw visual reference line at zero counts"},

    // 3. Markers & ROI
    {"Markers & ROI", "Spacebar", "Set Marker", "Place a standard analysis marker at cursor position"},
    {"Markers & ROI", "B", "Background Marker", "Place background boundary marker (Blue)"},
    {"Markers & ROI", "I", "Integral Marker", "Place peak integration boundary marker (Yellow)"},
    {"Markers & ROI", "R", "Fit Range Marker", "Place Gaussian fitting range boundary marker (Orange)"},
    {"Markers & ROI", "G", "Gauss Marker", "Place Gaussian peak centroid marker (Pink)"},
    {"Markers & ROI", "W", "Gate Marker", "Place coincidence energy gate marker (Purple)"},
    {"Markers & ROI", "ZA", "Clear All Markers", "Delete all active markers across the canvas"},
    {"Markers & ROI", "ZB", "Clear Bg Markers", "Delete only background markers"},
    {"Markers & ROI", "ZI", "Clear Int Markers", "Delete only integral markers"},
    {"Markers & ROI", "ZG", "Clear Gauss Markers", "Delete only Gaussian peak markers"},
    {"Markers & ROI", "ZW", "Clear Gate Markers", "Delete only coincidence gate markers"},

    // 4. Integration
    {"Integration", "CI", "Integrate (No Bg)", "Calculate gross raw area between two markers"},
    {"Integration", "CJ", "Integrate (With Bg)", "Calculate net peak area with linear background subtraction"},
    {"Integration", "CB", "Calculate Bg", "Calculate and display background baseline level"},
    {"Integration", "MI / MJ", "Show Int Markers", "Display / highlight integration markers on spectrum"},
    {"Integration", "DF / ZF", "Define/Close Log", "Open / Close output report file for area calculations"},

    // 5. Calibration
    {"Calibration", "K", "2-Point Calibrate", "Fast 2-point energy calibration using last two peak energies"},
    {"Calibration", "DK", "Define Calibration", "Open full energy and FWHM polynomial calibration dialog"},
    {"Calibration", "AK", "Auto Calibration", "Automated multi-source calibration (Co-60, Cs-137, Eu-152)"},
    {"Calibration", "DT", "TrackFit Calib", "Recalibration using track polynomial fitting"},
    {"Calibration", "DE", "Define Efficiency", "Open detector full-energy peak efficiency calibration dialog"},

    // 6. Fitting
    {"Fitting", "CP", "Calculate Peaks", "Automated peak search across visible spectrum range"},
    {"Fitting", "MP", "Show Peaks", "Display all detected peak centroids on screen"},
    {"Fitting", "ZP", "Clear Peaks", "Clear peak buffer and erase peak indicators"},
    {"Fitting", "CG", "Gaussian Fit", "Fit single or multi-Gaussian peak within range markers"},
    {"Fitting", "CV", "Gauss + Bg Fit", "Fit Gaussian with charge-trapping low-energy tail + Bg"},
    {"Fitting", "DG", "Define Gauss Params", "Open peak fitting configuration (fixed/free FWHM, tails)"},
    {"Fitting", "+ / -", "Add / Remove Peak", "Add or remove peak centroid at marker position in fit group"},

    // 7. Matrix
    {"2D Matrix", "Q", "Matrix Projection", "Display total projection of loaded compressed matrix (.cmat)"},
    {"2D Matrix", "CW", "Coincidence Cut", "Extract 1D coincidence spectrum within gate markers W"},
    {"2D Matrix", "DW", "Define Cut Window", "Set coincidence gate boundaries and background slices"},
    {"2D Matrix", "DQ", "Define Matrix Mode", "Select matrix file and background subtraction scheme"},
    {"2D Matrix", "MW", "Show Gate Markers", "Re-display active coincidence gate markers on projection"},

    // 8. Macros
    {"Macros & Help", "H / ?", "Interactive Help", "Open this User Manual & Command Reference Dialog"},
    {"Macros & Help", "DM", "Macro Manager", "Open Macro & Command String Manager Dialog (D+M)"},
    {"Macros & Help", "D + 0..9", "Define Macro", "Assign command sequence to numeric key 0 through 9"},
    {"Macros & Help", "C + 0..9", "Execute Macro", "Run stored command sequence 0 through 9"},
    {"Macros & Help", "Ctrl + C", "Quit NuTrackN", "Prompt confirmation dialog and exit cleanly"}
};

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("NuTrackN - Help & Reference Guide (Shortcut: 'H')"));
    setMinimumSize(850, 620);
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();
    populateCheatSheet();
    loadUserManual();
}

void HelpDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // Search bar header
    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *lblSearch = new QLabel(tr("<b>Quick Search:</b>"), this);
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Type command, key, or topic (e.g. 'integrate', 'L', 'calibrate', 'zoom')..."));
    m_searchBox->setClearButtonEnabled(true);
    connect(m_searchBox, &QLineEdit::textChanged, this, &HelpDialog::filterShortcuts);

    topBar->addWidget(lblSearch);
    topBar->addWidget(m_searchBox, 1);
    mainLayout->addLayout(topBar);

    // Tabs: 1. Cheat-Sheet, 2. Full Manual
    m_tabs = new QTabWidget(this);

    // Tab 1: Cheat-Sheet Table
    QWidget *tabCheat = new QWidget(this);
    QVBoxLayout *cheatLayout = new QVBoxLayout(tabCheat);
    cheatLayout->setContentsMargins(4, 4, 4, 4);

    m_cheatSheetTable = new QTableWidget(this);
    m_cheatSheetTable->setColumnCount(4);
    m_cheatSheetTable->setHorizontalHeaderLabels({tr("Category"), tr("Shortcut / Key"), tr("Action"), tr("Description")});
    m_cheatSheetTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cheatSheetTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_cheatSheetTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_cheatSheetTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_cheatSheetTable->setAlternatingRowColors(true);
    m_cheatSheetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cheatSheetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cheatLayout->addWidget(m_cheatSheetTable);
    m_tabs->addTab(tabCheat, tr("📋 Command Cheat-Sheet"));

    // Tab 2: Full User Manual
    QWidget *tabManual = new QWidget(this);
    QVBoxLayout *manualLayout = new QVBoxLayout(tabManual);
    manualLayout->setContentsMargins(4, 4, 4, 4);

    m_manualBrowser = new QTextBrowser(this);
    m_manualBrowser->setOpenExternalLinks(true);
    manualLayout->addWidget(m_manualBrowser);
    m_tabs->addTab(tabManual, tr("📖 Full User Manual"));

    mainLayout->addWidget(m_tabs, 1);

    // Bottom action buttons
    QHBoxLayout *btnBar = new QHBoxLayout();
    QLabel *lblHint = new QLabel(tr("💡 Tip: Press <b>H</b> on the canvas at any time to open this reference."), this);
    QPushButton *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setDefault(true);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnBar->addWidget(lblHint);
    btnBar->addStretch();
    btnBar->addWidget(btnClose);
    mainLayout->addLayout(btnBar);
}

void HelpDialog::populateCheatSheet() {
    m_cheatSheetTable->setRowCount(static_cast<int>(s_shortcuts.size()));
    for (int r = 0; r < static_cast<int>(s_shortcuts.size()); ++r) {
        const auto &s = s_shortcuts[r];
        QTableWidgetItem *itemCat = new QTableWidgetItem(s.category);
        QTableWidgetItem *itemKey = new QTableWidgetItem(s.shortcut);
        QTableWidgetItem *itemAct = new QTableWidgetItem(s.action);
        QTableWidgetItem *itemDesc = new QTableWidgetItem(s.description);

        itemKey->setFont(QFont(itemKey->font().family(), itemKey->font().pointSize(), QFont::Bold));

        m_cheatSheetTable->setItem(r, 0, itemCat);
        m_cheatSheetTable->setItem(r, 1, itemKey);
        m_cheatSheetTable->setItem(r, 2, itemAct);
        m_cheatSheetTable->setItem(r, 3, itemDesc);
    }
}

void HelpDialog::filterShortcuts(const QString &text) {
    if (m_tabs) m_tabs->setCurrentIndex(0); // Jump to cheat-sheet tab on search

    const QString query = text.trimmed().toLower();
    for (int r = 0; r < m_cheatSheetTable->rowCount(); ++r) {
        bool match = false;
        if (query.isEmpty()) {
            match = true;
        } else {
            for (int c = 0; c < m_cheatSheetTable->columnCount(); ++c) {
                QTableWidgetItem *it = m_cheatSheetTable->item(r, c);
                if (it && it->text().toLower().contains(query)) {
                    match = true;
                    break;
                }
            }
        }
        m_cheatSheetTable->setRowHidden(r, !match);
    }
}

void HelpDialog::loadUserManual() {
    // Attempt to locate USER_MANUAL.md from embedded resource or disk
    QStringList candidates = {
        ":/USER_MANUAL.md",
        QDir::currentPath() + "/docs/USER_MANUAL.md",
        QDir::currentPath() + "/../docs/USER_MANUAL.md",
        QCoreApplication::applicationDirPath() + "/docs/USER_MANUAL.md",
        QCoreApplication::applicationDirPath() + "/../Resources/docs/USER_MANUAL.md",
        "/home/lucian/Desktop/NuGASP/docs/USER_MANUAL.md"
    };

    QString manualContent;
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            manualContent = QString::fromUtf8(file.readAll());
            file.close();
            break;
        }
    }

    if (manualContent.isEmpty()) {
        manualContent = tr("# NuTrackN User Manual\n\nManual file could not be loaded from disk. Please refer to docs/USER_MANUAL.md in your repository.");
    }

    m_manualBrowser->setMarkdown(manualContent);
}

void HelpDialog::selectCheatSheetTab() {
    if (m_tabs) m_tabs->setCurrentIndex(0);
}

void HelpDialog::selectManualTab() {
    if (m_tabs) m_tabs->setCurrentIndex(1);
}
