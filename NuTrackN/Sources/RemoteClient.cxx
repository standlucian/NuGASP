#include "RemoteClient.h"

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFile>
#include <QCollator>
#include <algorithm>
#include <iostream>

//==============================================================================
// RemoteProfile Settings Serialization
//==============================================================================

std::vector<RemoteProfile> RemoteProfile::loadAllProfiles()
{
    std::vector<RemoteProfile> profiles;
    QSettings settings("NuGASP", "NuTrackN");
    int size = settings.beginReadArray("RemoteProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        RemoteProfile p;
        p.name = settings.value("name").toString();
        p.host = settings.value("host").toString();
        p.port = settings.value("port", 22).toInt();
        p.user = settings.value("user").toString();
        p.authType = static_cast<RemoteAuthType>(settings.value("authType", 0).toInt());
        p.keyFilePath = settings.value("keyFilePath").toString();
        p.lastUsedPath = settings.value("lastUsedPath").toString();

        if (!p.name.isEmpty() || !p.host.isEmpty()) {
            profiles.push_back(p);
        }
    }
    settings.endArray();

    // If no profiles exist, seed a sensible default
    if (profiles.empty()) {
        RemoteProfile defaultProfile;
        defaultProfile.name = "Data Acquisition PC";
        defaultProfile.host = "192.168.1.100";
        defaultProfile.port = 22;
        defaultProfile.user = "data";
        defaultProfile.authType = RemoteAuthType::KeyFile;
        QString defaultKey = QDir::homePath() + "/.ssh/id_ed25519";
        if (!QFile::exists(defaultKey)) {
            defaultKey = QDir::homePath() + "/.ssh/id_rsa";
        }
        defaultProfile.keyFilePath = defaultKey;
        defaultProfile.lastUsedPath = "/home/data";
        profiles.push_back(defaultProfile);
    }

    return profiles;
}

void RemoteProfile::saveAllProfiles(const std::vector<RemoteProfile> &profiles)
{
    QSettings settings("NuGASP", "NuTrackN");
    settings.beginWriteArray("RemoteProfiles");
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", profiles[i].name);
        settings.setValue("host", profiles[i].host);
        settings.setValue("port", profiles[i].port);
        settings.setValue("user", profiles[i].user);
        settings.setValue("authType", static_cast<int>(profiles[i].authType));
        settings.setValue("keyFilePath", profiles[i].keyFilePath);
        settings.setValue("lastUsedPath", profiles[i].lastUsedPath);
    }
    settings.endArray();
}

void RemoteProfile::updateLastUsedPath(const QString &profileName, const QString &path)
{
    if (profileName.isEmpty() || path.isEmpty()) return;
    auto profiles = loadAllProfiles();
    bool found = false;
    for (auto &p : profiles) {
        if (p.name.compare(profileName, Qt::CaseInsensitive) == 0 ||
            p.host.compare(profileName, Qt::CaseInsensitive) == 0) {
            p.lastUsedPath = path;
            found = true;
            break;
        }
    }
    if (found) {
        saveAllProfiles(profiles);
    }
}

//==============================================================================
// RemoteSession Implementation
//==============================================================================

RemoteSession::RemoteSession()
{
    libssh2_init(0);
}

RemoteSession::~RemoteSession()
{
    disconnect();
    libssh2_exit();
}

bool RemoteSession::isConnected() const
{
    return m_connected && m_session != nullptr && m_sftp != nullptr;
}

void RemoteSession::setCurrentRemoteDir(const QString &dir)
{
    m_currentRemoteDir = sanitizePath(dir);
    if (!m_profile.name.isEmpty()) {
        m_profile.lastUsedPath = m_currentRemoteDir;
        RemoteProfile::updateLastUsedPath(m_profile.name, m_currentRemoteDir);
    }
}

