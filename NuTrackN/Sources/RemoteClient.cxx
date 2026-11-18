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
#include <poll.h>
#include <errno.h>

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFile>
#include <QCollator>
#include <QElapsedTimer>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <cmath>

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

std::shared_ptr<RemoteSession> RemoteSession::globalSession()
{
    static std::shared_ptr<RemoteSession> s_instance = std::make_shared<RemoteSession>();
    return s_instance;
}

RemoteSession::RemoteSession()
{
    static std::once_flag s_sshInitFlag;
    std::call_once(s_sshInitFlag, []() {
        libssh2_init(0);
    });
}

RemoteSession::~RemoteSession()
{
    disconnect();
}

bool RemoteSession::isConnected() const
{
    QMutexLocker locker(&m_sessionMutex);
    return m_connected && m_session != nullptr && m_sftp != nullptr;
}

QString RemoteSession::getCurrentRemoteDir() const
{
    QMutexLocker locker(&m_sessionMutex);
    return m_currentRemoteDir;
}

void RemoteSession::setCurrentRemoteDir(const QString &dir)
{
    QMutexLocker locker(&m_sessionMutex);
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
    QMutexLocker locker(&m_sessionMutex);
    cleanupSocketsAndSession();
}

bool RemoteSession::reconnectIfDropped()
{
    QMutexLocker locker(&m_sessionMutex);
    if (!m_session) return false;

    int lastErr = libssh2_session_last_errno(m_session);
    // Only reconnect on transport/socket-level disconnects or timeouts, not file-not-found / permission errors
    if (lastErr == LIBSSH2_ERROR_SOCKET_SEND ||
        lastErr == LIBSSH2_ERROR_SOCKET_RECV ||
        lastErr == LIBSSH2_ERROR_SOCKET_DISCONNECT ||
        lastErr == LIBSSH2_ERROR_TIMEOUT ||
        lastErr == LIBSSH2_ERROR_CHANNEL_CLOSED ||
        lastErr == LIBSSH2_ERROR_CHANNEL_EOF_SENT) {
        QString savedDir = m_currentRemoteDir;
        RemoteProfile savedProfile = m_profile;
        if (connectToHost(savedProfile, nullptr)) {
            if (!savedDir.isEmpty()) {
                m_currentRemoteDir = savedDir;
            }
            return true;
        }
    }
    return false;
}

