#ifndef DEVICEACTIONSERVICE_H
#define DEVICEACTIONSERVICE_H

#include <QObject>
#include <QString>

class DeviceConnectionManager;

/**
 * @brief Service wrapping file play, run, and mount REST operations.
 *
 * Mediates between FileActionController and IRestClient so that UI
 * components have no direct dependency on IRestClient.
 *
 * All methods are no-ops and emit operationNotAvailable() when the
 * device connection cannot perform operations.
 */
class DeviceActionService : public QObject
{
    Q_OBJECT

public:
    explicit DeviceActionService(DeviceConnectionManager *connection, QObject *parent = nullptr);

    /**
     * @brief Sends a command to play a SID music file.
     * @param path Remote path to the SID file.
     * @param songNumber Song index for multi-song SIDs; -1 selects the default song.
     */
    void playSid(const QString &path, int songNumber = -1);

    /**
     * @brief Sends a command to play a MOD music file.
     * @param path Remote path to the MOD file.
     */
    void playMod(const QString &path);

    /**
     * @brief Sends a command to run a PRG program file.
     * @param path Remote path to the PRG file.
     */
    void runPrg(const QString &path);

    /**
     * @brief Sends a command to run a CRT cartridge image.
     * @param path Remote path to the CRT file.
     */
    void runCrt(const QString &path);

    /**
     * @brief Sends a command to mount a disk image.
     * @param drive Drive identifier (e.g., "A").
     * @param path Remote path to the disk image.
     */
    void mountImage(const QString &drive, const QString &path);

    /**
     * @brief Returns true when the underlying device connection can perform operations.
     */
    [[nodiscard]] bool canPerformOperations() const;

signals:
    /**
     * @brief Emitted when an operation is attempted but the device is not connected.
     * @param operation Human-readable name of the attempted operation.
     */
    void operationNotAvailable(const QString &operation);

private:
    DeviceConnectionManager *deviceConnection_ = nullptr;
};

#endif  // DEVICEACTIONSERVICE_H
