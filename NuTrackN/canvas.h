
#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <QWidget>
#include <iostream>
#include <fstream>

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
#include <TH1.h>
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

protected:
   TCanvas        *fCanvas;

   virtual void    mouseMoveEvent( QMouseEvent *e );
   virtual void    mousePressEvent( QMouseEvent *e );
   virtual void    mouseReleaseEvent( QMouseEvent *e );
   virtual void    paintEvent( QPaintEvent *e );
   virtual void    resizeEvent( QResizeEvent *e );
};

class QMainCanvas : public QWidget
{
   Q_OBJECT

public:
   QMainCanvas( QWidget *parent = 0);
   virtual ~QMainCanvas() {}
   virtual void changeEvent(QEvent * e);
   //The histogram which is declared globally so every function can access it
   TH1F *h1f;

public slots:
   void clicked1();
   void areaFunction();
   void areaFunctionWithBackground();
   void handle_root_events();

protected:
   //virtual void paintEvent(QPaintEvent *event);
   QRootCanvas    *canvas;
   QPushButton    *b;
   QTimer         *fRootTimer;
};


#endif


