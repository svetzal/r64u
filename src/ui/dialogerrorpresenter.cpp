#include "dialogerrorpresenter.h"

#include <QMessageBox>
#include <QPushButton>

DialogErrorPresenter::DialogErrorPresenter(QWidget *parent) : parent_(parent) {}

void DialogErrorPresenter::showErrorDialog(const QString &title, const QString &message)
{
    QMessageBox::warning(parent_, title, message);
}

bool DialogErrorPresenter::showRetryDialog(const QString &title, const QString &message,
                                           const std::function<void()> &retryCallback)
{
    QMessageBox msgBox(parent_);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton *retryButton = msgBox.addButton(QStringLiteral("Retry"), QMessageBox::AcceptRole);
    msgBox.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(retryButton);
    msgBox.exec();

    if (msgBox.clickedButton() == retryButton) {
        if (retryCallback) {
            retryCallback();
        }
        return true;
    }
    return false;
}
