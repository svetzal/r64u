#include "explorepanelcore.h"

namespace explorepanel {

ActionEnablement calculateActionEnablement(bool connected, bool hasSelection,
                                           filetype::FileType fileType, const QString &currentDir)
{
    ActionEnablement e;
    auto caps = filetype::capabilities(fileType);
    e.canPlay = connected && hasSelection && caps.canPlay;
    e.canRun = connected && hasSelection && caps.canRun;
    e.canMount = connected && hasSelection && caps.canMount;
    e.canLoadConfig = connected && hasSelection && caps.canLoadConfig;
    e.canDownload = connected && hasSelection;
    e.canRefresh = connected;
    e.canGoUp = connected && !currentDir.isEmpty() && currentDir != QLatin1String("/");
    return e;
}

DriveDisplayState calculateDriveDisplay(bool connected, const QList<DriveInfo> &drives)
{
    DriveDisplayState state;
    if (!connected) {
        return state;
    }
    for (const DriveInfo &drive : drives) {
        bool hasDisk = !drive.imageFile.isEmpty();
        if (drive.name.toLower() == QLatin1String("a")) {
            state.drive8Image = drive.imageFile;
            state.drive8Mounted = hasDisk;
        } else if (drive.name.toLower() == QLatin1String("b")) {
            state.drive9Image = drive.imageFile;
            state.drive9Mounted = hasDisk;
        }
    }
    return state;
}

}  // namespace explorepanel
