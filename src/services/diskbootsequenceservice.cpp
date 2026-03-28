/**
 * @file diskbootsequenceservice.cpp
 * @brief Imperative shell implementation for the disk boot sequence service.
 */

#include "diskbootsequenceservice.h"

#include "services/irestclient.h"

DiskBootSequenceService::DiskBootSequenceService(QObject *parent)
    : QObject(parent), stepTimer_(new QTimer(this))
{
    stepTimer_->setSingleShot(true);
    connect(stepTimer_, &QTimer::timeout, this, &DiskBootSequenceService::executeCurrentStep);
}

void DiskBootSequenceService::setRestClient(IRestClient *client)
{
    restClient_ = client;
}

void DiskBootSequenceService::startBootSequence(const QString &diskPath)
{
    startBootSequence(diskPath, diskboot::Config{});
}

void DiskBootSequenceService::startBootSequence(const QString &diskPath,
                                                const diskboot::Config &config)
{
    // Abort any in-progress sequence silently
    if (running_) {
        stepTimer_->stop();
    }

    state_ = diskboot::State{};
    state_.diskPath = diskPath;
    state_.config = config;
    running_ = true;

    executeCurrentStep();
}

void DiskBootSequenceService::abort()
{
    if (!running_) {
        return;
    }

    stepTimer_->stop();
    state_ = diskboot::abort(state_);
    running_ = false;

    emit statusMessage(diskboot::statusMessage(state_));
    emit aborted();
}

void DiskBootSequenceService::executeCurrentStep()
{
    if (!running_) {
        return;
    }

    if (diskboot::isTerminal(state_)) {
        running_ = false;
        if (state_.currentStep == diskboot::Step::Complete) {
            emit completed();
        }
        return;
    }

    emit stepChanged(state_.currentStep);
    emit statusMessage(diskboot::statusMessage(state_));

    diskboot::StepAction action = diskboot::actionForStep(state_);
    state_ = diskboot::advance(state_);

    std::visit(
        [this](auto &&act) {
            using T = std::decay_t<decltype(act)>;

            if constexpr (std::is_same_v<T, diskboot::MountDiskCall>) {
                if (restClient_) {
                    restClient_->mountImage(act.drive, act.path);
                }
                executeCurrentStep();
            } else if constexpr (std::is_same_v<T, diskboot::ResetMachineCall>) {
                if (restClient_) {
                    restClient_->resetMachine();
                }
                executeCurrentStep();
            } else if constexpr (std::is_same_v<T, diskboot::TypeTextCall>) {
                if (restClient_) {
                    restClient_->typeText(act.text);
                }
                executeCurrentStep();
            } else if constexpr (std::is_same_v<T, diskboot::WaitMs>) {
                stepTimer_->start(act.milliseconds);
                // Timer fires → executeCurrentStep() via signal connection
            } else if constexpr (std::is_same_v<T, diskboot::Done>) {
                running_ = false;
                if (act.success) {
                    emit completed();
                } else {
                    emit aborted();
                }
            }
        },
        action);
}
