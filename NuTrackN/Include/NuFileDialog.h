#ifndef NUFILEDIALOG_H
#define NUFILEDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include "RemoteClient.h"

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QProgressBar;
class QButtonGroup;
class QRadioButton;
class QListWidget;

enum class NuFileMode {
    OpenSpectrum = 0,   // Single 1D spectrum (*.spe, *.dat, *.txt, *.asc, etc.)
    OpenMatrix,         // 2D coincidence matrix (*.cmat, *.mat)
    OpenFile,           // Generic single file
    OpenMultipleFiles,  // Multiple files (e.g. sequence runs for RunByRunManager)
    SaveFile,           // File export / save
    SelectDirectory     // Directory selection
};

/**
 * @brief NuFileDialog is the unified file chooser for NuGASP / NuTrackN.
 *        It provides seamless switching between the Local filesystem and
 *        a connected Remote SSH/SFTP server for all I/O operations (reading,
 *        writing, matrix gating, run sequences, and calibration data).
 */
class NuFileDialog : public QDialog {
    Q_OBJECT

public:
    explicit NuFileDialog(NuFileMode mode,
                          const QString &caption = QString(),
                          const QString &initialDir = QString(),
                          const QString &filter = QString(),
                          QWidget *parent = nullptr);
    ~NuFileDialog() override = default;

    bool isRemoteSelected() const { return m_isRemoteMode; }
    QString getSelectedPath() const;              // Local path (or cached path if remote)
    QString getSelectedRemotePath() const;        // Target remote path (empty if local)
    QStringList getSelectedPaths() const;         // Local paths (cached if remote)
    QStringList getSelectedRemotePaths() const;   // Target remote paths

    // High-level static convenience methods
    static QString getOpenSpectrum(QWidget *parent = nullptr,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   QString *outRemotePath = nullptr,
                                   bool *outIsRemote = nullptr);

    static QString getOpenMatrix(QWidget *parent = nullptr,
                                 const QString &caption = QString(),
                                 const QString &dir = QString(),
                                 QString *outRemotePath = nullptr,
                                 bool *outIsRemote = nullptr);

    static QString getOpenFileName(QWidget *parent = nullptr,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = nullptr,
                                   QString *outRemotePath = nullptr,
                                   bool *outIsRemote = nullptr);

    static QStringList getOpenFileNames(QWidget *parent = nullptr,
                                        const QString &caption = QString(),
                                        const QString &dir = QString(),
                                        const QString &filter = QString(),
                                        QString *selectedFilter = nullptr,
                                        QStringList *outRemotePaths = nullptr,
                                        bool *outIsRemote = nullptr);

    static QString getSaveFileName(QWidget *parent = nullptr,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = nullptr,
                                   QString *outRemotePath = nullptr,
                                   bool *outIsRemote = nullptr);

    static QString getExistingDirectory(QWidget *parent = nullptr,
                                        const QString &caption = QString(),
                                        const QString &dir = QString(),
                                        QString *outRemotePath = nullptr,
                                        bool *outIsRemote = nullptr);

    // Upload helper to finalize remote saves from caller's staged file
    static bool finishRemoteSave(const QString &localStagedPath,
                                 const QString &remoteDestPath,
                                 QString *errorMessage = nullptr,
                                 QWidget *parent = nullptr);

private slots:
    void onLocationToggled();
    void onConnectSSHClicked();
    void onNavigateUp();
    void onNavigateHome();
    void onRefresh();
    void onNewFolder();
    void onPathEntered();
    void onTableItemDoubleClicked(int row, int column);
    void onTableSelectionChanged();
    void onFilterChanged();
    void onAcceptActionClicked();

private:
    void setupUI(const QString &caption, const QString &initialFilter);
    void loadDirectory(const QString &path);
    void populateTable();
    bool downloadRemoteFile(const QString &remotePath, QString &outLocalPath);
    bool downloadRemoteFiles(const QStringList &remotePaths, QStringList &outLocalPaths);

    NuFileMode                     m_mode{NuFileMode::OpenFile};
    std::shared_ptr<RemoteSession> m_session;
    bool                           m_isRemoteMode{false};

    // Paths
    QString                        m_currentLocalDir;
    QString                        m_currentRemoteDir;
    std::vector<RemoteFileInfo>    m_currentEntries;

    // Selection outputs
    QString                        m_selectedPath;
    QString                        m_selectedRemotePath;
    QStringList                    m_selectedPaths;
    QStringList                    m_selectedRemotePaths;

    // UI Widgets
    QPushButton  *m_btnRemoteTab{nullptr};
    QPushButton  *m_btnLocalTab{nullptr};
    QPushButton  *m_btnConnectSSH{nullptr};

    QPushButton  *m_btnUp{nullptr};
    QPushButton  *m_btnHome{nullptr};
    QPushButton  *m_btnRefresh{nullptr};
    QPushButton  *m_btnNewFolder{nullptr};
    QLineEdit    *m_editCurrentPath{nullptr};

    QComboBox    *m_comboTypeFilter{nullptr};
    QLineEdit    *m_editSearchFilter{nullptr};

    QTableWidget *m_tableFiles{nullptr};

    // Bottom Bar
    QLabel       *m_lblFileName{nullptr};
    QLineEdit    *m_editSaveFileName{nullptr};
    QLabel       *m_lblSelectionInfo{nullptr};
    QProgressBar *m_progressBar{nullptr};

    QPushButton  *m_btnAction{nullptr};
    QPushButton  *m_btnCancel{nullptr};
};

#endif // NUFILEDIALOG_H
