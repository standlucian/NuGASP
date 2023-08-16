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

//Here is the function that checks if the markers are overlapping that reutrns true if this is the case and false otherwise
bool overlapping_markers(std::vector<Double_t> markers)
{
    //We create a vector of the size of the histogram and at first we say that none of it is ocuppied
    std::vector<bool> vector_of_ocupation;
    vector_of_ocupation.resize(10240,false);
    //We go through all the markers and check the space between them
    for(Int_t iterat=0;iterat<=markers.size();iterat+=2)
    {
        for(Double_t space_between_markers=markers[iterat];space_between_markers<=markers[iterat+1];++space_between_markers)
        {
            //We look if the space we are on right now has already been filled if it has we have an overlap and return true if not we fill it
            if(vector_of_ocupation[space_between_markers]==true)
            {
                return true;
            }
            else
            {
                vector_of_ocupation[space_between_markers]=true;
            }
        }
    }
    //If we did not end the function by now it means that there are no overlaps so we return false
    return false;
}

//This function is given the histogram the background markers as a vector of double values and two variables as references which the function will return
void get_best_fitted_line(TH1F *histogram,std::vector<Double_t> background_markers,Double_t &slope,Double_t &addition)
{
    //First of all the background markers are sorted for now we don t care about intersections but this will be resolved in a future version
    sort(background_markers.begin(),background_markers.end());
    //Internet version
    //Here every variable is delcared and will be used acordingly later:x_medium gets the median values of the x axis, similar y_medium does it for the y axis
    //number_of_variators is the number of things that vary so this is basically the number of points in the background(betweeen the markers), top_fraction and bottom_fraction are some variables to clean up the code and make it more readeable which splits a dificult frcation
    //medium_value is the medium value of the line,observed value, expected value and reduced_chi2 are used to calculate the reduced chi2 which tells us how good is the line we chose
    Double_t x_medium=0,y_medium=0,number_of_variators=0,top_fraction=0,bottom_fraction=0,medium_value=0,observed_value,expected_value,reduced_chi2;
    //First we calculate the medium values for the x and y axis and also we calculate the number of points in between the markers
    for(Int_t iterat=0;iterat<=background_markers.size();iterat+=2)
    {
        for(Double_t space_between_markers=background_markers[iterat];space_between_markers<=background_markers[iterat+1];++space_between_markers)
        {
            x_medium+=space_between_markers;
            y_medium+=histogram->GetBinContent(space_between_markers);
            number_of_variators++;
         }
    }
    //We got the sum of all values for the xa nd y axis so now we divide the number of points so we get the average value
    x_medium/=number_of_variators;
    y_medium/=number_of_variators;
    //We calculate the top of the fraction and the bottom of the fraction that will give us the slope. The method is form a website which gives an example of calculating a slope
    for(Int_t iterat=0;iterat<=background_markers.size();iterat+=2)
    {
        for(Double_t space_between_markers=background_markers[iterat];space_between_markers<=background_markers[iterat+1];++space_between_markers)
        {
            top_fraction+=(space_between_markers-x_medium)*(histogram->GetBinContent(space_between_markers)-y_medium);
            bottom_fraction+=(space_between_markers-x_medium)*(space_between_markers-x_medium);
         }
    }
    //We divide the bottom from the top and we get the slope and also from the line equation we can get the addition
    slope=top_fraction/bottom_fraction;
    addition=y_medium-slope*x_medium;
    //Here we calculate the reduced chi2. We want the reduced chi2 to be around 1
    for(Int_t iterat=0;iterat<=background_markers.size();iterat+=2)
    {
        for(Double_t space_between_markers=background_markers[iterat];space_between_markers<=background_markers[iterat+1];++space_between_markers)
        {
            //The expected value is the value on our line
            expected_value=slope*space_between_markers+addition;
            //The observed value is the actual value from our histogram
            observed_value=histogram->GetBinContent(space_between_markers);
            medium_value+=expected_value;
            reduced_chi2+=((observed_value-expected_value)*(observed_value-expected_value))/expected_value;
        }
    }
    //Same for the medium x and y we divide by the number of points for the chi2 to get the reduced chi2 and from the sum of all values to get the average
    reduced_chi2/=number_of_variators;
    medium_value/=number_of_variators;
    //We output the medium value and Reduced chi2 to the screen so the user can see
    std::cout<<"Medium value:"<<medium_value<<"  Reduced chi2:"<<reduced_chi2<<std::endl;
}

