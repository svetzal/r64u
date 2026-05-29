/**
 * @file filedetailspanel_test_stubs.cpp
 * @brief Link-time stubs for FileDetailsPanel unit tests.
 *
 * FileDetailsPanel optionally uses GameBase64Service, HVSCMetadataService, and
 * SonglengthsDatabaseService.  These services have heavy dependencies (SQL, zlib, network).
 * The stubs here satisfy linker symbol requirements without pulling in those chains.
 * All method bodies return empty/default values.
 */

#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/songlengthsdatabaseservice.h"

// ---------------------------------------------------------------------------
// GameBase64Service stubs
// ---------------------------------------------------------------------------

GameBase64Service::GameBase64Service(IFileDownloaderService * /*downloader*/, QObject *parent)
    : QObject(parent)
{
}

GameBase64Service::~GameBase64Service() = default;

bool GameBase64Service::hasCachedDatabase() const
{
    return false;
}

QString GameBase64Service::databaseCacheFilePath() const
{
    return {};
}

GameBase64Service::GameInfo GameBase64Service::lookupByName(const QString & /*name*/) const
{
    return GameInfo{};
}

GameBase64Service::GameInfo GameBase64Service::lookupByFilename(const QString & /*filename*/) const
{
    return GameInfo{};
}

GameBase64Service::GameInfo
GameBase64Service::lookupBySidFilename(const QString & /*filename*/) const
{
    return GameInfo{};
}

GameBase64Service::SearchResults GameBase64Service::searchByName(const QString & /*query*/,
                                                                 int /*maxResults*/) const
{
    return SearchResults{};
}

GameBase64Service::SearchResults GameBase64Service::searchByMusician(const QString & /*musician*/,
                                                                     int /*maxResults*/) const
{
    return SearchResults{};
}

GameBase64Service::SearchResults GameBase64Service::searchByPublisher(const QString & /*publisher*/,
                                                                      int /*maxResults*/) const
{
    return SearchResults{};
}

void GameBase64Service::downloadDatabase() {}

void GameBase64Service::cancelDownload() {}

void GameBase64Service::loadFromCache() {}

void GameBase64Service::clearCache() {}

void GameBase64Service::openDatabase(const QString & /*path*/) {}

void GameBase64Service::closeDatabase() {}

bool GameBase64Service::decompressGzip(const QString & /*gzipPath*/, const QString & /*outputPath*/)
{
    return false;
}

// ---------------------------------------------------------------------------
// HVSCMetadataService stubs
// ---------------------------------------------------------------------------

HVSCMetadataService::HVSCMetadataService(IFileDownloaderService * /*stilDownloader*/,
                                         IFileDownloaderService * /*buglistDownloader*/,
                                         QObject *parent)
    : QObject(parent)
{
}

QString HVSCMetadataService::stilCacheFilePath() const
{
    return {};
}

QString HVSCMetadataService::buglistCacheFilePath() const
{
    return {};
}

bool HVSCMetadataService::hasCachedStil() const
{
    return false;
}

bool HVSCMetadataService::hasCachedBuglist() const
{
    return false;
}

bool HVSCMetadataService::loadStilFromCache()
{
    return false;
}

bool HVSCMetadataService::loadBuglistFromCache()
{
    return false;
}

HVSCMetadataService::StilInfo HVSCMetadataService::lookupStil(const QString & /*hvscPath*/) const
{
    return StilInfo{};
}

HVSCMetadataService::BugInfo HVSCMetadataService::lookupBuglist(const QString & /*hvscPath*/) const
{
    return BugInfo{};
}

void HVSCMetadataService::downloadStil() {}

void HVSCMetadataService::downloadBuglist() {}

bool HVSCMetadataService::parseStil(const QByteArray & /*data*/)
{
    return false;
}

bool HVSCMetadataService::parseStilFile(const QString & /*filePath*/)
{
    return false;
}

bool HVSCMetadataService::parseBuglist(const QByteArray & /*data*/)
{
    return false;
}

bool HVSCMetadataService::parseBuglistFile(const QString & /*filePath*/)
{
    return false;
}

// ---------------------------------------------------------------------------
// SonglengthsDatabaseService stubs
// ---------------------------------------------------------------------------

SonglengthsDatabaseService::SonglengthsDatabaseService(IFileDownloaderService * /*downloader*/,
                                                       QObject *parent)
    : QObject(parent), manager_(nullptr)
{
}

bool SonglengthsDatabaseService::hasCachedDatabase() const
{
    return false;
}

bool SonglengthsDatabaseService::loadFromCache()
{
    return false;
}

SonglengthsDatabaseService::SongLengths
SonglengthsDatabaseService::lookupByData(const QByteArray & /*sidData*/) const
{
    return {};
}

void SonglengthsDatabaseService::downloadDatabase() {}

bool SonglengthsDatabaseService::parseDatabaseData(const QByteArray & /*data*/)
{
    return false;
}

bool SonglengthsDatabaseService::parseDatabaseFile(const QString & /*filePath*/)
{
    return false;
}
