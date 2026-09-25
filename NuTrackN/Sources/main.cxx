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
  // Initialize standardized typography from Design module (Category 2: Dialogs & App default)
  Design::initializeTypography();
  app.setFont(Design::getDialogFont());

  // Instantiate main analysis canvas widget
  QMainCanvas mainWindow(nullptr);
  mainWindow.setWindowTitle("NuTrackN - Gamma Spectroscopy Analysis");
  mainWindow.setWindowIcon(QIcon("icon.png"));

  // Set initial window geometry to comfortably accommodate 50% larger UI elements
  mainWindow.setGeometry(80, 80, 1200, 800);
  mainWindow.show();

  // Attach command prompt panel to the canvas layout
  addCommandPrompt(&mainWindow);

  // Apply complete saved design theme (UI background, button styles, readouts, dialog styles, canvas bg & spectra)
  Design::applyUITheme(&mainWindow);

  // Ensure all color and typography settings are synced to disk upon application exit
  QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
    Design::saveSettings();
  });

  // Ensure the application exits when the last window is closed
  QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app,
                   &QCoreApplication::quit);

  return app.exec();
}
