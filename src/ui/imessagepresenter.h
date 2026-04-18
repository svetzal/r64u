#ifndef IMESSAGEPRESENTER_H
#define IMESSAGEPRESENTER_H

#include <QString>

class QWidget;

/**
 * @brief Gateway interface for displaying modal message dialogs.
 *
 * Isolates controllers from QMessageBox so they can be tested without
 * displaying actual dialogs.
 */
class IMessagePresenter
{
public:
    virtual ~IMessagePresenter() = default;

    /**
     * @brief Shows an informational message dialog.
     * @param parent Parent widget for the dialog.
     * @param title Dialog window title.
     * @param message Body text of the message.
     */
    virtual void showInfo(QWidget *parent, const QString &title, const QString &message) = 0;

    /**
     * @brief Shows a warning message dialog.
     * @param parent Parent widget for the dialog.
     * @param title Dialog window title.
     * @param message Body text of the message.
     */
    virtual void showWarning(QWidget *parent, const QString &title, const QString &message) = 0;
};

#endif  // IMESSAGEPRESENTER_H
