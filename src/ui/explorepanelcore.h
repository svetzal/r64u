#ifndef EXPLOREPANELCORE_H
#define EXPLOREPANELCORE_H

#include "services/devicetypes.h"
#include "services/filetypecore.h"

#include <QList>
#include <QString>

namespace explorepanel {

/// Computed enablement state for all toolbar/context actions.
struct ActionEnablement
{
    bool canPlay = false;
    bool canRun = false;
    bool canMount = false;
    bool canLoadConfig = false;
    bool canDownload = false;
    bool canRefresh = false;
    bool canGoUp = false;
};

/// Pure function: compute which actions should be enabled.
/// @param connected   true if device can perform operations
/// @param hasSelection true if a file or directory is selected
/// @param fileType    file type of the selected item
/// @param currentDir  current navigation directory (used to determine "go up" availability)
[[nodiscard]] ActionEnablement calculateActionEnablement(bool connected, bool hasSelection,
                                                         filetype::FileType fileType,
                                                         const QString &currentDir);

/// State of the two drive status widgets.
struct DriveDisplayState
{
    QString drive8Image;
    bool drive8Mounted = false;
    QString drive9Image;
    bool drive9Mounted = false;
};

/// Pure function: map a DriveInfo list to the display state for drive 8 and drive 9 widgets.
/// @param connected  true if device is connected and can report drive info
/// @param drives     list of drive info from the device
[[nodiscard]] DriveDisplayState calculateDriveDisplay(bool connected,
                                                      const QList<DriveInfo> &drives);

}  // namespace explorepanel

#endif  // EXPLOREPANELCORE_H
