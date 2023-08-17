//// Author: Sergey Linev, GSI  13/01/2021

/*************************************************************************
 * Copyright (C) 1995-2021, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <qstatusbar.h>
#include <qmessagebox.h>
#include <qmenubar.h>
#include <qapplication.h>
#include <qimage.h>
#include <qtimer.h>

#include "canvas.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "THttpServer.h"
#include "TH1F.h"
#include "TH2I.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TSystem.h"
#include "TApplication.h"
#include "TSystem.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TPluginManager.h"
#include "TVirtualGL.h"
#include "TVirtualX.h"


int main(int argc, char **argv)
{
    //Initializing the app
    TApplication rootapp("Simple Qt ROOT Application", &argc, argv);

    //Looking for the visualization manager
 #if QT_VERSION < 0x050000
    if (!gGLManager) {
       TString x = "win32";
       if (gVirtualX->InheritsFrom("TGX11"))
          x = "x11";
       else if (gVirtualX->InheritsFrom("TGCocoa"))
          x = "osx";
       TPluginHandler *ph = gROOT->GetPluginManager()->FindHandler("TGLManager", x);
       if (ph && ph->LoadPlugin() != -1)
          ph->ExecPlugin(0);
    }
    gStyle->SetCanvasPreferGL(true);
 #endif
    QApplication app(argc,argv);
    QMainCanvas m(0);

    m.resize(m.sizeHint());
    //Sets the title of the application window
    m.setWindowTitle("Qt Example - 2Canvas");
    //Sets the initial position of the application on the screen
    m.setGeometry( 100, 100, 699, 499 );
    m.show();
    //Sets the initial size of the application window
    m.resize(700, 500);


    //Closes the application when the window is closed
    QObject::connect( qApp, SIGNAL(lastWindowClosed()), qApp, SLOT(quit()) );

    return app.exec();
}
