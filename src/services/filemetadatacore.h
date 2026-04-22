#ifndef FILEMETADATACORE_H
#define FILEMETADATACORE_H

#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/sidfileparser.h"
#include "services/songlengthsdatabase.h"

#include <QString>

namespace filemetadata {

// ---------------------------------------------------------------------------
// SID metadata context
// ---------------------------------------------------------------------------

/**
 * @brief All data needed to compose the SID file details display string.
 *
 * Availability flags communicate service/database state so the pure function
 * can include appropriate diagnostic messages when metadata is unavailable.
 */
struct SidDisplayContext
{
    /// Parsed SID header (required; if !sidInfo.valid, format returns an error message)
    SidFileParser::SidInfo sidInfo;

    // ── Songlengths database ──────────────────────────────────────────────
    bool songlengthsServicePresent = false;  ///< Service is wired up (not null)
    bool songlengthsDatabaseLoaded = false;  ///< Database has been loaded from disk
    /// Songlengths lookup result (songLengths.found = false if DB not loaded or not found)
    SonglengthsDatabase::SongLengths songLengths;

    // ── HVSC STIL / BUGlist ───────────────────────────────────────────────
    bool stilLoaded = false;     ///< STIL database has been loaded
    bool buglistLoaded = false;  ///< BUGlist database has been loaded
    /// STIL lookup result (stilInfo.found = false if not loaded or not found)
    HVSCMetadataService::StilInfo stilInfo;
    /// BUGlist lookup result (bugInfo.found = false if not loaded or not found)
    HVSCMetadataService::BugInfo bugInfo;

    // ── GameBase64 ────────────────────────────────────────────────────────
    bool gameBase64ServicePresent = false;  ///< Service is wired up (not null)
    bool gameBase64DatabaseLoaded = false;  ///< Database has been loaded from disk
    /// GameBase64 lookup result (gameInfo.found = false if not loaded or not found)
    GameBase64Service::GameInfo gameInfo;
};

// ---------------------------------------------------------------------------
// Disk image metadata context
// ---------------------------------------------------------------------------

/**
 * @brief All data needed to compose the disk image directory display string.
 */
struct DiskDisplayContext
{
    /// Pre-formatted C64-style directory listing (from DiskImageReader::formatDirectoryListing)
    QString directoryListing;

    // ── GameBase64 ────────────────────────────────────────────────────────
    bool gameBase64ServicePresent = false;
    bool gameBase64DatabaseLoaded = false;
    GameBase64Service::GameInfo gameInfo;
};

// ---------------------------------------------------------------------------
// Pure format functions
// ---------------------------------------------------------------------------

/**
 * @brief Compose the full SID details display string from available metadata.
 *
 * Sections included:
 *  - Basic SID header info (always, from SidFileParser::formatForDisplay)
 *  - HVSC Songlengths (if database loaded; shows "not found" if lookup fails)
 *  - HVSC BUGlist warnings (if database loaded and bugs found)
 *  - HVSC STIL commentary (if database loaded and STIL entry found)
 *  - GameBase64 game info (if database loaded and game found)
 *
 * @param ctx All resolved metadata for this SID file.
 * @return Multi-line display string ready for QTextBrowser::setPlainText().
 */
[[nodiscard]] QString formatSidDetails(const SidDisplayContext &ctx);

/**
 * @brief Compose the full disk image details display string from available metadata.
 *
 * Sections included:
 *  - C64-style directory listing (always)
 *  - GameBase64 game info (if database loaded and game found)
 *
 * @param ctx All resolved metadata for this disk image.
 * @return Multi-line display string ready for QTextBrowser::setPlainText().
 */
[[nodiscard]] QString formatDiskDetails(const DiskDisplayContext &ctx);

}  // namespace filemetadata

#endif  // FILEMETADATACORE_H
