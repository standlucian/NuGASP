#include "tracknhistogram.h"

TracknHistogram::TracknHistogram(const char *name,const char *title,Int_t nbins,Double_t xlow,Double_t xup)
: TH1F(name,title,nbins,xlow,xup)
{

}

////////////////////////////////////////////////////////////////////////////////
/// Execute action corresponding to one event.
///
/// This member function is called when a histogram is clicked with the locator
///
/// If Left button clicked on the bin top value, then the content of this bin
/// is modified according to the new position of the mouse when it is released.
