#include "RemoteConnectDialog.h"
#include "Design.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QApplication>

RemoteConnectDialog::RemoteConnectDialog(std::shared_ptr<RemoteSession> session, QWidget *parent)
    : QDialog(parent), m_session(session)
{
    setWindowTitle(tr("Connect to Remote Data Computer (SSH / SFTP)"));
    resize(580, 520);
    setFont(Design::getDialogFont());
    setStyleSheet(Design::getDialogStyleSheet());

    setupUI();
    loadProfilesToCombo();
}

void RemoteConnectDialog::setupUI()
{
    const QFont dlgFont = Design::getDialogFont();
    const int pt = dlgFont.pointSize() > 0 ? dlgFont.pointSize() : 11;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // 1. Header Banner
    QLabel *lblTitle = new QLabel(
        tr("<b>Remote Data Session:</b> Connect directly to DAQ/lab computers over secure SFTP.<br>"
           "Files are loaded locally with 0 network lag, with full run cycling and remote write-back."), this);
    lblTitle->setFont(dlgFont);
    lblTitle->setStyleSheet(QString("color: %1; margin-bottom: 4px; font-family: \"%2\"; font-size: %3pt;")
        .arg(Design::getDialogAccentColor().name(), dlgFont.family()).arg(pt));
    mainLayout->addWidget(lblTitle);

    // 2. Profile Selection Bar
    QGroupBox *grpProfile = new QGroupBox(tr("Saved Profiles & Bookmarks"), this);
    QHBoxLayout *profileLayout = new QHBoxLayout(grpProfile);
    profileLayout->setSpacing(6);

    m_comboProfiles = new QComboBox(grpProfile);
    m_comboProfiles->setFont(dlgFont);
    m_comboProfiles->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(m_comboProfiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteConnectDialog::onProfileSelected);
    profileLayout->addWidget(m_comboProfiles, 1);

    m_btnNewProfile = new QPushButton(tr("+ New"), grpProfile);
    m_btnNewProfile->setFont(dlgFont);
    connect(m_btnNewProfile, &QPushButton::clicked, this, &RemoteConnectDialog::onNewProfileClicked);
    profileLayout->addWidget(m_btnNewProfile);

    m_btnSaveProfile = new QPushButton(tr("Save"), grpProfile);
    m_btnSaveProfile->setFont(dlgFont);
    connect(m_btnSaveProfile, &QPushButton::clicked, this, &RemoteConnectDialog::onSaveProfileClicked);
    profileLayout->addWidget(m_btnSaveProfile);

    m_btnDeleteProfile = new QPushButton(tr("Delete"), grpProfile);
    m_btnDeleteProfile->setFont(dlgFont);
    connect(m_btnDeleteProfile, &QPushButton::clicked, this, &RemoteConnectDialog::onDeleteProfileClicked);
    profileLayout->addWidget(m_btnDeleteProfile);

    mainLayout->addWidget(grpProfile);

    // 3. Connection Parameters Group
    QGroupBox *grpParams = new QGroupBox(tr("Host & Authentication Details"), this);
    QGridLayout *grid = new QGridLayout(grpParams);
    grid->setSpacing(8);

    // Profile Name
    grid->addWidget(new QLabel(tr("Profile Name:"), grpParams), 0, 0);
    m_editName = new QLineEdit(grpParams);
    m_editName->setFont(dlgFont);
    m_editName->setPlaceholderText(tr("e.g. Data PC 1 (Lab)"));
    grid->addWidget(m_editName, 0, 1, 1, 3);

    // Host & Port
    grid->addWidget(new QLabel(tr("Host / IP Address:"), grpParams), 1, 0);
    m_editHost = new QLineEdit(grpParams);
    m_editHost->setFont(dlgFont);
    m_editHost->setPlaceholderText(tr("e.g. 192.168.1.100 or daq.physics.lab"));
    grid->addWidget(m_editHost, 1, 1);

    grid->addWidget(new QLabel(tr("Port:"), grpParams), 1, 2);
    m_spinPort = new QSpinBox(grpParams);
    m_spinPort->setFont(dlgFont);
    m_spinPort->setRange(1, 65535);
    m_spinPort->setValue(22);
    m_spinPort->setFixedWidth(80);
    grid->addWidget(m_spinPort, 1, 3);

    // Username
    grid->addWidget(new QLabel(tr("Username:"), grpParams), 2, 0);
    m_editUser = new QLineEdit(grpParams);
    m_editUser->setFont(dlgFont);
    m_editUser->setPlaceholderText(tr("e.g. data or gasp"));
    grid->addWidget(m_editUser, 2, 1, 1, 3);

    // Authentication Mode
    grid->addWidget(new QLabel(tr("Authentication:"), grpParams), 3, 0);
    QHBoxLayout *authRadioLayout = new QHBoxLayout();
    m_radioKeyAuth = new QRadioButton(tr("SSH Private Key (Recommended)"), grpParams);
    m_radioPassAuth = new QRadioButton(tr("Password"), grpParams);
    m_radioKeyAuth->setFont(dlgFont);
    m_radioPassAuth->setFont(dlgFont);
    m_radioKeyAuth->setChecked(true);

    QButtonGroup *authGroup = new QButtonGroup(this);
    authGroup->addButton(m_radioKeyAuth, 0);
    authGroup->addButton(m_radioPassAuth, 1);
    connect(authGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &RemoteConnectDialog::onAuthTypeChanged);

    authRadioLayout->addWidget(m_radioKeyAuth);
    authRadioLayout->addWidget(m_radioPassAuth);
    authRadioLayout->addStretch(1);
    grid->addLayout(authRadioLayout, 3, 1, 1, 3);

    // SSH Key file row
    QLabel *lblKey = new QLabel(tr("Private Key File:"), grpParams);
    grid->addWidget(lblKey, 4, 0);
    QHBoxLayout *keyLayout = new QHBoxLayout();
    m_editKeyFile = new QLineEdit(grpParams);
    m_editKeyFile->setFont(dlgFont);
    m_editKeyFile->setPlaceholderText(tr("~/.ssh/id_ed25519 or ~/.ssh/id_rsa"));
    m_btnBrowseKey = new QPushButton(tr("Browse..."), grpParams);
    m_btnBrowseKey->setFont(dlgFont);
    connect(m_btnBrowseKey, &QPushButton::clicked, this, &RemoteConnectDialog::onBrowseKeyFileClicked);
    keyLayout->addWidget(m_editKeyFile, 1);
    keyLayout->addWidget(m_btnBrowseKey);
    grid->addLayout(keyLayout, 4, 1, 1, 3);

    // Password row
    QLabel *lblPass = new QLabel(tr("Password / Passphrase:"), grpParams);
    grid->addWidget(lblPass, 5, 0);
    m_editPassword = new QLineEdit(grpParams);
    m_editPassword->setFont(dlgFont);
    m_editPassword->setEchoMode(QLineEdit::Password);
    m_editPassword->setPlaceholderText(tr("Remote account password or private key passphrase"));
    grid->addWidget(m_editPassword, 5, 1, 1, 3);

    // Remote Default / Last Used Folder
    grid->addWidget(new QLabel(tr("Remote Folder:"), grpParams), 6, 0);
    m_editStartDir = new QLineEdit(grpParams);
    m_editStartDir->setFont(dlgFont);
    m_editStartDir->setPlaceholderText(tr("e.g. /home/data/exp_2026/runs"));
    grid->addWidget(m_editStartDir, 6, 1, 1, 3);

    mainLayout->addWidget(grpParams);

    // Status / Message Label
    m_lblStatus = new QLabel(this);
    m_lblStatus->setFont(dlgFont);
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet("color: #e57373; margin-top: 2px;");
    mainLayout->addWidget(m_lblStatus);

    // 4. Bottom Action Buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(1);

    m_btnConnect = new QPushButton(tr("Connect & Open Browser"), this);
    m_btnConnect->setFont(dlgFont);
    m_btnConnect->setStyleSheet(QString("QPushButton { background-color: %1; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"
                                        "QPushButton:hover { opacity: 0.9; }")
                                .arg(Design::getDialogAccentColor().name()));
    connect(m_btnConnect, &QPushButton::clicked, this, &RemoteConnectDialog::onConnectClicked);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setFont(dlgFont);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottomLayout->addWidget(m_btnConnect);
    bottomLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(bottomLayout);

    onAuthTypeChanged();
}

