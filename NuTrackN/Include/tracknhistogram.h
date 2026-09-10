#ifndef TRACKNHISTOGRAM_H
#define TRACKNHISTOGRAM_H

#include <TH1.h>
#include "iostream"

class TracknHistogram : public TH1F
{
public:
    TracknHistogram(const char *name,const char *title,Int_t nbins,Double_t xlow,Double_t xup);

    void ExecuteEvent(Int_t, Int_t , Int_t);
};

#endif // TRACKNHISTOGRAM_H
