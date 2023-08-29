#include "canvas.h"
#include "Design.h"
#include <QPalette>
#include <TCanvas.h>
#include <TStyle.h>
#include <TColor.h>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QProcess>
#include <QDebug>
#include <QKeyEvent>
#include <QTcpSocket>



class CommandPrompt: public QPlainTextEdit //this class manages the command prompt and its functions, and is derived from QPlainTextEdit to access its functions;
{
    public:
        CommandPrompt(QMainCanvas *m):QPlainTextEdit(m) //constructor for the CommandPrompt class, passing the main canvas m to both QPlainTextEdit class and the CommandPrompt derived from it
    {
        setPlaceholderText("Enter your command here:"); //This text will be temporarly displayed before the user enters text
        setReadOnly(false);//this allows the user to insert text into the command prompt

        connect(this,&QPlainTextEdit::cursorPositionChanged,this,&CommandPrompt::setCursorPositionToEnd);
        //this command gives a passes a signal to the setCursorPositionToEnd function whenever the position of the cursor is changed
        //Esentially, this doesnt let the user edit the text entered above or the output the terminal gives
    };

protected:
        void setCursorPositionToEnd() //function used to set the text cursor to the end of the availabile text whenever its position is not at the end
        {
            QTextCursor text_cursor=QTextCursor(document());//creates the text_cursor object and links it to the QPlainText document, without linking, it would result in a segmentation error
            if(text_cursor.position()<this->toPlainText().length()) //checking to see if current position of the text cursor is any other than at the last character
            {
                text_cursor.movePosition(QTextCursor::End); // if so, move the position to end to allow user input only at the end
                setTextCursor(text_cursor); //updates the position of the cursor
            }
        }



    public slots:
        void handleTextChanges()
        {

            if(this->toPlainText().endsWith('\n'))//this checks if the last character in the command is a newline,
                                                  //mimmicking the enter key press to send activate the function

            {
                QString text_event = this->toPlainText().trimmed();//this takes all the text availabile in the commandprompt, and trims the leading and trailing whitespaces and newlines
                QList command_list = text_event.split('\n');
                QString command_to_send = command_list.last();
                command_to_send.replace("Terminal: ", "");

                if(command_to_send!='\n')
                {
                disconnect(this, &QPlainTextEdit::textChanged, this, &CommandPrompt::handleTextChanges);
                    QProcess process;
                    process.start(command_to_send);
                    process.waitForFinished(-1);
                    QString output;
                    output = process.readAllStandardOutput();
                    this->setPlainText(this->toPlainText().left(this->toPlainText().length()-1));

                    if(!output.isEmpty())
                        this->appendPlainText("Terminal: "+output);
                    else
                    {
                        this->appendPlainText("Terminal: Command not recognized by terminal");
                        this->appendPlainText("");
                    }

                        connect(this,&CommandPrompt::textChanged,this,&CommandPrompt::handleTextChanges);
                }
            }
                    }
};


void addCommandPrompt(QMainCanvas *m) {

    CommandPrompt *commandPrompt = new CommandPrompt(m);
    QObject::connect(commandPrompt, &CommandPrompt::textChanged,commandPrompt, &CommandPrompt::handleTextChanges);


    QTcpSocket socket;

        // Connect to the socket server on the local machine and port 12345
        socket.connectToHost("127.0.0.1", 12345);




    // Set up the command prompt geometry and layout
    int canvasHeight = m->height(); // Gets the height of the canvas
    int commandPromptHeight = canvasHeight * 0.2; // Sets the Command Prompt height variable to 20% of that height
    commandPrompt->setGeometry(0, canvasHeight - commandPromptHeight, 700, commandPromptHeight); // Applies the height to the geometry

    // Add the command prompt widget to the layout
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m->layout());
    if (layout) {
        layout->addWidget(commandPrompt);
    }
}

void changeBackgroundColor(TCanvas* canvas) {
    // Change the background color of the canvas
    if (canvas) {
            //canvas->SetFillColor(kRed);
            canvas->SetFillColor(TColor::GetColor("#919191"));//set the color of the baackground, in hexa color code

            TStyle* style = gStyle; // Get the current style
            style->SetPadGridX(1); // Show grid lines along the X-axis
            style->SetPadGridY(1); // Show grid lines along the Y-axis
            style->SetGridColor(kGray); // Set grid color to gray
            style->SetGridStyle(3); // Set grid line style

            style->SetNdivisions(540, "X");//set the size of the squares in the background
            style->SetNdivisions(540, "Y");//the higher the number, the smaller the squares are


            canvas->Update();//Updates the canvas
    }
};
