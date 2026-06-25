#include "transferconfirmationdialogs.h"

transfer::OverwriteResponse TransferConfirmationDialogs::askOverwrite(IMessagePresenter &presenter,
                                                                      QWidget *parent,
                                                                      const QString &fileName)
{
    const QList<IMessagePresenter::DialogButton> buttons = {
        {QObject::tr("Overwrite"), IMessagePresenter::ButtonRole::Accept},
        {QObject::tr("Overwrite All"), IMessagePresenter::ButtonRole::Accept},
        {QObject::tr("Skip"), IMessagePresenter::ButtonRole::Reject},
        {QObject::tr("Cancel"), IMessagePresenter::ButtonRole::Reject},
    };

    const QString title = QObject::tr("File Already Exists");
    const QString message = QObject::tr("The file '%1' already exists.\n\n"
                                        "Do you want to overwrite it?")
                                .arg(fileName);

    // Default to "Skip" (index 2)
    const int result = presenter.confirm(parent, title, message, buttons,
                                         IMessagePresenter::MessageIcon::Question, 2);

    switch (result) {
    case 0:
        return transfer::OverwriteResponse::Overwrite;
    case 1:
        return transfer::OverwriteResponse::OverwriteAll;
    case 2:
        return transfer::OverwriteResponse::Skip;
    default:
        return transfer::OverwriteResponse::Cancel;
    }
}

transfer::FolderExistsResponse
TransferConfirmationDialogs::askFolderExists(IMessagePresenter &presenter, QWidget *parent,
                                             const QStringList &folderNames)
{
    QString title;
    QString message;

    if (folderNames.size() == 1) {
        title = QObject::tr("Folder Already Exists");
        message = QObject::tr("The folder '%1' already exists on the remote device.\n\n"
                              "What would you like to do?")
                      .arg(folderNames.first());
    } else {
        title = QObject::tr("Folders Already Exist");
        message = QObject::tr("The following %1 folders already exist on the remote device:\n\n"
                              "%2\n\n"
                              "What would you like to do?")
                      .arg(folderNames.size())
                      .arg(folderNames.join("\n"));
    }

    const QList<IMessagePresenter::DialogButton> buttons = {
        {QObject::tr("Merge"), IMessagePresenter::ButtonRole::Accept},
        {QObject::tr("Replace"), IMessagePresenter::ButtonRole::Destructive},
        {QObject::tr("Cancel"), IMessagePresenter::ButtonRole::Reject},
    };

    // Default to "Merge" (index 0)
    const int result = presenter.confirm(parent, title, message, buttons,
                                         IMessagePresenter::MessageIcon::Question, 0);

    switch (result) {
    case 0:
        return transfer::FolderExistsResponse::Merge;
    case 1:
        return transfer::FolderExistsResponse::Replace;
    default:
        return transfer::FolderExistsResponse::Cancel;
    }
}
