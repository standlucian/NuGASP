#include "calib.h"
#include "Design.h"

void CalibrareIn2P(std::vector<Float_t> puncte_calib2p,double energie1,double energie2){
    int i = puncte_calib2p.size();
    float m,a;
    m=(energie2-energie1)/(puncte_calib2p[i-1]-puncte_calib2p[i-2]);
    a=energie2-m*puncte_calib2p[i-1];
    CommandPrompt::getInstance()->appendPlainText("Calibration coef :   A(0)= "+QString::number(a, 'f', 3)+ "   A(1)="+QString::number(m, 'f', 3));
    std::cout<<"Calibration coef :   "<<"A(0)= "<<a<<"  "<<"A(1)= "<<m<<"\n";
    std::cout<<std::setw(10);


}
