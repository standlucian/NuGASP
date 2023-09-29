#ifndef DESIGN_H
#define DESIGN_H

#include "canvas.h"
#include <QColor>
#include <QPalette>
#include <TCanvas.h>
#include <TStyle.h>
#include <TColor.h>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QProcess>
#include <QDebug>
#include <QKeyEvent>




class CommandPrompt: public QPlainTextEdit //this class manages the command prompt and its functions, and is derived from QPlainTextEdit to access its functions;
{

public:
    static CommandPrompt* getInstance();//getter for the class instance, allowing usage in any other file of the program
    static void setMainCanvas(QMainCanvas* m);//setter for the main canvas variable for the getInstance function to not require the QMainCanvas m as a parameter
    std::string allowUserInput();//currently not in use
    std::string getUserInput();//currently not in use
    bool userInputRequested = false; // bool to check if any file requests user input, currently not in use

private:
    CommandPrompt(QMainCanvas* m);
    static CommandPrompt* instance;
    static QMainCanvas* mainCanvas; // Declare mainCanvas here
    void setAvailableText(QString string);

    bool commandPromptHasFocus; // bool to check if command  prompt has focus, currently not in use
    bool b;
    std::string command; //  should have been used to be returned by getUserInput()
    QString availableText;
protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

public slots:
        void handleTextChanges();
        void allowUserToCopyText(bool yes);


};


void changeBackgroundColor(TCanvas* canvas);
void addCommandPrompt(QMainCanvas* mainCanvas);

#endif // DESIGN_H

