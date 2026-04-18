#ifndef QMESSAGEBOXPRESENTER_H
#define QMESSAGEBOXPRESENTER_H

#include "imessagepresenter.h"

#include <QMessageBox>

/**
 * @brief Production implementation of IMessagePresenter using QMessageBox.
 */
class QMessageBoxPresenter : public IMessagePresenter
{
public:
    void showInfo(QWidget *parent, const QString &title, const QString &message) override
    {
        QMessageBox::information(parent, title, message);
    }

    void showWarning(QWidget *parent, const QString &title, const QString &message) override
    {
        QMessageBox::warning(parent, title, message);
    }
};

#endif  // QMESSAGEBOXPRESENTER_H
