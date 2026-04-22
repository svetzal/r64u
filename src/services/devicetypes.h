#ifndef DEVICETYPES_H
#define DEVICETYPES_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief Device information returned by the Ultimate API.
 */
struct DeviceInfo
{
    QString product;          ///< Product name (e.g., "Ultimate 64")
    QString firmwareVersion;  ///< Firmware version string
    QString fpgaVersion;      ///< FPGA core version
    QString coreVersion;      ///< Core version string
    QString hostname;         ///< Network hostname
    QString uniqueId;         ///< Unique device identifier
    QString apiVersion;       ///< REST API version
};

/**
 * @brief Metadata for a configuration item including available options.
 */
struct ConfigItemMetadata
{
    QVariant current;       ///< Current value
    QVariant defaultValue;  ///< Default value
    QStringList values;     ///< Available options for enum types
    QStringList presets;    ///< Preset options for file-type items
    int min = 0;            ///< Minimum value for numeric types
    int max = 0;            ///< Maximum value for numeric types
    QString format;         ///< Format string (e.g., "%d" or "%d00 ms")
    bool hasRange = false;  ///< True if min/max are valid
};

/**
 * @brief Information about a single drive on the device.
 */
struct DriveInfo
{
    QString name;          ///< Drive identifier (e.g., "A", "B")
    bool enabled = false;  ///< Whether the drive is enabled
    int busId = 0;         ///< IEC bus device number
    QString type;          ///< Drive type (1541, 1571, 1581, etc.)
    QString rom;           ///< Current ROM image
    QString imageFile;     ///< Filename of mounted disk image
    QString imagePath;     ///< Full path to mounted image
    QString lastError;     ///< Last error message, if any
};

#endif  // DEVICETYPES_H
