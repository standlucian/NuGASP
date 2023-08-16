#ifndef INTEGRAL_H
#define INTEGRAL_H
#include "TH1F.h"
#include "TH1.h"
#include <vector>
#include <algorithm>
#include<iostream>

Double_t integral_no_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right);

//void get_dreapta(Double_t x1,Double_t y1,Double_t x2,Double_t y2,Double_t &m,Double_t &n);

//Double_t absolute_nonzero(Double_t);

//Double_t integral_background(TH1F *histogram,Double_t &error, Double_t pozition_marker_left, Double_t pozition_marker_right, std::vector<Double_t>,Double_t &m,Double_t &n);

bool overlapping_markers(std::vector<Double_t> markers);

void get_best_fitted_line(TH1F *histogram,std::vector<Double_t> background_markers,Double_t &slope,Double_t &addition);

void switch_values(Double_t &value1,Double_t &value2);

void integral_function(TH1F *histogram,std::vector<Double_t> integral_markers,std::vector<Double_t> background_markers,Double_t &slope,Double_t &addition);

#endif
