#ifndef INTEGRAL_H
#define INTEGRAL_H
#include "TH1F.h"
#include "TH1.h"
#include <vector>
#include <algorithm>

Double_t integral_no_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right);

void get_dreapta(Double_t x1,Double_t y1,Double_t x2,Double_t y2,Double_t &m,Double_t &n);

Double_t absolute_nonzero(Double_t);

Double_t integral_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right, std::vector<Double_t>,Double_t &m,Double_t &n);

#endif