void RemoteConnectDialog::loadProfilesToCombo()
{
    m_comboProfiles->blockSignals(true);
    m_comboProfiles->clear();

    m_profiles = RemoteProfile::loadAllProfiles();
    for (const auto &p : m_profiles) {
        QString label = p.name.isEmpty() ? p.host : p.name;
        m_comboProfiles->addItem(label);
    }
    m_comboProfiles->blockSignals(false);

    if (!m_profiles.empty()) {
        onProfileSelected(0);
    } else {
        onNewProfileClicked();
    }
}

void RemoteConnectDialog::populateFormFromProfile(const RemoteProfile &profile)
{
    m_editName->setText(profile.name);
    m_editHost->setText(profile.host);
    m_spinPort->setValue(profile.port > 0 ? profile.port : 22);
    m_editUser->setText(profile.user);
    m_radioKeyAuth->setChecked(profile.authType == RemoteAuthType::KeyFile);
    m_radioPassAuth->setChecked(profile.authType == RemoteAuthType::Password);
    m_editKeyFile->setText(profile.keyFilePath);
    m_editPassword->setText(profile.password);
    m_editStartDir->setText(profile.lastUsedPath);

    onAuthTypeChanged();
}

RemoteProfile RemoteConnectDialog::readFormToProfile() const
{
    RemoteProfile p;
    p.name = m_editName->text().trimmed();
    p.host = m_editHost->text().trimmed();
    p.port = m_spinPort->value();
    p.user = m_editUser->text().trimmed();
    p.authType = m_radioKeyAuth->isChecked() ? RemoteAuthType::KeyFile : RemoteAuthType::Password;
    p.keyFilePath = m_editKeyFile->text().trimmed();
    p.password = m_editPassword->text();
    p.lastUsedPath = m_editStartDir->text().trimmed();
    return p;
}

