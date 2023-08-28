
#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <QWidget>
#include <iostream>
#include <fstream>
#include <tracknhistogram.h>

#include "Integral.h"
#include <QPushButton>
#include <QLayout>
#include <QTimer>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QAction>
#include <QKeySequence>

#include <stdlib.h>

#include <TCanvas.h>
#include <TVirtualX.h>
#include <TSystem.h>
#include <TFormula.h>
#include <TF1.h>
#include <TFrame.h>
#include <TTimer.h>

#include <QLabel>
#include <QPicture>
#include <QPainter>

#include <vector>

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
   bool controlKeyIsPressed=0;
   bool cKeyWasPressed=0;

protected:
   TCanvas        *fCanvas;

   virtual void    mouseMoveEvent( QMouseEvent *e );
   virtual void    mousePressEvent( QMouseEvent *e );
   virtual void    mouseReleaseEvent( QMouseEvent *e );
   virtual void    keyPressEvent(QKeyEvent *event);
   virtual void    keyReleaseEvent(QKeyEvent *event);
   virtual void    paintEvent( QPaintEvent *e );
   virtual void    resizeEvent( QResizeEvent *e );

signals:
   void requestIntegrationNoBackground();
   void requestIntegrationWithBackground();
   void autoFitRequested(int, int);
};

class QMainCanvas : public QWidget
{
   Q_OBJECT

public:
   QMainCanvas( QWidget *parent = 0);
   virtual ~QMainCanvas() {}
   virtual void changeEvent(QEvent * e);
   //The histogram which is declared globally so every function can access it
   TracknHistogram *h1f;
   //These are some global variables for the integral function which are the parameters for the best fitted line of the background
   Double_t slope=0,addition=0;

public slots:
   void clicked1();
   void areaFunction();
   void areaFunctionWithBackground();
   void handle_root_events();
   void autoFit(int, int);

protected:
   //virtual void paintEvent(QPaintEvent *event);
   QRootCanvas    *canvas;
   QPushButton    *b;
   QTimer         *fRootTimer;
};


#endif


