#ifndef MOCKRESTCLIENT_H
#define MOCKRESTCLIENT_H

// This mock replaces C64URestClient for testing.
// It provides the same interface but allows controlling responses.

#include <QObject>
#include <QJsonObject>

// Forward declare the structs that the real header defines
struct DeviceInfo {
    QString product;
    QString firmwareVersion;
    QString fpgaVersion;
    QString coreVersion;
    QString hostname;
    QString uniqueId;
    QString apiVersion;
};

struct ConfigItemMetadata {
    QVariant current;
    QVariant defaultValue;
    QStringList values;
    QStringList presets;
    int min = 0;
    int max = 0;
    QString format;
    bool hasRange = false;
};

struct DriveInfo {
    QString name;
    bool enabled = false;
    int busId = 0;
    QString type;
    QString rom;
    QString imageFile;
    QString imagePath;
    QString lastError;
};

/**
 * Mock replacement for C64URestClient.
 * Named C64URestClient so ConfigFileLoader can use it directly.
 */
class C64URestClient : public QObject
{
    Q_OBJECT

public:
    static constexpr int ErrorResponsePreviewLength = 200;
    static constexpr int RequestTimeoutMs = 15000;

    explicit C64URestClient(QObject *parent = nullptr);
    ~C64URestClient() override = default;

    void setHost(const QString &host);
    QString host() const { return host_; }
    void setPassword(const QString &password);
    bool hasPassword() const { return !password_.isEmpty(); }

    // Device info
    void getVersion();
    void getInfo();

    // Content playback
    void playSid(const QString &filePath, int songNumber = -1);
    void playMod(const QString &filePath);
    void loadPrg(const QString &filePath);
    void runPrg(const QString &filePath);
    void runCrt(const QString &filePath);

    // Drive control
    void getDrives();
    void mountImage(const QString &drive, const QString &imagePath,
                    const QString &mode = "readwrite");
    void unmountImage(const QString &drive);
    void resetDrive(const QString &drive);

    // Machine control
    void resetMachine();
    void rebootMachine();
    void pauseMachine();
    void resumeMachine();
    void powerOffMachine();
    void pressMenuButton();
    void writeMem(const QString &address, const QByteArray &data);
    void typeText(const QString &text);

    // File operations
    void getFileInfo(const QString &path);
    void createD64(const QString &path, const QString &diskName = QString(),
                   int tracks = 35);
    void createD81(const QString &path, const QString &diskName = QString());

    // Configuration
    void getConfigCategories();
    void getConfigCategoryItems(const QString &category);
    void getConfigItem(const QString &category, const QString &item);
    void setConfigItem(const QString &category, const QString &item,
                       const QVariant &value);
    void updateConfigsBatch(const QJsonObject &configs);
    void saveConfigToFlash();
    void loadConfigFromFlash();
    void resetConfigToDefaults();

    // === Mock control methods ===
    void mockEmitConfigsUpdated();
    void mockEmitOperationFailed(const QString &operation, const QString &error);

    // Track what was called for assertions
    QJsonObject mockLastUpdateConfigsBatchArg() const { return lastConfigsBatch_; }

signals:
    void versionReceived(const QString &version);
    void infoReceived(const DeviceInfo &info);
    void drivesReceived(const QList<DriveInfo> &drives);
    void fileInfoReceived(const QString &path, qint64 size, const QString &extension);
    void configCategoriesReceived(const QStringList &categories);
    void configCategoryItemsReceived(const QString &category,
                                     const QHash<QString, ConfigItemMetadata> &items);
    void configItemReceived(const QString &category, const QString &item,
                            const QVariant &value);
    void configItemSet(const QString &category, const QString &item);
    void configsUpdated();
    void configSavedToFlash();
    void configLoadedFromFlash();
    void configResetToDefaults();
    void operationSucceeded(const QString &operation);
    void operationFailed(const QString &operation, const QString &error);
    void connectionError(const QString &error);

private:
    QString host_;
    QString password_;
    QJsonObject lastConfigsBatch_;
};

#endif // MOCKRESTCLIENT_H
