cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(NuTrackN LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics Gui MathCore Spectrum)
include(${ROOT_USE_FILE})

include_directories(
    Include
    ${ROOT_INCLUDE_DIRS}
)

set(SOURCES
    Sources/canvas.cxx
    Sources/ZoomHUD.cxx
    Sources/RootCanvas.cxx
    Sources/CanvasGrid.cxx
    Sources/CanvasNavigation.cxx
    Sources/CanvasMarkers.cxx
    Sources/CanvasPeaks.cxx
    Sources/main.cxx
    Sources/Integral.cxx
    Sources/calib.cxx
    Sources/tracknhistogram.cxx
    Sources/Design.cxx
    Sources/PeakFit.cxx
    Sources/SpectrumImportDialog.cxx
    Sources/SpectrumExportDialog.cxx
    Sources/TrackFitDialog.cxx
)

set(HEADERS
    Include/canvas.h
    Include/Integral.h
    Include/calib.h
    Include/tracknhistogram.h
    Include/Design.h
    Include/PeakFit.h
    Include/SpectrumImportDialog.h
    Include/SpectrumExportDialog.h
    Include/TrackFitDialog.h
)

add_executable(nutrackn ${SOURCES} ${HEADERS})

target_link_libraries(nutrackn
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${ROOT_LIBRARIES}
)
