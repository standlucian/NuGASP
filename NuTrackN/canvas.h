
#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <iostream>
#include <QFileDialog>
#include <QDataStream>
#include <QFile>
#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <tracknhistogram.h>
#include "Integral.h"
#include "calib.h"
#include <cstdlib>
#include <cstdio>
#include <QComboBox>


#include <QWidget>
#include <QPushButton>
#include <QLayout>
#include <QTimer>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QAction>
#include <QKeySequence>
#include <QFileDialog>
#include <QDataStream>
#include <QFile>
#include <QtMath>
#include <QInputDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QWheelEvent>
#include <QMenu>
#include <QIcon>
#include <QMainWindow>



#include <TCanvas.h>
#include <TVirtualX.h>
#include <TSystem.h>
#include <TFormula.h>
#include <TF1.h>
#include <TFormula.h>
#include <TFrame.h>
#include <TTimer.h>
#include <TFitResult.h>
#include <TLatex.h>
#include <TLine.h>
#include <TMatrixD.h>
#include <Math/Minimizer.h>
#include <TRandom.h>
#include <THStack.h>
#include <TContextMenu.h>
#include <TObjArray.h>



#include <QLabel>
#include <QPicture>
#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <QCloseEvent>

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QPushButton;
class QTimer;
class TCanvas;

class QRootCanvas : public QWidget
{
   Q_OBJECT

public:
   QRootCanvas( QWidget *parent = 0);
   virtual ~QRootCanvas() {}
   TCanvas* getCanvas() { return fCanvas;}


protected:
   TCanvas        *fCanvas;
   Int_t xMousePosition, yMousePosition;   std::vector<Double_t> addSpaceBarMatrixRequested[4][4];


   virtual void    mouseMoveEvent( QMouseEvent *e );
   virtual void    mousePressEvent( QMouseEvent *e );
   virtual void    mouseReleaseEvent( QMouseEvent *e );
   virtual void    keyPressEvent(QKeyEvent *event);
   virtual void    keyReleaseEvent(QKeyEvent *event);
   virtual void    paintEvent( QPaintEvent *e );
   virtual void    resizeEvent( QResizeEvent *e );
   virtual void    wheelEvent(QWheelEvent *e);
   virtual void    showContextMenu(QMouseEvent *e);
   

   bool controlKeyIsPressed=0;
   bool cKeyWasPressed=0;
   bool zKeyWasPressed=0;
   bool mKeyWasPressed=0;
   bool fKeyWasPressed=0;
signals:
   void requestIntegrationNoBackground();
   void requestIntegrationWithBackground();
   void autoFitRequested(int, int);
   void requestClearTheScreen();
   void addBackgroundMarkerRequested(Int_t, Int_t);
   void addIntegralMarkerRequested(Int_t, Int_t);
   void requestDeleteBackgroundMarkers();
   void requestDeleteIntegralMarkers();
   void requestDeleteAllMarkers();
   void requestShowBackgroundMarkers();
   void requestShowIntegralMarkers();
   void requestShowAllMarkers();
   void requestAddRangeMarker(Int_t, Int_t);
   void requestDeleteRangeMarkers();
   void requestShowRangeMarkers();
   void requestAddGaussMarker(Int_t, Int_t);
   void requestDeleteGaussMarkers();
   void requestShowGaussMarkers();
   void requestFitGauss();
   void killSwitch();
   void addSpaceBarMarkerRequested(Int_t, Int_t);
   void requestZoomTheScreen();
   void requesttranslateplusTheScreen();
   void requesttranslateminusTheScreen();
   void requesttranslatedownTheScreen();
   void requesttranslateupTheScreen();
   void fullscreen();
   void requestHelp();
   void mousePilgrimCoordRequest(Double_t , Double_t );
   void mouseLeftClickCoordRequest(Double_t , Double_t );
   void AddLineRequest();
   void DeleteLineRequest();
   void AddCulomnRequest();
   void DeleteCulomnRequest();
   void RefreshScreenRequest();
   void showXY(Double_t , Double_t);
   
};

class QMainCanvas : public QWidget
{
   Q_OBJECT

public:
   QMainCanvas( QWidget *parent = 0);
   virtual ~QMainCanvas() {}
   virtual void changeEvent(QEvent * e);
   virtual void closeEvent(QCloseEvent *e);
   Double_t findMinValueInInterval(int, int);
   Double_t findMaxValueInInterval(int, int);
         int numberoftimes=1;
   std::vector<int> colors_hist={4,2,3,7,6,1,5,28,38,30,8};

