#include "Integral.h"


//This function has as parameters the histogram, an error which will be returned and the marker positions in which the integral will be calculated
Double_t integral_no_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right)
{
    //We check if the markers are in the corect position if the left marker is in the right we invert them
    if(pozition_marker_left<=pozition_marker_right)
        //A root function that calculates the integral is called. It also returns an error which is calculated with the propagation of errors
        return histogram->IntegralAndError((Int_t)pozition_marker_left,(Int_t)pozition_marker_right,error);
    //This lines is the same as the previous one but the markers are inverted
    return histogram->IntegralAndError((Int_t)pozition_marker_right,(Int_t)pozition_marker_left,error);
}

/*//integrala no background scirsa "de mana"
double_t integrala_no_background(TH1F *histogram, Double_t pozitie_marke_stanga, Double_t pozitie_marker_dreapta, )
{
    double_t aria=0;
    for(int_t iterator=(int_t)pozitie_marker_stanga;iterator<=(int_t)pozitie_marker_dreapta;++iterator)
    {
        aria+=histogram->GetBinContent(iterator);
    }
    return aria;
}
*/

//Stiind doua puncte aceasta functie returneaza o dreapta adica din ecuatia y=mx+n returneaza m si n
void get_dreapta(Double_t x1,Double_t y1,Double_t x2,Double_t y2,Double_t &m,Double_t &n)
{
    m=(y1-y2)/(x1-x2);
    n=y2-((y1-y2)/(x1-x2))*x2;
}
/*
//Aceasta functie se foloseste de functia anterioara de background, dar acum se iau in calcul doua puncte care formeza o dreapta care va fi background-ul, astfel din integrala anterioara se scade aria pe care o formeaza dreapta cu oX
Double_t integral_with_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right, Double_t poz_background_1, Double_t poz_background_2)
{
    Double_t aria=histogram->IntegralAndError((int_t)pozitie_marker_stanga,(int_t)pozitie_marker_dreapta,eroare);
    Double_t m,n;
    get_dreapta(poz_background_1,histogram->GetBinContent(poz_background_1),poz_background_2,histogram->GetBinContent(poz_background_2),m,n);
    for(int_t iterator=(int_t)pozitie_marker_stanga;iterator<=(int_t)pozitie_marker_dreapta;++iterator)
    {
        aria=aria-(m*iterator+n);
    }
    return aria;
}*/
