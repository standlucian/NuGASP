#include "calib.h"
#include "Design.h"

void CalibrareIn2P(std::vector<Float_t> puncte_calib2p,double energie1,double energie2){
    int i = puncte_calib2p.size();
    float m,a;
    m=(energie2-energie1)/(puncte_calib2p[i-1]-puncte_calib2p[i-2]);
    a=energie2-m*puncte_calib2p[i-1];
    CommandPrompt::getInstance()->appendPlainText("a="+QString::number(a, 'f', 3)+ "  b="+QString::number(m, 'f', 3));
    std::cout<<"a="<<a<<"  "<<"b="<<m<<"\n";
    std::cout<<std::setw(10);


}