   //The histogram which is declared globally so every function can access it
   TracknHistogram *h1f;
   //These are some global variables for the integral function which are the parameters for the best fitted line of the background
   Double_t slope=0,addition=0;
   int SelectedElement_i=1, SelectedElement_j=1; 
   int PilgrimElement_i=1, PilgrimElement_j=1;
   int maxElement_i=1, maxElement_j=1;
   Double_t mousePilgrimX, mousePilgrimY, mouseLeftClickXcoord, mouseLeftClickYcoord;
   TH1F* HijF[12][12];
   std::vector<TH1F*> HijC[12][12];
   TH1F* selectedHisto;
   int maxk=0;
   int maxkk=0;

public slots:
   void clicked1();
   void areaFunction();
   void areaFunctionWithBackground();
   void handle_root_events();
   void autoFit(int, int);
   void clearTheScreen();
   void addBackgroundMarker(Int_t, Int_t);
   void addIntegralMarker(Int_t, Int_t);
   void deleteBackgroundMarkers();
   void deleteIntegralMarkers();
   void deleteAllMarkers();
   void showBackgroundMarkers();
   void showIntegralMarkers();
   void showAllMarkers();
   void addRangeMarker(Int_t, Int_t);
   void deleteRangeMarkers();
   void showRangeMarkers();
   void addGaussMarker(Int_t, Int_t);
   void deleteGaussMarkers();
   void showGaussMarkers();
   void fitGauss();
   void Cal2pMain();
   void zoomTheScreen();
   void translateplusTheScreen();
   void translateminusTheScreen();
   void translatedownTheScreen();
   void translateupTheScreen();
   void zoomOut();
   void addSpaceBarMarker(Int_t, Int_t);
   void offerHelp();
   void AddCulomn();
   void AddLine();
   void IdentifyLastClickedHistogram(Double_t z, Double_t y);
   void IdentifyLastPilgrimHistogram(Double_t x, Double_t y);
   void ColorTheFrameOfTheHistogram();
   void OpenColorSelectionDialog();
   void showXYcoord(Double_t, Double_t);
   void findHistoWithMaxY(std::vector<TH1F> histos);
   void DeleteCulomn();
   void DeleteLine();
   void RefreshScreen();
   //void DrawHisto();

protected:
   //virtual void paintEvent(QPaintEvent *event);
   void checkBackgrounds();
   bool checkRanges();
   bool checkGauss();
   void fitBackground();

   QRootCanvas    *canvas;
   QPushButton    *b;
   QTimer         *fRootTimer;
   TList listOfObjectsDrawnOnScreen;
   std::vector<Double_t> integral_markers;
   std::vector<Double_t> background_markers;
   std::vector<Double_t> spacebar_markers;
   std::vector<Double_t> range_markers;
   std::vector<Double_t> gauss_markers;
   std::vector<Double_t> zoom_markers;
   std::vector<TF1> backgroundFunctionVector;
   std::vector<TF1> gaussianWithBackgroundFunctionVector;
   std::vector<TObject*> autoFitMarkers[12][12];
   std::vector<Double_t> gaussCenters[12][12];
   std::vector<Double_t> gaussCentersHeight[12][12];
   TList autoFitLatex[12][12];
   std::vector<std::string> latexTexts;
   
   Float_t maxValueInHistogram;
   std::vector<Float_t> puncte_calib2p;
   double_t backgroundA0, backgroundA1;
   double_t backgroundIntegral, backgroundIntegralError;
   TMatrixD *backgroundCovarianceMatrix;
      TLine *lineR;TLine *lineL;TLine *lineD;TLine *lineU;
      //Tline *backgroundLine;
      QLabel *labelX;
      QLabel *labelY;
    TFormula *gaussianWithBackground;
    TF1 *gaussianWithBackgroundFunction;
    //TLine *backgroundLine1;
    //TLine *backgroundLine2;


    TFormula *background; 
    TF1 *backgroundFunction;
    TLatex *gaussianCenterMarkerText;
   
   signals:
   void RequestSelectHistogram();
   public slots:


   
};


#endif


