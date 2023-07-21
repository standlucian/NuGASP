#include "TH1F.h"
#include "TCanvas.h"
#include <iostream>
#include <fstream>


void spectru()
{
    TH1F *spectrum = new TH1F("spectrum","Spectru", 30000, 0, 30000); // cream o histograma
    fstream file; // declaram fisier
    file.open("date.txt", ios::in); // Deschidem fisierul

    double value; // variabila in care se citeste

    while(1) // cat timp se citeste
    {
        file >> value; // se citeste in variabila value
        spectrum -> Fill(value); // spectrul se umple cu valori
        if(file.eof()) break; 
    }
    file.close();

    TCanvas *c1 = new TCanvas(); // zona in care dam plot
    spectrum->Draw(); // plot

}