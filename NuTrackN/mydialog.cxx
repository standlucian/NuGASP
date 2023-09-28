#include "mydialog.h"
#include "ui_mydialog.h"

MyDialog::MyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MyDialog)
{
    ui->setupUi(this);
}

MyDialog::~MyDialog()
{
    delete ui;
}

void MyDialog::on_calibrateButton_clicked()
{
    bool ok1, ok2;
    double energie1 = ui->energy1LineEdit->text().toDouble(&ok1);
    double energie2 = ui->energy2LineEdit->text().toDouble(&ok2);

    if(ok1 && ok2)
    {
        // Call your calibration function here with energie1 and energie2
        // Example: CalibrareIn2P(puncte_calib2p, energie1, energie2);
    }
}