//This is a function that switches the values between to variables
void switch_values(Double_t &value1,Double_t &value2)
{
    Double_t placeholder;
    placeholder=value1;
    value1=value2;
    value2=placeholder;
}

//This function is given the histogram the integral markers and the background markers as vectors and the slope and addition references which will be returned to draw the best fitted line
void integral_function(TH1F *histogram,std::vector<Double_t> integral_markers,std::vector<Double_t> background_markers,Double_t &slope,Double_t &addition)
{
    //We declare the error of the integral and the area which will be used later on
    Double_t error=0,area=0;
    //We set the line to pe y=0 as if having no background
    slope=0;
    addition=0;
    //We check if we even have background markers if not we proceed but if we do we try to calculate the best fitted line
    if(background_markers.size()>0)
    {
        //We check if the background markers are in pairs, if they are we calculate the best fitted line if not we tell the user to put markers in pairs and then stop the integral execution
        if(background_markers.size()%2==0)
        {
            //We check if some of the background markers are overlapping, if they are we invert them when we sort them but we still infrom the user of this
            if(overlapping_markers(background_markers)==true)
            {
                std::cout<<"Some markers are overlapping, so the overlapping markers were inverted.\n";
            }
            //the function from above is called which gives us the slope and the addition of the line basically the equation of the best fitted line
            get_best_fitted_line(histogram,background_markers,slope,addition);
        }
        else
        {
            std::cout<<"Background markers have to be in multiples of 2\n\n";
            return;
        }
    }
    //Now we are done with the background we go on with the integral
    //We check if we even have markers if we don t we tell the user to put some and stop the function if we do have them we proceed
    if(integral_markers.size()>0)
    {
        //We also chek if the markers are in pairs if they are not we tell the user to put them in pairs if they are we proceed
        if(integral_markers.size()%2==0)
        {
            //Then we get the markers by pairs and feed them in the function above that calculates the integral with no background
            for(Int_t iterator1=0;iterator1<integral_markers.size();iterator1+=2)
            {
                area=integral_no_background(histogram,error,integral_markers[iterator1],integral_markers[iterator1+1]);
                //We check if the two markers are ordered apropietly if they are not we switch them. This is done so our for function can go from lowest to highest.
                if(integral_markers[iterator1]>integral_markers[iterator1+1])
                {
                    switch_values(integral_markers[iterator1],integral_markers[iterator1+1]);
                }
                //We also subtract the values of the background from the integral even if we have markers or not
                for(Int_t iterator2=(Int_t)integral_markers[iterator1];iterator2<=(Int_t)integral_markers[iterator1+1];++iterator2)
                {
                    area-=(slope*iterator2+addition);
                }
                //Here we check if we have more than 2 markers so to know if we should start numbering the integrals
                if(integral_markers.size()>2)
                    std::cout<<"Integral #"<<iterator1/2+1<<":"<<area<<" Error of integral:"<<error<<std::endl;
                else
                    std::cout<<"Integral:"<<area<<" Error of integral:"<<error<<std::endl;
            }
            //We put a new line so that the comand prompt is cleaner
            std::cout<<std::endl;
        }
        else
        {
            std::cout<<"Integral markers have to be in multiples of 2\n\n";
            return;
        }
    }
    else
    {
        std::cout<<"Place markers for the integral to be calculated\n";
        return;
    }
}




/*
//This is a previous version which will be kept for inspiration



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


//integrala no background scirsa "de mana"
double_t integrala_no_background(TH1F *histogram, Double_t pozitie_marke_stanga, Double_t pozitie_marker_dreapta, )
{
    double_t aria=0;
    for(int_t iterator=(int_t)pozitie_marker_stanga;iterator<=(int_t)pozitie_marker_dreapta;++iterator)
    {
        aria+=histogram->GetBinContent(iterator);
    }
    return aria;
}


//Stiind doua puncte aceasta functie returneaza o dreapta adica din ecuatia y=mx+n returneaza m si n
void get_dreapta(Double_t x1,Double_t y1,Double_t x2,Double_t y2,Double_t &m,Double_t &n)
{
    m=(y1-y2)/(x1-x2);
    n=y2-((y1-y2)/(x1-x2))*x2;
}

Double_t absolute_nonzero(Double_t number)
{
    if(number<0)
        return -number;
    if(number==0)
        return 1;
    return number;
}*/
