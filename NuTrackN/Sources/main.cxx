#include <QApplication>
#include <QIcon>

#include "Design.h"
#include "TApplication.h"
#include "canvas.h"

int main(int argc, char **argv) {
  // Initialize CERN ROOT application environment embedded within Qt
  TApplication rootapp("NuTrackN", &argc, argv);

  // Initialize Qt Application
  QApplication app(argc, argv);
  app.setApplicationName("NuTrackN");
  app.setApplicationDisplayName("NuTrackN - Gamma Spectroscopy Analysis");
  app.setOrganizationName("NuGASP");
  // Scale global application default font by 50%
  QFont appFont = app.font();
  if (appFont.pointSizeF() > 0) {
    appFont.setPointSizeF(appFont.pointSizeF() * 1.5);
  } else if (appFont.pointSize() > 0) {
    appFont.setPointSize(static_cast<int>(std::round(appFont.pointSize() * 1.5)));
  } else if (appFont.pixelSize() > 0) {
    appFont.setPixelSize(static_cast<int>(std::round(appFont.pixelSize() * 1.5)));
  } else {
    appFont.setPointSize(14);
  }
  app.setFont(appFont);

  // Instantiate main analysis canvas widget
  QMainCanvas mainWindow(nullptr);
  mainWindow.setWindowTitle("NuTrackN - Gamma Spectroscopy Analysis");
  mainWindow.setWindowIcon(QIcon("icon.png"));

  // Set initial window geometry to comfortably accommodate 50% larger UI elements
  mainWindow.setGeometry(80, 80, 1200, 800);
  mainWindow.show();

  // Attach command prompt panel to the canvas layout
  addCommandPrompt(&mainWindow);

  // Ensure the application exits when the last window is closed
  QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app,
                   &QCoreApplication::quit);

  return app.exec();
}
