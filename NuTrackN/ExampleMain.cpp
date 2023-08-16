// Author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "THttpServer.h"
#include "TH1F.h"
#include "TH2I.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TSystem.h"
#include <iostream>
#include <fstream>


int main(int argc, char **argv)
{
    auto serv = new THttpServer("http:8081");
    auto c1 = new TCanvas("c1");

    std::ifstream file("../NuTrackN/Ge2-2DN", std::ios::in);
    if (!file) {
        std::cerr << "Error: could not open file" << std::endl;
        return 1;
    }

    // read the data into a vector
    std::vector<uint32_t> data;
    uint32_t value;
    while (file.read(reinterpret_cast<char*>(&value), sizeof(value))) {
        data.push_back(value);
    }

    uint matrix_size;
    matrix_size=sqrt(data.size());

    auto TH2matrix = new TH2I("Matrix", "Matrix title", 1024, 0, matrix_size-1, 1024, 0, matrix_size-1);

    int factor=matrix_size/1024;

    // print the contents of the vector
    for (int i=0;i<matrix_size;i++) {
        for (int j=0;j<matrix_size;j++)
            TH2matrix->AddBinContent( TH2matrix->GetBin(i/factor,j/factor,0) ,data[i*matrix_size+j]);
    }

    c1->cd();
    TH2matrix->Draw("COLZ");

    auto matrixXAxis = TH2matrix->GetXaxis();

    while (!gSystem->ProcessEvents()) {

         matrixXAxis = TH2matrix->GetXaxis();
         std::cout<<gPad->GetUxmin()<<" "<<gPad->GetUxmax()<<std::endl;

         c1->Modified();
         c1->Update();
         gSystem->Sleep(1000);
      }
}