QString RemoteSession::sanitizePath(const QString &path)
{
    QString p = path.trimmed();
    p.replace('\\', '/');
    while (p.contains("//")) {
        p.replace("//", "/");
    }
    if (p.length() > 1 && p.endsWith('/')) {
        p.chop(1);
    }
    return p.isEmpty() ? "/" : p;
}

void RemoteSession::cleanupSocketsAndSession()
{
    if (m_sftp) {
        libssh2_sftp_shutdown(m_sftp);
        m_sftp = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect(m_session, "Normal Shutdown");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
    m_connected = false;
}

void RemoteSession::disconnect()
{
    cleanupSocketsAndSession();
}

bool RemoteSession::connectToHost(const RemoteProfile &profile, QString *errorMessage)
{
    disconnect();
    m_profile = profile;

    if (profile.host.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = "Host/IP address cannot be empty.";
        return false;
    }
    if (profile.user.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = "Username cannot be empty.";
        return false;
    }

    // 1. Resolve host and connect TCP socket
    struct addrinfo hints{}, *res = nullptr, *rp = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    QString portStr = QString::number(profile.port > 0 ? profile.port : 22);
    int rc = getaddrinfo(profile.host.toUtf8().constData(), portStr.toUtf8().constData(), &hints, &res);
    if (rc != 0 || !res) {
        if (errorMessage) *errorMessage = QString("Cannot resolve host '%1': %2").arg(profile.host, gai_strerror(rc));
        return false;
    }

    int sock = -1;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) continue;

        // Set connect timeout (5 seconds)
        struct timeval tv{};
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // Successfully connected
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        if (errorMessage) *errorMessage = QString("Could not connect to %1:%2 (Connection refused or timed out).").arg(profile.host).arg(profile.port);
        return false;
    }
    m_socket = sock;

    // 2. Initialize libssh2 session
    m_session = libssh2_session_init();
    if (!m_session) {
        cleanupSocketsAndSession();
        if (errorMessage) *errorMessage = "Failed to initialize SSH session structure.";
        return false;
    }

    // Handshake
    libssh2_session_set_blocking(m_session, 1);
    rc = libssh2_session_handshake(m_session, m_socket);
    if (rc != 0) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("SSH Handshake failed: %1").arg(errStr ? errStr : "Unknown handshake error");
        cleanupSocketsAndSession();
        return false;
    }

    // 3. Authenticate
    QByteArray userBytes = profile.user.toUtf8();
    bool authSuccess = false;

    if (profile.authType == RemoteAuthType::KeyFile) {
        QString keyFile = profile.keyFilePath.trimmed();
        if (keyFile.isEmpty()) {
            keyFile = QDir::homePath() + "/.ssh/id_ed25519";
            if (!QFile::exists(keyFile)) {
                keyFile = QDir::homePath() + "/.ssh/id_rsa";
            }
        }
        QString pubKeyFile = keyFile + ".pub";
        const char *pubKeyPath = QFile::exists(pubKeyFile) ? pubKeyFile.toUtf8().constData() : nullptr;
        QByteArray privKeyBytes = keyFile.toUtf8();
        QByteArray passBytes = profile.password.toUtf8();

        rc = libssh2_userauth_publickey_fromfile(
            m_session,
            userBytes.constData(),
            pubKeyPath,
            privKeyBytes.constData(),
            passBytes.isEmpty() ? nullptr : passBytes.constData()
        );

        if (rc == 0) {
            authSuccess = true;
        } else {
            char *errStr = nullptr;
            libssh2_session_last_error(m_session, &errStr, nullptr, 0);
            if (errorMessage) *errorMessage = QString("SSH Public Key Auth failed (%1): %2")
                .arg(QFileInfo(keyFile).fileName(), errStr ? errStr : "Access denied");
        }
    } else if (profile.authType == RemoteAuthType::Password) {
        QByteArray passBytes = profile.password.toUtf8();
        rc = libssh2_userauth_password(m_session, userBytes.constData(), passBytes.constData());
        if (rc == 0) {
            authSuccess = true;
        } else {
            char *errStr = nullptr;
            libssh2_session_last_error(m_session, &errStr, nullptr, 0);
            if (errorMessage) *errorMessage = QString("SSH Password Auth failed: %1").arg(errStr ? errStr : "Invalid username or password");
        }
    } else {
        // Agent / Keyboard-interactive fallback
        rc = libssh2_userauth_password(m_session, userBytes.constData(), profile.password.toUtf8().constData());
        if (rc == 0) authSuccess = true;
    }

    if (!authSuccess) {
        cleanupSocketsAndSession();
        return false;
    }

    // 4. Initialize SFTP Subsystem
    m_sftp = libssh2_sftp_init(m_session);
    if (!m_sftp) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("Failed to initialize SFTP subsystem on remote host: %1")
            .arg(errStr ? errStr : "SFTP not available on server");
        cleanupSocketsAndSession();
        return false;
    }

    m_connected = true;

    // Determine starting directory: profile.lastUsedPath or user home dir '.'
    QString startDir = profile.lastUsedPath.trimmed();
    if (startDir.isEmpty()) {
        startDir = "/home/" + profile.user;
    }
    m_currentRemoteDir = sanitizePath(startDir);

    return true;
}

