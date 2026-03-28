/**
 * @file diskbootsequencecore.cpp
 * @brief Implementation of pure disk boot sequence state machine functions.
 */

#include "diskbootsequencecore.h"

#include <QCoreApplication>

namespace diskboot {

StepAction actionForStep(const State &state)
{
    switch (state.currentStep) {
    case Step::MountDisk:
        return MountDiskCall{state.config.drive, state.diskPath};

    case Step::WaitForMount:
        return WaitMs{state.config.mountDelayMs};

    case Step::ResetMachine:
        return ResetMachineCall{};

    case Step::WaitForBoot:
        return WaitMs{state.config.bootDelayMs};

    case Step::TypeLoadCommand:
        return TypeTextCall{state.config.loadCommand};

    case Step::WaitForBuffer:
        return WaitMs{state.config.bufferDelayMs};

    case Step::SendReturn:
        return TypeTextCall{"\n"};

    case Step::WaitForLoad:
        return WaitMs{state.config.loadDelayMs};

    case Step::TypeRunCommand:
        return TypeTextCall{state.config.runCommand};

    case Step::Complete:
        return Done{true};

    case Step::Aborted:
        return Done{false};
    }

    return Done{false};
}

State advance(const State &state)
{
    // Terminal steps do not advance further
    if (state.currentStep == Step::Complete || state.currentStep == Step::Aborted) {
        return state;
    }

    State next = state;
    switch (state.currentStep) {
    case Step::MountDisk:
        next.currentStep = Step::WaitForMount;
        break;
    case Step::WaitForMount:
        next.currentStep = Step::ResetMachine;
        break;
    case Step::ResetMachine:
        next.currentStep = Step::WaitForBoot;
        break;
    case Step::WaitForBoot:
        next.currentStep = Step::TypeLoadCommand;
        break;
    case Step::TypeLoadCommand:
        next.currentStep = Step::WaitForBuffer;
        break;
    case Step::WaitForBuffer:
        next.currentStep = Step::SendReturn;
        break;
    case Step::SendReturn:
        next.currentStep = Step::WaitForLoad;
        break;
    case Step::WaitForLoad:
        next.currentStep = Step::TypeRunCommand;
        break;
    case Step::TypeRunCommand:
        next.currentStep = Step::Complete;
        break;
    case Step::Complete:
    case Step::Aborted:
        break;
    }
    return next;
}

State abort(const State &state)
{
    State aborted = state;
    aborted.currentStep = Step::Aborted;
    return aborted;
}

bool isTerminal(const State &state)
{
    return state.currentStep == Step::Complete || state.currentStep == Step::Aborted;
}

QString statusMessage(const State &state)
{
    switch (state.currentStep) {
    case Step::MountDisk:
        return QCoreApplication::translate("diskboot", "Mounting disk image: %1")
            .arg(state.diskPath);
    case Step::WaitForMount:
        return QCoreApplication::translate("diskboot", "Mounting and running: %1")
            .arg(state.diskPath);
    case Step::ResetMachine:
        return QCoreApplication::translate("diskboot", "Resetting C64...");
    case Step::WaitForBoot:
        return QCoreApplication::translate("diskboot", "Waiting for C64 to boot...");
    case Step::TypeLoadCommand:
    case Step::WaitForBuffer:
    case Step::SendReturn:
        return QCoreApplication::translate("diskboot", "Loading...");
    case Step::WaitForLoad:
        return QCoreApplication::translate("diskboot", "Loading from disk...");
    case Step::TypeRunCommand:
    case Step::Complete:
        return QCoreApplication::translate("diskboot", "Running disk image");
    case Step::Aborted:
        return QCoreApplication::translate("diskboot", "Boot sequence aborted");
    }
    return {};
}

}  // namespace diskboot
