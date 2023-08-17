#ifndef TRACKNHISTOGRAM_H
#define TRACKNHISTOGRAM_H

#include <TH1.h>

class TracknHistogram : public TH1F
{
public:
    TracknHistogram(const char *name,const char *title,Int_t nbins,Double_t xlow,Double_t xup);
};

#endif // TRACKNHISTOGRAM_H