bool RemoteSession::listDirectory(const QString &remotePath, std::vector<RemoteFileInfo> &outList, QString *errorMessage)
{
    outList.clear();
    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QString sanitized = sanitizePath(remotePath);
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_opendir(m_sftp, sanitized.toUtf8().constData());
    if (!handle) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("Failed to open remote directory '%1': %2")
            .arg(sanitized, errStr ? errStr : "Directory not found or permission denied");
        return false;
    }

    char mem[1024];
    char longentry[2048];
    LIBSSH2_SFTP_ATTRIBUTES attrs;

    while (true) {
        int rc = libssh2_sftp_readdir_ex(handle, mem, sizeof(mem), longentry, sizeof(longentry), &attrs);
        if (rc <= 0) {
            break; // EOF or error
        }

        QString entryName = QString::fromUtf8(mem);
        if (entryName == "." || entryName == "..") {
            continue;
        }

        RemoteFileInfo info;
        info.name = entryName;
        info.fullPath = sanitizePath(sanitized + "/" + entryName);
        info.size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? static_cast<qint64>(attrs.filesize) : 0;
        info.isDirectory = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        if (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) {
            info.lastModified = QDateTime::fromSecsSinceEpoch(attrs.mtime);
        }
        info.permissions = attrs.permissions;

        outList.push_back(info);
    }

    libssh2_sftp_closedir(handle);

    // Natural sort: directories first, then filenames numerically sorted
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::sort(outList.begin(), outList.end(), [&collator](const RemoteFileInfo &a, const RemoteFileInfo &b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory; // Directories first
        }
        return collator.compare(a.name, b.name) < 0;
    });

    m_currentRemoteDir = sanitized;
    RemoteProfile::updateLastUsedPath(m_profile.name, m_currentRemoteDir);

    return true;
}

