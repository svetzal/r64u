#ifndef DISKBOOTSEQUENCECORE_H
#define DISKBOOTSEQUENCECORE_H

#include <QString>

#include <variant>

namespace diskboot {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/**
 * @brief Configurable parameters for the boot sequence timing and commands.
 *
 * Default values match the standard C64 boot timing. Tests can inject
 * faster timings to avoid slow unit tests.
 */
struct Config
{
    int mountDelayMs = 500;   ///< Delay after mounting before reset
    int bootDelayMs = 3000;   ///< Delay for C64 boot ROM to complete
    int bufferDelayMs = 500;  ///< Delay for keyboard buffer to drain after LOAD command
    int loadDelayMs = 5000;   ///< Delay for the disk program to load
    QString drive = "a";      ///< Drive letter to mount to (default "a")
    QString loadCommand = "LOAD\"*\",8,1";  ///< LOAD command to type
    QString runCommand = "RUN\n";           ///< RUN command including newline
};

// ---------------------------------------------------------------------------
// State machine steps
// ---------------------------------------------------------------------------

/**
 * @brief Each step in the disk image boot sequence.
 *
 * Steps alternate between REST API calls and timer waits.
 */
enum class Step {
    MountDisk,        ///< Mount image to Drive A (REST: mountImage)
    WaitForMount,     ///< 500 ms delay after mount
    ResetMachine,     ///< Reset the C64 (REST: resetMachine)
    WaitForBoot,      ///< 3000 ms delay for boot ROM
    TypeLoadCommand,  ///< Type LOAD"*",8,1 into keyboard buffer (REST: typeText)
    WaitForBuffer,    ///< 500 ms delay for buffer to drain
    SendReturn,       ///< Send RETURN to execute LOAD (REST: typeText)
    WaitForLoad,      ///< 5000 ms delay for disk load
    TypeRunCommand,   ///< Type RUN + RETURN (REST: typeText)
    Complete,         ///< Sequence completed successfully
    Aborted           ///< Sequence was aborted
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

/**
 * @brief Complete mutable state of the boot sequence.
 *
 * All pure functions in this namespace take State by value and return new State.
 */
struct State
{
    Step currentStep = Step::MountDisk;
    QString diskPath;  ///< Path to the disk image being booted
    Config config;
};

// ---------------------------------------------------------------------------
// Step actions — what the imperative shell must do for each step
// ---------------------------------------------------------------------------

/** @brief REST call: mount a disk image on a drive. */
struct MountDiskCall
{
    QString drive;
    QString path;
};

/** @brief REST call: reset the C64 machine. */
struct ResetMachineCall
{
};

/** @brief REST call: type text into the C64 keyboard buffer. */
struct TypeTextCall
{
    QString text;
};

/** @brief Instruction to wait for a given number of milliseconds. */
struct WaitMs
{
    int milliseconds;
};

/** @brief The sequence has finished (successfully or aborted). */
struct Done
{
    bool success;
};

/**
 * @brief Union of all actions the imperative shell must execute for a given step.
 *
 * DiskBootSequenceService calls actionForStep(), executes the action
 * (REST call or QTimer::singleShot), then calls advance() to move to the next step.
 */
using StepAction = std::variant<MountDiskCall, ResetMachineCall, TypeTextCall, WaitMs, Done>;

// ---------------------------------------------------------------------------
// Pure functions
// ---------------------------------------------------------------------------

/**
 * @brief Determine what action the shell must perform for the current step.
 * @param state Current sequence state.
 * @return The action to execute (REST call, wait, or done).
 */
[[nodiscard]] StepAction actionForStep(const State &state);

/**
 * @brief Advance the state machine to the next step.
 *
 * Returns an identical State if already in a terminal step (Complete/Aborted).
 *
 * @param state Current sequence state.
 * @return New state with currentStep advanced by one.
 */
[[nodiscard]] State advance(const State &state);

/**
 * @brief Transition the sequence to Aborted, regardless of current step.
 * @param state Current sequence state.
 * @return New state with currentStep = Step::Aborted.
 */
[[nodiscard]] State abort(const State &state);

/**
 * @brief Returns true if the sequence has reached a terminal state.
 * @param state Current sequence state.
 * @return True for Step::Complete or Step::Aborted.
 */
[[nodiscard]] bool isTerminal(const State &state);

/**
 * @brief Returns a user-facing status message for the current step.
 * @param state Current sequence state.
 * @return Human-readable description of what is happening.
 */
[[nodiscard]] QString statusMessage(const State &state);

}  // namespace diskboot

#endif  // DISKBOOTSEQUENCECORE_H
