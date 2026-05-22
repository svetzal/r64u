#ifndef DIALOGERRORPRESENTER_H
#define DIALOGERRORPRESENTER_H

#include "services/ierrorpresenter.h"

class QWidget;

/**
 * @brief IErrorPresenter implementation that shows QMessageBox dialogs.
 *
 * Lives in the UI layer so that ErrorHandler (service layer) does not
 * depend on QMessageBox or QWidget.
 */
class DialogErrorPresenter : public IErrorPresenter
{
public:
    explicit DialogErrorPresenter(QWidget *parent = nullptr);

    void showErrorDialog(const QString &title, const QString &message) override;
    bool showRetryDialog(const QString &title, const QString &message,
                         const std::function<void()> &retryCallback) override;

private:
    QWidget *parent_ = nullptr;
};

#endif  // DIALOGERRORPRESENTER_H
