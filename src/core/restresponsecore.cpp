/**
 * @file restresponsecore.cpp
 * @brief Implementation of pure REST API JSON response parsing functions.
 */

#include "restresponsecore.h"

#include <QJsonArray>
#include <QJsonValue>

namespace restresponse {

QStringList extractErrors(const QJsonObject &json)
{
    QStringList errors;
    QJsonArray errorsArray = json["errors"].toArray();
    for (const QJsonValue &error : errorsArray) {
        QString errorStr = error.toString();
        if (!errorStr.isEmpty()) {
            errors.append(errorStr);
        }
    }
    return errors;
}

QString parseVersionResponse(const QJsonObject &json)
{
    return json["version"].toString();
}

DeviceInfo parseInfoResponse(const QJsonObject &json)
{
    DeviceInfo info;
    info.product = json["product"].toString();
    info.firmwareVersion = json["firmware_version"].toString();
    info.fpgaVersion = json["fpga_version"].toString();
    info.coreVersion = json["core_version"].toString();
    info.hostname = json["hostname"].toString();
    info.uniqueId = json["unique_id"].toString();
    return info;
}

QList<DriveInfo> parseDrivesResponse(const QJsonObject &json)
{
    QList<DriveInfo> drives;
    QJsonArray drivesArray = json["drives"].toArray();

    for (const QJsonValue &driveVal : drivesArray) {
        QJsonObject driveObj = driveVal.toObject();
        // Each drive is an object with a single key (drive name)
        for (const QString &driveName : driveObj.keys()) {
            QJsonObject driveData = driveObj[driveName].toObject();
            DriveInfo drive;
            drive.name = driveName;
            drive.enabled = driveData["enabled"].toBool();
            drive.busId = driveData["bus_id"].toInt();
            drive.type = driveData["type"].toString();
            drive.rom = driveData["rom"].toString();
            drive.imageFile = driveData["image_file"].toString();
            drive.imagePath = driveData["image_path"].toString();
            drive.lastError = driveData["last_error"].toString();
            drives.append(drive);
        }
    }

    return drives;
}

FileInfoResult parseFileInfoResponse(const QJsonObject &json)
{
    QJsonObject files = json["files"].toObject();
    FileInfoResult result;
    result.path = files["path"].toString();
    result.size = files["size"].toInteger();
    result.extension = files["extension"].toString();
    return result;
}

QStringList parseConfigCategoriesResponse(const QJsonObject &json)
{
    QStringList categories;
    QJsonArray categoriesArray = json["categories"].toArray();
    for (const QJsonValue &val : categoriesArray) {
        categories.append(val.toString());
    }
    return categories;
}

QHash<QString, ConfigItemMetadata> parseConfigCategoryItemsResponse(const QString &category,
                                                                    const QJsonObject &json)
{
    QHash<QString, ConfigItemMetadata> items;

    // The response has the category name as the key containing item objects
    QJsonObject categoryObj = json[category].toObject();
    for (auto it = categoryObj.begin(); it != categoryObj.end(); ++it) {
        QString itemName = it.key();
        QJsonObject itemObj = it.value().toObject();

        ConfigItemMetadata meta;

        // Parse current value
        QJsonValue currentVal = itemObj["current"];
        if (currentVal.isBool()) {
            meta.current = currentVal.toBool();
        } else if (currentVal.isDouble()) {
            double d = currentVal.toDouble();
            if (d == qRound(d)) {
                meta.current = static_cast<int>(d);
            } else {
                meta.current = d;
            }
        } else {
            meta.current = currentVal.toString();
        }

        // Parse default value
        QJsonValue defaultVal = itemObj["default"];
        if (defaultVal.isBool()) {
            meta.defaultValue = defaultVal.toBool();
        } else if (defaultVal.isDouble()) {
            double d = defaultVal.toDouble();
            if (d == qRound(d)) {
                meta.defaultValue = static_cast<int>(d);
            } else {
                meta.defaultValue = d;
            }
        } else {
            meta.defaultValue = defaultVal.toString();
        }

        // Parse values (enum options)
        if (itemObj.contains("values")) {
            QJsonArray valuesArray = itemObj["values"].toArray();
            for (const QJsonValue &val : valuesArray) {
                meta.values.append(val.toString());
            }
        }

        // Parse presets (file options)
        if (itemObj.contains("presets")) {
            QJsonArray presetsArray = itemObj["presets"].toArray();
            for (const QJsonValue &val : presetsArray) {
                meta.presets.append(val.toString());
            }
        }

        // Parse numeric range
        if (itemObj.contains("min") && itemObj.contains("max")) {
            meta.min = itemObj["min"].toInt();
            meta.max = itemObj["max"].toInt();
            meta.hasRange = true;
        }

        // Parse format string
        if (itemObj.contains("format")) {
            meta.format = itemObj["format"].toString();
        }

        items[itemName] = meta;
    }

    return items;
}

}  // namespace restresponse
