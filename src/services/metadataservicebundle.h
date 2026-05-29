#ifndef METADATASERVICEBUNDLE_H
#define METADATASERVICEBUNDLE_H

class HttpFileDownloaderService;
class SonglengthsDatabaseService;
class HVSCMetadataService;
class GameBase64Service;

/// Bundles all metadata-related download services into one aggregate.
/// These services are created together, share a common purpose (enriching file metadata),
/// and are always passed together to panels and dialogs.
struct MetadataServiceBundle
{
    HttpFileDownloaderService *songlengthsDownloader = nullptr;
    SonglengthsDatabaseService *songlengthsDatabase = nullptr;
    HttpFileDownloaderService *stilDownloader = nullptr;
    HttpFileDownloaderService *buglistDownloader = nullptr;
    HVSCMetadataService *hvscMetadataService = nullptr;
    HttpFileDownloaderService *gameBase64Downloader = nullptr;
    GameBase64Service *gameBase64Service = nullptr;
};

#endif  // METADATASERVICEBUNDLE_H
