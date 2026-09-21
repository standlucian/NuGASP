TEMPLATE = app

# Qt modules
QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Compiler configuration
CONFIG += qt warn_on thread

QMAKE_CXXFLAGS += -fPIC

# Project and ROOT header directories
# ROOT installation is specified through the ROOTSYS environment variable.
INCLUDEPATH += \
            $(ROOTSYS)/include \
            Include

# ROOT libraries
LIBS += \
    -L$(ROOTSYS)/lib \
    -lCore \
    -lRIO \
    -lNet \
    -lHist \
    -lGraf \
    -lGraf3d \
    -lGpad \
    -lTree \
    -lRint \
    -lPostscript \
    -lMatrix \
    -lPhysics \
    -lGui \
    -lMathCore \
    -lSpectrum

# Project headers
HEADERS += \
    Include/canvas.h \
    Include/Integral.h \
    Include/calib.h \
    Include/tracknhistogram.h \
    Include/Design.h \
    Include/PeakFit.h \
    Include/SpectrumImportDialog.h \
    Include/SpectrumExportDialog.h \
    Include/TrackFitDialog.h

# Project source files
SOURCES += \
    Sources/canvas.cxx \
    Sources/ZoomHUD.cxx \
    Sources/RootCanvas.cxx \
    Sources/CanvasGrid.cxx \
    Sources/CanvasNavigation.cxx \
    Sources/CanvasMarkers.cxx \
    Sources/CanvasPeaks.cxx \
    Sources/main.cxx \
    Sources/Integral.cxx \
    Sources/calib.cxx \
    Sources/tracknhistogram.cxx \
    Sources/Design.cxx \
    Sources/PeakFit.cxx \
    Sources/SpectrumImportDialog.cxx \
    Sources/SpectrumExportDialog.cxx \
    Sources/TrackFitDialog.cxx


