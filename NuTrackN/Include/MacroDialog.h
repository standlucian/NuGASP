#ifndef MACRODIALOG_H
#define MACRODIALOG_H

#include <QDialog>
#include <vector>
#include <map>
#include <QString>

class QMainCanvas;
class QTableWidget;
class QTextBrowser;
class QPushButton;

class MacroDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MacroDialog(QMainCanvas *mainCanvas, QWidget *parent = nullptr);
    ~MacroDialog() override = default;

private slots:
    void onExecuteClicked(int macroId);
    void onCycleClicked(int macroId);
    void onClearClicked(int macroId);
    void onSaveToFile();
    void onLoadFromFile();
    void applySettings();
    void onAccept();

private:
    void setupUI();
    void loadMacros();
    void saveRowToMacro(int row);

    QMainCanvas *m_mainCanvas{nullptr};
    QTableWidget *m_table{nullptr};
};

#endif // MACRODIALOG_H
