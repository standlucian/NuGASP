TEMPLATE = app

# Qt modules
QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Compiler configuration (C++17 required by modern CERN ROOT)
CONFIG += qt warn_on thread c++17
CONFIG -= c++11 c++14
QMAKE_CXXFLAGS += -fPIC -std=c++17
QMAKE_CXXFLAGS_CXX11 = -std=c++17
QMAKE_CXXFLAGS_CXX14 = -std=c++17
QMAKE_CXXFLAGS_CXX1Z = -std=c++17
QMAKE_CXXFLAGS_CXX17 = -std=c++17

# Project and ROOT header directories
# Automatically detect ROOT via root-config or ROOTSYS environment variable.
ROOT_INCDIR = $$system(root-config --incdir 2>/dev/null)
isEmpty(ROOT_INCDIR) {
    ROOT_INCDIR = $(ROOTSYS)/include $(ROOTSYS)/include/root
}
INCLUDEPATH += \
            $$ROOT_INCDIR \
            $(ROOTSYS)/include \
            $(ROOTSYS)/include/root \
            Include

ROOT_LIBDIR = $$system(root-config --libdir 2>/dev/null)
isEmpty(ROOT_LIBDIR) {
    ROOT_LIBDIR = $(ROOTSYS)/lib
}

# ROOT libraries
LIBS += \
    -L$$ROOT_LIBDIR \
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
    Include/TrackFitDialog.h \
    Include/complib.h \
    Include/MatrixReader.h \
    Include/MatrixDialog.h \
    Include/IntegralDialog.h \
    Include/DisplayParamsDialog.h \
    Include/EfficiencyDialog.h \
    Include/AutoCalibDialog.h \
    Include/MacroDialog.h \
    Include/HelpDialog.h

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
    Sources/TrackFitDialog.cxx \
    Sources/complib.c \
    Sources/MatrixReader.cxx \
    Sources/MatrixDialog.cxx \
    Sources/IntegralDialog.cxx \
    Sources/DisplayParamsDialog.cxx \
    Sources/EfficiencyDialog.cxx \
    Sources/AutoCalibDialog.cxx \
    Sources/MacroDialog.cxx \
    Sources/HelpDialog.cxx

RESOURCES += \
    nutrackn.qrc
