#ifndef ISTREAMCONTROLSERVICE_H
#define ISTREAMCONTROLSERVICE_H

#include <QObject>
#include <QString>

/**
 * @brief Abstract interface for controlling C64 Ultimate streaming sessions.
 *
 * Concrete implementations include StreamControlService (production) and
 * MockStreamControlService (test).
 */
class IStreamControlService : public QObject
{
    Q_OBJECT

public:
    explicit IStreamControlService(QObject *parent = nullptr) : QObject(parent) {}
    ~IStreamControlService() override = default;

    /**
     * @brief Sets the target C64 Ultimate host.
     * @param host Hostname or IP address of the device.
     */
    virtual void setHost(const QString &host) = 0;

    /**
     * @brief Returns the currently configured host.
     */
    [[nodiscard]] virtual QString host() const = 0;

    /**
     * @brief Starts both video and audio streams to the specified target.
     * @param targetHost IP address to receive the streams.
     * @param videoPort UDP port for video.
     * @param audioPort UDP port for audio.
     */
    virtual void startAllStreams(const QString &targetHost, quint16 videoPort,
                                 quint16 audioPort) = 0;

    /**
     * @brief Stops both video and audio streams.
     */
    virtual void stopAllStreams() = 0;

    /**
     * @brief Clears any pending commands without sending them.
     */
    virtual void clearPendingCommands() = 0;

signals:
    /**
     * @brief Emitted when a command succeeds.
     * @param command Description of the command that succeeded.
     */
    void commandSucceeded(const QString &command);

    /**
     * @brief Emitted when a command fails.
     * @param command Description of the command that failed.
     * @param error Error description.
     */
    void commandFailed(const QString &command, const QString &error);
};

#endif  // ISTREAMCONTROLSERVICE_H
