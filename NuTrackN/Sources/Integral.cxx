#include "Integral.h"
#include "Design.h"


// Calculates the integral between two marker positions.
// The marker order is normalized before calling ROOT's IntegralAndError().
Double_t integral_no_background(TH1F *histogram,
                                Double_t &error,
                                Double_t pozition_marker_left,
                                Double_t pozition_marker_right)
{
    //We check if the markers are in the corect position if the left marker is in the right we invert them
    const Int_t left = static_cast<Int_t>(
        std::min(pozition_marker_left, pozition_marker_right));

    const Int_t right = static_cast<Int_t>(
        std::max(pozition_marker_left, pozition_marker_right));

    return histogram->IntegralAndError(left, right, error);
}

// Checks whether any of the marker intervals overlap.
bool overlapping_markers(const std::vector<Int_t>& markers)
{
    // Markers are stored in pairs: left boundary, right boundary.
    // Each pair defines one interval to be checked against the others.
    for (std::size_t i = 0; i + 1 < markers.size(); i += 2) {
        // Compare this interval with every subsequent interval.
        // Intervals that only touch at a boundary are not considered overlapping.
        for (std::size_t j = i + 2; j + 1 < markers.size(); j += 2) {
            //Order the markers in each interval
            const Double_t left1 = std::min(markers[i], markers[i + 1]);
            const Double_t right1 = std::max(markers[i], markers[i + 1]);

            const Double_t left2 = std::min(markers[j], markers[j + 1]);
            const Double_t right2 = std::max(markers[j], markers[j + 1]);

            if (left1 < right2 && left2 < right1) {
                // An overlap was found; no need to check the remaining intervals, notify the user.
                return true;
            }
        }
    }

    // No pair of intervals overlaps.
    return false;
}

// Calculates the best-fit linear background for the selected background regions.
// The resulting slope and intercept are returned through the reference parameters.
void get_best_fitted_line(TH1F* histogram,
                          std::vector<Int_t> background_markers,
                          Double_t& slope,
                          Double_t& yIntercept)
{
    // Accumulators for the mean x and y values of the selected background bins.
    Double_t xMean=0;
    Double_t yMean=0;

    // Total number of histogram bins included in the background regions.
    size_t numberOfPoints=0;

    // Accumulators used to calculate the slope of the best-fit line.
    Double_t numerator=0;
    Double_t denominator=0;

    // Calculate the mean x and y values of all bins within the background regions.
    for(std::size_t i=0 ; i+1 < background_markers.size(); i += 2)
    {
        for(Int_t bin = background_markers[i] ; bin <= background_markers[i+1] ; ++bin)
        {
            xMean += bin;
            yMean += histogram->GetBinContent(bin);
            ++numberOfPoints;
        }
    }

    // Convert the accumulated sums into mean values.
    xMean /= numberOfPoints;
    yMean /= numberOfPoints;

    // Calculate the numerator and denominator of the least-squares slope.
    //
    // For a linear fit y = slope*x + yIntercept:
    //
    // slope = sum((x - xMean) * (y - yMean))
    //         --------------------------------
    //              sum((x - xMean)^2)
    //
    for(std::size_t i=0 ; i+1 < background_markers.size(); i += 2)
    {
        for(Int_t bin = background_markers[i] ; bin <= background_markers[i+1] ; ++bin)
        {
            numerator += (bin - xMean) * (histogram->GetBinContent(bin) - yMean);
            denominator += (bin - xMean) * (bin - xMean);
         }
    }

    // Calculate the slope and y-intercept of the best-fit line.
    slope = numerator / denominator;
    yIntercept = yMean - slope * xMean;
}

