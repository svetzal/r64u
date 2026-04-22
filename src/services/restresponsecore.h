#ifndef RESTRESPONSECORE_H
#define RESTRESPONSECORE_H

#include "devicetypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace restresponse {

// ---------------------------------------------------------------------------
// Error extraction
// ---------------------------------------------------------------------------

/**
 * @brief Extracts error strings from a JSON response object.
 *
 * Looks for an @c "errors" JSON array and returns each non-empty string in it.
 *
 * @param json The root JSON object from the API response.
 * @return List of error strings; empty if no errors are present.
 */
[[nodiscard]] QStringList extractErrors(const QJsonObject &json);

// ---------------------------------------------------------------------------
// Device information
// ---------------------------------------------------------------------------

/**
 * @brief Parses a version response.
 * @param json Root JSON object from @c /v1/version.
 * @return Version string, or empty if the key is absent.
 */
[[nodiscard]] QString parseVersionResponse(const QJsonObject &json);

/**
 * @brief Parses a device info response.
 * @param json Root JSON object from @c /v1/info.
 * @return Populated DeviceInfo; fields default to empty string when absent.
 */
[[nodiscard]] DeviceInfo parseInfoResponse(const QJsonObject &json);

// ---------------------------------------------------------------------------
// Drive information
// ---------------------------------------------------------------------------

/**
 * @brief Parses a drives response.
 * @param json Root JSON object from @c /v1/drives.
 * @return List of DriveInfo; empty if the drives array is absent or empty.
 */
[[nodiscard]] QList<DriveInfo> parseDrivesResponse(const QJsonObject &json);

// ---------------------------------------------------------------------------
// File information
// ---------------------------------------------------------------------------

/**
 * @brief Result of parsing a file info response.
 */
struct FileInfoResult
{
    QString path;       ///< Remote file path
    qint64 size = 0;    ///< File size in bytes
    QString extension;  ///< File extension
};

/**
 * @brief Parses a file info response.
 * @param json Root JSON object from @c /v1/files/:path:info.
 * @return Populated FileInfoResult; fields default to empty/zero when absent.
 */
[[nodiscard]] FileInfoResult parseFileInfoResponse(const QJsonObject &json);

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/**
 * @brief Parses a configuration categories response.
 * @param json Root JSON object from @c /v1/configs.
 * @return List of category name strings; empty if the array is absent.
 */
[[nodiscard]] QStringList parseConfigCategoriesResponse(const QJsonObject &json);

/**
 * @brief Parses a configuration category items response.
 *
 * Handles all value types: bool, integer (whole double), fractional double, string.
 * Also parses optional fields: values (enum), presets (files), min/max range, format.
 *
 * @param category The category name (used as the top-level JSON key).
 * @param json     Root JSON object from @c /v1/configs/:category/\* endpoint.
 * @return Map from item name to ConfigItemMetadata.
 */
[[nodiscard]] QHash<QString, ConfigItemMetadata>
parseConfigCategoryItemsResponse(const QString &category, const QJsonObject &json);

}  // namespace restresponse

#endif  // RESTRESPONSECORE_H
