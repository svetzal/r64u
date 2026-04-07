#ifndef FILEACTIONCONTROLLER_H
#define FILEACTIONCONTROLLER_H

#include "services/filetypecore.h"

#include <QObject>
#include <QString>

class DeviceConnection;
class ConfigFileLoader;
class DiskBootSequenceService;
class StreamingManager;
class QAction;

class FileActionController : public QObject
{
    Q_OBJECT

public:
    explicit FileActionController(DeviceConnection *connection, ConfigFileLoader *configLoader,
                                  QObject *parent = nullptr);

    void setStreamingManager(StreamingManager *manager);
    void setActions(QAction *play, QAction *run, QAction *mount);

    void updateActionStates(filetype::FileType type, bool canOperate);

public slots:
    void play(const QString &path, filetype::FileType type);
    void run(const QString &path, filetype::FileType type);
    void mountToDrive(const QString &path, const QString &drive);
    void loadConfig(const QString &path, filetype::FileType type);
    void download(const QString &path);

signals:
    void statusMessage(const QString &message, int timeout = 0);

private:
    void runDiskImage(const QString &path);
    void ensureStreamingStarted();

    DeviceConnection *deviceConnection_ = nullptr;
    ConfigFileLoader *configFileLoader_ = nullptr;
    StreamingManager *streamingManager_ = nullptr;
    DiskBootSequenceService *bootService_ = nullptr;

    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
};

#endif  // FILEACTIONCONTROLLER_H
