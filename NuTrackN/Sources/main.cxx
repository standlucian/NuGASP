#include <QApplication>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QStringList>

#include "Design.h"
#include "TApplication.h"
#include "canvas.h"

class NuApplication : public QApplication {
public:
  NuApplication(int &argc, char **argv)
      : QApplication(argc, argv), m_mainWindow(nullptr) {}

  void setMainWindow(QMainCanvas *w) {
    m_mainWindow = w;
    if (m_mainWindow && !m_pendingFile.isEmpty()) {
      if (m_pendingFile.endsWith(".cmat", Qt::CaseInsensitive)) {
        m_mainWindow->openMatrixFile(m_pendingFile);
      }
      m_pendingFile.clear();
    }
  }

protected:
  bool event(QEvent *event) override {
    if (event->type() == QEvent::FileOpen) {
      QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
      QString file = openEvent->file();
      if (!file.isEmpty()) {
        if (m_mainWindow) {
          if (file.endsWith(".cmat", Qt::CaseInsensitive)) {
            m_mainWindow->openMatrixFile(file);
          }
        } else {
          m_pendingFile = file;
        }
        return true;
      }
    }
    return QApplication::event(event);
  }

private:
  QMainCanvas *m_mainWindow;
  QString m_pendingFile;
};

int main(int argc, char **argv) {
  // Initialize CERN ROOT application environment embedded within Qt
  TApplication rootapp("NuTrackN", &argc, argv);

  // Initialize Qt Application
  NuApplication app(argc, argv);
  app.setApplicationName("NuTrackN");
  app.setApplicationDisplayName("NuTrackN - Gamma Spectroscopy Analysis");
  app.setOrganizationName("NuGASP");
  // Initialize standardized typography from Design module (Category 2: Dialogs & App default)
  Design::initializeTypography();
  app.setFont(Design::getDialogFont());

  // Instantiate main analysis canvas widget
  QMainCanvas mainWindow(nullptr);
  mainWindow.setWindowTitle("NuTrackN - Gamma Spectroscopy Analysis");
  // Set application and window icon
  QIcon appIcon(":/nutrackn.png");
  if (appIcon.isNull()) appIcon = QIcon("nutrackn.png");
  if (appIcon.isNull()) appIcon = QIcon("icon.png");
  app.setWindowIcon(appIcon);
  mainWindow.setWindowIcon(appIcon);

  // Set initial window geometry to comfortably accommodate 50% larger UI elements
  mainWindow.setGeometry(80, 80, 1200, 800);
  mainWindow.show();

  // Attach command prompt panel to the canvas layout
  addCommandPrompt(&mainWindow);

  // Apply complete saved design theme (UI background, button styles, readouts, dialog styles, canvas bg & spectra)
  Design::applyUITheme(&mainWindow);

  // Connect main window to application for file opening events
  app.setMainWindow(&mainWindow);

  // If a .cmat file was passed on the command line (e.g. via double-click / file association or CLI)
  const QStringList args = app.arguments();
  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if (!arg.startsWith("-") && QFileInfo::exists(arg)) {
      if (arg.endsWith(".cmat", Qt::CaseInsensitive)) {
        mainWindow.openMatrixFile(arg);
        break;
      }
    }
  }

  // Ensure all color and typography settings are synced to disk upon application exit
  QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
    Design::saveSettings();
  });

  // Ensure the application exits when the last window is closed
  QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app,
                   &QCoreApplication::quit);

  return app.exec();
}
