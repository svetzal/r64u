#ifndef ERRORTYPES_H
#define ERRORTYPES_H

#include <QMetaType>

/**
 * @brief Categories of errors for appropriate handling.
 */
enum class ErrorCategory {
    Connection,     ///< Network/connection errors (FTP, REST)
    FileOperation,  ///< File transfer, delete, listing errors
    Validation,     ///< Input validation, configuration errors
    System          ///< General system/application errors
};

/**
 * @brief Severity levels determining how errors are displayed.
 */
enum class ErrorSeverity {
    Info,     ///< Informational - status bar only, short timeout
    Warning,  ///< Warning - status bar, longer timeout
    Critical  ///< Critical - status bar + dialog box
};

#endif  // ERRORTYPES_H
