#ifndef IERROREMITTER_H
#define IERROREMITTER_H

#include "errortypes.h"

#include <QObject>
#include <QString>

/**
 * @brief Abstract base class for objects that report errors via a uniform signal.
 *
 * All error sources inherit from IErrorEmitter instead of QObject directly,
 * gaining a single canonical errorReported() signal. ErrorHandler::registerSource()
 * wires this signal to handleError() in one call, replacing the manual per-signal
 * wiring in the legacy connectSources() helper methods.
 *
 * @par Example usage:
 * @code
 * class MyService : public IErrorEmitter {
 *     Q_OBJECT
 * public:
 *     explicit MyService(QObject *parent = nullptr) : IErrorEmitter(parent) {}
 *
 *     void doSomething() {
 *         if (failed) {
 *             emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
 *                                tr("Operation failed"), errorDetail);
 *         }
 *     }
 * };
 *
 * // Registration replaces multiple connect() calls:
 * errorHandler->registerSource(&myService);
 * @endcode
 */
class IErrorEmitter : public QObject
{
    Q_OBJECT

public:
    explicit IErrorEmitter(QObject *parent = nullptr) : QObject(parent) {}
    ~IErrorEmitter() override = default;

signals:
    /**
     * @brief Emitted to report an error through the uniform signal chain.
     * @param category The error category.
     * @param severity The error severity.
     * @param title Short error title/summary.
     * @param details Detailed error message (may be empty).
     */
    void errorReported(ErrorCategory category, ErrorSeverity severity, const QString &title,
                       const QString &details);
};

#endif  // IERROREMITTER_H
