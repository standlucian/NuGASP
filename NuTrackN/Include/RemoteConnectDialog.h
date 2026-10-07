#ifndef REMOTECONNECTDIALOG_H
#define REMOTECONNECTDIALOG_H

#include <QDialog>
#include <memory>
#include <vector>
#include "RemoteClient.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QRadioButton;
class QPushButton;
class QLabel;

class RemoteConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteConnectDialog(std::shared_ptr<RemoteSession> session, QWidget *parent = nullptr);
    ~RemoteConnectDialog() override = default;

    RemoteProfile getActiveProfile() const { return m_activeProfile; }

private slots:
    void onProfileSelected(int index);
    void onNewProfileClicked();
    void onSaveProfileClicked();
    void onDeleteProfileClicked();
    void onBrowseKeyFileClicked();
    void onAuthTypeChanged();
    void onConnectClicked();

private:
    void setupUI();
    void loadProfilesToCombo();
    void populateFormFromProfile(const RemoteProfile &profile);
    RemoteProfile readFormToProfile() const;

    std::shared_ptr<RemoteSession> m_session;
    std::vector<RemoteProfile>      m_profiles;
    RemoteProfile                  m_activeProfile;

    QComboBox    *m_comboProfiles{nullptr};
    QPushButton  *m_btnNewProfile{nullptr};
    QPushButton  *m_btnDeleteProfile{nullptr};
    QPushButton  *m_btnSaveProfile{nullptr};

    QLineEdit    *m_editName{nullptr};
    QLineEdit    *m_editHost{nullptr};
    QSpinBox     *m_spinPort{nullptr};
    QLineEdit    *m_editUser{nullptr};
    QRadioButton *m_radioKeyAuth{nullptr};
    QRadioButton *m_radioPassAuth{nullptr};
    QLineEdit    *m_editKeyFile{nullptr};
    QPushButton  *m_btnBrowseKey{nullptr};
    QLineEdit    *m_editPassword{nullptr};
    QLineEdit    *m_editStartDir{nullptr};

    QLabel       *m_lblStatus{nullptr};
    QPushButton  *m_btnConnect{nullptr};
    QPushButton  *m_btnCancel{nullptr};
};

#endif // REMOTECONNECTDIALOG_H
