#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief Interactive in-app User Manual and Keyboard Command Cheat-Sheet dialog.
 * Activated via 'H', '?', 'F1', or the Help menu/toolbar icon.
 */
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);
    ~HelpDialog() override = default;

    void selectCheatSheetTab();
    void selectManualTab();

private slots:
    void filterShortcuts(const QString &text);

private:
    void setupUI();
    void populateCheatSheet();
    void loadUserManual();

    QTabWidget *m_tabs{nullptr};
    QTableWidget *m_cheatSheetTable{nullptr};
    QTextBrowser *m_manualBrowser{nullptr};
    QLineEdit *m_searchBox{nullptr};
};

#endif // HELPDIALOG_H
