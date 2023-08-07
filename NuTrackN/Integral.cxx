#include "Integral.h"
#include<iostream>


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
/*
//Stiind doua puncte aceasta functie returneaza o dreapta adica din ecuatia y=mx+n returneaza m si n
void get_dreapta(Double_t x1,Double_t y1,Double_t x2,Double_t y2,Double_t &m,Double_t &n)
{
    m=(y1-y2)/(x1-x2);
    n=y2-((y1-y2)/(x1-x2))*x2;
}*/

Double_t absolute_nonzero(Double_t number)
{
    if(number<0)
        return -number;
    if(number==0)
        return 1;
    return number;
}

//Aceasta functie se foloseste de functia anterioara de background, dar acum se iau in calcul doua puncte care formeza o dreapta care va fi background-ul, astfel din integrala anterioara se scade aria pe care o formeaza dreapta cu oX
Double_t integral_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right, std::vector<Double_t> backgroundMarkers,Double_t &m,Double_t &n)
{
    Double_t area=histogram->IntegralAndError((Int_t)pozition_marker_left,(Int_t)pozition_marker_right,error);
    sort(backgroundMarkers.begin(),backgroundMarkers.end());

    //Internet version
    Double_t x_medium=0,y_medium=0,number_of_variators=0;
    for(Int_t iterat=0;iterat<=backgroundMarkers.size();iterat+=2)
    {
        for(Double_t space_between_markers=backgroundMarkers[iterat];space_between_markers<=backgroundMarkers[iterat+1];++space_between_markers)
        {
            x_medium+=space_between_markers;
            y_medium+=histogram->GetBinContent(space_between_markers);
            number_of_variators++;
         }
    }
    x_medium/=number_of_variators;
    y_medium/=number_of_variators;
    Double_t top_fraction=0,bottom_fraction=0;
    for(Int_t iterat=0;iterat<=backgroundMarkers.size();iterat+=2)
    {
        for(Double_t space_between_markers=backgroundMarkers[iterat];space_between_markers<=backgroundMarkers[iterat+1];++space_between_markers)
        {
            top_fraction+=(space_between_markers-x_medium)*(histogram->GetBinContent(space_between_markers)-y_medium);
            bottom_fraction+=(space_between_markers-x_medium)*(space_between_markers-x_medium);
         }
    }
    Double_t slope=top_fraction/bottom_fraction;
    Double_t addition=y_medium-slope*x_medium;
    Double_t e2bg1=slope,e2bg0=addition;

/*
    //xtracnk version
    Double_t minimumBackgroundMarker=backgroundMarkers[0];
    Double_t s1=0,sx=0,sy=0,sxx=0,sxy=0,xx=0,yy=0,el=0;
    for(Int_t iterat=0;iterat<=backgroundMarkers.size();iterat+=2)
    {
        for(Double_t space_between_markers=backgroundMarkers[iterat];space_between_markers<=backgroundMarkers[iterat+1];++space_between_markers)
        {
            xx=space_between_markers-minimumBackgroundMarker+0.5;
            yy=histogram->GetBinContent(space_between_markers);
            el=(Double_t)1/(absolute_nonzero(space_between_markers));
            //std::cout<<el<<std::endl;
            s1+=el;
            sx+=(xx*el);
            sxx+=(xx*xx*el);
            sy+=(yy*el);
            sxy+=(xx*yy*el);
         }
    }
    Double_t determinant=s1*sxx-sx*sx;
    std::cout<<determinant<<std::endl;
    if(determinant<=0)
    {
          error=100;
          return -1;
    }
    Double_t bg0=(sxx*sy-sx*sxy)/determinant,bg1=(s1*sxy-sx*sy)/determinant,e2bg0=sxx/determinant,e2bg1=s1/determinant,ebg01=-sx/determinant;
*/
    m=e2bg1;
    n=e2bg0;
    std::cout<<e2bg1<<" "<<e2bg0<<std::endl;
    for(Int_t iterat=(Int_t)pozition_marker_left;iterat<=(Int_t)pozition_marker_right;++iterat)
    {
        area-=(e2bg1*iterat+e2bg0);
    }
    error=0;
    Double_t medium_value=0;
    Double_t observed_value,expected_value,degree_of_freedom=0;
    for(Int_t iterat=0;iterat<=backgroundMarkers.size();iterat+=2)
    {
        for(Double_t space_between_markers=backgroundMarkers[iterat];space_between_markers<=backgroundMarkers[iterat+1];++space_between_markers)
        {
            degree_of_freedom++;
            observed_value=e2bg1*space_between_markers+e2bg0;
            medium_value+=observed_value;
            expected_value=histogram->GetBinContent(space_between_markers);
            error+=((observed_value-expected_value)*(observed_value-expected_value))/expected_value;
        }
    }
    error/=degree_of_freedom;
    medium_value/=degree_of_freedom;
    std::cout<<"Medium value:"<<medium_value<<std::endl;
    return area;

}
