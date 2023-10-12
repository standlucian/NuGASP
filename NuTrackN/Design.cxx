#include "canvas.h"
#include "Design.h"
#include <iostream>

CommandPrompt* CommandPrompt::instance = nullptr;//initialize the instance of the command prompt as a null ppointer
QMainCanvas* CommandPrompt::mainCanvas = nullptr;//as well as the mainCanvas variable inside the CommandPrompt class; the actual mainCanvas will be passed to this variable using a setter


CommandPrompt::CommandPrompt(QMainCanvas *m):QPlainTextEdit(m) //constructor for the CommandPrompt class, passing the main canvas m to both QPlainTextEdit class and the CommandPrompt derived from it
 //It is a singleton class, it can only be instantiated once, check the getInsance function in this file for details about usage throughout any other file
{
setPlaceholderText("Enter your command here:"); //This text will be temporarly displayed before the user enters text
setReadOnly(false);//this allows the user to insert text into the command prompt
availableText=this->toPlainText();//we set the available text to basically empty when the commandPrompt object is created
connect(this,&QPlainTextEdit::copyAvailable,this,&CommandPrompt::allowUserToCopyText);//connects the signal that will be sent when the user selects and deselects text (check the slot function for further information
connect(this, &CommandPrompt::textChanged,this, &CommandPrompt::handleTextChanges);//this sends signals every time a key is pressed in the command prompt (because Enter signal iss not availabile) and it handles the text accordingly
connect(this,&CommandPrompt::cursorPositionChanged,this, &CommandPrompt::handleCursorMoved);
};

void CommandPrompt::setMainCanvas(QMainCanvas* m)//setter for the mainCanvas variable; this allows class usage from any file without passing the QMainCanvas m to the ClassInstance getter
{
    mainCanvas = m;
}

void CommandPrompt::setAvailableText(QString string)
{
    this->availableText = string;
}
void CommandPrompt::allowUserToCopyText(bool yes) //slot function used to allow the user to copy text from the terminal, without actually deleting the terminal output
{

    if(yes) //we check if the signal copyAvailabile with bool value "yes" is true or false. if it is true, the user has made a selection and we restrict the ability to modify the terminal text
        {
            setReadOnly(true);//disallow the user to moify text
            copyAvailabile = true;

        }
        else //if the signal is false, that means the text was deselected. we allow the user to modigy text, but we move the text cursor to the end to make sure the already outputted terminal text stays intact
        {
            setReadOnly(false);
            disconnect(this, &QPlainTextEdit::copyAvailable, this, &CommandPrompt::allowUserToCopyText);//temporarly disconnect the copyAvailabile signal, because it is triggered infinitely by the move cursor action, creating an infinite loop (seg fault)

            QTextCursor cursor = this->textCursor();//create a cursor object and set it at the end
            cursor.movePosition(QTextCursor::End);
            setTextCursor(cursor);//updates the cursor in QPlainTextEdit to the value of the created cursor (at the end)
            copyAvailabile=false;
            // Reconnect the copyAvailable signal
            connect(this, &QPlainTextEdit::copyAvailable, this, &CommandPrompt::allowUserToCopyText);

        }



}

void CommandPrompt::handleCursorMoved()
{
    QTextCursor cursor = this->textCursor();//create a cursor object and set it at the end



    if(!cursor.atEnd())
    {
        setReadOnly(true);
    }
    else
    {
        if(copyAvailabile)
            setReadOnly(true);//if cursor is at the end but user has made a selection we still dont let him modify
        else setReadOnly(false); //if no selection is made we let the user modify onlyy when the cursor is at the end
    }
}


