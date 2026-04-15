#ifndef TRANSFERCONFIRMATIONDIALOGS_H
#define TRANSFERCONFIRMATIONDIALOGS_H

#include "models/transferqueue.h"

#include <QStringList>
#include <QWidget>

class TransferConfirmationDialogs
{
public:
    static OverwriteResponse askOverwrite(QWidget *parent, const QString &fileName);
    static FolderExistsResponse askFolderExists(QWidget *parent, const QStringList &folderNames);
};

#endif  // TRANSFERCONFIRMATIONDIALOGS_H