bool RemoteSession::downloadFile(const QString &remotePath, const QString &localPath,
                                 std::function<void(qint64, qint64)> progressCallback,
                                 QString *errorMessage)
{
    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QString cleanRemote = sanitizePath(remotePath);
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(m_sftp, cleanRemote.toUtf8().constData(), LIBSSH2_FXF_READ, 0);
    if (!handle) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("Failed to open remote file '%1': %2")
            .arg(cleanRemote, errStr ? errStr : "File not found or permission denied");
        return false;
    }

    // Get remote file size for progress reporting
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    qint64 totalBytes = 0;
    if (libssh2_sftp_fstat(handle, &attrs) == 0 && (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE)) {
        totalBytes = static_cast<qint64>(attrs.filesize);
    }

    // Ensure parent directory exists locally
    QFileInfo localInfo(localPath);
    QDir().mkpath(localInfo.absolutePath());

    QString tempLocalPath = localPath + ".part";
    QFile localFile(tempLocalPath);
    if (!localFile.open(QIODevice::WriteOnly)) {
        libssh2_sftp_close(handle);
        if (errorMessage) *errorMessage = QString("Cannot create local destination file: %1").arg(localFile.errorString());
        return false;
    }

    constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB buffer
    std::vector<char> buffer(BUFFER_SIZE);
    qint64 bytesDone = 0;

    while (true) {
        ssize_t rc = libssh2_sftp_read(handle, buffer.data(), buffer.size());
        if (rc < 0) {
            localFile.close();
            QFile::remove(tempLocalPath);
            libssh2_sftp_close(handle);
            if (errorMessage) *errorMessage = QString("SFTP read error while downloading '%1'").arg(cleanRemote);
            return false;
        }
        if (rc == 0) {
            break; // EOF reached
        }

        qint64 written = localFile.write(buffer.data(), rc);
        if (written != rc) {
            localFile.close();
            QFile::remove(tempLocalPath);
            libssh2_sftp_close(handle);
            if (errorMessage) *errorMessage = "Local disk write error.";
            return false;
        }

        bytesDone += rc;
        if (progressCallback) {
            progressCallback(bytesDone, totalBytes);
        }
    }

    localFile.close();
    libssh2_sftp_close(handle);

    // Atomically replace target
    if (QFile::exists(localPath)) {
        QFile::remove(localPath);
    }
    if (!QFile::rename(tempLocalPath, localPath)) {
        if (errorMessage) *errorMessage = "Failed to finalize downloaded file in local cache.";
        return false;
    }

    return true;
}

bool RemoteSession::uploadFile(const QString &localPath, const QString &remotePath,
                               std::function<void(qint64, qint64)> progressCallback,
                               QString *errorMessage)
{
    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QFile localFile(localPath);
    if (!localFile.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QString("Cannot open local file '%1' for reading.").arg(localPath);
        return false;
    }

    const qint64 totalBytes = localFile.size();
    QString cleanRemote = sanitizePath(remotePath);

    int flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC;
    long mode = LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH;

    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(m_sftp, cleanRemote.toUtf8().constData(), flags, mode);
    if (!handle) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("Failed to open remote destination '%1' for writing: %2")
            .arg(cleanRemote, errStr ? errStr : "Permission denied");
        return false;
    }

    constexpr size_t BUFFER_SIZE = 64 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);
    qint64 bytesDone = 0;

    while (!localFile.atEnd()) {
        qint64 bytesRead = localFile.read(buffer.data(), buffer.size());
        if (bytesRead <= 0) break;

        char *ptr = buffer.data();
        size_t toWrite = static_cast<size_t>(bytesRead);

        while (toWrite > 0) {
            ssize_t written = libssh2_sftp_write(handle, ptr, toWrite);
            if (written < 0) {
                libssh2_sftp_close(handle);
                if (errorMessage) *errorMessage = QString("SFTP error writing to remote file '%1'").arg(cleanRemote);
                return false;
            }
            toWrite -= written;
            ptr += written;
        }

        bytesDone += bytesRead;
        if (progressCallback) {
            progressCallback(bytesDone, totalBytes);
        }
    }

    libssh2_sftp_close(handle);
    return true;
}

QString RemoteSession::getLocalCachePath(const QString &remotePath) const
{
    QString baseCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseCache.isEmpty()) {
        baseCache = QDir::homePath() + "/.cache/nutrackn";
    }
    baseCache += "/remote";

    // Hash host + user + port to prevent collision across different hosts
    QString hostKey = QString("%1@%2:%3").arg(m_profile.user, m_profile.host).arg(m_profile.port);
    QString hostHash = QString::fromUtf8(QCryptographicHash::hash(hostKey.toUtf8(), QCryptographicHash::Md5).toHex()).left(10);

    QString clean = sanitizePath(remotePath);
    if (clean.startsWith('/')) {
        clean = clean.mid(1);
    }
    return QDir::cleanPath(baseCache + "/" + hostHash + "/" + clean);
}

