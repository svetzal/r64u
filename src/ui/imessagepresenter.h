#ifndef IMESSAGEPRESENTER_H
#define IMESSAGEPRESENTER_H

#include <QList>
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
    /**
     * @brief Icon style for confirmation dialogs.
     */
    enum class MessageIcon {
        Question,  ///< Question mark icon (non-destructive choice).
        Warning,   ///< Warning icon (destructive or irreversible action).
    };

    /**
     * @brief Semantic role of a dialog button.
     */
    enum class ButtonRole {
        Accept,       ///< Positive / accept action.
        Reject,       ///< Cancel / reject action.
        Destructive,  ///< Irreversible destructive action.
    };

    /**
     * @brief A labeled button with a semantic role for use in confirm().
     */
    struct DialogButton
    {
        QString text;                          ///< Label shown on the button.
        ButtonRole role = ButtonRole::Accept;  ///< Semantic role of the button.
    };

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

    /**
     * @brief Shows a multi-button confirmation dialog and returns the chosen button index.
     *
     * @param parent      Parent widget for the dialog.
     * @param title       Dialog window title.
     * @param message     Body text presented to the user.
     * @param buttons     Ordered list of buttons to display.
     * @param icon        Icon style shown in the dialog.
     * @param defaultIndex Index of the button that receives the default/Enter key.
     * @return Index of the clicked button within @p buttons, or -1 if the dialog was
     *         dismissed without clicking any listed button (e.g. window close).
     */
    virtual int confirm(QWidget *parent, const QString &title, const QString &message,
                        const QList<DialogButton> &buttons, MessageIcon icon, int defaultIndex) = 0;
};

#endif  // IMESSAGEPRESENTER_H