bool RemoteSession::connectToHost(const RemoteProfile &profile, QString *errorMessage)
{
    QMutexLocker locker(&m_sessionMutex);
    cleanupSocketsAndSession();
    m_profile = profile;

    if (profile.host.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = "Host/IP address cannot be empty.";
        return false;
    }
    if (profile.user.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = "Username cannot be empty.";
        return false;
    }

    // 1. Resolve host and connect TCP socket with non-blocking 5s timeout
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

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }

        int connRc = ::connect(sock, rp->ai_addr, rp->ai_addrlen);
        bool connectedOk = false;
        if (connRc == 0) {
            connectedOk = true;
        } else if (errno == EINPROGRESS) {
            struct pollfd pfd{};
            pfd.fd = sock;
            pfd.events = POLLOUT;
            int pollRc = poll(&pfd, 1, 5000); // 5 second connect timeout
            if (pollRc > 0 && (pfd.revents & POLLOUT)) {
                int soError = 0;
                socklen_t len = sizeof(soError);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soError, &len) == 0 && soError == 0) {
                    connectedOk = true;
                }
            }
        }

        if (connectedOk) {
            // Restore blocking mode on socket and set 10s read/write timeout
            if (flags != -1) {
                fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
            }
            struct timeval tv{};
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
            break;
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

    // Set 10-second timeout on blocking libssh2 operations
    libssh2_session_set_blocking(m_session, 1);
    libssh2_session_set_timeout(m_session, 10000);

    // Handshake
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

    auto trySshAgentAuth = [&]() -> bool {
        LIBSSH2_AGENT *agent = libssh2_agent_init(m_session);
        if (!agent) return false;
        bool agentOk = false;
        if (libssh2_agent_connect(agent) == 0) {
            if (libssh2_agent_list_identities(agent) == 0) {
                struct libssh2_agent_publickey *identity = nullptr, *prevIdentity = nullptr;
                while (libssh2_agent_get_identity(agent, &identity, prevIdentity) == 0) {
                    if (libssh2_agent_userauth(agent, userBytes.constData(), identity) == 0) {
                        agentOk = true;
                        break;
                    }
                    prevIdentity = identity;
                }
            }
            libssh2_agent_disconnect(agent);
        }
        libssh2_agent_free(agent);
        return agentOk;
    };

    if (profile.authType == RemoteAuthType::KeyFile) {
        QString keyFile = profile.keyFilePath.trimmed();
        if (keyFile.isEmpty()) {
            keyFile = QDir::homePath() + "/.ssh/id_ed25519";
            if (!QFile::exists(keyFile)) {
                keyFile = QDir::homePath() + "/.ssh/id_rsa";
            }
        }
        QString pubKeyFile = keyFile + ".pub";
        QByteArray pubKeyBytes = pubKeyFile.toUtf8();
        const char *pubKeyPath = QFile::exists(pubKeyFile) ? pubKeyBytes.constData() : nullptr;
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
        } else if (trySshAgentAuth()) {
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
        // Agent or password fallback
        if (trySshAgentAuth()) {
            authSuccess = true;
        } else {
            QByteArray passBytes = profile.password.toUtf8();
            rc = libssh2_userauth_password(m_session, userBytes.constData(), passBytes.constData());
            if (rc == 0) authSuccess = true;
        }
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

    // Determine starting directory: profile.lastUsedPath or root '/'
    QString startDir = profile.lastUsedPath.trimmed();
    if (startDir.isEmpty()) {
        startDir = "/";
    }
    m_currentRemoteDir = sanitizePath(startDir);

    return true;
}

bool RemoteSession::listDirectory(const QString &remotePath, std::vector<RemoteFileInfo> &outList, QString *errorMessage)
{
    QMutexLocker locker(&m_sessionMutex);
    outList.clear();
    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QString sanitized = sanitizePath(remotePath);
    QByteArray pathBytes = sanitized.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_opendir(m_sftp, pathBytes.constData());
    if (!handle && reconnectIfDropped()) {
        handle = libssh2_sftp_opendir(m_sftp, pathBytes.constData());
    }
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

bool RemoteSession::createDirectory(const QString &remotePath, QString *errorMessage)
{
    QMutexLocker locker(&m_sessionMutex);
    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QString clean = sanitizePath(remotePath);
    if (clean.isEmpty() || clean == "/") return true;

    // Check if it already exists
    RemoteFileInfo info;
    if (fileExists(clean, &info) && info.isDirectory) {
        return true;
    }

    // Split components to recursively create parent dirs
    QStringList parts = clean.split('/', Qt::SkipEmptyParts);
    QString cur = clean.startsWith('/') ? "" : "";

    for (int i = 0; i < parts.size(); ++i) {
        cur += "/" + parts[i];

        RemoteFileInfo stepInfo;
        if (fileExists(cur, &stepInfo)) {
            if (!stepInfo.isDirectory) {
                if (errorMessage) *errorMessage = QString("Path '%1' exists but is not a directory.").arg(cur);
                return false;
            }
            continue;
        }

        int rc = libssh2_sftp_mkdir(m_sftp, cur.toUtf8().constData(),
                                    LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
        if (rc != 0) {
            char *errStr = nullptr;
            libssh2_session_last_error(m_session, &errStr, nullptr, 0);
            if (errorMessage) *errorMessage = QString("Failed to create remote directory '%1': %2")
                .arg(cur, errStr ? errStr : "Permission denied");
            return false;
        }
    }

    return true;
}

bool RemoteSession::fileExists(const QString &remotePath, RemoteFileInfo *outInfo)
{
    QMutexLocker locker(&m_sessionMutex);
    if (!isConnected()) return false;

    QString clean = sanitizePath(remotePath);
    QByteArray cleanBytes = clean.toUtf8();
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(m_sftp, cleanBytes.constData(), &attrs);
    if (rc != 0 && reconnectIfDropped()) {
        rc = libssh2_sftp_stat(m_sftp, cleanBytes.constData(), &attrs);
    }
    if (rc != 0) {
        return false;
    }

    if (outInfo) {
        outInfo->name = QFileInfo(clean).fileName();
        outInfo->fullPath = clean;
        outInfo->size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? static_cast<qint64>(attrs.filesize) : 0;
        outInfo->isDirectory = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        outInfo->permissions = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) ? attrs.permissions : 0;
        if (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) {
            outInfo->lastModified = QDateTime::fromSecsSinceEpoch(attrs.mtime);
        }
    }
    return true;
}

bool RemoteSession::testConnection(const RemoteProfile &profile, QString &outDetails, QString *errorMessage)
{
    QElapsedTimer timer;
    timer.start();

    RemoteSession testSession;
    QString err;
    if (!testSession.connectToHost(profile, &err)) {
        if (errorMessage) *errorMessage = err;
        return false;
    }

    qint64 connectMs = timer.elapsed();

    // Check remote working directory
    QString targetDir = profile.lastUsedPath.trimmed();
    if (targetDir.isEmpty()) {
        targetDir = "/";
    }

    std::vector<RemoteFileInfo> entries;
    QString listErr;
    bool dirOk = testSession.listDirectory(targetDir, entries, &listErr);

    outDetails = QString("✓ Successfully connected and verified in %1 ms\n"
                         "• Host: %2@%3:%4\n"
                         "• SSH/SFTP Session: Handshake & Auth OK\n")
                     .arg(connectMs)
                     .arg(profile.user, profile.host).arg(profile.port);

    if (dirOk) {
        outDetails += QString("• Remote Directory '%1': Verified (%2 items found)")
                          .arg(targetDir).arg(entries.size());
    } else {
        outDetails += QString("• Remote Directory '%1': Note: %2")
                          .arg(targetDir, listErr);
    }

    testSession.disconnect();
    return true;
}

bool RemoteSession::downloadFile(const QString &remotePath, const QString &localPath,
                                 std::function<void(qint64, qint64)> progressCallback,
                                 QString *errorMessage,
                                 std::function<bool()> cancelCheck,
                                 bool forceReDownload)
{
    QMutexLocker locker(&m_sessionMutex);

    if (!isConnected()) {
        if (errorMessage) *errorMessage = "Not connected to remote host.";
        return false;
    }

    QString cleanRemote = sanitizePath(remotePath);
    QByteArray remoteBytes = cleanRemote.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(m_sftp, remoteBytes.constData(), LIBSSH2_FXF_READ, 0);
    if (!handle && reconnectIfDropped()) {
        handle = libssh2_sftp_open(m_sftp, remoteBytes.constData(), LIBSSH2_FXF_READ, 0);
    }
    if (!handle) {
        char *errStr = nullptr;
        libssh2_session_last_error(m_session, &errStr, nullptr, 0);
        if (errorMessage) *errorMessage = QString("Failed to open remote file '%1': %2")
            .arg(cleanRemote, errStr ? errStr : "File not found or permission denied");
        return false;
    }

    // Get remote file size and modification timestamp for cache verification and progress reporting
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    qint64 totalBytes = 0;
    bool hasRemoteMtime = false;
    qint64 remoteMtime = 0;
    if (libssh2_sftp_fstat(handle, &attrs) == 0) {
        if (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) {
            totalBytes = static_cast<qint64>(attrs.filesize);
        }
        if (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) {
            hasRemoteMtime = true;
            remoteMtime = static_cast<qint64>(attrs.mtime);
        }
    }

    // Fast-path: Check if already cached with matching file size AND matching modification timestamp
    if (!forceReDownload && QFile::exists(localPath)) {
        QFileInfo localExisting(localPath);
        qint64 localMtime = localExisting.lastModified().toSecsSinceEpoch();
        bool mtimeMatches = !hasRemoteMtime || (std::abs(localMtime - remoteMtime) <= 1);
        if (totalBytes > 0 && localExisting.size() == totalBytes && mtimeMatches) {
            libssh2_sftp_close(handle);
            if (progressCallback) {
                progressCallback(totalBytes, totalBytes);
            }
            return true;
        }
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
        if (cancelCheck && cancelCheck()) {
            localFile.close();
            QFile::remove(tempLocalPath);
            libssh2_sftp_close(handle);
            if (errorMessage) *errorMessage = "Download cancelled by user.";
            return false;
        }

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

    localFile.flush();
    if (hasRemoteMtime) {
        localFile.setFileTime(QDateTime::fromSecsSinceEpoch(remoteMtime), QFileDevice::FileModificationTime);
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
    QMutexLocker locker(&m_sessionMutex);
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

    // Ensure remote parent directory exists
    int lastSlash = cleanRemote.lastIndexOf('/');
    if (lastSlash > 0) {
        QString parentDir = cleanRemote.left(lastSlash);
        createDirectory(parentDir);
    }

    int flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC;
    long mode = LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH;

    QByteArray remoteBytes = cleanRemote.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(m_sftp, remoteBytes.constData(), flags, mode);
    if (!handle && reconnectIfDropped()) {
        handle = libssh2_sftp_open(m_sftp, remoteBytes.constData(), flags, mode);
    }
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

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    bool hasRemoteMtime = (libssh2_sftp_fstat(handle, &attrs) == 0) && (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME);
    qint64 remoteMtime = hasRemoteMtime ? static_cast<qint64>(attrs.mtime) : 0;

    libssh2_sftp_close(handle);
    localFile.close();

    if (hasRemoteMtime) {
        QFile f(localPath);
        if (f.open(QIODevice::ReadWrite)) {
            f.setFileTime(QDateTime::fromSecsSinceEpoch(remoteMtime), QFileDevice::FileModificationTime);
            f.close();
        }
    }
    return true;
}

QString RemoteSession::getLocalCachePath(const QString &remotePath) const
{
    QMutexLocker locker(&m_sessionMutex);
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
                                         QString *errorMessage,
                                         std::function<bool()> cancelCheck,
                                         bool forceReDownload)
{
    outLocalPath = getLocalCachePath(remotePath);
    return downloadFile(remotePath, outLocalPath, progressCallback, errorMessage, cancelCheck, forceReDownload);
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
            struct RemoteRunItem { RemoteFileInfo info; int runNum; };
            std::vector<RemoteRunItem> gaspRuns;
            for (const auto &info : allEntries) {
                if (info.isDirectory) continue;
                if (info.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QString rest = info.name.mid(prefix.length());
                    bool ok = false;
                    int r = rest.toInt(&ok);
                    if (ok) {
                        gaspRuns.push_back({info, r});
                    }
                }
            }

            std::sort(gaspRuns.begin(), gaspRuns.end(), [](const RemoteRunItem &a, const RemoteRunItem &b) {
                return a.runNum < b.runNum;
            });
            for (const auto &item : gaspRuns) {
                outRunFiles.push_back(item.info);
            }
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
        struct RemoteRunItem { RemoteFileInfo info; int runNum; };
        std::vector<RemoteRunItem> standardRuns;
        for (const auto &info : allEntries) {
            if (info.isDirectory) continue;
            QFileInfo eInfo(info.name);
            if (eInfo.completeSuffix().toLower() == suffix && eInfo.baseName().startsWith(prefix, Qt::CaseInsensitive)) {
                QString rest = eInfo.baseName().mid(prefix.length());
                bool ok = false;
                int r = rest.toInt(&ok);
                if (ok) {
                    standardRuns.push_back({info, r});
                }
            }
        }

        std::sort(standardRuns.begin(), standardRuns.end(), [](const RemoteRunItem &a, const RemoteRunItem &b) {
            return a.runNum < b.runNum;
        });
        for (const auto &item : standardRuns) {
            outRunFiles.push_back(item.info);
        }
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
