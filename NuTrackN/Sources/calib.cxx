#include "calib.h"
#include "Design.h"
#include <iostream>

// Performs a two-point energy calibration using the last two fitted
// points and their corresponding reference energies.
// Uses a first-degree polynomial (linear calibration)
void TwoPointCalibration(const std::vector<Float_t>& puncte_calib2p,
                         double energie1,
                         double energie2)
{
    const std::size_t i = puncte_calib2p.size();

    //Guard against calling the function with insufficient number of fitted points
    if (i < 2) {
        CommandPrompt::getInstance()->appendPlainText(
            "Two-point calibration requires at least two points.");
        return;
    }

    //Calculate slope
    const double m = (energie2 - energie1)
              / (puncte_calib2p[i-1] - puncte_calib2p[i-2]);

    //Calculate y-intercept
    const double a = energie2 - m * puncte_calib2p[i-1];

    // Output to the application's command prompt
    CommandPrompt::getInstance()->appendPlainText(
        "Calibration coef :   A(0)= "+
        QString::number(a, 'f', 3)+
        "   A(1)="+
        QString::number(m, 'f', 3));

    //Output to system terminal
    std::cout<<"Calibration coef :   "
              <<"A(0)= "<<a<<"  "
              <<"A(1)= "<<m
              <<std::endl;
}
