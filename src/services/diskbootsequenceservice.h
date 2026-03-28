/**
 * @file diskbootsequenceservice.h
 * @brief Imperative shell that drives the disk-image boot sequence state machine.
 *
 * This service is the thin imperative wrapper around the pure
 * diskboot::State machine defined in diskbootsequencecore.h.  It owns a
 * QTimer, executes REST calls through IRestClient, and emits Qt signals to
 * report progress to the UI.
 *
 * ## Usage
 *
 * @code
 * auto *svc = new DiskBootSequenceService(this);
 * svc->setRestClient(deviceConnection->restClient());
 * connect(svc, &DiskBootSequenceService::statusMessage, statusBar, &QStatusBar::showMessage);
 * connect(svc, &DiskBootSequenceService::completed, this, []{ qDebug() << "boot done"; });
 *
 * svc->startBootSequence("/SD/games/Bubble Bobble.d64");
 * @endcode
 */

#ifndef DISKBOOTSEQUENCESERVICE_H
#define DISKBOOTSEQUENCESERVICE_H

#include "diskbootsequencecore.h"

#include <QObject>
#include <QTimer>

class IRestClient;

class DiskBootSequenceService : public QObject
{
    Q_OBJECT

public:
    explicit DiskBootSequenceService(QObject *parent = nullptr);
    ~DiskBootSequenceService() override = default;

    /**
     * @brief Sets the REST client used to send commands to the C64.
     * @param client Pointer to the REST client (not owned).
     */
    void setRestClient(IRestClient *client);

    /**
     * @brief Starts the boot sequence for a disk image using default timing config.
     * @param diskPath Remote path to the disk image file.
     */
    void startBootSequence(const QString &diskPath);

    /**
     * @brief Starts the boot sequence with custom timing configuration.
     * @param diskPath Remote path to the disk image file.
     * @param config Custom timing and command configuration.
     */
    void startBootSequence(const QString &diskPath, const diskboot::Config &config);

    /**
     * @brief Aborts the currently running boot sequence (if any).
     *
     * Stops the internal timer and transitions the state to Aborted.
     * Emits aborted() if a sequence was running.
     */
    void abort();

    /**
     * @brief Returns true if a boot sequence is currently in progress.
     */
    [[nodiscard]] bool isRunning() const { return running_; }

signals:
    /**
     * @brief Emitted when the current step changes during the sequence.
     * @param step The new current step.
     */
    void stepChanged(diskboot::Step step);

    /**
     * @brief Emitted with a user-facing description of the current step.
     * @param message The status message.
     * @param timeout Suggested display timeout in milliseconds (0 = persistent).
     */
    void statusMessage(const QString &message, int timeout = 0);

    /**
     * @brief Emitted when the full sequence completes successfully.
     */
    void completed();

    /**
     * @brief Emitted when the sequence is aborted before completion.
     */
    void aborted();

private:
    void executeCurrentStep();

    IRestClient *restClient_ = nullptr;
    diskboot::State state_;
    QTimer *stepTimer_ = nullptr;
    bool running_ = false;
};

#endif  // DISKBOOTSEQUENCESERVICE_H
