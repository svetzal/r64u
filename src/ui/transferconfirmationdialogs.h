#ifndef TRANSFERCONFIRMATIONDIALOGS_H
#define TRANSFERCONFIRMATIONDIALOGS_H

#include "core/transfertypes.h"
#include "ui/imessagepresenter.h"

#include <QStringList>
#include <QWidget>

class TransferConfirmationDialogs
{
public:
    static transfer::OverwriteResponse askOverwrite(IMessagePresenter &presenter, QWidget *parent,
                                                    const QString &fileName);
    static transfer::FolderExistsResponse
    askFolderExists(IMessagePresenter &presenter, QWidget *parent, const QStringList &folderNames);
};

#endif  // TRANSFERCONFIRMATIONDIALOGS_H
