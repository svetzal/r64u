#ifndef METADATASERVICEBUNDLE_H
#define METADATASERVICEBUNDLE_H

class HttpFileDownloader;
class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;

/// Bundles all metadata-related download services into one aggregate.
/// These services are created together, share a common purpose (enriching file metadata),
/// and are always passed together to panels and dialogs.
struct MetadataServiceBundle
{
    HttpFileDownloader *songlengthsDownloader = nullptr;
    SonglengthsDatabase *songlengthsDatabase = nullptr;
    HttpFileDownloader *stilDownloader = nullptr;
    HttpFileDownloader *buglistDownloader = nullptr;
    HVSCMetadataService *hvscMetadataService = nullptr;
    HttpFileDownloader *gameBase64Downloader = nullptr;
    GameBase64Service *gameBase64Service = nullptr;
};

#endif  // METADATASERVICEBUNDLE_H