// Calculates the integral between the selected integral markers after
// subtracting the best-fit linear background defined by the background markers.
// The resulting background slope and intercept are returned by reference.
void integral_function(TH1F* histogram,
                       std::vector<Int_t> integral_markers,
                       std::vector<Int_t> background_markers,
                       Double_t& slope,
                       Double_t& addition)
{
    // Variables used for the integral calculation and output formatting.
    Double_t error=0;
    Double_t area=0;
    Double_t middle_of_integration=0;
    Double_t width_of_integral=0;

    std::string temp;
    std::ostringstream tempStringStream;

    // Start with a zero background. This is also the result when no
    // background markers have been specified.
    slope=0;
    addition=0;

    // Calculate the best-fit background line if background markers are present.
    if(!background_markers.empty())
    {
        // Background markers must define complete pairs of intervals.
        if(background_markers.size() % 2 == 0)
        {
            // Warn the user if any background intervals overlap.
            // The calculation is still performed using the selected markers.
            if(overlapping_markers(background_markers) == true)
            {

                const QString warning =
                    "Some background markers are overlapping. "
                    "Please make sure these are the intended marker positions.";

                CommandPrompt::getInstance()->appendPlainText(warning);
                std::cout<<warning.toStdString()<<std::endl;
            }
            //the function from above is called which gives us the slope and the addition of the line basically the equation of the best fitted line
            get_best_fitted_line(histogram,background_markers,slope,addition);
        }
        else
        {
            CommandPrompt::getInstance()->appendPlainText("Background markers have to  be in multiples of 2 \n\n");
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
            //First (fixed) row

            std::cout<<std::left;
            std::cout<<std::setw(10);
            std::cout<<"Integral#";
            std::cout<<std::setw(10);
            std::cout<<"Channel";
            std::cout<<std::setw(15);
            std::cout<<"Energy";
            std::cout<<std::setw(25);
            std::cout<<"Area";
            std::cout<<std::setw(10);
            std::cout<<"Width"<<std::endl;

            QString peakLabel = QString("%1").arg("Integral#",-10,QChar(' '));
            QString channelLabel = QString("%1").arg("Channel",-10, QChar(' '));
            QString energyLabel = QString("%1").arg("Energy",-15, QChar(' '));
            QString areaLabel = QString("%1").arg("Area",-25, QChar(' '));
            QString widthLabel = QString("%1").arg("Width",-10, QChar(' '));

            QString headerRow = QString("%1%2%3%4%5")
                .arg(peakLabel)
                .arg(channelLabel)
                .arg(energyLabel)
                .arg(areaLabel)
                .arg(widthLabel);
            CommandPrompt::getInstance()->appendPlainText(headerRow);


            //Then we get the markers by pairs and feed them in the function above that calculates the integral with no background
            for(unsigned long int iterator1=0;iterator1<integral_markers.size();iterator1+=2)
            {
                area=integral_no_background(histogram,error,integral_markers[iterator1],integral_markers[iterator1+1]);
                //We check if the two markers are ordered apropietly if they are not we switch them. This is done so our for function can go from lowest to highest.
                if(integral_markers[iterator1]>integral_markers[iterator1+1])
                {
                    std::swap(integral_markers[iterator1],integral_markers[iterator1+1]);
                }
                //We also subtract the values of the background from the integral even if we have markers or not
                for(Int_t iterator2=(Int_t)integral_markers[iterator1];iterator2<=(Int_t)integral_markers[iterator1+1];++iterator2)
                {
                    area-=(slope*iterator2+addition);
                }
                //Here we calculate sove values for the output
                //The output is similar to the auto fit output so for more informations check that
                middle_of_integration=(integral_markers[iterator1]+integral_markers[iterator1+1])/2;
                width_of_integral=integral_markers[iterator1+1]-integral_markers[iterator1];
                //Second row that contains variable numbers
                std::cout<<std::setw(10);
                std::cout<<iterator1/2+1;
                std::cout<<std::setw(10);
                std::cout << std::fixed;
                std::cout<<std::setprecision(2)<<middle_of_integration;
                std::cout<<std::setw(15);
                tempStringStream<<std::fixed<<std::setprecision(2)<<middle_of_integration<<"("<<std::setprecision(0)<<ceil(sqrt(middle_of_integration))<<")";
                temp=tempStringStream.str();
                std::cout<<temp;

                tempStringStream.str(std::string());
                std::cout<<std::setw(25);
                tempStringStream<<std::fixed<<std::setprecision(0)<<area<<"("<<std::setprecision(0)<<round(error)<<")";
                temp=tempStringStream.str();
                std::cout<<temp;

                tempStringStream.str(std::string());
                std::cout<<std::setw(10);
                tempStringStream<<std::fixed<<std::setprecision(2)<<width_of_integral<<"("<<std::setprecision(0)<<ceil(sqrt(width_of_integral))<<")"<<std::endl;
                temp=tempStringStream.str();
                std::cout<<temp;

                tempStringStream.str(std::string());


                QString numberStr = QString("%1").arg(QString::number(iterator1/2+1), 0, QChar(' '));
                QString gaussianCenterStr = QString("%1").arg(middle_of_integration,0, ' ', 2);
                QString energyStr = QString("%1(%2)").arg(middle_of_integration, 0, ' ', 2).arg(static_cast<qlonglong>(qCeil(sqrt(middle_of_integration))));
                QString gaussianIntegralStr = QString("%1(%2)").arg(area, 0, ' ', 0).arg(qRound(error));
                QString gaussianFWHMStr = QString("%1(%2)").arg(width_of_integral, 0, ' ', 2).arg(qCeil(sqrt(width_of_integral)));


                QString dataRow = QString("%1%2%3%4%5")
                    .arg(numberStr,-10,QChar(' '))
                    .arg(gaussianCenterStr, -10, QChar(' '))
                    .arg(energyStr, -15, QChar(' '))
                    .arg(gaussianIntegralStr, -25, QChar(' '))
                    .arg(gaussianFWHMStr, -10, QChar(' '));

                // Insert data row into QPlainTextEdit
                CommandPrompt::getInstance()->appendPlainText(dataRow);

            }
            //We put a new line so that the comand prompt is cleaner
            std::cout<<std::endl;
            CommandPrompt::getInstance()->appendPlainText("");
        }
        else
        {
            CommandPrompt::getInstance()->appendPlainText("Integral  markers have to be in multiples of 2 \n\n");
            std::cout<<"Integral markers have to be in multiples of 2\n\n";
            return;
        }
    }
    else
    {
        std::cout<<"Place markers for the integral to be calculated\n";
        CommandPrompt::getInstance()->appendPlainText("Place markers for the integral to be calculated\n");
        return;
    }
}