void RemoteConnectDialog::onProfileSelected(int index)
{
    if (index >= 0 && index < static_cast<int>(m_profiles.size())) {
        populateFormFromProfile(m_profiles[index]);
        m_lblStatus->clear();
    }
}

void RemoteConnectDialog::onNewProfileClicked()
{
    RemoteProfile newProfile;
    newProfile.name = tr("New Connection Profile");
    newProfile.host = "";
    newProfile.port = 22;
    newProfile.user = "";
    newProfile.authType = RemoteAuthType::KeyFile;
    QString defaultKey = QDir::homePath() + "/.ssh/id_ed25519";
    if (!QFile::exists(defaultKey)) {
        defaultKey = QDir::homePath() + "/.ssh/id_rsa";
    }
    newProfile.keyFilePath = defaultKey;
    newProfile.lastUsedPath = "";

    populateFormFromProfile(newProfile);
    m_editHost->setFocus();
    m_lblStatus->clear();
}

void RemoteConnectDialog::onSaveProfileClicked()
{
    RemoteProfile current = readFormToProfile();
    if (current.host.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Host"), tr("Please provide a Host or IP address to save."));
        return;
    }
    if (current.name.isEmpty()) {
        current.name = current.host;
    }

    int currentIdx = m_comboProfiles->currentIndex();
    if (currentIdx >= 0 && currentIdx < static_cast<int>(m_profiles.size())) {
        m_profiles[currentIdx] = current;
    } else {
        m_profiles.push_back(current);
    }

    RemoteProfile::saveAllProfiles(m_profiles);
    loadProfilesToCombo();
    m_lblStatus->setStyleSheet("color: #81c784;");
    m_lblStatus->setText(tr("Profile '%1' saved successfully.").arg(current.name));
}

void RemoteConnectDialog::onDeleteProfileClicked()
{
    int currentIdx = m_comboProfiles->currentIndex();
    if (currentIdx < 0 || currentIdx >= static_cast<int>(m_profiles.size())) return;

    QString name = m_profiles[currentIdx].name;
    if (QMessageBox::question(this, tr("Delete Profile"),
            tr("Are you sure you want to delete profile '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_profiles.erase(m_profiles.begin() + currentIdx);
        RemoteProfile::saveAllProfiles(m_profiles);
        loadProfilesToCombo();
    }
}

void RemoteConnectDialog::onBrowseKeyFileClicked()
{
    QString initial = m_editKeyFile->text();
    if (initial.isEmpty() || !QFile::exists(initial)) {
        initial = QDir::homePath() + "/.ssh";
    }
    QString keyPath = QFileDialog::getOpenFileName(this, tr("Select SSH Private Key"), initial,
                                                   tr("All Files (*);;SSH Keys (id_*);;PEM Files (*.pem)"));
    if (!keyPath.isEmpty()) {
        m_editKeyFile->setText(keyPath);
    }
}

void RemoteConnectDialog::onAuthTypeChanged()
{
    bool isKey = m_radioKeyAuth->isChecked();
    m_editKeyFile->setEnabled(isKey);
    m_btnBrowseKey->setEnabled(isKey);
    m_editPassword->setPlaceholderText(isKey ? tr("Passphrase for private key (optional)")
                                             : tr("Remote account password"));
}

void RemoteConnectDialog::onConnectClicked()
{
    m_lblStatus->clear();
    RemoteProfile profile = readFormToProfile();

    if (profile.host.isEmpty()) {
        m_lblStatus->setStyleSheet("color: #e57373;");
        m_lblStatus->setText(tr("Error: Host address cannot be empty."));
        return;
    }
    if (profile.user.isEmpty()) {
        m_lblStatus->setStyleSheet("color: #e57373;");
        m_lblStatus->setText(tr("Error: Username cannot be empty."));
        return;
    }

    m_btnConnect->setEnabled(false);
    m_btnConnect->setText(tr("Connecting..."));
    qApp->processEvents();

    QString errorMsg;
    if (!m_session->connectToHost(profile, &errorMsg)) {
        m_btnConnect->setEnabled(true);
        m_btnConnect->setText(tr("Connect & Open Browser"));
        m_lblStatus->setStyleSheet("color: #e57373;");
        m_lblStatus->setText(QString("Connection failed: %1").arg(errorMsg));
        return;
    }

    // Auto-save the profile upon successful connection
    onSaveProfileClicked();
    m_activeProfile = profile;

    m_btnConnect->setEnabled(true);
    m_btnConnect->setText(tr("Connected!"));

    accept();
}
