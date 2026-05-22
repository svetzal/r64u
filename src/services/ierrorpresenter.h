#ifndef IERRORPRESENTER_H
#define IERRORPRESENTER_H

#include <QString>

#include <functional>

/**
 * @brief Abstract interface for presenting error dialogs.
 *
 * Decouples the service layer (ErrorHandler) from the UI layer (QMessageBox)
 * so that the service can be tested without a display.
 */
class IErrorPresenter
{
public:
    virtual ~IErrorPresenter() = default;
    virtual void showErrorDialog(const QString &title, const QString &message) = 0;
    virtual bool showRetryDialog(const QString &title, const QString &message,
                                 const std::function<void()> &retryCallback) = 0;
};

#endif  // IERRORPRESENTER_H
