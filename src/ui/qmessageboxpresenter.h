#ifndef QMESSAGEBOXPRESENTER_H
#define QMESSAGEBOXPRESENTER_H

#include "imessagepresenter.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

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

    int confirm(QWidget *parent, const QString &title, const QString &message,
                const QList<DialogButton> &buttons, MessageIcon icon, int defaultIndex) override
    {
        QMessageBox msgBox(parent);
        msgBox.setWindowTitle(title);
        msgBox.setText(message);
        msgBox.setIcon(icon == MessageIcon::Question ? QMessageBox::Question
                                                     : QMessageBox::Warning);

        QList<QPushButton *> addedButtons;
        addedButtons.reserve(buttons.size());
        for (const auto &btn : buttons) {
            QMessageBox::ButtonRole qtRole = QMessageBox::RejectRole;
            switch (btn.role) {
            case ButtonRole::Accept:
                qtRole = QMessageBox::AcceptRole;
                break;
            case ButtonRole::Reject:
                qtRole = QMessageBox::RejectRole;
                break;
            case ButtonRole::Destructive:
                qtRole = QMessageBox::DestructiveRole;
                break;
            }
            auto *pushed = msgBox.addButton(btn.text, qtRole);
            addedButtons.append(pushed);
        }

        if (defaultIndex >= 0 && defaultIndex < addedButtons.size()) {
            msgBox.setDefaultButton(addedButtons[defaultIndex]);
        }

        msgBox.exec();

        QAbstractButton *clicked = msgBox.clickedButton();
        for (int i = 0; i < addedButtons.size(); ++i) {
            if (addedButtons[i] == clicked) {
                return i;
            }
        }
        return -1;
    }
};

#endif  // QMESSAGEBOXPRESENTER_H
