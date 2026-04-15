#include "transferconfirmationdialogs.h"

#include <QMessageBox>
#include <QPushButton>

OverwriteResponse TransferConfirmationDialogs::askOverwrite(QWidget *parent,
                                                            const QString &fileName)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(QObject::tr("File Already Exists"));
    msgBox.setText(QObject::tr("The file '%1' already exists.\n\n"
                               "Do you want to overwrite it?")
                       .arg(fileName));
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *overwriteButton =
        msgBox.addButton(QObject::tr("Overwrite"), QMessageBox::AcceptRole);
    QPushButton *overwriteAllButton =
        msgBox.addButton(QObject::tr("Overwrite All"), QMessageBox::AcceptRole);
    QPushButton *skipButton = msgBox.addButton(QObject::tr("Skip"), QMessageBox::RejectRole);
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(skipButton);
    msgBox.exec();

    QAbstractButton *clicked = msgBox.clickedButton();

    if (clicked == overwriteButton) {
        return OverwriteResponse::Overwrite;
    }
    if (clicked == overwriteAllButton) {
        return OverwriteResponse::OverwriteAll;
    }
    if (clicked == skipButton) {
        return OverwriteResponse::Skip;
    }
    return OverwriteResponse::Cancel;
}

FolderExistsResponse TransferConfirmationDialogs::askFolderExists(QWidget *parent,
                                                                  const QStringList &folderNames)
{
    QMessageBox msgBox(parent);

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

    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *mergeButton = msgBox.addButton(QObject::tr("Merge"), QMessageBox::AcceptRole);
    QPushButton *replaceButton =
        msgBox.addButton(QObject::tr("Replace"), QMessageBox::DestructiveRole);
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(mergeButton);
    msgBox.exec();

    QAbstractButton *clicked = msgBox.clickedButton();

    if (clicked == mergeButton) {
        return FolderExistsResponse::Merge;
    }
    if (clicked == replaceButton) {
        return FolderExistsResponse::Replace;
    }
    return FolderExistsResponse::Cancel;
}
