#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include <QString>
#include <QDateTime>
#include <QSettings>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

// Forward declarations for libssh2 pointers to keep header clean
struct _LIBSSH2_SESSION;
typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
struct _LIBSSH2_SFTP;
typedef struct _LIBSSH2_SFTP LIBSSH2_SFTP;

enum class RemoteAuthType {
    KeyFile = 0,
    Password = 1,
    Agent = 2
};

struct RemoteProfile {
    QString        name;
    QString        host;
    int            port{22};
    QString        user;
    RemoteAuthType authType{RemoteAuthType::KeyFile};
    QString        keyFilePath;
    QString        password;        // Not persisted to disk for security unless requested
    QString        lastUsedPath;    // Persisted remote directory (e.g. /data/runs)

    static std::vector<RemoteProfile> loadAllProfiles();
    static void saveAllProfiles(const std::vector<RemoteProfile> &profiles);
    static void updateLastUsedPath(const QString &profileName, const QString &path);
};

struct RemoteFileInfo {
    QString   name;
    QString   fullPath;
    qint64    size{0};
    bool      isDirectory{false};
    QDateTime lastModified;
    uint32_t  permissions{0};
};

/**
 * @brief RemoteSession manages a secure SSH/SFTP connection to a remote data computer
 *        using libssh2, handling authentication, directory listing, and smart local file caching.
 */
class RemoteSession {
public:
    RemoteSession();
    ~RemoteSession();

    // Connection lifecycle
    bool connectToHost(const RemoteProfile &profile, QString *errorMessage = nullptr);
    void disconnect();
    bool isConnected() const;

    const RemoteProfile& getProfile() const { return m_profile; }
    QString getCurrentRemoteDir() const { return m_currentRemoteDir; }
    void setCurrentRemoteDir(const QString &dir);

    // SFTP Directory and File Operations
    bool listDirectory(const QString &remotePath, std::vector<RemoteFileInfo> &outList, QString *errorMessage = nullptr);
    bool downloadFile(const QString &remotePath, const QString &localPath,
                      std::function<void(qint64 bytesDone, qint64 bytesTotal)> progressCallback = nullptr,
                      QString *errorMessage = nullptr);
    bool uploadFile(const QString &localPath, const QString &remotePath,
                    std::function<void(qint64 bytesDone, qint64 bytesTotal)> progressCallback = nullptr,
                    QString *errorMessage = nullptr);

    // Staging & Caching Helper
    QString getLocalCachePath(const QString &remotePath) const;
    bool syncRemoteFileToCache(const QString &remotePath, QString &outLocalPath,
                              std::function<void(qint64, qint64)> progressCallback = nullptr,
                              QString *errorMessage = nullptr);

    // Query remote files in same directory matching run pattern (for next/prev run cycling)
    bool listRunFiles(const QString &remoteDirPath, const QString &currentFileName,
                      std::vector<RemoteFileInfo> &outRunFiles, QString *errorMessage = nullptr);

private:
    void cleanupSocketsAndSession();
    static QString sanitizePath(const QString &path);

    RemoteProfile     m_profile;
    int               m_socket{-1};
    LIBSSH2_SESSION  *m_session{nullptr};
    LIBSSH2_SFTP     *m_sftp{nullptr};
    bool              m_connected{false};
    QString           m_currentRemoteDir;
};

#endif // REMOTECLIENT_H
