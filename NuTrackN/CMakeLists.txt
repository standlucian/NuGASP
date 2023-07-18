cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(NuTrackN)

find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED WebEngine WebEngineWidgets)

add_executable(NuTrackN
    ExampleMain.cpp
)

target_link_libraries(NuTrackN
   Qt5::Core Qt5::Widgets Qt5::WebEngine Qt5::WebEngineWidgets
   ${ROOT_LIBRARIES} ROOT::ROOTWebDisplay ROOT::Gpad ROOT::WebGui6 ROOT::ROOTGpadv7 ROOT::ROOTCanvasPainter ROOT::Geom ROOT::ROOTBrowserv7 ROOT::ROOTEve
)
