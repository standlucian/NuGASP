#ifndef REMOTEFILEDIALOG_H
#define REMOTEFILEDIALOG_H

#include <QDialog>
#include <memory>
#include <vector>
#include "RemoteClient.h"

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QProgressBar;

enum class RemoteOpenAction {
    OpenSpectrum = 0,
    OpenMatrix = 1,
    DownloadOnly = 2,
    Cancelled = 3
};

class RemoteFileDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteFileDialog(std::shared_ptr<RemoteSession> session, QWidget *parent = nullptr);
    ~RemoteFileDialog() override = default;

    QString getSelectedRemotePath() const { return m_selectedRemotePath; }
    QString getLocalCachedPath() const { return m_localCachedPath; }
    RemoteOpenAction getOpenAction() const { return m_openAction; }

private slots:
    void onNavigateUp();
    void onNavigateHome();
    void onRefresh();
    void onPathEntered();
    void onTableItemDoubleClicked(int row, int column);
    void onTableSelectionChanged();
    void onFilterChanged();
    void onOpenSpectrumClicked();
    void onOpenMatrixClicked();
    void onDownloadOnlyClicked();

private:
    void setupUI();
    void loadDirectory(const QString &dirPath);
    void populateTable();
    bool downloadSelectedFile(QString &outLocalPath);

    std::shared_ptr<RemoteSession> m_session;
    std::vector<RemoteFileInfo>    m_currentEntries;
    QString                        m_currentDir;
    QString                        m_selectedRemotePath;
    QString                        m_localCachedPath;
    RemoteOpenAction               m_openAction{RemoteOpenAction::Cancelled};

    // UI Controls
    QPushButton  *m_btnUp{nullptr};
    QPushButton  *m_btnHome{nullptr};
    QPushButton  *m_btnRefresh{nullptr};
    QLineEdit    *m_editCurrentPath{nullptr};
    QLineEdit    *m_editSearchFilter{nullptr};
    QComboBox    *m_comboTypeFilter{nullptr};

    QTableWidget *m_tableFiles{nullptr};
    QLabel       *m_lblSelectionInfo{nullptr};
    QProgressBar *m_progressBar{nullptr};

    QPushButton  *m_btnOpenSpectrum{nullptr};
    QPushButton  *m_btnOpenMatrix{nullptr};
    QPushButton  *m_btnDownloadOnly{nullptr};
    QPushButton  *m_btnClose{nullptr};
};

#endif // REMOTEFILEDIALOG_H