void CommandPrompt::handleTextChanges() // handles the text each time the text changes in the terminal
{

    if(this->toPlainText()==availableText) // we check if the text in the terminal is the same text as it was when we sent our last command
    {
        disconnect(this, &QPlainTextEdit::textChanged, this, &CommandPrompt::handleTextChanges);
        //space to take action in the terrminal (write especially) without triggering the textChanged signal because its temporarly disconnected
        appendPlainText("");
        //if so, we append a \n to the commandPrompt to disallow the user to delete from that point on
        connect(this,&CommandPrompt::textChanged,this,&CommandPrompt::handleTextChanges);
    }

    else if(this->toPlainText().endsWith('\n'))//this checks if the last character in the command is a newline,
                                          //mimmicking the enter key press to send activate the function

    {

            availableText=this->toPlainText();//if we recognize that a new command has been entered, we set the availabileText variable to the text availabile in the prompt right now
            availableText.chop(1); //we trim the new line character to be able to make comparisons in the beginning if section next time
            QString command_to_send;
            QString text_event = this->toPlainText();
            text_event.chop(1);
            int lastIndex = text_event.lastIndexOf('\n');//we check the last index of a /n in the command prompt

            if (lastIndex != -1)// if it exists we set the command to send to the last string, after the new line index
                command_to_send = text_event.mid(lastIndex);
            else command_to_send = this->toPlainText();//if it doesnt exist, it means it is the first command to be ever sent, and we set it to be the whole avaulabile text in the prompt

            if(command_to_send!='\n')//this just checks if the command is empty, this can be used in texting and has no effect on the current code
            {
                disconnect(this, &QPlainTextEdit::textChanged, this, &CommandPrompt::handleTextChanges);
                //space to take action in the terrminal (write especially) without triggering the textChanged signal because its temporarly disconnected
                connect(this,&CommandPrompt::textChanged,this,&CommandPrompt::handleTextChanges);

            }
            else
            {
                disconnect(this, &QPlainTextEdit::textChanged, this, &CommandPrompt::handleTextChanges);
                //space to take action in the terrminal (write especially) without triggering the textChanged signal because its temporarly disconnected
                connect(this,&CommandPrompt::textChanged,this,&CommandPrompt::handleTextChanges);
            }
    }

};



CommandPrompt* CommandPrompt::getInstance() // getter for the class instance
//TO USE ANY FUNCTION FROM THE COMMAND PROMPT CLASS:
//include the Design.h header
// use CommandPrompt::getInstance()->(followed by function to be used)
// Eg: CommandPrompt::getInstance()->appendPlainText("MESSAGE"); writes "MESSAGE" in the app terminal
// variavila  + "sadfahsdf"
{
    if (!instance)//checks to see if the CommandPrompt already has been instantiated
    {
        instance = new CommandPrompt(mainCanvas); // if not it will instantiate a new one
        //Important: This is a singleton class, so it can only be instantiaaated once
    }
    return instance;
}


void addCommandPrompt(QMainCanvas *m) {
    CommandPrompt::setMainCanvas(m);//this sets the QMainCanvas parent, because it will not be availabile from other file; this way, you can call the Command Prompt class getter without passing it the QMainCanvas m created in the main.cxx file
    CommandPrompt* commandPrompt = CommandPrompt::getInstance();//the getter acts ass a class instanciator;if it is already existent, it just returns you the already madee command prompt, if not, it instances one



     //Set up the command prompt geometry and layout
    int canvasHeight = m->height(); // Gets the height of the canvas
    int commandPromptHeight = canvasHeight * 0.5; // Sets the Command Prompt height variable to 20% of that height
    commandPrompt->setFixedHeight(commandPromptHeight);//sets the command prompt height to 50% of the canvas height
     //Add the command prompt widget to the layout
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m->layout());//tries to get the layout of m to be able to add the command prompt
    if (layout) {//if it exiists it  adds  the commmand prompt and tells the layout to update itself
        layout->addWidget(commandPrompt);
        layout->update();
    }

}

void CommandPrompt::focusInEvent(QFocusEvent* event)//  function that changes the status of the bool variable commandPromptHasFocus, currently not in use
{
    QPlainTextEdit::focusInEvent(event);
    commandPromptHasFocus = true;
}

void CommandPrompt::focusOutEvent(QFocusEvent* event) //  function that changes the status of the bool variable commandPromptHasFocus, currently not in use
{
    QPlainTextEdit::focusOutEvent(event);
    commandPromptHasFocus = false;
    //CommandPrompt::getInstance()->appendPlainText("bool changed");
}



void changeBackgroundColor(TCanvas* canvas) {
    // Change the background color of the canvas

    if (canvas) {
            //canvas->SetFillColor(kRed);
            //canvas->SetFillColor(TColor::GetColor("#919191"));//set the color of the baackground, in hexa color code

            //TStyle* style = gStyle; // Get the current style
            //style->SetGridColor(kGray); // Set grid color to gray
            //style->SetGridStyle(3); // Set grid line style

    }
};
