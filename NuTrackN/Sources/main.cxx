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
  app.setWindowIcon(QIcon("icon.png"));

  // Instantiate main analysis canvas widget
  QMainCanvas mainWindow(nullptr);
  mainWindow.setWindowTitle("NuTrackN - Gamma Spectroscopy Analysis");
  mainWindow.setWindowIcon(QIcon("icon.png"));

  // Set initial window geometry and attach the command prompt interface
  mainWindow.setGeometry(100, 100, 1024, 720);
  mainWindow.show();

  // Attach command prompt panel to the canvas layout
  addCommandPrompt(&mainWindow);

  // Ensure the application exits when the last window is closed
  QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app,
                   &QCoreApplication::quit);

  return app.exec();
}