bool RemoteSession::syncRemoteFileToCache(const QString &remotePath, QString &outLocalPath,
                                         std::function<void(qint64, qint64)> progressCallback,
                                         QString *errorMessage)
{
    outLocalPath = getLocalCachePath(remotePath);
    return downloadFile(remotePath, outLocalPath, progressCallback, errorMessage);
}

bool RemoteSession::listRunFiles(const QString &remoteDirPath, const QString &currentFileName,
                                 std::vector<RemoteFileInfo> &outRunFiles, QString *errorMessage)
{
    std::vector<RemoteFileInfo> allEntries;
    if (!listDirectory(remoteDirPath, allEntries, errorMessage)) {
        return false;
    }

    outRunFiles.clear();
    QString fileName = QFileInfo(currentFileName).fileName();

    // Case 1: GASP style "PREFIX.DIGITS" (e.g. G0.0007, G1.0042, F.0001)
    int lastDot = fileName.lastIndexOf('.');
    if (lastDot > 0) {
        QString afterDot = fileName.mid(lastDot + 1);
        bool isAllDigits = !afterDot.isEmpty();
        for (QChar c : afterDot) {
            if (!c.isDigit()) { isAllDigits = false; break; }
        }
        if (isAllDigits) {
            QString prefix = fileName.left(lastDot + 1); // e.g. "G0."
            for (const auto &info : allEntries) {
                if (info.isDirectory) continue;
                if (info.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QString rest = info.name.mid(prefix.length());
                    bool ok = false;
                    rest.toInt(&ok);
                    if (ok) {
                        outRunFiles.push_back(info);
                    }
                }
            }

            QCollator collator;
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            std::sort(outRunFiles.begin(), outRunFiles.end(), [&collator](const RemoteFileInfo &a, const RemoteFileInfo &b) {
                return collator.compare(a.name, b.name) < 0;
            });
            return !outRunFiles.empty();
        }
    }

    // Case 2: Standard filenames with embedded run numbers (e.g. run_007.spk, r05.mat)
    QFileInfo fInfo(fileName);
    QString base = fInfo.baseName();
    QString suffix = fInfo.completeSuffix().toLower();
    int digitStart = base.length();
    while (digitStart > 0 && base[digitStart - 1].isDigit()) {
        digitStart--;
    }

    if (digitStart < base.length()) {
        QString prefix = base.left(digitStart);
        for (const auto &info : allEntries) {
            if (info.isDirectory) continue;
            QFileInfo eInfo(info.name);
            if (eInfo.completeSuffix().toLower() == suffix && eInfo.baseName().startsWith(prefix, Qt::CaseInsensitive)) {
                QString rest = eInfo.baseName().mid(prefix.length());
                bool ok = false;
                rest.toInt(&ok);
                if (ok) {
                    outRunFiles.push_back(info);
                }
            }
        }

        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(outRunFiles.begin(), outRunFiles.end(), [&collator](const RemoteFileInfo &a, const RemoteFileInfo &b) {
            return collator.compare(a.name, b.name) < 0;
        });
        return !outRunFiles.empty();
    }

    // Fallback: Same extension
    for (const auto &info : allEntries) {
        if (info.isDirectory) continue;
        if (suffix.isEmpty() || QFileInfo(info.name).completeSuffix().toLower() == suffix) {
            outRunFiles.push_back(info);
        }
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(outRunFiles.begin(), outRunFiles.end(), [&collator](const RemoteFileInfo &a, const RemoteFileInfo &b) {
        return collator.compare(a.name, b.name) < 0;
    });

    return !outRunFiles.empty();
}
