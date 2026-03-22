/**
 * @file logging.h
 * @brief Simple logging utility with runtime verbose flag.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>

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

#endif  // LOGGING_H
