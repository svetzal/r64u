#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>
#include <QLoggingCategory>

namespace r64u {

/// Global verbose logging flag, set via --verbose command line argument
inline bool verboseLogging = false;

}  // namespace r64u

/// @brief Returns a QDebug stream for verbose logging, or a no-op stream if not enabled.
///
/// Usage: LOG_VERBOSE() << "message";
///
/// The macro form is retained to enable the compiler to elide the entire
/// log expression when verbose mode is off, avoiding argument evaluation.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage) -- required: conditional no-op debug stream
#define LOG_VERBOSE()         \
    if (r64u::verboseLogging) \
    qDebug()

Q_DECLARE_LOGGING_CATEGORY(LogPlaylist)
Q_DECLARE_LOGGING_CATEGORY(LogFtp)
Q_DECLARE_LOGGING_CATEGORY(LogTransfer)
Q_DECLARE_LOGGING_CATEGORY(LogStreaming)
Q_DECLARE_LOGGING_CATEGORY(LogMetadata)
Q_DECLARE_LOGGING_CATEGORY(LogConfig)
Q_DECLARE_LOGGING_CATEGORY(LogDevice)
Q_DECLARE_LOGGING_CATEGORY(LogUi)
Q_DECLARE_LOGGING_CATEGORY(LogFileOps)

#endif  // LOGGING_H
