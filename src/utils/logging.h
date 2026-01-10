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

} // namespace r64u

/// Log only when verbose mode is enabled
#define LOG_VERBOSE() if (r64u::verboseLogging) qDebug()

#endif // LOGGING_H
