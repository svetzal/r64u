#ifndef EXPLOREPANELSERVICES_H
#define EXPLOREPANELSERVICES_H

class DeviceActionService;
class ConfigFileLoaderService;
class FilePreviewService;
class FavoritesService;
class PlaylistService;

/**
 * @brief Bundles the application services required by ExplorePanel.
 *
 * Groups services that are always provided together so the constructor
 * parameter count stays manageable. All pointers must be non-null.
 */
struct ExplorePanelServices
{
    DeviceActionService *deviceActionService = nullptr;
    ConfigFileLoaderService *configLoader = nullptr;
    FilePreviewService *previewService = nullptr;
    FavoritesService *favoritesService = nullptr;
    PlaylistService *playlistService = nullptr;
};

#endif  // EXPLOREPANELSERVICES_H
